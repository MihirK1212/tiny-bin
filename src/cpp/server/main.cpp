#include <cxxopts.hpp>
#include <iostream>

#include "app/app.h"
#include "config.h"
#include "server.h"

int main(int argc, char* argv[]) {
    cxxopts::Options options("TinyBinServer", "TinyBin backend HTTP server");
    options.add_options()
        ("p,port", "Override PORT", cxxopts::value<int>())
        ("n,num_workers", "Override NUM_WORKERS", cxxopts::value<int>());
    auto result = options.parse(argc, argv);

    ServerConfig config;
    if (result.count("port")) {
        config.port = result["port"].as<int>();
    }
    if (result.count("num_workers")) {
        config.num_workers = result["num_workers"].as<int>();
    }

    if (const std::string error = config.validate(); !error.empty()) {
        std::cerr << error << '\n';
        return 1;
    }

    TinyBinApp app(config.database_url, config.public_base_url);
    
    Server server(config.host, config.port, config.num_workers, app.get_router());
    server.start_workers();
    server.block_until_shutdown();
    server.stop_workers();
    server.join_workers();
    
    return 0;
}
