#ifndef SERVER_APP_ROUTER_H
#define SERVER_APP_ROUTER_H

#include <iostream>
#include <string>
#include <string_view>

#include "db/query.h"
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

inline std::string json_escape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
        }
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

inline HTTPResponse handle_list_users(const HTTPRequest& /*request*/, DBConnection& db) {
    try {
        pqxx::result result = execute_query(db, "SELECT * FROM tiny_bin.users ORDER BY 1");

        std::string body = "[";
        for (pqxx::result::size_type row_index = 0; row_index < result.size(); ++row_index) {
            if (row_index > 0) {
                body += ",";
            }
            body += "{";
            const pqxx::row& row = result[row_index];
            for (pqxx::row::size_type col = 0; col < row.size(); ++col) {
                if (col > 0) {
                    body += ",";
                }
                body += "\"" + std::string(result.column_name(col)) + "\":";
                if (row[col].is_null()) {
                    body += "null";
                } else {
                    body += "\"" + json_escape(row[col].c_str()) + "\"";
                }
            }
            body += "}";
        }
        body += "]\n";

        HTTPResponse response = HTTPResponse::text(200, "OK", body);
        response.headers["Content-Type"] = "application/json; charset=utf-8";
        return response;
    } catch (const std::exception& error) {
        std::cerr << "Failed to list users: " << error.what() << std::endl;
        return HTTPResponse::text(500, "Internal Server Error", "database error\n");
    }
}

inline Router build_router(DBConnection& db) {
    Router router;
    router.add_route(HTTPMethod::GET, "/ping", handle_ping);
    router.add_route(HTTPMethod::POST, "/ping", handle_ping);
    router.add_route(HTTPMethod::GET, "/healthz", handle_healthz);
    router.add_route(HTTPMethod::GET, "/users", [&db](const HTTPRequest& request) {
        return handle_list_users(request, db);
    });
    return router;
}

#endif
