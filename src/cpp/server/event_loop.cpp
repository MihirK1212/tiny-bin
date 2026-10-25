#include <cassert>
#include <iostream>
#include <sys/poll.h>

#include "event_loop.h"
#include "connection.h"

EventLoop::EventLoop(int server_fd) : server_fd(server_fd), running(true) {}

EventLoop::~EventLoop() {}

void EventLoop::run() {
    std::cout << "Event loop started... on server fd " << server_fd << std::endl;

    /*
        1. fd2conn maintains a map from each client connection fd to the Connection object
        2. using fd2conn and server_fd, create poll_args list where poll_args[0] = server_pfd, other_poll_args[i] = client_pfds
        3. each pfd contains -> {fd, event, revent}
        4. call poll() using the poll_args
        5. use the poll_args[i].revents from each poll_arg after poll to handle accordingly 
    */

    // map all all client connections, keyed by fd
    std::vector<Connection*> fd2conn;

    // pollfd is the communication template with the poll() call
    // it stores fd, events (request) and revents (response)
    std::vector<struct pollfd> poll_args;
    
    while (running.load()) {
        poll_args.clear();

        // add server poll fd to poll args list
        // accept() is treated as read() in readiness notifications, so it uses POLLIN
        struct pollfd server_pfd = {server_fd, POLLIN, 0};
        poll_args.push_back(server_pfd);

        // add all client connections to poll args list
        for(Connection* conn: fd2conn) {
            if(!conn) {continue;}

            struct pollfd client_pfd = {conn->fd, 0, 0};

            if(conn->want_read) {
                client_pfd.events |= POLLIN;
            }
            if(conn->want_write) {
                client_pfd.events |= POLLOUT;
            }

            poll_args.push_back(client_pfd);
        }

        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if(rv < 0 && errno == EINTR) {
            continue; // not an error
        }
        if(rv < 0) {
            throw std::system_error(errno, std::generic_category(), "poll");
        }

        // handle the server socket for new connections
        if(poll_args[0].revents & POLLIN) {
            if(Connection* conn = Connection::handle_accept(server_fd)) {
                if(fd2conn.size() <= (size_t)conn->fd) {
                    fd2conn.resize((size_t)conn->fd + 1);
                }
                fd2conn[conn->fd] = conn;
            }
        }

        // handle client connection sockets
        for(size_t i=1; i < poll_args.size(); i++) {
            uint32_t ready = poll_args[i].revents;
            if(ready == 0) {
                continue;
            }

            Connection* conn = fd2conn[poll_args[i].fd];

            if(ready & POLLIN) {
                assert(conn->want_read);
                std::vector<ReadResponse> requests = conn->handle_read();
                if (!conn->want_close) {
                    for(ReadResponse& request: requests) {
                        if(!request.success || !request.data.has_value()) {
                            continue;
                        }

                        const std::vector<uint8_t>& request_data = request.data.value();

                        std::cout << "Server received: ";
                        for(uint32_t byte : request_data) {
                            std::cout << (char)byte << " ";
                        }
                        std::cout << std::endl;

                        std::vector<uint8_t> response_data;
                        response_data.reserve(6 + request_data.size());
                        response_data.insert(response_data.end(), {'P', 'O', 'N', 'G', ':', ' '});
                        response_data.insert(response_data.end(), request_data.begin(), request_data.end());
                        
                        // echo response
                        conn->enqueue_response(response_data);
                    }
                }
            }

            if(!conn->want_close && (ready & POLLOUT)) {
                assert(conn->want_write);
                conn->handle_write();
            }

            if ((ready & POLLERR) || (ready & POLLHUP) || conn->want_close) {
                close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }
    }

    // cleanup remaining connections on shutdown
    for (Connection* conn : fd2conn) {
        if (conn) {
            close(conn->fd);
            delete conn;
        }
    }
}

void EventLoop::stop() {
    running.store(false);
    if (server_fd >= 0) {
        shutdown(server_fd, SHUT_RDWR);
    }
    std::cout << "Event loop stopped on server fd " << server_fd << std::endl;
}