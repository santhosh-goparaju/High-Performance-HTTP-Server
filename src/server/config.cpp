#include "server/config.hpp"
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace httpserver::server {

Config Config::parse_args(int argc, char* argv[]) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port <port>\n"
                      << "  --workers <count> (0 = thread-per-connection)\n"
                      << "  --queue-size <size>\n"
                      << "  --static-dir <dir>\n"
                      << "  --no-keepalive\n"
                      << "  --timeout <sec>\n"
                      << "  --help\n";
            std::exit(0);
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--workers" && i + 1 < argc) {
            config.num_workers = std::stoi(argv[++i]);
        } else if (arg == "--queue-size" && i + 1 < argc) {
            config.max_queue_size = static_cast<size_t>(std::stoull(argv[++i]));
        } else if (arg == "--static-dir" && i + 1 < argc) {
            config.static_dir = argv[++i];
        } else if (arg == "--no-keepalive") {
            config.enable_keepalive = false;
        } else if (arg == "--timeout" && i + 1 < argc) {
            int t = std::stoi(argv[++i]);
            config.read_timeout_sec = t;
            config.write_timeout_sec = t;
            config.keepalive_timeout_sec = t;
        }
    }
    return config;
}

} // namespace httpserver::server
