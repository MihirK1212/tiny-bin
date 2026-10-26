#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <atomic>

#include "connection.h"

class EventLoop {
    public:
        EventLoop(int server_fd);
        ~EventLoop();

        void run();
        void stop();
    
    private:
        int server_fd;

        std::atomic<bool> running{true};

        static void process_connection(Connection* conn);
};

#endif