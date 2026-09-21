#include "http/status.hpp"

namespace httpserver::http {

std::string_view reason_phrase(StatusCode code) {
    switch (code) {
        case StatusCode::OK: return "OK";
        case StatusCode::NoContent: return "No Content";
        case StatusCode::BadRequest: return "Bad Request";
        case StatusCode::Forbidden: return "Forbidden";
        case StatusCode::NotFound: return "Not Found";
        case StatusCode::MethodNotAllowed: return "Method Not Allowed";
        case StatusCode::PayloadTooLarge: return "Payload Too Large";
        case StatusCode::URITooLong: return "URI Too Long";
        case StatusCode::HeaderFieldsTooLarge: return "Request Header Fields Too Large";
        case StatusCode::InternalServerError: return "Internal Server Error";
        case StatusCode::ServiceUnavailable: return "Service Unavailable";
        default: return "Unknown";
    }
}

} // namespace httpserver::http
