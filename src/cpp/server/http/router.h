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
                return HTTPResponse::json(404, "Not Found",
                    "{\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Route not found\"}}");
            }

            auto method_it = path_it->second.find(request.method);
            if (method_it == path_it->second.end()) {
                return HTTPResponse::json(405, "Method Not Allowed",
                    "{\"error\":{\"code\":\"METHOD_NOT_ALLOWED\",\"message\":\"Method not allowed\"}}");
            }

            return method_it->second(request);
        }

    private:
        std::unordered_map<std::string, std::unordered_map<HTTPMethod, Handler>> routes;
};

#endif
