#pragma once

#include "http/request.hpp"
#include "http/response.hpp"

#include <string>
#include <vector>
#include <functional>

namespace httpserver::router {

class Router {
public:
    using handler = std::function<httpserver::http::Response(const httpserver::http::Request&)>;

    void add_route(const std::string& method, const std::string& path, handler func);
    void add_static(const std::string& prefix, const std::string& directory);
    httpserver::http::Response route(const httpserver::http::Request& request) const;

private:
    struct Route {
        std::string method;
        std::string path;
        handler func;
    };
    std::vector<Route> routes_;

    struct StaticMount {
        std::string prefix;
        std::string directory;
    };
    std::vector<StaticMount> static_mounts_;

    httpserver::http::Response serve_static_file(const StaticMount& mount, const std::string& path) const;
    static std::string get_mime_type(const std::string& path);
    static bool is_path_safe(const std::string& path);
};

} // namespace httpserver::router
