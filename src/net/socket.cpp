#include "net/socket.hpp"
#include <iostream>
#include <sys/time.h>
#include <cerrno>
#include <cstring>

namespace httpserver::net {

Socket::Socket(int fd) noexcept : fd_(fd) {}

Socket::Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

Socket::~Socket() {
    close();
}

void Socket::close() noexcept {
    if (is_valid()) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Socket::set_nonblocking() const noexcept {
    if (!is_valid()) return false;
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags == -1) return false;
    return ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != -1;
}

bool Socket::set_reuse_addr() const noexcept {
    if (!is_valid()) return false;
    int optval = 1;
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != -1;
}

bool Socket::set_reuse_port() const noexcept {
    if (!is_valid()) return false;
    int optval = 1;
#ifdef SO_REUSEPORT
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) != -1;
#else
    return false;
#endif
}

bool Socket::set_recv_timeout(int seconds) const noexcept {
    if (!is_valid()) return false;
    struct timeval tv{};
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != -1;
}

bool Socket::set_send_timeout(int seconds) const noexcept {
    if (!is_valid()) return false;
    struct timeval tv{};
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != -1;
}

Socket Socket::create(int domain, int type, int protocol) noexcept {
    int fd = ::socket(domain, type, protocol);
    return Socket{fd};
}

} // namespace httpserver::net
