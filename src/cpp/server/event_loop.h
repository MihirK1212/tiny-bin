#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <atomic>

#include "connection.h"
#include "http/router.h"

class EventLoop {
    public:
        EventLoop(int server_fd, const Router& router);
        ~EventLoop();

        void run();
        void stop();
    
    private:
        int server_fd;
        const Router& router;

        std::atomic<bool> running{true};

        void process_connection(Connection* conn);
};

#endif
