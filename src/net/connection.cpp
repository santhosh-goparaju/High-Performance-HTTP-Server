#include "net/connection.hpp"
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <vector>

namespace httpserver::net {

Connection::Connection(Socket&& socket) noexcept : socket_(std::move(socket)) {}

ssize_t Connection::read_some(void* buffer, size_t max_size) noexcept {
    if (!is_open() || max_size == 0) {
        return 0;
    }

    while (true) {
        ssize_t bytes = ::recv(socket_.fd(), buffer, max_size, 0);
        if (bytes == -1) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, retry
            }
            return -1; // Error (including EAGAIN/EWOULDBLOCK for timeout)
        }
        return bytes; // 0 = disconnect, >0 = data read
    }
}

ssize_t Connection::read_all(void* buffer, size_t max_size) noexcept {
    if (!is_open() || max_size == 0) {
        return 0;
    }

    auto* char_buffer = static_cast<char*>(buffer);
    ssize_t total_read = 0;
    size_t remaining = max_size;

    while (remaining > 0) {
        ssize_t bytes = ::recv(socket_.fd(), char_buffer + total_read, remaining, 0);
        if (bytes == -1) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, retry
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more data right now
            }
            return -1; // Error
        }
        if (bytes == 0) {
            break; // Connection closed
        }
        total_read += bytes;
        remaining -= static_cast<size_t>(bytes);
    }
    return total_read;
}

bool Connection::read_until(std::string& buffer, std::string_view delimiter, size_t max_size) noexcept {
    if (!is_open()) return false;

    // First check if delimiter is already in read_buffer_
    auto pos = read_buffer_.find(delimiter);
    if (pos != std::string::npos) {
        buffer = read_buffer_.substr(0, pos + delimiter.length());
        read_buffer_.erase(0, pos + delimiter.length());
        return true;
    }

    constexpr size_t CHUNK_SIZE = 4096;
    std::vector<char> temp(CHUNK_SIZE);

    while (read_buffer_.size() < max_size) {
        size_t to_read = std::min(CHUNK_SIZE, max_size - read_buffer_.size());
        ssize_t bytes = ::recv(socket_.fd(), temp.data(), to_read, 0);

        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        }

        if (bytes == 0) {
            break;
        }

        size_t old_size = read_buffer_.size();
        read_buffer_.append(temp.data(), static_cast<size_t>(bytes));

        // Optimization: search only near the new data
        size_t start_pos = (old_size >= delimiter.length()) ? old_size - delimiter.length() + 1 : 0;
        pos = read_buffer_.find(delimiter, start_pos);
        if (pos != std::string::npos) {
            buffer = read_buffer_.substr(0, pos + delimiter.length());
            read_buffer_.erase(0, pos + delimiter.length());
            return true;
        }
    }

    return false;
}

bool Connection::write_all(const void* data, size_t length) noexcept {
    if (!is_open() || length == 0) {
        return true; // Nothing to write is success
    }

    const char* char_data = static_cast<const char*>(data);
    size_t total_written = 0;
    size_t remaining = length;

    while (remaining > 0) {
        ssize_t bytes = ::send(socket_.fd(), char_data + total_written, remaining, 0);
        if (bytes == -1) {
            if (errno == EINTR) {
                continue; // Retry
            }
            return false;
        }
        total_written += static_cast<size_t>(bytes);
        remaining -= static_cast<size_t>(bytes);
    }
    return true;
}

bool Connection::write_all(std::string_view data) noexcept {
    return write_all(data.data(), data.length());
}

void Connection::set_read_timeout(int seconds) noexcept {
    socket_.set_recv_timeout(seconds);
}

void Connection::set_write_timeout(int seconds) noexcept {
    socket_.set_send_timeout(seconds);
}

void Connection::close() noexcept {
    socket_.close();
}

} // namespace httpserver::net
