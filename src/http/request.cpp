#include "http/request.hpp"
#include <algorithm>
#include <cctype>

namespace httpserver::http {

static std::string to_lower(std::string_view s) {
    std::string result(s);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

std::optional<std::string_view> Request::header(std::string_view name) const {
    auto it = headers.find(to_lower(name));
    if (it != headers.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Request::has_header(std::string_view name) const {
    return headers.find(to_lower(name)) != headers.end();
}

size_t Request::content_length() const {
    auto cl = header("content-length");
    if (cl) {
        try {
            return std::stoull(std::string(*cl));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

bool Request::should_keep_alive() const {
    auto conn = header("connection");
    if (conn) {
        if (to_lower(*conn) == "close") {
            return false;
        }
    }
    return version == "HTTP/1.1";
}

} // namespace httpserver::http
