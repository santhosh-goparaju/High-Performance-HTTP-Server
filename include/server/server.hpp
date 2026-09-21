#pragma once

#include "server/config.hpp"
#include "server/thread_pool.hpp"
#include "router/router.hpp"
#include "net/listener.hpp"
#include "net/socket.hpp"
#include <memory>
#include <atomic>

namespace httpserver::server {

class Server {
public:
    explicit Server(Config config);

    /// Register default routes (/, /health, /api/*, /static)
    void setup_routes();
    /// Start accepting connections (blocking call)
    void start();
    /// Graceful shutdown
    void stop();

    /// Access router for adding custom routes
    router::Router& router();

private:
    Config config_;
    net::Listener listener_;
    router::Router router_;
    std::unique_ptr<ThreadPool> pool_;
    std::atomic<bool> running_;

    void handle_connection(net::Socket client_socket);
    void accept_loop();
    void accept_loop_baseline();
};

} // namespace httpserver::server
