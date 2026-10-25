#ifndef SERVER_H
#define SERVER_H

#include <vector>

#include "worker.h"

class Server {
    public:
        Server(int port, int num_workers);

        void start_workers(); // start the workers
        void block_until_shutdown(); // block until SIGINT/SIGTERM
        void stop_workers(); // signal workers to shut down
        void join_workers(); // wait for all worker threads to finish

    private:
        int port;
        int num_workers;

        std::vector<std::unique_ptr<Worker>> workers;    

        void setup_signal_handlers();
};

#endif