#ifndef CONNECTION_H
#define CONNECTION_H

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
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

#include "socket_utils.h"

const size_t k_max_msg = 32 << 20; 

struct ReadResponse {
    bool success;
    std::optional<std::vector<uint8_t>> data;
};


struct Connection {
    int fd = -1;
    
    bool want_read = false; // there is something in  the connection that needs to be read by event loop
    bool want_write = false; // something needs to be written to the connection 
    bool want_close = false;

    std::vector<uint8_t> incoming_bytes; // bytes received from the connection
    std::vector<uint8_t> outgoing_bytes; // bytes to be written into the connection

    std::vector<ReadResponse> handle_read() {
        /*
            - keep filling data into the `incoming_bytes` buffer
            - after ever fill, do `try_one_request()` to see if we have valid request data
            - inside each `try_one_request`, process one valid request and then remove its data from the incoming buffer 
        */

        uint8_t data[64*1024];
        ssize_t rv = read(fd, data, sizeof(data));
        if (rv<0 && errno == EAGAIN) {
            return {};
        }
        if (rv == 0) {
            want_close = true;
            return {};
        }
        if (rv < 0) {
            want_close = true;
            return {};
        }

        size_t len = (size_t)rv;
        buf_append(incoming_bytes, data, len);

        std::vector<ReadResponse> responses;

        while(true) {
            ReadResponse response = try_one_request();
            if(!response.success || !response.data.has_value()) {
                break;
            }
            responses.push_back(response);
        }
        
        return responses;
    }

    void handle_write() {
        assert(outgoing_bytes.size() > 0);

        ssize_t rv = write(fd, outgoing_bytes.data(), outgoing_bytes.size());

        if(rv<0 && errno == EAGAIN) {
            return; // actually not ready
        }
        if(rv < 0) {
            want_close = true;
            return;
        }

        buf_consume(outgoing_bytes, (size_t)rv);

        if(outgoing_bytes.size() == 0) {
            want_read = true;
            want_write = false;
        }
    }

    ReadResponse try_one_request() {
        /*
            for now, assume that message is of the following format:
            4 bytes -> len of message
            followed by
            `len` bytes -> actual message        
        */

        if(incoming_bytes.size() < 4) {
            return ReadResponse{false, std::nullopt};
        }

        uint32_t len_network = 0;
        memcpy(&len_network, incoming_bytes.data(), 4);
        uint32_t len = ntohl(len_network);

        if(len > k_max_msg) {
            want_close = true;
            return ReadResponse{false, std::nullopt};
        }

        if(incoming_bytes.size() < (4 + len)) {
            return ReadResponse{false, std::nullopt};
        }

        const uint8_t *read_data = &incoming_bytes[4];

        // store the response in a vector
        std::vector<uint8_t> data(len);
        memcpy(data.data(), read_data, len);

        // remove message from incoming
        buf_consume(incoming_bytes, 4 + len);

        return ReadResponse{true, data};
    }

    void enqueue_response(std::vector<uint8_t> data) {
        uint32_t len = htonl(data.size());
        buf_append(outgoing_bytes, (const uint8_t *)&len, 4);
        buf_append(outgoing_bytes, data.data(), data.size());
        want_read = false;
        want_write = true;
    }

    static Connection* handle_accept(int server_fd) {
        sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);

        int connfd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if(connfd < 0) {
            return NULL;
        }

        set_nonblocking(connfd);

        Connection* conn = new Connection();
        conn->fd = connfd;
        conn->want_read = true;
        return conn;
    }

    static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
        buf.insert(buf.end(), data, data + len);
    }

    static void buf_consume(std::vector<uint8_t> &buf, int n) {
        buf.erase(buf.begin(), buf.begin() + n);
    }
};

#endif