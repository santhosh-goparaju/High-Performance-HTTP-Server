#pragma once

#include <cstdint>
#include <string>
#include <cstddef>

namespace httpserver::server {

struct Config {
    uint16_t port = 8080;
    int backlog = 128;
    int num_workers = 4;
    size_t max_queue_size = 256;
    int read_timeout_sec = 30;
    int write_timeout_sec = 30;
    int keepalive_timeout_sec = 30;
    int max_requests_per_conn = 1000;
    std::string static_dir = "./static";
    bool enable_keepalive = true;

    static Config parse_args(int argc, char* argv[]);
};

} // namespace httpserver::server
