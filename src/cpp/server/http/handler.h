#ifndef SERVER_HTTP_HANDLER_H
#define SERVER_HTTP_HANDLER_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "request.h"
#include "response.h"
#include "method.h"

inline std::string request_to_string(const HTTPRequest& request) {
    std::string out = std::string(to_string(request.method)) + " " + request.path;
    if (!request.query.empty()) {
        out += "?" + request.query;
    }
    out += " " + request.version;
    for (const auto& [k, v] : request.headers) {
        out += " " + k + ":" + v;
    }
    if (!request.body.empty()) {
        out += " ";
        out.append(reinterpret_cast<const char*>(request.body.data()), request.body.size());
    }
    return out;
}

inline std::vector<uint8_t> handle_request(const HTTPRequest& request) {
    std::string stringified = request_to_string(request);

    std::cout << "Server received: " << stringified << std::endl;

    std::string reply_body = "PONG: " + stringified + "\n";

    HTTPResponse response;
    response.status_code = 200;
    response.reason_phrase = "OK";
    response.headers["Content-Type"] = "text/plain; charset=utf-8";
    response.body.assign(reply_body.begin(), reply_body.end());

    return response.serialize();
}

#endif
