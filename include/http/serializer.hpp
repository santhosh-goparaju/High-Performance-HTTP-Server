#pragma once
#include "http/response.hpp"
#include <string>

namespace httpserver::http {

/**
 * @brief Serializes HTTP responses to strings.
 */
class HttpSerializer {
public:
    /**
     * @brief Serialize a Response object to an HTTP/1.1 response string.
     * @param response The response to serialize
     * @return std::string The serialized response string
     */
    static std::string serialize(const Response& response);

private:
    static std::string format_date();
};

} // namespace httpserver::http
