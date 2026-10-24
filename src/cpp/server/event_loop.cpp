#include <iostream>
#include <thread>
#include <chrono>

#include "event_loop.h"

EventLoop::EventLoop(int server_fd) : server_fd(server_fd), running(false) {}

EventLoop::~EventLoop() {}

void EventLoop::run() {
    running.store(true);
    std::cout << "Event loop started..." << std::endl;
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void EventLoop::stop() {
    running.store(false);
    std::cout << "Event loop stopped" << std::endl;
}