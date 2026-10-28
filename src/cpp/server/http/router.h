#ifndef SERVER_HTTP_ROUTER_H
#define SERVER_HTTP_ROUTER_H

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "method.h"
#include "request.h"
#include "response.h"

class Router {
    public:
        using Handler = std::function<HTTPResponse(const HTTPRequest&)>;

        Router() = default;

        void add_route(HTTPMethod method, std::string path, Handler handler) {
            routes[std::move(path)][method] = std::move(handler);
        }

        HTTPResponse handle_request(const HTTPRequest& request) const {
            auto path_it = routes.find(request.path);
            if (path_it == routes.end()) {
                return HTTPResponse::text(404, "Not Found", "not found\n");
            }

            const auto& method_map = path_it->second;
            auto method_it = method_map.find(request.method);
            if (method_it == method_map.end()) {
                return HTTPResponse::text(405, "Method Not Allowed", "method not allowed\n");
            }

            return method_it->second(request);
        }

    private:
        std::unordered_map<std::string, std::unordered_map<HTTPMethod, Handler>> routes;
};

#endif
