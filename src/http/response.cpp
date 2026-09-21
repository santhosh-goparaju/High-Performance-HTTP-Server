#include "http/response.hpp"

namespace httpserver::http {

Response Response::ok(const std::string& body, const std::string& content_type) {
    Response res;
    res.status = StatusCode::OK;
    res.set_body(body, content_type);
    return res;
}

Response Response::error(StatusCode code, const std::string& message) {
    Response res;
    res.status = code;
    if (!message.empty()) {
        res.set_body(message, "text/plain");
    } else {
        res.set_body(std::string(reason_phrase(code)), "text/plain");
    }
    return res;
}

Response Response::json(const std::string& json_string) {
    Response res;
    res.status = StatusCode::OK;
    res.set_body(json_string, "application/json");
    return res;
}

Response Response::not_found() {
    return error(StatusCode::NotFound, "Not Found");
}

Response Response::bad_request(const std::string& message) {
    return error(StatusCode::BadRequest, message.empty() ? "Bad Request" : message);
}

Response Response::method_not_allowed(const std::string& allowed_methods) {
    Response res;
    res.status = StatusCode::MethodNotAllowed;
    res.set_header("Allow", allowed_methods);
    res.set_body("Method Not Allowed", "text/plain");
    return res;
}

Response Response::make_response(StatusCode code) {
    Response res;
    res.status = code;
    std::string reason(reason_phrase(code));
    if (code != StatusCode::NoContent) {
        res.set_body(reason, "text/plain");
    }
    return res;
}

Response& Response::set_header(const std::string& key, const std::string& value) {
    headers[key] = value;
    return *this;
}

void Response::set_body(const std::string& new_body, const std::string& content_type) {
    body = new_body;
    set_header("Content-Type", content_type);
}

} // namespace httpserver::http
