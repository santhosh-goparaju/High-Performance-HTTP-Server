#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <utility>

namespace httpserver::net {

/**
 * @brief RAII wrapper for a POSIX socket file descriptor.
 */
class Socket {
public:
    Socket() noexcept = default;
    explicit Socket(int fd) noexcept;
    
    // Move-only semantics
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    ~Socket();

    int fd() const noexcept { return fd_; }
    bool is_valid() const noexcept { return fd_ != -1; }
    
    void close() noexcept;
    
    bool set_nonblocking() const noexcept;
    bool set_reuse_addr() const noexcept;
    bool set_reuse_port() const noexcept;
    bool set_recv_timeout(int seconds) const noexcept;
    bool set_send_timeout(int seconds) const noexcept;

    static Socket create(int domain, int type, int protocol) noexcept;

private:
    int fd_{-1};
};

} // namespace httpserver::net
