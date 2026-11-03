#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <string>
#include <vector>

#include "http/router.h"
#include "worker.h"

class Server {
    public:
        Server(const std::string& host, int port, int num_workers, Router router);

        void start_workers(); // start the workers
        void block_until_shutdown(); // block until SIGINT/SIGTERM
        void stop_workers(); // signal workers to shut down
        void join_workers(); // wait for all worker threads to finish

    private:
        std::string host;
        int port;
        int num_workers;
        Router router;

        std::vector<std::unique_ptr<Worker>> workers;    

        void setup_signal_handlers();
};

#endif
