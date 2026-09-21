#include "server/server.hpp"
#include "net/connection.hpp"
#include "http/parser.hpp"
#include "http/response.hpp"
#include "http/serializer.hpp"

#include <iostream>
#include <thread>
#include <vector>

namespace httpserver::server {

Server::Server(Config config)
    : config_(std::move(config)),
      listener_(config_.port, config_.backlog),
      running_(false) {
}

void Server::setup_routes() {
    router_.add_route("GET", "/", [](const http::Request&) {
        return http::Response::ok(
            "<html><body><h1>Welcome to High-Performance HTTP Server!</h1></body></html>",
            "text/html");
    });

    router_.add_route("GET", "/health", [](const http::Request&) {
        return http::Response::json("{\"status\":\"ok\"}");
    });

    router_.add_route("GET", "/api/info", [](const http::Request&) {
        return http::Response::json(
            "{\"name\":\"httpserver\",\"version\":\"1.0\",\"language\":\"C++20\"}");
    });

    router_.add_route("POST", "/api/echo", [](const http::Request& req) {
        return http::Response::json(req.body);
    });

    router_.add_route("DELETE", "/api/data", [](const http::Request&) {
        return http::Response::make_response(http::StatusCode::NoContent);
    });

    router_.add_static("/static", config_.static_dir);
}

void Server::start() {
    if (!listener_.start()) {
        std::cerr << "Failed to start listener on port " << config_.port << "\n";
        return;
    }
    running_ = true;

    std::cerr << "Server listening on port " << config_.port << "\n";

    if (config_.num_workers > 0) {
        pool_ = std::make_unique<ThreadPool>(config_.num_workers, config_.max_queue_size);
        std::cerr << "Thread pool mode: " << config_.num_workers << " workers, queue size "
                  << config_.max_queue_size << "\n";
        accept_loop();
    } else {
        std::cerr << "Baseline mode: thread-per-connection\n";
        accept_loop_baseline();
    }
}

void Server::stop() {
    running_ = false;
    listener_.stop();
    if (pool_) {
        pool_->shutdown();
    }
    std::cerr << "Server stopped.\n";
}

router::Router& Server::router() {
    return router_;
}

void Server::accept_loop() {
    while (running_) {
        net::Socket client = listener_.accept();
        if (!client.is_valid()) {
            continue;
        }

        // Wrap move-only Socket in shared_ptr so the lambda is copyable
        // (required by std::function)
        auto shared_sock = std::make_shared<net::Socket>(std::move(client));

        auto task = [this, shared_sock]() {
            handle_connection(std::move(*shared_sock));
        };

        if (!pool_->enqueue(std::move(task))) {
            // Queue full — send 503 and close immediately
            net::Connection conn(std::move(*shared_sock));
            auto res = http::Response::make_response(http::StatusCode::ServiceUnavailable);
            res.keep_alive = false;
            std::string data = http::HttpSerializer::serialize(res);
            conn.write_all(data);
        }
    }
}

void Server::accept_loop_baseline() {
    std::vector<std::thread> threads;
    while (running_) {
        net::Socket client = listener_.accept();
        if (!client.is_valid()) {
            if (running_) {
                // accept failed but still running — retry
            }
            continue;
        }

        threads.emplace_back([this, sock = std::move(client)]() mutable {
            handle_connection(std::move(sock));
        });
    }

    // Wait for all in-flight threads on shutdown
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void Server::handle_connection(net::Socket client_socket) {
    net::Connection conn(std::move(client_socket));
    conn.set_read_timeout(config_.read_timeout_sec);
    conn.set_write_timeout(config_.write_timeout_sec);

    int request_count = 0;
    http::HttpParser parser;

    while (request_count < config_.max_requests_per_conn) {
        parser.reset();
        bool keep_alive = config_.enable_keepalive;

        // Read and parse request
        std::vector<char> buf(4096);
        bool request_complete = false;

        while (!request_complete) {
            ssize_t bytes_read = conn.read_some(buf.data(), buf.size());

            if (bytes_read <= 0) {
                // Client disconnected or error
                return;
            }

            auto result = parser.feed(buf.data(), static_cast<size_t>(bytes_read));

            if (result == http::ParseResult::COMPLETE) {
                request_complete = true;
            } else if (result == http::ParseResult::ERROR) {
                // Send error response based on parser error code
                auto res = http::Response::error(parser.error_code());
                res.keep_alive = false;
                std::string data = http::HttpSerializer::serialize(res);
                conn.write_all(data);
                return;
            }
            // INCOMPLETE — continue reading
        }

        // Successfully parsed request
        auto req = parser.get_request();

        // Check keep-alive preference
        if (!config_.enable_keepalive || !req.should_keep_alive()) {
            keep_alive = false;
        }

        // Route and handle
        auto res = router_.route(req);
        res.keep_alive = keep_alive;

        // Serialize and send
        std::string response_data = http::HttpSerializer::serialize(res);
        if (!conn.write_all(response_data)) {
            return; // Write failed
        }

        request_count++;

        if (!keep_alive) {
            break;
        }
    }
}

} // namespace httpserver::server
