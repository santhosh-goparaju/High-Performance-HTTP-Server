#pragma once

#include "net/socket.hpp"
#include <string>
#include <string_view>
#include <cstdint>
#include <sys/types.h>

namespace httpserver::net {

/**
 * @brief Handles I/O operations for an established connection.
 */
class Connection {
public:
    explicit Connection(Socket&& socket) noexcept;

    // Non-copyable, move-able
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept = default;
    Connection& operator=(Connection&&) noexcept = default;

    /// Read up to max_size bytes in a single recv() call.
    /// Returns bytes read, 0 on disconnect, -1 on error.
    ssize_t read_some(void* buffer, size_t max_size) noexcept;

    /// Read until buffer is full or connection closes/blocks.
    ssize_t read_all(void* buffer, size_t max_size) noexcept;

    /// Read until delimiter is found, accumulating in buffer.
    bool read_until(std::string& buffer, std::string_view delimiter, size_t max_size) noexcept;

    /// Write all bytes, handling partial writes. Returns true on success.
    bool write_all(const void* data, size_t length) noexcept;
    bool write_all(std::string_view data) noexcept;

    void set_read_timeout(int seconds) noexcept;
    void set_write_timeout(int seconds) noexcept;

    void close() noexcept;
    bool is_open() const noexcept { return socket_.is_valid(); }
    int fd() const noexcept { return socket_.fd(); }

private:
    Socket socket_;
    std::string read_buffer_;
};

} // namespace httpserver::net
