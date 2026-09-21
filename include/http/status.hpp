#pragma once
#include <string_view>

namespace httpserver::http {

enum class StatusCode : int {
    OK = 200,
    NoContent = 204,
    BadRequest = 400,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    PayloadTooLarge = 413,
    URITooLong = 414,
    HeaderFieldsTooLarge = 431,
    InternalServerError = 500,
    ServiceUnavailable = 503
};

/**
 * @brief Get the standard reason phrase for a given HTTP status code.
 * 
 * @param code The HTTP status code
 * @return std::string_view The corresponding reason phrase
 */
std::string_view reason_phrase(StatusCode code);

} // namespace httpserver::http
