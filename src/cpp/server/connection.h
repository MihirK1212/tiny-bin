#ifndef CONNECTION_H
#define CONNECTION_H

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
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

const size_t k_max_msg = 32 << 20; 
const size_t k_idle_buf_cap = 64 * 1024;

struct ReadResponse {
    bool success;
    std::optional<std::vector<uint8_t>> data;
};

enum class ConnectionState {
    INIT,
    READING, // reading from connection 
    READ_FLUSH, // waiting for all read responses to be enqueued 
    WRITING, // writing to connection
    WRITE_FLUSH, // there is data in outgoing_bytes to write
    WAITING_CLOSE, // waiting for close to complete
    CLOSED, // connection is closed
};

/*
State machine transitions:
- INIT -> READING: new connection is accepted 
- general rule: enqueued responses will all be written to the connection before moving back to READING state 
- READING -> WRITE_FLUSH: there is no new data to read (EOF) but outgoing_bytes is not empty 
- WRITE_FLUSH means that there is data in outgoing_bytes that needs to be drained (flushed) before closing. no new data will be read
- READ_FLUSH: inside try_one_request, if there is an invalid large payload, flush all the previously parsed requests and enqueu them
- once state moves to READ_FLUSH, no new data will be read. the responses will be enqueed and state will move to either WRITE_FLUSH or WATING_CLOSE 
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

    std::vector<ReadResponse> handle_read() {
        /*
            - keep filling data into the `incoming_bytes` buffer
            - after ever fill, do `try_one_request()` to see if we have valid request data
            - inside each `try_one_request`, process one valid request and then remove its data from the incoming buffer 
        */

        if(!(get_state() == ConnectionState::READING)) {
            throw std::logic_error("handle_read called in invalid state");
        }

        uint8_t data[64*1024];
        ssize_t rv = 0;
        do {
            rv = read(fd, data, sizeof(data));
        } while (rv < 0 && errno == EINTR);

        if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return {};
        }
        if (rv < 0) {
            state_manager.transition(ConnectionState::WAITING_CLOSE);
            return {};
        }

        // EOF
        if (rv == 0) {
            if(outgoing_bytes.empty()) {
                state_manager.transition(ConnectionState::WAITING_CLOSE);
            } 
            else {
                // A peer may finish sending (eof) while it is still waiting for replies.
                state_manager.transition(ConnectionState::WRITE_FLUSH);
            }
            return {};
        }

        size_t len = (size_t)rv;
        buf_append(incoming_bytes, data, len);

        std::vector<ReadResponse> responses;

        while(get_state() == ConnectionState::READING) {
            ReadResponse response = try_one_request();
            if(!response.success || !response.data.has_value()) {
                break;
            }
            responses.push_back(std::move(response));
        }
        
        return responses;
    }

    void handle_write() {
        if(!(get_state() == ConnectionState::WRITING)  && !(get_state() == ConnectionState::WRITE_FLUSH)) {
            throw std::logic_error("handle_write called in invalid state");
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
                state_manager.transition(ConnectionState::READING);
            }
            else {
                throw std::logic_error("handle_write called in invalid state");
            }
        }
    }

    ReadResponse try_one_request() {
        /*
            for now, assume that message is of the following format:
            4 bytes -> len of message
            followed by
            `len` bytes -> actual message        
        */

        if(!(get_state() == ConnectionState::READING)) {
            throw std::logic_error("try_one_request called in invalid state");
        }

        if(incoming_bytes.size() < 4) {
            return ReadResponse{false, std::nullopt};
        }

        uint32_t len_network = 0;
        memcpy(&len_network, incoming_bytes.data(), 4);
        uint32_t len = ntohl(len_network);

        if(len > k_max_msg) {
            state_manager.transition(ConnectionState::READ_FLUSH);
            return ReadResponse{false, std::nullopt};
        }

        if(incoming_bytes.size() < (4 + len)) {
            return ReadResponse{false, std::nullopt};
        }

        // store the response in a vector
        std::vector<uint8_t> data(incoming_bytes.begin() + 4,
                                  incoming_bytes.begin() + 4 + len);

        // remove message from incoming
        buf_consume(incoming_bytes, 4 + len);

        return ReadResponse{true, std::move(data)};
    }

    void enqueue_response(const std::vector<uint8_t> &data) {
        if(get_state() != ConnectionState::READING && get_state() != ConnectionState::READ_FLUSH) {
            throw std::logic_error("enqueue_response called in invalid state");
        }

        uint32_t len = htonl(data.size());
        buf_append(outgoing_bytes, (const uint8_t *)&len, 4);
        buf_append(outgoing_bytes, data.data(), data.size());
    }

    void mark_read_complete() {
        if(get_state() == ConnectionState::READ_FLUSH) {
            // A protocol error ends the read side. Flush replies already queued, or close now.
            if(outgoing_bytes.empty()) {
                state_manager.transition(ConnectionState::WAITING_CLOSE);
            } else {
                state_manager.transition(ConnectionState::WRITE_FLUSH);
            }
        }
        else if(get_state() == ConnectionState::READING) {
            // A partial read or EAGAIN has nothing to send, so remain in READING state if outgoing_bytes is empty
            // if it is not empty, the transition to WRITING
            if(!outgoing_bytes.empty()) {
                state_manager.transition(ConnectionState::WRITING);
            } 
        }
        else if(get_state() == ConnectionState::WRITE_FLUSH) {
            // EOF already chose flush-then-close.
        }
        else {
            throw std::logic_error("mark_read_complete called in invalid state");
        }
    }

    void request_close() {
        if(get_state() == ConnectionState::CLOSED) {
            throw std::logic_error("request_close called in invalid state");
        }
        state_manager.transition(ConnectionState::WAITING_CLOSE);
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
    bool wants_write() const {
        return get_state() == ConnectionState::WRITING || get_state() == ConnectionState::WRITE_FLUSH;
    }
    bool wants_close() const {
        return get_state() == ConnectionState::WAITING_CLOSE;
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
