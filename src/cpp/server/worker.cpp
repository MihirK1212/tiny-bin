#include <iostream>
#include <thread>

#include "worker.h"

Worker::Worker(int id, int port) : id(id), port(port) {}

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

void Worker::run() {
    int server_fd = -1;
    event_loop = std::make_unique<EventLoop>(server_fd);
    std::cout << " Worker number "<<id<<" is running ..." << std::endl;
    event_loop->run();
}