#include "http/parser.hpp"
#include <algorithm>
#include <cctype>

namespace httpserver::http {

static std::string trim(std::string_view s) {
    if (s.empty()) return std::string();
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        start++;
    }
    if (start == s.end()) return std::string();
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

static std::string to_lower(std::string_view s) {
    std::string result(s);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

ParseResult HttpParser::feed(const char* data, size_t len) {
    return feed(std::string_view(data, len));
}

ParseResult HttpParser::feed(std::string_view data) {
    if (state_ == ParseState::ERROR) return ParseResult::ERROR;
    if (state_ == ParseState::COMPLETE) return ParseResult::COMPLETE;

    buffer_.append(data);

    while (state_ != ParseState::COMPLETE && state_ != ParseState::ERROR) {
        auto old_state = state_;
        if (state_ == ParseState::REQUEST_LINE) {
            parse_request_line();
        } else if (state_ == ParseState::HEADERS) {
            parse_headers();
        } else if (state_ == ParseState::BODY) {
            parse_body();
        }
        
        // If state hasn't changed, we need more data
        if (state_ == old_state) {
            break;
        }
    }

    if (state_ == ParseState::ERROR) return ParseResult::ERROR;
    if (state_ == ParseState::COMPLETE) return ParseResult::COMPLETE;
    return ParseResult::INCOMPLETE;
}

Request HttpParser::get_request() {
    return std::move(request_);
}

StatusCode HttpParser::error_code() const {
    return error_code_;
}

void HttpParser::reset() {
    buffer_.clear();
    state_ = ParseState::REQUEST_LINE;
    request_ = Request{};
    error_code_ = StatusCode::OK;
    headers_size_ = 0;
    header_count_ = 0;
}

ParseState HttpParser::state() const {
    return state_;
}

void HttpParser::parse_request_line() {
    auto pos = buffer_.find("\r\n");
    if (pos == std::string::npos) {
        if (buffer_.size() > MAX_REQUEST_LINE) {
            state_ = ParseState::ERROR;
            error_code_ = StatusCode::URITooLong;
        }
        return;
    }

    if (pos > MAX_REQUEST_LINE) {
        state_ = ParseState::ERROR;
        error_code_ = StatusCode::URITooLong;
        return;
    }

    std::string_view req_line(buffer_.data(), pos);
    
    size_t m_end = req_line.find(' ');
    if (m_end == std::string::npos) {
        state_ = ParseState::ERROR;
        error_code_ = StatusCode::BadRequest;
        return;
    }
    
    size_t t_end = req_line.find(' ', m_end + 1);
    if (t_end == std::string::npos) {
        state_ = ParseState::ERROR;
        error_code_ = StatusCode::BadRequest;
        return;
    }

    request_.method = std::string(req_line.substr(0, m_end));
    request_.target = std::string(req_line.substr(m_end + 1, t_end - m_end - 1));
    request_.version = std::string(req_line.substr(t_end + 1));

    if (request_.method != "GET" && request_.method != "HEAD" &&
        request_.method != "POST" && request_.method != "DELETE") {
        state_ = ParseState::ERROR;
        error_code_ = StatusCode::MethodNotAllowed;
        return;
    }

    if (request_.target.empty() || request_.target[0] != '/') {
        state_ = ParseState::ERROR;
        error_code_ = StatusCode::BadRequest;
        return;
    }

    if (request_.version != "HTTP/1.1") {
        state_ = ParseState::ERROR;
        error_code_ = StatusCode::BadRequest;
        return;
    }

    buffer_.erase(0, pos + 2);
    state_ = ParseState::HEADERS;
}

void HttpParser::parse_headers() {
    while (true) {
        auto pos = buffer_.find("\r\n");
        if (pos == std::string::npos) {
            if (headers_size_ + buffer_.size() > MAX_TOTAL_HEADERS) {
                state_ = ParseState::ERROR;
                error_code_ = StatusCode::HeaderFieldsTooLarge;
            }
            return;
        }

        if (pos == 0) { // End of headers
            buffer_.erase(0, 2);
            state_ = ParseState::BODY;
            return;
        }

        if (pos > MAX_HEADER_SIZE) {
            state_ = ParseState::ERROR;
            error_code_ = StatusCode::HeaderFieldsTooLarge;
            return;
        }

        if (header_count_ >= MAX_HEADER_COUNT) {
            state_ = ParseState::ERROR;
            error_code_ = StatusCode::HeaderFieldsTooLarge;
            return;
        }

        headers_size_ += pos + 2;
        if (headers_size_ > MAX_TOTAL_HEADERS) {
            state_ = ParseState::ERROR;
            error_code_ = StatusCode::HeaderFieldsTooLarge;
            return;
        }

        std::string_view header_line(buffer_.data(), pos);
        auto colon_pos = header_line.find(':');
        if (colon_pos == std::string::npos) {
            state_ = ParseState::ERROR;
            error_code_ = StatusCode::BadRequest;
            return;
        }

        std::string_view name = header_line.substr(0, colon_pos);
        if (name.find(' ') != std::string::npos) {
            state_ = ParseState::ERROR;
            error_code_ = StatusCode::BadRequest;
            return;
        }

        std::string_view value = header_line.substr(colon_pos + 1);
        std::string name_lower = to_lower(name);
        std::string val_trimmed = trim(value);

        request_.headers[name_lower] = val_trimmed;
        header_count_++;
        
        buffer_.erase(0, pos + 2);
    }
}

void HttpParser::parse_body() {
    size_t expected_length = request_.content_length();
    
    if (expected_length > MAX_BODY_SIZE) {
        state_ = ParseState::ERROR;
        error_code_ = StatusCode::PayloadTooLarge;
        return;
    }

    if (request_.method == "POST" && !request_.has_header("content-length")) {
        state_ = ParseState::ERROR;
        error_code_ = StatusCode::BadRequest;
        return;
    }

    if (buffer_.size() >= expected_length) {
        request_.body = buffer_.substr(0, expected_length);
        buffer_.erase(0, expected_length);
        state_ = ParseState::COMPLETE;
    }
}

} // namespace httpserver::http
