#ifndef SERVER_HTTP_PARSE_H
#define SERVER_HTTP_PARSE_H

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

    std::string to_string() const {
        std::string out = method + " " + path;
        if (!query.empty()) {
            out += "?" + query;
        }
        out += " " + version;
        for (const auto& [k, v] : headers) {
            out += " " + k + ":" + v;
        }
        if (!body.empty()) {
            out += " ";
            out.append(reinterpret_cast<const char*>(body.data()), body.size());
        }
        return out;
    }
};

namespace detail {

inline bool is_tchar(unsigned char c) {
    if (std::isalnum(c)) {
        return true;
    }
    switch (c) {
        case '!': case '#': case '$': case '%': case '&':
        case '\'': case '*': case '+': case '-': case '.':
        case '^': case '_': case '`': case '|': case '~':
            return true;
        default:
            return false;
    }
}

inline bool is_valid_token(std::string_view s) {
    if (s.empty()) {
        return false;
    }
    for (unsigned char c : s) {
        if (!is_tchar(c)) {
            return false;
        }
    }
    return true;
}

inline bool has_forbidden_target_chars(std::string_view s) {
    for (unsigned char c : s) {
        if (c <= 0x20 || c == 0x7F) {
            return true;
        }
    }
    return false;
}

inline bool is_valid_field_value(std::string_view s) {
    for (unsigned char c : s) {
        if ((c < 0x20 && c != '\t') || c == 0x7F) {
            return false;
        }
    }
    return true;
}

inline std::string_view trim_ows(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        start++;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
        end--;
    }
    return s.substr(start, end - start);
}

} // namespace detail

inline HTTPRequest parse_one_request(const std::vector<uint8_t>& incoming_bytes) {
    if (incoming_bytes.empty()) {
        return HTTPRequest::make_incomplete();
    }

    const uint8_t k_crlf2[4] = {'\r', '\n', '\r', '\n'};
    const size_t search_limit = std::min(incoming_bytes.size(), k_max_header_bytes);

    auto end_it = std::search(
        incoming_bytes.begin(),
        incoming_bytes.begin() + search_limit,
        std::begin(k_crlf2),
        std::end(k_crlf2)
    );

    if (end_it == incoming_bytes.begin() + search_limit) {
        if (incoming_bytes.size() >= k_max_header_bytes) {
            return HTTPRequest::make_invalid();
        }
        return HTTPRequest::make_incomplete();
    }

    const size_t header_end_pos = static_cast<size_t>(std::distance(incoming_bytes.begin(), end_it));
    const size_t header_block_len = header_end_pos + 4;

    std::string_view header_block(
        reinterpret_cast<const char*>(incoming_bytes.data()),
        header_end_pos
    );

    // Split request line
    size_t line_end = header_block.find("\r\n");
    std::string_view request_line = (line_end == std::string_view::npos)
        ? header_block
        : header_block.substr(0, line_end);

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

    if (!detail::is_valid_token(method_sv)) {
        return HTTPRequest::make_invalid();
    }
    if (target_sv.empty() || target_sv[0] != '/' || detail::has_forbidden_target_chars(target_sv)) {
        return HTTPRequest::make_invalid();
    }
    if (version_sv != "HTTP/1.1") {
        return HTTPRequest::make_invalid();
    }

    HTTPRequest req;
    req.method = std::string(method_sv);
    req.version = std::string(version_sv);

    size_t qpos = target_sv.find('?');
    if (qpos == std::string_view::npos) {
        req.path = std::string(target_sv);
    } else {
        req.path = std::string(target_sv.substr(0, qpos));
        req.query = std::string(target_sv.substr(qpos + 1));
    }

    // Parse header lines
    size_t pos = (line_end == std::string_view::npos) ? header_block.size() : line_end + 2;
    while (pos < header_block.size()) {
        size_t next_crlf = header_block.find("\r\n", pos);
        std::string_view line = (next_crlf == std::string_view::npos)
            ? header_block.substr(pos)
            : header_block.substr(pos, next_crlf - pos);
        pos = (next_crlf == std::string_view::npos) ? header_block.size() : next_crlf + 2;

        if (line.empty()) {
            return HTTPRequest::make_invalid();
        }
        // Reject obsolete line folding
        if (line[0] == ' ' || line[0] == '\t') {
            return HTTPRequest::make_invalid();
        }

        size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return HTTPRequest::make_invalid();
        }

        std::string_view raw_name = line.substr(0, colon);
        if (!detail::is_valid_token(raw_name)) {
            return HTTPRequest::make_invalid();
        }

        std::string name(raw_name);
        for (char& c : name) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        std::string_view value_sv = detail::trim_ows(line.substr(colon + 1));
        if (!detail::is_valid_field_value(value_sv)) {
            return HTTPRequest::make_invalid();
        }

        // Reject unsupported Transfer-Encoding and Expect headers
        if (name == "transfer-encoding" || name == "expect") {
            return HTTPRequest::make_invalid();
        }

        // Reject duplicate Host or Content-Length headers
        if ((name == "host" || name == "content-length") && req.headers.find(name) != req.headers.end()) {
            return HTTPRequest::make_invalid();
        }

        req.headers[std::move(name)] = std::string(value_sv);
    }

    // Require non-empty Host header without whitespace
    auto host_it = req.headers.find("host");
    if (host_it == req.headers.end() || host_it->second.empty() ||
        detail::has_forbidden_target_chars(host_it->second)) {
        return HTTPRequest::make_invalid();
    }

    // Parse Content-Length if present
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
