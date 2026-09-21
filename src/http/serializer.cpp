#include "http/serializer.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace httpserver::http {

std::string HttpSerializer::format_date() {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::gmtime(&t);
    
    char buffer[128];
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    return std::string(buffer);
}

std::string HttpSerializer::serialize(const Response& response) {
    std::string res;
    // Estimate size to avoid reallocations
    size_t headers_size = 256; 
    for (const auto& [k, v] : response.headers) {
        headers_size += k.size() + v.size() + 4;
    }
    res.reserve(headers_size + response.body.size() + 128);

    res += "HTTP/1.1 ";
    res += std::to_string(static_cast<int>(response.status));
    res += " ";
    res += std::string(reason_phrase(response.status));
    res += "\r\n";

    res += "Server: httpserver/1.0\r\n";
    res += "Date: " + format_date() + "\r\n";
    res += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    
    if (response.keep_alive) {
        res += "Connection: keep-alive\r\n";
    } else {
        res += "Connection: close\r\n";
    }

    for (const auto& [key, value] : response.headers) {
        // Skip headers we've already added manually
        std::string k_lower = key;
        for (char& c : k_lower) c = std::tolower(c);
        
        if (k_lower != "server" && k_lower != "date" && 
            k_lower != "content-length" && k_lower != "connection") {
            res += key + ": " + value + "\r\n";
        }
    }

    res += "\r\n";
    res += response.body;

    return res;
}

} // namespace httpserver::http
