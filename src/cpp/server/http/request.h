#ifndef SERVER_HTTP_REQUEST_H
#define SERVER_HTTP_REQUEST_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct HTTPRequest {
    bool success = false;
    bool invalid_request = false;
    size_t bytes_consumed = 0;

    std::string method;
    std::string path;
    std::string query;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    static HTTPRequest make_incomplete() {
        return HTTPRequest{false, false, 0, {}, {}, {}, {}, {}, {}};
    }

    static HTTPRequest make_invalid() {
        return HTTPRequest{false, true, 0, {}, {}, {}, {}, {}, {}};
    }
};

#endif
