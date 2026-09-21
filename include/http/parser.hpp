#pragma once
#include "http/request.hpp"
#include "http/status.hpp"
#include <string_view>

namespace httpserver::http {

enum class ParseState { REQUEST_LINE, HEADERS, BODY, COMPLETE, ERROR };
enum class ParseResult { INCOMPLETE, COMPLETE, ERROR };

/**
 * @brief State machine based HTTP/1.1 request parser.
 */
class HttpParser {
public:
    HttpParser() = default;

    /**
     * @brief Feed data incrementally to the parser.
     * @param data Pointer to the data
     * @param len Length of the data
     * @return ParseResult The current result state of parsing
     */
    ParseResult feed(const char* data, size_t len);

    /**
     * @brief Feed data incrementally to the parser.
     * @param data The data
     * @return ParseResult The current result state of parsing
     */
    ParseResult feed(std::string_view data);
    
    /**
     * @brief Get the parsed request (moves it out).
     * @return Request The parsed request
     */
    Request get_request();

    /**
     * @brief Get the error status code if parsing failed.
     * @return StatusCode The error code
     */
    StatusCode error_code() const;

    /**
     * @brief Reset the parser state for the next request.
     */
    void reset();

    /**
     * @brief Get the current parser state.
     * @return ParseState The state
     */
    ParseState state() const;

private:
    static constexpr size_t MAX_REQUEST_LINE = 8192;
    static constexpr size_t MAX_HEADER_SIZE = 8192;
    static constexpr size_t MAX_TOTAL_HEADERS = 65536;
    static constexpr int MAX_HEADER_COUNT = 100;
    static constexpr size_t MAX_BODY_SIZE = 1048576;

    void parse_request_line();
    void parse_headers();
    void parse_body();

    std::string buffer_;
    ParseState state_{ParseState::REQUEST_LINE};
    Request request_;
    StatusCode error_code_{StatusCode::OK};
    size_t headers_size_{0};
    int header_count_{0};
};

} // namespace httpserver::http
