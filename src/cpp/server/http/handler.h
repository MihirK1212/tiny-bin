#ifndef SERVER_HTTP_HANDLER_H
#define SERVER_HTTP_HANDLER_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "parse.h"

inline std::vector<uint8_t> handle_request(const HTTPRequest& request) {
    const std::vector<uint8_t>& request_data = request.data.value();

    std::cout << "Server received: ";
    for (uint32_t byte : request_data) {
        std::cout << (char)byte << " ";
    }
    std::cout << std::endl;

    std::vector<uint8_t> response_data;
    response_data.reserve(6 + request_data.size());
    response_data.insert(response_data.end(), {'P', 'O', 'N', 'G', ':', ' '});
    response_data.insert(response_data.end(), request_data.begin(), request_data.end());

    return response_data;
}

#endif
