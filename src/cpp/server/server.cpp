#include <thread>
#include <chrono>
#include <iostream>

#include "server.h"

TinyBinServer::TinyBinServer() {
    // Initialize server
}

void TinyBinServer::start() {
    std::cout << "Server is running..." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

