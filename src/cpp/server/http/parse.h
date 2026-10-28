#ifndef SERVER_HTTP_PARSE_H
#define SERVER_HTTP_PARSE_H

#include "parse_utils.h"
#include "request.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

const size_t k_max_header_bytes = 8 * 1024;
const size_t k_max_body_bytes = 16 * 1024;

inline HTTPRequest parse_one_request(const std::vector<uint8_t>& incoming_bytes) {
    if (incoming_bytes.empty()) {
        return HTTPRequest::make_incomplete();
    }

    /*
    for the request:
        GET /api/v1/pastes?id=123 HTTP/1.1\r\n
        Host: localhost\r\n
        \r\n
    - the header is: "GET /api/v1/pastes?id=123 HTTP/1.1\r\nHost: localhost\r\n"
        - the request line is: "GET /api/v1/pastes?id=123 HTTP/1.1"
    - the body is empty 

    for the request: 
        POST /paste HTTP/1.1\r\n
        Host: localhost\r\n
        Content-Length: 11\r\n
        \r\n
        hello world 
    - the header is: "POST /paste HTTP/1.1\r\nHost: localhost\r\nContent-Length: 11\r\n" 
        - the request line is: "POST /paste HTTP/1.1" 
    - the body is: "hello world" 
    
    HEADER:
        - from begginning to first occurrence of "\r\n\r\n"  (after this we have the body)
        - the first line is the request line (up to the first \r\n)
        - request line conssts of method, target (path) and version split by a space
        - eg:- one request line: "GET /api/v1/pastes?id=123 HTTP/1.1\r\n
        - the target (path) can also have a query string, which is the part after the ?

    */ 

    // search for the first occurrence of "\r\n\r\n" in the incoming bytes 
    const uint8_t k_crlf2[4] = {'\r', '\n', '\r', '\n'};
    const size_t search_limit = std::min(incoming_bytes.size(), k_max_header_bytes);
    
    auto crlf_first_iter = std::search(
        incoming_bytes.begin(),
        incoming_bytes.begin() + search_limit,
        std::begin(k_crlf2),
        std::end(k_crlf2)
    );

    if (crlf_first_iter == incoming_bytes.begin() + search_limit) {
        if (incoming_bytes.size() >= k_max_header_bytes) {
            return HTTPRequest::make_invalid();
        }
        return HTTPRequest::make_incomplete();
    }

    const size_t header_end_pos = static_cast<size_t>(std::distance(incoming_bytes.begin(), crlf_first_iter));
    const size_t header_block_len = header_end_pos + 4;

    std::string_view header_block(
        reinterpret_cast<const char*>(incoming_bytes.data()),
        header_end_pos
    );

    // split request line
    size_t request_line_end = header_block.find("\r\n");
    std::string_view request_line = (request_line_end == std::string_view::npos)
        ? header_block
        : header_block.substr(0, request_line_end);

    size_t sp1 = request_line.find(' ');
    if (sp1 == std::string_view::npos) {
        return HTTPRequest::make_invalid();
    }
    size_t sp2 = request_line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos || request_line.find(' ', sp2 + 1) != std::string_view::npos) {
        return HTTPRequest::make_invalid();
    }

    std::string_view method_sv = request_line.substr(0, sp1);
    std::string_view target_sv = request_line.substr(sp1 + 1, sp2 - (sp1 + 1));
    std::string_view version_sv = request_line.substr(sp2 + 1);

    if (!is_valid_token(method_sv)) {
        return HTTPRequest::make_invalid();
    }
    if (target_sv.empty() || target_sv[0] != '/' || has_forbidden_target_chars(target_sv)) {
        return HTTPRequest::make_invalid();
    }
    if (version_sv != "HTTP/1.1") {
        return HTTPRequest::make_invalid();
    }

    HTTPRequest req;
    req.method = from_string(method_sv);
    if (req.method == HTTPMethod::INVALID) {
        return HTTPRequest::make_invalid();
    }
    req.version = std::string(version_sv);

    size_t qpos = target_sv.find('?');
    if (qpos == std::string_view::npos) {
        req.path = std::string(target_sv);
    } else {
        req.path = std::string(target_sv.substr(0, qpos));
        req.query = std::string(target_sv.substr(qpos + 1));
    }

    // parse header lines (each header is of the form name: value followed by a \r\n)
    size_t pos = (request_line_end == std::string_view::npos) ? header_block.size() : request_line_end + 2;
    while (pos < header_block.size()) {
        size_t next_crlf = header_block.find("\r\n", pos);
        std::string_view line = (next_crlf == std::string_view::npos)
            ? header_block.substr(pos)
            : header_block.substr(pos, next_crlf - pos);
        pos = (next_crlf == std::string_view::npos) ? header_block.size() : next_crlf + 2;

        if (line.empty()) {
            return HTTPRequest::make_invalid();
        }
        // reject obsolete line folding (line should not start with a space or TAB)
        if (line[0] == ' ' || line[0] == '\t') {
            return HTTPRequest::make_invalid();
        }

        size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return HTTPRequest::make_invalid();
        }

        std::string_view raw_name = line.substr(0, colon);
        if (!is_valid_token(raw_name)) {
            return HTTPRequest::make_invalid();
        }

        std::string name(raw_name);
        for (char& c : name) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        std::string_view value_sv = trim_whitespace(line.substr(colon + 1));
        if (!is_valid_field_value(value_sv)) {
            return HTTPRequest::make_invalid();
        }

        // reject unsupported Transfer-Encoding and Expect headers
        if (name == "transfer-encoding" || name == "expect") {
            return HTTPRequest::make_invalid();
        }

        // reject duplicate Host or Content-Length headers
        if ((name == "host" || name == "content-length") && req.headers.find(name) != req.headers.end()) {
            return HTTPRequest::make_invalid();
        }

        req.headers[std::move(name)] = std::string(value_sv);
    }

    // require non-empty Host header without whitespace
    auto host_it = req.headers.find("host");
    if (host_it == req.headers.end() || host_it->second.empty() || has_forbidden_target_chars(host_it->second)) {
        return HTTPRequest::make_invalid();
    }

    // parse Content-Length if present
    size_t content_length = 0;
    auto cl_it = req.headers.find("content-length");
    if (cl_it != req.headers.end()) {
        const std::string& cl_str = cl_it->second;
        if (cl_str.empty()) {
            return HTTPRequest::make_invalid();
        }
        for (char c : cl_str) {
            if (c < '0' || c > '9') {
                return HTTPRequest::make_invalid();
            }
            content_length = content_length * 10 + static_cast<size_t>(c - '0');
            if (content_length > k_max_body_bytes) {
                return HTTPRequest::make_invalid();
            }
        }
    }

    if (incoming_bytes.size() < header_block_len + content_length) {
        return HTTPRequest::make_incomplete();
    }

    if (content_length > 0) {
        req.body.assign(
            incoming_bytes.begin() + header_block_len,
            incoming_bytes.begin() + header_block_len + content_length
        );
    }

    req.success = true;
    req.invalid_request = false;
    req.bytes_consumed = header_block_len + content_length;
    return req;
}

#endif
