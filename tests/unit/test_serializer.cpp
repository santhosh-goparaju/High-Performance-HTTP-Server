#include "http/serializer.hpp"
#include "http/response.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace httpserver::http;

TEST(HttpSerializerTest, Serialize200OKWithBody) {
    auto response = Response::ok("Hello World");

    std::string out = HttpSerializer::serialize(response);

    EXPECT_TRUE(out.starts_with("HTTP/1.1 200 OK\r\n"));
    EXPECT_NE(out.find("Content-Length: 11\r\n"), std::string::npos);
    EXPECT_NE(out.find("Server: "), std::string::npos);
    EXPECT_NE(out.find("Date: "), std::string::npos);
    EXPECT_TRUE(out.ends_with("\r\n\r\nHello World"));
}

TEST(HttpSerializerTest, Serialize404NotFound) {
    auto response = Response::not_found();
    std::string out = HttpSerializer::serialize(response);
    EXPECT_TRUE(out.starts_with("HTTP/1.1 404 Not Found\r\n"));
}

TEST(HttpSerializerTest, Serialize400BadRequestWithMessage) {
    auto response = Response::bad_request("Bad Request Msg");
    std::string out = HttpSerializer::serialize(response);
    EXPECT_TRUE(out.starts_with("HTTP/1.1 400 Bad Request\r\n"));
    EXPECT_NE(out.find("Content-Length: 15\r\n"), std::string::npos);
    EXPECT_TRUE(out.ends_with("\r\n\r\nBad Request Msg"));
}

TEST(HttpSerializerTest, CustomHeaders) {
    auto response = Response::ok("test");
    response.set_header("X-Custom-Header", "CustomValue");
    std::string out = HttpSerializer::serialize(response);
    EXPECT_NE(out.find("X-Custom-Header: CustomValue\r\n"), std::string::npos);
}

TEST(HttpSerializerTest, ConnectionKeepAlive) {
    auto response = Response::ok("test");
    response.keep_alive = true;
    std::string out = HttpSerializer::serialize(response);
    EXPECT_NE(out.find("Connection: keep-alive\r\n"), std::string::npos);
}

TEST(HttpSerializerTest, ConnectionClose) {
    auto response = Response::ok("test");
    response.keep_alive = false;
    std::string out = HttpSerializer::serialize(response);
    EXPECT_NE(out.find("Connection: close\r\n"), std::string::npos);
}

TEST(HttpSerializerTest, Serialize204NoContent) {
    auto response = Response::make_response(StatusCode::NoContent);
    std::string out = HttpSerializer::serialize(response);
    EXPECT_TRUE(out.starts_with("HTTP/1.1 204 No Content\r\n"));
    EXPECT_NE(out.find("Content-Length: 0\r\n"), std::string::npos);
}

TEST(HttpSerializerTest, Serialize503ServiceUnavailable) {
    auto response = Response::make_response(StatusCode::ServiceUnavailable);
    std::string out = HttpSerializer::serialize(response);
    EXPECT_TRUE(out.starts_with("HTTP/1.1 503 Service Unavailable\r\n"));
}
