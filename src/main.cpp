#include "server/server.hpp"
#include <csignal>
#include <iostream>

using httpserver::server::Server;
using httpserver::server::Config;

Server* g_server = nullptr;

void signal_handler(int signum) {
    std::cerr << "\nReceived signal " << signum << ", shutting down...\n";
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    Config config = Config::parse_args(argc, argv);
    Server server(config);
    g_server = &server;
    
    server.setup_routes();
    
    server.start();
    
    return 0;
}
