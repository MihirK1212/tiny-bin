#ifndef CONNECTION_H
#define CONNECTION_H

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <sys/types.h>
#include <vector>
#include <asm-generic/socket.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>
#include <netinet/tcp.h> 
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <sys/poll.h>
#include <unistd.h>
#include <optional>
#include <utility>

#include "socket_utils.h"
#include "http/parse.h"

const size_t k_idle_buf_cap = 64 * 1024;

enum class ConnectionState {
    INIT,
    READING, // reading from connection 
    PARSING, // parsing the incoming bytes
    PROCESSING, // processing a parsed request
    WRITING, // writing to connection
    WRITE_FLUSH, // there is data in outgoing_bytes to write
    WAITING_CLOSE, // waiting for close to complete
    CLOSED, // connection is closed
};

/*
- READING: messages are read from the connection socket and put into the incoming_bytes buffer 
- PARSING: messages are parsed from the incoming_bytes buffer 
- PROCESSING: messages are processed 
- WRITING: outgoing_bytes are written to the connection socket. keep writing until all the outgoing_bytes have been written
- WRITE_FLUSH: remaining outgoing_bytes are written to the connection socket and then the connection is closed
- WAITING_CLOSE: the connection is waiting for close to complete
- CLOSED: the connection is closed

Some important state machine rules:
- INIT -> READING: new connection is accepted 
- The only way to move to the READING state again is :
    - from the PARSING state if there are no more parsable message but EOF / invalid request was not reached
    - from the WRITING state after all the outgoing_bytes have been written
- general rule: enqueued responses in outgoing_bytes will all be written to the connection before moving back to READING state 
- WRITE_FLUSH means that there is data in outgoing_bytes that needs to be drained (flushed) before closing. no new data will be read 
- never go into READING state if new reads are blocked 
*/

struct ConnectionStateManager {
    ConnectionState state = ConnectionState::INIT;

    void transition(ConnectionState new_state) {
        state = new_state;
    }
};

struct Connection {
    int fd = -1;

    ConnectionStateManager state_manager;

    Connection(int fd) : fd(fd) {
        state_manager.transition(ConnectionState::INIT);
    }
    
    std::vector<uint8_t> incoming_bytes; // bytes received from the connection
    std::vector<uint8_t> outgoing_bytes; // bytes to be written into the connection

    std::deque<HTTPRequest> parsed_requests; // requests that have been parsed from the incoming_bytes buffer

    bool reached_eof = false;
    bool reached_invalid_request = false; // set when an oversized payload is seen; drain existing responses before closing

    /*
        - keep filling data into the `incoming_bytes` buffer (READ)
        - after ever fill, do `try_one_request()` to see if we have valid request data then remove its data from the incoming buffer (PARSE)
        - after parsing all valid requests, process one valid request (PROCESS)
    */

    void read_from_socket() {
        if(!(get_state() == ConnectionState::READING)) {
            throw std::logic_error("read_from_socket called in invalid state");
        }
 
        if(new_reads_blocked()) { 
            throw std::logic_error("read_from_socket called when new reads are blocked");
        }

        uint8_t data[64*1024];
        ssize_t rv = 0;
        do {
            rv = read(fd, data, sizeof(data));
        } while (rv < 0 && errno == EINTR);

        if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        if (rv < 0) {
            state_manager.transition(ConnectionState::WAITING_CLOSE);
            return;
        }

        // EOF
        if (rv == 0) {
            reached_eof = true; 
            assert(outgoing_bytes.empty()); // outgoing_bytes should be empty at this point since we go into READING state again only after writing all outgoing_bytes

            if(!incoming_bytes.empty()) {
                state_manager.transition(ConnectionState::PARSING);
            }
            else if(!parsed_requests.empty()) {
                state_manager.transition(ConnectionState::PROCESSING);
            }
            else {
                state_manager.transition(ConnectionState::WAITING_CLOSE);
            }
            return;
        }

        size_t len = (size_t)rv;
        buf_append(incoming_bytes, data, len);

        state_manager.transition(ConnectionState::PARSING);
    }

    void parse_incoming_buffer() {
        if(!(get_state() == ConnectionState::PARSING)) {
            throw std::logic_error("parse_incoming_buffer called in invalid state");
        }
        
        while(true) {
            HTTPRequest request = try_one_request();
            if(!request.success || !request.data.has_value()) {
                break;
            }
            parsed_requests.push_back(std::move(request));
        }

        if(!parsed_requests.empty()) {
            state_manager.transition(ConnectionState::PROCESSING);
        }
        else if (new_reads_blocked()) {
            assert (outgoing_bytes.empty()); // outgoing_bytes should be empty at this point since we go into READING->PARSING state again only after writing all outgoing_bytes

            // no more parasable message and EOF / invalid request was reached
            state_manager.transition(ConnectionState::WAITING_CLOSE);
        } else {
            // no more parsable messages but EOF was not reached. continue reading
            state_manager.transition(ConnectionState::READING);
        } 
    }

    std::vector<HTTPRequest> pop_parsed_requests(size_t num_requests) {
        if(get_state() != ConnectionState::PROCESSING) {
            throw std::logic_error("pop_parsed_requests called in invalid state");
        }

        const size_t count = std::min(num_requests, parsed_requests.size());
        std::vector<HTTPRequest> requests;
        requests.reserve(count);
        for(size_t i = 0; i < count; i++) {
            requests.push_back(std::move(parsed_requests.front()));
            parsed_requests.pop_front();
        }
        return requests;
    }

    void write_to_socket() {
        if(!(get_state() == ConnectionState::WRITING)  && !(get_state() == ConnectionState::WRITE_FLUSH)) {
            throw std::logic_error("write_to_socket called in invalid state");
        }

        assert(!outgoing_bytes.empty());

        ssize_t rv = 0;
        do {
            rv = write(fd, outgoing_bytes.data(), outgoing_bytes.size());
        } while (rv < 0 && errno == EINTR);

        if(rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return; // actually not ready
        }
        if(rv < 0) {
            state_manager.transition(ConnectionState::WAITING_CLOSE);
            return;
        }

        buf_consume(outgoing_bytes, (size_t)rv);

        if(outgoing_bytes.size() == 0) {
            if(get_state() == ConnectionState::WRITE_FLUSH) {
                state_manager.transition(ConnectionState::WAITING_CLOSE);
            }
            else if(get_state() == ConnectionState::WRITING) {
                if(!parsed_requests.empty()) {
                    state_manager.transition(ConnectionState::PROCESSING);
                }
                else if(!new_reads_blocked()) {
                    state_manager.transition(ConnectionState::READING);
                }
            }
            else {
                throw std::logic_error("write_to_socket called in invalid state");
            }
        }
    }

    HTTPRequest try_one_request() {
        if(!(get_state() == ConnectionState::PARSING)) {
            throw std::logic_error("try_one_request called in invalid state");
        }

        HTTPRequest request = parse_one_request(incoming_bytes);
        if (request.invalid_request) {
            // signal that the connection should be closed after flushing any already-parsed responses
            reached_invalid_request = true;
            return request;
        }

        if (request.success && request.bytes_consumed > 0) {
            // remove message from incoming
            buf_consume(incoming_bytes, request.bytes_consumed);
        }

        return request;
    }

    void enqueue_response(const std::vector<uint8_t> &data) {
        if(get_state() != ConnectionState::PROCESSING) {
            throw std::logic_error("enqueue_response called in invalid state");
        }

        uint32_t len = htonl(data.size());
        buf_append(outgoing_bytes, (const uint8_t *)&len, 4);
        buf_append(outgoing_bytes, data.data(), data.size());
    }

    void mark_processing_complete() {
        if(get_state() != ConnectionState::PROCESSING) {
            throw std::logic_error("mark_processing_complete called in invalid state");
        }
        if(outgoing_bytes.empty()) {
            throw std::logic_error("mark_processing_complete called with empty outgoing_bytes");
        }
        if(parsed_requests.empty() && new_reads_blocked()) {
            // no more pipelined requests to process and new reads are blocked, so we need to flush the outgoing_bytes and close the connection
            state_manager.transition(ConnectionState::WRITE_FLUSH);
        } else {
            state_manager.transition(ConnectionState::WRITING);
        }
    }

    static Connection* handle_accept(int server_fd) {
        sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);

        int connfd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if(connfd < 0) {
            return NULL;
        }

        if (!set_nonblocking(connfd)) {
            int error = errno;
            close(connfd);
            errno = error;
            return NULL;
        }

        Connection* conn = new Connection(connfd);
        conn->state_manager.transition(ConnectionState::READING);
        return conn;
    }

    ConnectionState get_state() const {
        return state_manager.state;
    }

    bool wants_read() const {
        return get_state() == ConnectionState::READING;
    }

    bool wants_parse() const {
        return get_state() == ConnectionState::PARSING;
    }

    bool wants_write() const {
        return get_state() == ConnectionState::WRITING || get_state() == ConnectionState::WRITE_FLUSH;
    }
    
    bool wants_close() const {
        return get_state() == ConnectionState::WAITING_CLOSE;
    }

    bool wants_processing() const {
        return get_state() == ConnectionState::PROCESSING;
    }

    bool new_reads_blocked() const {
        return reached_eof || reached_invalid_request;
    }

    void request_close() {
        if(get_state() == ConnectionState::CLOSED) {
            throw std::logic_error("request_close called in invalid state");
        }
        state_manager.transition(ConnectionState::WAITING_CLOSE);
    }

    void close_connection() {
        if(get_state() != ConnectionState::WAITING_CLOSE) {
            throw std::logic_error("close_connection called in invalid state");
        }
        close(fd);
        fd = -1;
        state_manager.transition(ConnectionState::CLOSED);
    }

    static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
        buf.insert(buf.end(), data, data + len);
    }

    static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
        buf.erase(buf.begin(), buf.begin() + n);
        
        // buf erase doesnt free up capacity so we do it explicitly
        if (buf.empty() && buf.capacity() > k_idle_buf_cap) {
            buf.shrink_to_fit();
        }
    }
};

#endif
