#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <arpa/inet.h>
#include <cstdlib>
#include <string>

inline std::string env_value(const char* name, const std::string& fallback = "") {
    const char* value = std::getenv(name);
    return value ? std::string(value) : fallback;
}

struct ServerConfig {
    std::string host = env_value("HOST", "127.0.0.1");
    int port = std::stoi(env_value("PORT", "8000"));
    int num_workers = std::stoi(env_value("NUM_WORKERS", "1"));
    std::string database_url = env_value("DATABASE_URL");
    std::string public_base_url = env_value("PUBLIC_BASE_URL", "http://localhost:5173");

    std::string validate() const {
        if (database_url.empty()) {
            return "DATABASE_URL is required";
        }
        if (port < 1 || port > 65535) {
            return "PORT must be between 1 and 65535";
        }
        if (num_workers < 1) {
            return "NUM_WORKERS must be at least 1";
        }
        in_addr addr{};
        if (inet_pton(AF_INET, host.c_str(), &addr) != 1) {
            return "HOST must be an IPv4 address";
        }
        if (public_base_url.rfind("http://", 0) != 0 && public_base_url.rfind("https://", 0) != 0) {
            return "PUBLIC_BASE_URL must start with http:// or https://";
        }
        return "";
    }
};

#endif
