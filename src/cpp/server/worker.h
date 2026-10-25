#ifndef WORKER_H
#define WORKER_H

#include <thread>

#include "event_loop.h"

class Worker { 
    public: 
        Worker(int id, int port);
        ~Worker();

        void start(); // spawns thread worker
        void stop();  // signals shutdown 
        void join(); // waits for thread/worker to finish 

    private: 
        int id;
        int port;

        int server_fd;

        std::unique_ptr<EventLoop> event_loop;
        std::thread thread;

        void setup_event_loop();

        void run();
};

#endif