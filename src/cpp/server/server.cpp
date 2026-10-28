#include <iostream>
#include <csignal>
#include <pthread.h>
#include <memory>
#include <stdexcept>
#include <utility>

#include "server.h"


Server::Server(int port, int num_workers, Router router)
    : port(port), num_workers(num_workers), router(std::move(router)) {
    if(port <= 0) {
        throw std::invalid_argument("Port must be greater than 0");
    }
    if(num_workers <= 0) {
        throw std::invalid_argument("Number of workers must be greater than 0");
    }

    setup_signal_handlers();

    for(int i=0; i<this->num_workers; i++) {
        this->workers.push_back(std::make_unique<Worker>(i, this->port, this->router)); 
    }
}

void Server::start_workers() {
    for(auto& worker : this->workers) {
        worker->start();
    }
    std::cout << "Server is running on port " << this->port << " with " << this->num_workers << " workers" << std::endl;
}

void Server::stop_workers() {
    for(auto& worker : this->workers) {
        worker->stop();
    }
}

/*
    sigset_t set => bitmap of signal numbers 
    sigaddset(&set, SIGINT) and sigaddset(&set, SIGTERM) => flags for Ctrl+C (SIGINT) or kill (SIGTERM) signals

    before workers are created (in setup_signal_handlers):
        - we run pthread_sigmask(SIG_BLOCK, &set, nullptr)
        - this blocks the SIGINT and SIGTERM signals on the main server thread 
        - the worker threads, when spawned, will inherit the blocked signals configuration from the main server thread

    after workers are created (in block_until_shutdown):
        - sigwait(&set, &sig) waits for shutdown signals i.e. SIGINT and SIGTERM signals are enabled again 
        - these are only eenabled for the main server thread, and remain disabled on the worker threads 

    thus, Ctrl+C / SIGTERM do not run the kill handler on a random worker thread
    and the main server thread alone waits for a shutdown signal in block_until_shutdown
*/

void Server::block_until_shutdown() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    int sig = 0;
    if (sigwait(&set, &sig) != 0) {
        throw std::runtime_error("sigwait failed while waiting for shutdown");
    }
    std::cerr << "Received signal " << sig << ", shutting down\n";
}

void Server::join_workers() {
    for(auto& worker : this->workers) {
        worker->join();
    }
}

void Server::setup_signal_handlers() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0) {
        throw std::runtime_error("failed to block SIGINT/SIGTERM");
    }

    // instead of terminating the process on SIGPIPE, ignore it
    // SIGPIPE occurs when a write operation is attempted by server on a closed pipe or socket (client disconnected)
    // in this case, we want to ignore it and continue the server
    signal(SIGPIPE, SIG_IGN);
}
