#ifndef WORKER_H
#define WORKER_H

#include <memory>
#include <string>
#include <thread>

#include "event_loop.h"
#include "http/router.h"

class Worker { 
    public: 
        Worker(int id, const std::string& host, int port, const Router& router);
        ~Worker();

        void start(); // spawns thread worker
        void stop();  // signals shutdown 
        void join(); // waits for thread/worker to finish 

    private: 
        int id;
        std::string host;
        int port;
        const Router& router;

        int server_fd;

        std::unique_ptr<EventLoop> event_loop;
        std::thread thread;

        void setup_event_loop();

        void run();
};

#endif
