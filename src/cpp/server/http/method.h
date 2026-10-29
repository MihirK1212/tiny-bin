#ifndef SERVER_HTTP_METHOD_H
#define SERVER_HTTP_METHOD_H

#include <string_view>

enum class HTTPMethod { 
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS,
    HEAD,
    CONNECT,
    INVALID,
};

inline HTTPMethod from_string(std::string_view s) {
    if (s == "GET") return HTTPMethod::GET;
    if (s == "POST") return HTTPMethod::POST;
    if (s == "PUT") return HTTPMethod::PUT;
    if (s == "DELETE") return HTTPMethod::DELETE;
    if (s == "PATCH") return HTTPMethod::PATCH;
    if (s == "OPTIONS") return HTTPMethod::OPTIONS;
    if (s == "HEAD") return HTTPMethod::HEAD;
    if (s == "CONNECT") return HTTPMethod::CONNECT;
    return HTTPMethod::INVALID;
}

inline std::string_view to_string(HTTPMethod method) {
    switch (method) {
        case HTTPMethod::GET: return "GET";
        case HTTPMethod::POST: return "POST";
        case HTTPMethod::PUT: return "PUT";
        case HTTPMethod::DELETE: return "DELETE";
        case HTTPMethod::PATCH: return "PATCH";
        case HTTPMethod::OPTIONS: return "OPTIONS";
        case HTTPMethod::HEAD: return "HEAD";
        case HTTPMethod::CONNECT: return "CONNECT";
        case HTTPMethod::INVALID: return "INVALID";
        default: return "INVALID";
    }
}

#endif