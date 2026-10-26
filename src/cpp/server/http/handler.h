#ifndef SERVER_HTTP_HANDLER_H
#define SERVER_HTTP_HANDLER_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "parse.h"
#include "response.h"

inline std::vector<uint8_t> handle_request(const HTTPRequest& request) {
    std::string stringified = request.to_string();

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
