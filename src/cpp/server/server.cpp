#include <thread>
#include <chrono>
#include <iostream>

#include "server.h"

TinyBinServer::TinyBinServer(int port, int num_workers) {
    if(port <= 0) {
        throw std::invalid_argument("Port must be greater than 0");
    }
    if(num_workers <= 0) {
        throw std::invalid_argument("Number of workers must be greater than 0");
    }
    
    this->port = port;
    this->num_workers = num_workers;
}

void TinyBinServer::start() {
    std::cout << "Server is running on port " << this->port << " with " << this->num_workers << " workers" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

