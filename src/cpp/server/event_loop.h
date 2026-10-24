#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <atomic>

class EventLoop {
    public:
        EventLoop(int server_fd);
        ~EventLoop();

        void run();
        void stop();
    
    private:
        int server_fd;
        
        std::atomic<bool> running{false};
};

#endif