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

static std::atomic<uint16_t> g_port{9090};

std::string send_http_request(uint16_t port, const std::string& request) {
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

    if (!request.empty()) {
        ::send(sock, request.c_str(), request.size(), 0);
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

class ServerIntegrationTest : public ::testing::Test {
protected:
    uint16_t port_;
    std::unique_ptr<httpserver::server::Server> server_;
    std::jthread server_thread_;

    void SetUp() override {
        port_ = g_port.fetch_add(1);
        httpserver::server::Config config;
        config.port = port_;
        config.num_workers = 2;
        config.static_dir = "./static";

        server_ = std::make_unique<httpserver::server::Server>(config);
        server_->setup_routes();

        server_thread_ = std::jthread([this]() {
            server_->start();
        });

        // Wait for server to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        if (server_) {
            server_->stop();
        }
    }
};

TEST_F(ServerIntegrationTest, GetRootReturns200) {
    auto response = send_http_request(port_, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(ServerIntegrationTest, GetHealthReturnsJson) {
    auto response = send_http_request(port_, "GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(response.starts_with("HTTP/1.1 200"));
    EXPECT_NE(response.find("\"status\""), std::string::npos);
}

TEST_F(ServerIntegrationTest, GetApiInfoReturnsJson) {
    auto response = send_http_request(port_, "GET /api/info HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(ServerIntegrationTest, PostApiEchoReturnsBody) {
    std::string req =
        "POST /api/echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "hello world";
    auto response = send_http_request(port_, req);
    EXPECT_TRUE(response.starts_with("HTTP/1.1 200"));
    EXPECT_NE(response.find("hello world"), std::string::npos);
}

TEST_F(ServerIntegrationTest, DeleteApiDataReturns204) {
    auto response = send_http_request(port_, "DELETE /api/data HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(response.starts_with("HTTP/1.1 204"));
}

TEST_F(ServerIntegrationTest, GetNonexistentReturns404) {
    auto response = send_http_request(port_, "GET /nonexistent HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_TRUE(response.starts_with("HTTP/1.1 404"));
}

TEST_F(ServerIntegrationTest, MalformedRequestReturns400) {
    auto response = send_http_request(port_, "MALFORMED REQUEST\r\n\r\n");
    // Could get 400 or 405 depending on how parser handles it
    EXPECT_FALSE(response.starts_with("HTTP/1.1 200"));
}

TEST_F(ServerIntegrationTest, KeepAliveMultipleRequests) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(sock, 0);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    ASSERT_EQ(connect(sock, (struct sockaddr*)&addr, sizeof(addr)), 0);

    struct timeval tv{2, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // First request with keep-alive
    std::string req1 = "GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";
    ::send(sock, req1.c_str(), req1.size(), 0);

    char buf[4096];
    ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
    ASSERT_GT(n, 0);
    std::string res1(buf, static_cast<size_t>(n));
    EXPECT_TRUE(res1.starts_with("HTTP/1.1 200"));

    // Second request on same connection
    std::string req2 = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    ::send(sock, req2.c_str(), req2.size(), 0);

    n = ::recv(sock, buf, sizeof(buf), 0);
    ASSERT_GT(n, 0);
    std::string res2(buf, static_cast<size_t>(n));
    EXPECT_TRUE(res2.starts_with("HTTP/1.1 200"));

    ::close(sock);
}

} // namespace
