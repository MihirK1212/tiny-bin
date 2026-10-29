#ifndef SERVER_APP_ROUTER_H
#define SERVER_APP_ROUTER_H

#include <iostream>
#include <string>

#include "server/http/method.h"
#include "server/http/request.h"
#include "server/http/response.h"
#include "server/http/router.h"

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

inline HTTPResponse handle_ping(const HTTPRequest& request) {
    std::string stringified = request_to_string(request);
    std::cout << "Server received: " << stringified << std::endl;

    std::string reply_body = "PONG: " + stringified + "\n";
    return HTTPResponse::text(200, "OK", reply_body);
}

inline HTTPResponse handle_healthz(const HTTPRequest& /*request*/) {
    return HTTPResponse::text(200, "OK", "ok\n");
}

inline Router build_router() {
    Router router;
    router.add_route(HTTPMethod::GET, "/ping", handle_ping);
    router.add_route(HTTPMethod::POST, "/ping", handle_ping);
    router.add_route(HTTPMethod::GET, "/healthz", handle_healthz);
    return router;
}

#endif
