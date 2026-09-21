#pragma once
#include "http/status.hpp"
#include <string>
#include <unordered_map>

namespace httpserver::http {

/**
 * @brief Represents an HTTP/1.1 response.
 */
struct Response {
    StatusCode status{StatusCode::OK};
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool keep_alive{true};

    /// Create a 200 OK response with body and content type
    static Response ok(const std::string& body, const std::string& content_type = "text/plain");
    /// Create an error response with optional message body
    static Response error(StatusCode code, const std::string& message = "");
    /// Create a JSON response (200 OK with application/json)
    static Response json(const std::string& json_string);
    /// Convenience: 404 Not Found
    static Response not_found();
    /// Convenience: 400 Bad Request
    static Response bad_request(const std::string& message = "");
    /// Convenience: 405 Method Not Allowed with Allow header
    static Response method_not_allowed(const std::string& allowed_methods);
    /// Create a response with just a status code and default body
    static Response make_response(StatusCode code);

    /// Set an HTTP header. Returns *this for chaining.
    Response& set_header(const std::string& key, const std::string& value);

    /// Set the body and Content-Type header.
    void set_body(const std::string& body, const std::string& content_type);
};

} // namespace httpserver::http
