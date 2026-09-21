#include "router/router.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace httpserver::router {

void Router::add_route(const std::string& method, const std::string& path, handler func) {
    routes_.push_back({method, path, std::move(func)});
}

void Router::add_static(const std::string& prefix, const std::string& directory) {
    static_mounts_.push_back({prefix, directory});
}

httpserver::http::Response Router::route(const httpserver::http::Request& request) const {
    bool path_found = false;

    for (const auto& route : routes_) {
        if (route.path == request.target) {
            path_found = true;
            if (route.method == request.method) {
                try {
                    return route.func(request);
                } catch (const std::exception& e) {
                    return httpserver::http::Response::error(
                        httpserver::http::StatusCode::InternalServerError,
                        "Internal Server Error");
                } catch (...) {
                    return httpserver::http::Response::error(
                        httpserver::http::StatusCode::InternalServerError,
                        "Internal Server Error");
                }
            }
        }
    }

    if (path_found) {
        // Path matched but method didn't — 405
        std::string allow_methods;
        for (const auto& route : routes_) {
            if (route.path == request.target) {
                if (!allow_methods.empty()) {
                    allow_methods += ", ";
                }
                allow_methods += route.method;
            }
        }
        return httpserver::http::Response::method_not_allowed(allow_methods);
    }

    // Check static file mounts
    for (const auto& mount : static_mounts_) {
        if (request.target.starts_with(mount.prefix)) {
            if (request.method == "GET" || request.method == "HEAD") {
                return serve_static_file(mount, request.target);
            } else {
                return httpserver::http::Response::method_not_allowed("GET, HEAD");
            }
        }
    }

    return httpserver::http::Response::not_found();
}

httpserver::http::Response Router::serve_static_file(
    const StaticMount& mount, const std::string& path) const {

    std::string rel_path = path.substr(mount.prefix.length());
    if (rel_path.empty() || rel_path[0] != '/') {
        rel_path = "/" + rel_path;
    }

    if (!is_path_safe(rel_path)) {
        return httpserver::http::Response::error(
            httpserver::http::StatusCode::Forbidden, "Forbidden");
    }

    // Safely combine directory and relative path
    namespace fs = std::filesystem;
    fs::path full_path = fs::path(mount.directory) / fs::path(rel_path).relative_path();

    // Resolve and verify the canonical path is within the static directory
    std::error_code ec;
    fs::path canonical = fs::canonical(full_path, ec);
    if (ec) {
        return httpserver::http::Response::not_found();
    }

    fs::path canonical_root = fs::canonical(mount.directory, ec);
    if (ec) {
        return httpserver::http::Response::error(
            httpserver::http::StatusCode::InternalServerError);
    }

    // Verify the resolved path is within the mount directory
    auto root_str = canonical_root.string();
    auto file_str = canonical.string();
    if (file_str.substr(0, root_str.size()) != root_str) {
        return httpserver::http::Response::error(
            httpserver::http::StatusCode::Forbidden, "Forbidden");
    }

    if (!fs::is_regular_file(canonical)) {
        return httpserver::http::Response::not_found();
    }

    std::ifstream file(canonical, std::ios::binary);
    if (!file) {
        return httpserver::http::Response::error(
            httpserver::http::StatusCode::InternalServerError);
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    auto res = httpserver::http::Response::ok(ss.str());
    res.set_header("Content-Type", get_mime_type(canonical.string()));
    return res;
}

std::string Router::get_mime_type(const std::string& path) {
    auto dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(dot_pos);

    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".txt") return "text/plain";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";

    return "application/octet-stream";
}

bool Router::is_path_safe(const std::string& path) {
    if (path.find("..") != std::string::npos) return false;
    if (path.find('\0') != std::string::npos) return false;
    if (path.empty() || path[0] != '/') return false;
    return true;
}

} // namespace httpserver::router
