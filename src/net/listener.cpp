#include "net/listener.hpp"
#include <iostream>
#include <cerrno>
#include <cstring>

namespace httpserver::net {

Listener::Listener(uint16_t port, int backlog) noexcept
    : port_(port), backlog_(backlog) {}

bool Listener::start() noexcept {
    socket_ = Socket::create(AF_INET, SOCK_STREAM, 0);
    if (!socket_.is_valid()) {
        std::cerr << "Failed to create socket: " << std::strerror(errno) << "\n";
        return false;
    }

    if (!socket_.set_reuse_addr()) {
        std::cerr << "Failed to set SO_REUSEADDR: " << std::strerror(errno) << "\n";
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (::bind(socket_.fd(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::cerr << "Failed to bind to port " << port_ << ": " << std::strerror(errno) << "\n";
        return false;
    }

    if (::listen(socket_.fd(), backlog_) == -1) {
        std::cerr << "Failed to listen on socket: " << std::strerror(errno) << "\n";
        return false;
    }

    return true;
}

Socket Listener::accept() noexcept {
    if (!socket_.is_valid()) {
        return Socket{-1};
    }

    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = ::accept(socket_.fd(), reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    
    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            std::cerr << "Failed to accept connection: " << std::strerror(errno) << "\n";
        }
    }
    
    return Socket{client_fd};
}

void Listener::stop() noexcept {
    socket_.close();
}

} // namespace httpserver::net
