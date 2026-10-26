#ifndef SERVER_HTTP_PARSE_H
#define SERVER_HTTP_PARSE_H

#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

const size_t k_max_msg = 32 << 20;

struct ReadResponse {
    bool success = false;
    bool invalid_request = false;
    size_t bytes_consumed = 0;
    std::optional<std::vector<uint8_t>> data = std::nullopt;
};

inline ReadResponse parse_one_request(const std::vector<uint8_t>& incoming_bytes) {
    /*
        for now, assume that message is of the following format:
        4 bytes -> len of message
        followed by
        `len` bytes -> actual message
    */
    if (incoming_bytes.size() < 4) {
        return ReadResponse{false, false, 0, std::nullopt};
    }

    uint32_t len_network = 0;
    memcpy(&len_network, incoming_bytes.data(), 4);
    uint32_t len = ntohl(len_network);

    if (len > k_max_msg) {
        return ReadResponse{false, true, 0, std::nullopt};
    }

    if (incoming_bytes.size() < (4 + len)) {
        return ReadResponse{false, false, 0, std::nullopt};
    }

    std::vector<uint8_t> data(incoming_bytes.begin() + 4,
                              incoming_bytes.begin() + 4 + len);

    return ReadResponse{true, false, 4 + (size_t)len, std::move(data)};
}

#endif
