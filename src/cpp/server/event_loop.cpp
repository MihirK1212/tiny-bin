#include <cassert>
#include <iostream>
#include <sys/poll.h>

#include "event_loop.h"
#include "connection.h"
#include "http/handler.h"

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

            if(conn->wants_read()) {
                client_pfd.events |= POLLIN;
            }
            if(conn->wants_write()) {
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

        /*
            READ:
                - READ: read new data from the connection socket. 
                - If EOF is reached during READ, move to PARSING (if incoming_bytes is not empty) or PROCESSING (if parsed_requests is not empty) else WAITING_CLOSE
            PARSE:
                - PARSE: if data present in incoming_bytes, parse it and keep filling the parsed_requests queue 
                - if parsed_requests is not empty, move to PROCESSING 
                - if no parsed_requests left and new reads are blocked, move to WAITING_CLOSE 
                - else if no more parasable message but new reads are not blocked, move to READING 

            PROCESSING: 
                - process one valid request from the parsed_requests queue and write the response to the connection socket
                - in normal case, move to WRITING so that the response is written to the connection socket 
                - but if there are no more pipelined requests to process and new reads are blocked, move to WRITE_FLUSH (flush remaining output and then close)

            WRITE:
                - keep writing the outgoing_bytes to the connection socket
                - we stay in WRITE for possibly multiple "poll" calls until all the outgoing_bytes have been written 
                - once all the outgoing_bytes have been written, move to PROCESSING again to process any remaining pipelined request 
                - if all the outgoing_bytes have been written and no more pipelined requests to process, move to READING to read new data

            WRITE_FLUSH: 
               - keep writing the outgoing_bytes to the connection socket
               - we stay in WRITE for possibly multiple "poll" calls until all the outgoing_bytes have been written  
               - once all the outgoing_bytes have been written, move to WAITING_CLOSE
            
            - PROCESSING again
                - any remaining pipelined request transitioned to PROCESSING after WRITE 
                - same behavior as PROCESSING state 
        */

        // handle client connection sockets
        for(size_t i=1; i < poll_args.size(); i++) {
            uint32_t ready = poll_args[i].revents;

            Connection* conn = fd2conn[poll_args[i].fd];
            
            // READ
            if(conn->wants_read() && (ready & (POLLIN | POLLHUP))) {
                assert(conn->wants_read());
                conn->read_from_socket();
            }

            // PARSE
            if (!conn->wants_close() && conn->wants_parse()) {
                conn->parse_incoming_buffer();
            }

            // PROCESS
            process_connection(conn);
            
            // WRITE
            if(!conn->wants_close() && conn->wants_write() && (ready & (POLLOUT | POLLHUP))) {
                assert(conn->wants_write());
                conn->write_to_socket();
            }

            // PROCESS any remaining pipelined request transitioned to PROCESSING after WRITE
            process_connection(conn);

            // CLOSE
            if ((ready & (POLLERR | POLLNVAL)) || conn->wants_close()) {
                int fd = conn->fd;
                if (!conn->wants_close()) {
                    conn->request_close();
                }
                conn->close_connection();
                fd2conn[fd] = NULL;
                delete conn;
            }
        }
    }

    // cleanup remaining connections on shutdown
    for (Connection* conn : fd2conn) {
        if (conn) {
            if (conn->get_state() != ConnectionState::CLOSED) {
                if (!conn->wants_close()) {
                    conn->request_close();
                }
                conn->close_connection();
            }
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


void EventLoop::process_connection(Connection* conn) {
    if (!conn->wants_processing()) {
        return;
    }

    std::vector<HTTPRequest> requests = conn->pop_parsed_requests(1);
    for (HTTPRequest& request : requests) {
        if (!request.success || !request.data.has_value()) {
            throw std::logic_error("Invalid request in parsed_requests");
        }

        std::vector<uint8_t> response_data = handle_request(request);

        // echo response
        conn->enqueue_response(response_data);
    }

    conn->mark_processing_complete();
}