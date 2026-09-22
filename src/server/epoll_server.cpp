#ifdef __linux__

#include "server/epoll_server.hpp"

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>

namespace httpserver::server {

EpollServer::EpollServer(uint16_t port, int backlog, int max_events)
    : listener_(port, backlog), max_events_(max_events) {
    epoll_fd_ = ::epoll_create1(0);
    if (epoll_fd_ == -1) {
        std::cerr << "epoll_create1 failed: " << std::strerror(errno) << "\n";
    }
}

EpollServer::~EpollServer() {
    stop();
    if (epoll_fd_ != -1) {
        ::close(epoll_fd_);
    }
}

void EpollServer::set_keepalive(bool enable, int timeout_sec, int max_requests) {
    enable_keepalive_ = enable;
    keepalive_timeout_sec_ = timeout_sec;
    max_requests_per_conn_ = max_requests;
}

void EpollServer::start() {
    if (!listener_.start()) {
        std::cerr << "Failed to start listener\n";
        return;
    }

    // Make listener non-blocking
    int listen_fd = listener_.fd();
    int flags = ::fcntl(listen_fd, F_GETFL, 0);
    ::fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    if (!add_to_epoll(listen_fd, EPOLLIN)) {
        std::cerr << "Failed to add listener to epoll\n";
        return;
    }

    running_ = true;
    std::cerr << "epoll server listening on port " << listener_.port() << "\n";

    std::vector<struct epoll_event> events(static_cast<size_t>(max_events_));

    while (running_) {
        int n = ::epoll_wait(epoll_fd_, events.data(), max_events_, 1000);
        if (n == -1) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait error: " << std::strerror(errno) << "\n";
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[static_cast<size_t>(i)].data.fd;
            uint32_t ev = events[static_cast<size_t>(i)].events;

            if (fd == listen_fd) {
                accept_connections();
            } else if (ev & (EPOLLERR | EPOLLHUP)) {
                close_connection(fd);
            } else if (ev & EPOLLIN) {
                handle_read(fd);
            } else if (ev & EPOLLOUT) {
                handle_write(fd);
            }
        }
    }
}

void EpollServer::stop() {
    running_ = false;
    listener_.stop();
    // Close all connections
    for (auto& [fd, conn] : connections_) {
        remove_from_epoll(fd);
    }
    connections_.clear();
}

void EpollServer::accept_connections() {
    while (true) {
        net::Socket client = listener_.accept();
        if (!client.is_valid()) {
            break; // No more pending connections
        }

        int fd = client.fd();

        // Set non-blocking
        int flags = ::fcntl(fd, F_GETFL, 0);
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        if (!add_to_epoll(fd, EPOLLIN | EPOLLET)) {
            continue;
        }

        auto conn = std::make_unique<EpollConnection>(std::move(client));
        connections_[fd] = std::move(conn);
    }
}

void EpollServer::handle_read(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    auto& econn = *it->second;
    char buf[4096];

    while (true) {
        ssize_t bytes = ::recv(fd, buf, sizeof(buf), 0);
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more data
            }
            if (errno == EINTR) continue;
            close_connection(fd);
            return;
        }
        if (bytes == 0) {
            close_connection(fd);
            return;
        }

        auto result = econn.parser.feed(buf, static_cast<size_t>(bytes));

        if (result == http::ParseResult::COMPLETE) {
            auto req = econn.parser.get_request();
            bool keep_alive = enable_keepalive_ && req.should_keep_alive();

            http::Response res;
            if (router_) {
                res = router_->route(req);
            } else {
                res = http::Response::error(http::StatusCode::InternalServerError);
            }
            res.keep_alive = keep_alive;

            econn.write_buffer = http::HttpSerializer::serialize(res);
            econn.write_offset = 0;
            econn.keep_alive = keep_alive;
            econn.request_count++;
            econn.state = EpollConnection::State::WRITING;

            // Switch to write mode
            modify_epoll(fd, EPOLLOUT | EPOLLET);

            // Try to write immediately
            handle_write(fd);
            return;

        } else if (result == http::ParseResult::ERROR) {
            auto res = http::Response::error(econn.parser.error_code());
            res.keep_alive = false;
            econn.write_buffer = http::HttpSerializer::serialize(res);
            econn.write_offset = 0;
            econn.keep_alive = false;
            econn.state = EpollConnection::State::WRITING;
            modify_epoll(fd, EPOLLOUT | EPOLLET);
            handle_write(fd);
            return;
        }
        // INCOMPLETE — continue reading
    }
}

void EpollServer::handle_write(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    auto& econn = *it->second;

    while (econn.write_offset < econn.write_buffer.size()) {
        const char* data = econn.write_buffer.data() + econn.write_offset;
        size_t remaining = econn.write_buffer.size() - econn.write_offset;

        ssize_t bytes = ::send(fd, data, remaining, MSG_NOSIGNAL);
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return; // Will be called again on EPOLLOUT
            }
            if (errno == EINTR) continue;
            close_connection(fd);
            return;
        }
        econn.write_offset += static_cast<size_t>(bytes);
    }

    // Write complete
    if (!econn.keep_alive || econn.request_count >= max_requests_per_conn_) {
        close_connection(fd);
        return;
    }

    // Reset for next request on this connection
    econn.parser.reset();
    econn.write_buffer.clear();
    econn.write_offset = 0;
    econn.state = EpollConnection::State::READING;
    modify_epoll(fd, EPOLLIN | EPOLLET);
}

void EpollServer::close_connection(int fd) {
    remove_from_epoll(fd);
    connections_.erase(fd);
}

bool EpollServer::add_to_epoll(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool EpollServer::modify_epoll(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

void EpollServer::remove_from_epoll(int fd) {
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

} // namespace httpserver::server

#endif // __linux__
