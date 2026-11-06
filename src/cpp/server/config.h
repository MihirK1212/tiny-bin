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
    std::string db_user = env_value("DB_USER");
    std::string db_password = env_value("DB_PASSWORD");
    std::string db_host = env_value("DB_HOST");
    std::string db_port = env_value("DB_PORT", "5432");
    std::string db_name = env_value("DB_NAME");
    std::string public_base_url = env_value("PUBLIC_BASE_URL", "http://localhost:5173");

    std::string database_url() const {
        return "postgresql://" + db_user + ":" + db_password + "@" + db_host + ":" + db_port + "/" +
               db_name + "?connect_timeout=5";
    }

    std::string validate() const {
        if (db_user.empty()) {
            return "DB_USER is required";
        }
        if (db_password.empty()) {
            return "DB_PASSWORD is required";
        }
        if (db_host.empty()) {
            return "DB_HOST is required";
        }
        if (db_name.empty()) {
            return "DB_NAME is required";
        }
        if (db_port.empty()) {
            return "DB_PORT is required";
        }
        const int db_port_num = std::stoi(db_port);
        if (db_port_num < 1 || db_port_num > 65535) {
            return "DB_PORT must be between 1 and 65535";
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
