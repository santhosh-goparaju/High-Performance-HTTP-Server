#include "server/server.hpp"
#include "server/config.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>

namespace {

static std::atomic<uint16_t> g_sec_port{9190};

std::string send_sec_request(uint16_t port, const char* data, size_t len) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(sock);
        return "";
    }

    struct timeval tv{2, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (len > 0) {
        ::send(sock, data, len, 0);
    }

    char buf[8192];
    std::string response;
    ssize_t n;
    while ((n = ::recv(sock, buf, sizeof(buf), 0)) > 0) {
        response.append(buf, static_cast<size_t>(n));
    }
    ::close(sock);
    return response;
}

std::string send_sec_request(uint16_t port, const std::string& request) {
    return send_sec_request(port, request.c_str(), request.size());
}

class SecurityIntegrationTest : public ::testing::Test {
protected:
    uint16_t port_;
    std::unique_ptr<httpserver::server::Server> server_;
    std::jthread server_thread_;

    void SetUp() override {
        port_ = g_sec_port.fetch_add(1);
        httpserver::server::Config config;
        config.port = port_;
        config.num_workers = 2;
        config.static_dir = "./static";

        server_ = std::make_unique<httpserver::server::Server>(config);
        server_->setup_routes();

        server_thread_ = std::jthread([this]() {
            server_->start();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        if (server_) {
            server_->stop();
        }
    }
};

TEST_F(SecurityIntegrationTest, PathTraversal) {
    auto response = send_sec_request(port_, "GET /static/../../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_FALSE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(SecurityIntegrationTest, OversizedRequestLine) {
    std::string req = "GET /" + std::string(9000, 'a') + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    auto response = send_sec_request(port_, req);
    EXPECT_FALSE(response.empty());
    EXPECT_FALSE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(SecurityIntegrationTest, OversizedHeaders) {
    std::string req = "GET / HTTP/1.1\r\nHost: localhost\r\n";
    for (int i = 0; i < 150; i++) {
        req += "X-Custom-Header-" + std::to_string(i) + ": " + std::string(100, 'a') + "\r\n";
    }
    req += "\r\n";
    auto response = send_sec_request(port_, req);
    EXPECT_FALSE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(SecurityIntegrationTest, OversizedBody) {
    std::string req = "POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100000000\r\n\r\n";
    req += std::string(1024, 'a');
    auto response = send_sec_request(port_, req);
    EXPECT_FALSE(response.empty());
    EXPECT_FALSE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(SecurityIntegrationTest, EmptyRequest) {
    auto response = send_sec_request(port_, "");
    // Empty request should either get no response or a 400
    EXPECT_TRUE(response.empty() || response.starts_with("HTTP/1.1 400"));
}

TEST_F(SecurityIntegrationTest, InvalidMethod) {
    auto response = send_sec_request(port_, "TRACE / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_FALSE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(SecurityIntegrationTest, MissingHostHeader) {
    // HTTP/1.1 technically requires Host, but we're lenient
    auto response = send_sec_request(port_, "GET / HTTP/1.1\r\n\r\n");
    EXPECT_TRUE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(SecurityIntegrationTest, VeryLongUrlPath) {
    std::string req = "GET /" + std::string(8192, 'a') + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    auto response = send_sec_request(port_, req);
    EXPECT_FALSE(response.starts_with("HTTP/1.1 200"));
}

} // namespace
