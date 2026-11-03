#ifndef SERVER_APP_ROUTER_H
#define SERVER_APP_ROUTER_H

#include <iostream>
#include <nlohmann/json.hpp>

#include "db/pastes.h"
#include "server/http/router.h"

using Json = nlohmann::json;

inline HTTPResponse json_response(int status, const std::string& reason, const Json& body) {
    return HTTPResponse::json(status, reason, body.dump());
}

inline HTTPResponse api_error(int status, const std::string& reason,
                              const std::string& code, const std::string& message) {
    return json_response(status, reason, {{"error", {{"code", code}, {"message", message}}}});
}

inline bool read_content(const HTTPRequest& request, std::string& content, HTTPResponse& error) {
    auto ct = request.headers.find("content-type");
    if (ct == request.headers.end() || ct->second.find("application/json") == std::string::npos) {
        error = api_error(415, "Unsupported Media Type", "UNSUPPORTED_MEDIA_TYPE", "Use application/json");
        return false;
    }

    for (uint8_t byte : request.body) {
        if (byte == 0) {
            error = api_error(400, "Bad Request", "INVALID_CONTENT", "Invalid content");
            return false;
        }
    }

    Json body = Json::parse(request.body.begin(), request.body.end(), nullptr, false);
    if (body.is_discarded() || !body.is_object() || !body["content"].is_string()) {
        error = api_error(400, "Bad Request", "INVALID_CONTENT", "Expected {\"content\": \"...\"}");
        return false;
    }

    content = body["content"].get<std::string>();
    if (content.empty() || content.find('\0') != std::string::npos) {
        error = api_error(400, "Bad Request", "INVALID_CONTENT", "Invalid content");
        return false;
    }
    if (content.size() > 10240) {
        error = api_error(413, "Content Too Large", "CONTENT_TOO_LARGE", "Max 10240 bytes");
        return false;
    }
    return true;
}

inline Json get_paste_json(const pqxx::row& row, const std::string& base_url) {
    const std::string id = row["id"].as<std::string>();
    return {{"id", id}, {"content", row["content"].as<std::string>()},
            {"created_at", row["created_at"].as<std::string>()}, {"url", base_url + "/p/" + id}};
}

inline std::string get_query_value(const std::string& query, const std::string& key) {
    const std::string prefix = key + "=";
    size_t start = 0;
    while (start <= query.size()) {
        size_t end = query.find('&', start);
        std::string part = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part.rfind(prefix, 0) == 0) return part.substr(prefix.size());
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

inline HTTPResponse handle_get_paste(const HTTPRequest& request, DBConnection& db, const std::string& public_base_url) 
{
    std::string id = get_query_value(request.query, "id");
    try {
        pqxx::result result = get_paste(db, id);
        if (result.empty()) return api_error(404, "Not Found", "NOT_FOUND", "Paste not found");
        return json_response(200, "OK", get_paste_json(result[0], public_base_url));
    } catch (const std::exception&) {
        std::cerr << "Paste database operation failed\n";
        return api_error(503, "Service Unavailable", "DATABASE_UNAVAILABLE", "Database unavailable. Please try again.");
    }
}

inline HTTPResponse handle_create_paste(const HTTPRequest& request, DBConnection& db, const std::string& public_base_url) 
{
    std::string content;
    HTTPResponse error;
    if (!read_content(request, content, error)) return error;
    try {
        pqxx::result result = create_paste(db, content);
        auto response = json_response(201, "Created", get_paste_json(result[0], public_base_url));
        response.headers["Location"] = public_base_url + "/p/" + result[0]["id"].as<std::string>();
        return response;
    } catch (const std::exception&) {
        std::cerr << "Paste database operation failed\n";
        return api_error(503, "Service Unavailable", "DATABASE_UNAVAILABLE", "Database unavailable. Please try again.");
    }
}

inline Router build_router(DBConnection& db, const std::string& public_base_url) {
    Router router;
    router.add_route(HTTPMethod::GET, "/healthz", [](const HTTPRequest&) {
        return HTTPResponse::text(200, "OK", "ok\n");
    });
    router.add_route(HTTPMethod::GET, "/readyz", [&db](const HTTPRequest&) {
        try {
            execute_query(db, "SELECT id FROM tiny_bin.pastes LIMIT 0");
            return HTTPResponse::text(200, "OK", "ready\n");
        } catch (const std::exception&) {
            return api_error(503, "Service Unavailable", "NOT_READY", "Database or paste schema unavailable");
        }
    });
    router.add_route(HTTPMethod::GET, "/api/v1/pastes", [&db, public_base_url](const HTTPRequest& request) {
        return handle_get_paste(request, db, public_base_url);
    });
    router.add_route(HTTPMethod::POST, "/api/v1/pastes", [&db, public_base_url](const HTTPRequest& request) {
        return handle_create_paste(request, db, public_base_url);
    });
    return router;
}

#endif
