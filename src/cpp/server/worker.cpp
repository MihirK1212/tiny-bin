#include <iostream>
#include <thread>
#include <memory>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include "worker.h"
#include "socket_utils.h"

Worker::Worker(int id, int port) : id(id), port(port) {
    setup_event_loop();
}

Worker::~Worker() {
    stop();
    join();
}

void Worker::start() {
    /*
        `this` needs to be passed to a thread created from member function
        it is equivalent of this->run()
    */
    thread = std::thread(&Worker::run, this);
    std::cout << "Started worker " << this->id << " on port " << this->port << std::endl;
}

void Worker::stop() {
    if (event_loop) {
        event_loop->stop();
    }
    std::cout << "Stopped worker " << this->id << " on port " << this->port << std::endl;
}

void Worker::join() {
    if (thread.joinable()) {
        std::cout << "Joining worker " << this->id << " on port " << this->port << std::endl;
        thread.join();
    }
}

void Worker::setup_event_loop() {
    server_fd = create_socket_and_bind(port);
    event_loop = std::make_unique<EventLoop>(server_fd);
}

void Worker::run() {
    try {
        event_loop->run();
        close(server_fd);
    } catch (const std::exception& e) {
        fprintf(stderr, "Worker %d fatal error: %s\n", id, e.what());
    }
}