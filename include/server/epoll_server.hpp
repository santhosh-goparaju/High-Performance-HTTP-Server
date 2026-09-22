#pragma once

#ifdef __linux__

#include "net/socket.hpp"
#include "net/connection.hpp"
#include "router/router.hpp"
#include "http/parser.hpp"
#include "http/response.hpp"
#include "http/serializer.hpp"

#include <sys/epoll.h>
#include <functional>
#include <unordered_map>
#include <memory>
#include <vector>
#include <atomic>
#include <cstdint>

namespace httpserver::server {

/**
 * @brief Per-connection state for the epoll event loop.
 */
struct EpollConnection {
    net::Connection conn;
    http::HttpParser parser;
    std::string write_buffer;
    size_t write_offset{0};
    int request_count{0};
    bool keep_alive{true};

    enum class State { READING, WRITING, CLOSING };
    State state{State::READING};

    explicit EpollConnection(net::Socket&& sock)
        : conn(std::move(sock)) {}
};

/**
 * @brief Event-driven I/O using Linux epoll.
 *
 * Single-threaded event loop that handles all connections
 * via non-blocking I/O and epoll_wait. Only compiled on Linux.
 */
class EpollServer {
public:
    EpollServer(uint16_t port, int backlog, int max_events = 1024);
    ~EpollServer();

    // Non-copyable, non-movable
    EpollServer(const EpollServer&) = delete;
    EpollServer& operator=(const EpollServer&) = delete;

    void set_router(router::Router* router) { router_ = router; }
    void set_keepalive(bool enable, int timeout_sec, int max_requests);
    void start();
    void stop();

private:
    void accept_connections();
    void handle_read(int fd);
    void handle_write(int fd);
    void close_connection(int fd);

    bool add_to_epoll(int fd, uint32_t events);
    bool modify_epoll(int fd, uint32_t events);
    void remove_from_epoll(int fd);

    int epoll_fd_{-1};
    net::Listener listener_;
    router::Router* router_{nullptr};
    std::unordered_map<int, std::unique_ptr<EpollConnection>> connections_;
    std::atomic<bool> running_{false};

    int max_events_;
    bool enable_keepalive_{true};
    int keepalive_timeout_sec_{30};
    int max_requests_per_conn_{1000};
};

} // namespace httpserver::server

#endif // __linux__
