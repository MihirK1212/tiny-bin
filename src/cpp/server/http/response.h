#ifndef SERVER_HTTP_RESPONSE_H
#define SERVER_HTTP_RESPONSE_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct HTTPResponse {
    int status_code = 200;
    std::string reason_phrase = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    static HTTPResponse text(int status, std::string reason, std::string_view text_body) {
        HTTPResponse resp;
        resp.status_code = status;
        resp.reason_phrase = std::move(reason);
        resp.headers["Content-Type"] = "text/plain; charset=utf-8";
        resp.body.assign(text_body.begin(), text_body.end());
        return resp;
    }

    std::vector<uint8_t> serialize() const {
        std::string head = "HTTP/1.1 " + std::to_string(status_code) + " " + reason_phrase + "\r\n";

        if (status_code != 204) {
            head += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        }

        if (headers.find("Connection") == headers.end() &&
            headers.find("connection") == headers.end()) {
            head += "Connection: keep-alive\r\n";
        }

        for (const auto& [name, value] : headers) {
            if (name == "Content-Length" || name == "content-length") {
                continue;
            }
            head += name + ": " + value + "\r\n";
        }

        head += "\r\n";

        std::vector<uint8_t> out;
        out.reserve(head.size() + body.size());
        out.insert(out.end(), head.begin(), head.end());
        if (status_code != 204 && !body.empty()) {
            out.insert(out.end(), body.begin(), body.end());
        }
        return out;
    }
};

#endif
