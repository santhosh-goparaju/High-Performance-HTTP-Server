#pragma once

#include "net/socket.hpp"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdint>

namespace httpserver::net {

/**
 * @brief TCP Listener that accepts incoming connections.
 */
class Listener {
public:
    explicit Listener(uint16_t port, int backlog = 128) noexcept;

    // Non-copyable, move-able
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener(Listener&&) noexcept = default;
    Listener& operator=(Listener&&) noexcept = default;

    bool start() noexcept;
    Socket accept() noexcept;
    void stop() noexcept;
    void close() noexcept { stop(); }

    uint16_t port() const noexcept { return port_; }
    int fd() const noexcept { return socket_.fd(); }

private:
    Socket socket_;
    uint16_t port_;
    int backlog_;
};

} // namespace httpserver::net
