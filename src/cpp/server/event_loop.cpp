#include <iostream>
#include <thread>
#include <chrono>

#include "event_loop.h"

EventLoop::EventLoop(int server_fd) : server_fd(server_fd), running(true) {}

EventLoop::~EventLoop() {}

void EventLoop::run() {
    std::cout << "Event loop started... on server fd " << server_fd << std::endl;
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void EventLoop::stop() {
    running.store(false);
    std::cout << "Event loop stopped on server fd " << server_fd << std::endl;
}