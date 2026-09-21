#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>

namespace httpserver::http {

/**
 * @brief Represents an HTTP/1.1 request.
 */
struct Request {
    std::string method;
    std::string target;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    /**
     * @brief Retrieve the value of a specific header.
     * @param name The name of the header (case-insensitive)
     * @return std::optional<std::string_view> The value of the header if present
     */
    std::optional<std::string_view> header(std::string_view name) const;

    /**
     * @brief Check if a header exists.
     * @param name The name of the header (case-insensitive)
     * @return true if the header exists, false otherwise
     */
    bool has_header(std::string_view name) const;

    /**
     * @brief Get the content length of the request.
     * @return size_t The content length, or 0 if not present or invalid
     */
    size_t content_length() const;

    /**
     * @brief Check if the connection should be kept alive.
     * @return true if keep-alive is requested (or default for HTTP/1.1), false otherwise
     */
    bool should_keep_alive() const;
};

} // namespace httpserver::http
