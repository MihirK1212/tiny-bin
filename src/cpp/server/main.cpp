#include <cxxopts.hpp>
#include <iostream>
#include <thread>
#include <algorithm>


#include "server.h"


int main(int argc, char* argv[])  {
    cxxopts::Options options("TinyBinServer", "TinyBin backend HTTP server");

    options.add_options()
        ("p,port", "Port", cxxopts::value<int>())
        ("nw,num_workers", "Number of workers", cxxopts::value<int>());

    auto result = options.parse(argc, argv);

    int num_workers = -1;
    int port = -1;

    if (result.count("port")) {
        port = result["port"].as<int>();
        std::cout << "Port: " << port << "\n";
    }

    if (result.count("num_workers")) {
        num_workers = result["num_workers"].as<int>();
        std::cout << "Number of workers: " << num_workers << "\n";
    }

    if (port == -1) {
        std::cerr << "Port is required\n";
        return 1;
    }

    if (num_workers == -1) { 
        std::cout << "Number of workers is not set. Will use the max number of hardware concurrency...\n";
        num_workers = std::max<int>(1, std::thread::hardware_concurrency());
    }

    TinyBinServer server(port, num_workers);
    server.start();
    return 0;
}