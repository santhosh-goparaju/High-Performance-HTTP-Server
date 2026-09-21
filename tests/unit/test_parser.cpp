#include "http/parser.hpp"
#include "http/status.hpp"
#include <gtest/gtest.h>

using namespace httpserver::http;

TEST(HttpParserTest, ValidGetRequest) {
    HttpParser parser;
    std::string req = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nAccept: text/html\r\n\r\n";

    auto result = parser.feed(req);
    EXPECT_EQ(result, ParseResult::COMPLETE);

    auto request = parser.get_request();
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.target, "/index.html");
    EXPECT_EQ(request.version, "HTTP/1.1");
    EXPECT_EQ(request.headers.at("host"), "localhost");
    EXPECT_EQ(request.headers.at("accept"), "text/html");
    EXPECT_TRUE(request.body.empty());
    EXPECT_TRUE(request.should_keep_alive());
    EXPECT_EQ(request.content_length(), static_cast<size_t>(0));
}

TEST(HttpParserTest, ValidPostRequest) {
    HttpParser parser;
    std::string body = "{\"key\":\"value\"}";
    std::string req = "POST /api/data HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
                      std::to_string(body.size()) + "\r\n\r\n" + body;

    auto result = parser.feed(req);
    EXPECT_EQ(result, ParseResult::COMPLETE);

    auto request = parser.get_request();
    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.target, "/api/data");
    EXPECT_EQ(request.headers.at("content-length"), std::to_string(body.size()));
    EXPECT_EQ(request.body, body);
    EXPECT_EQ(request.content_length(), body.size());
}

TEST(HttpParserTest, ValidDeleteRequest) {
    HttpParser parser;
    std::string req = "DELETE /resource/1 HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::COMPLETE);
    EXPECT_EQ(parser.get_request().method, "DELETE");
}

TEST(HttpParserTest, ValidHeadRequest) {
    HttpParser parser;
    std::string req = "HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::COMPLETE);
    EXPECT_EQ(parser.get_request().method, "HEAD");
}

TEST(HttpParserTest, CaseInsensitiveHeaders) {
    HttpParser parser;
    std::string req = "GET / HTTP/1.1\r\nHoSt: localhost\r\nCONTENT-TYPE: text/plain\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::COMPLETE);
    auto request = parser.get_request();
    EXPECT_TRUE(request.headers.contains("host"));
    EXPECT_TRUE(request.headers.contains("content-type"));
}

TEST(HttpParserTest, MultipleHeaders) {
    HttpParser parser;
    std::string req = "GET / HTTP/1.1\r\nHost: localhost\r\nH1: v1\r\nH2: v2\r\nH3: v3\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::COMPLETE);
    EXPECT_EQ(parser.get_request().headers.size(), static_cast<size_t>(4));
}

TEST(HttpParserTest, ConnectionClose) {
    HttpParser parser;
    std::string req = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::COMPLETE);
    EXPECT_FALSE(parser.get_request().should_keep_alive());
}

TEST(HttpParserTest, InvalidMethod) {
    HttpParser parser;
    std::string req = "PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::MethodNotAllowed);
}

TEST(HttpParserTest, MissingHttpVersion) {
    HttpParser parser;
    std::string req = "GET /\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::BadRequest);
}

TEST(HttpParserTest, WrongHttpVersion) {
    HttpParser parser;
    std::string req = "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::BadRequest);
}

TEST(HttpParserTest, TargetNotStartingWithSlash) {
    HttpParser parser;
    std::string req = "GET index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::BadRequest);
}

TEST(HttpParserTest, HeaderWithoutColon) {
    HttpParser parser;
    std::string req = "GET / HTTP/1.1\r\nHost localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::BadRequest);
}

TEST(HttpParserTest, SpaceInHeaderName) {
    HttpParser parser;
    std::string req = "GET / HTTP/1.1\r\nInvalid Header: value\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::BadRequest);
}

TEST(HttpParserTest, RequestLineTooLong) {
    HttpParser parser;
    std::string target(9000, 'a');
    std::string req = "GET /" + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::URITooLong);
}

TEST(HttpParserTest, SingleHeaderTooLong) {
    HttpParser parser;
    std::string value(9000, 'a');
    std::string req = "GET / HTTP/1.1\r\nHost: localhost\r\nLong: " + value + "\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::HeaderFieldsTooLarge);
}

TEST(HttpParserTest, TooManyHeaders) {
    HttpParser parser;
    std::string req = "GET / HTTP/1.1\r\n";
    for (int i = 0; i < 101; ++i) {
        req += "H" + std::to_string(i) + ": v\r\n";
    }
    req += "\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::HeaderFieldsTooLarge);
}

TEST(HttpParserTest, BodyTooLarge) {
    HttpParser parser;
    std::string req = "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 2000000\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::PayloadTooLarge);
}

TEST(HttpParserTest, PostWithoutContentLength) {
    HttpParser parser;
    std::string req = "POST / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req), ParseResult::ERROR);
    EXPECT_EQ(parser.error_code(), StatusCode::BadRequest);
}

TEST(HttpParserTest, IncrementalParsing) {
    HttpParser parser;
    EXPECT_EQ(parser.feed("GET / HT"), ParseResult::INCOMPLETE);
    EXPECT_EQ(parser.feed("TP/1.1\r\nHost: "), ParseResult::INCOMPLETE);
    EXPECT_EQ(parser.feed("localhost\r\n\r\n"), ParseResult::COMPLETE);
    EXPECT_EQ(parser.get_request().target, "/");
}

TEST(HttpParserTest, KeepAliveReset) {
    HttpParser parser;
    std::string req1 = "GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req1), ParseResult::COMPLETE);
    EXPECT_EQ(parser.get_request().target, "/1");

    parser.reset();

    std::string req2 = "GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.feed(req2), ParseResult::COMPLETE);
    EXPECT_EQ(parser.get_request().target, "/2");
}

TEST(HttpParserTest, EmptyRequest) {
    HttpParser parser;
    EXPECT_EQ(parser.feed(""), ParseResult::INCOMPLETE);
}

TEST(HttpParserTest, PartialRequestLine) {
    HttpParser parser;
    EXPECT_EQ(parser.feed("GET / HTTP/1"), ParseResult::INCOMPLETE);
}

TEST(HttpParserTest, PartialHeaders) {
    HttpParser parser;
    EXPECT_EQ(parser.feed("GET / HTTP/1.1\r\nHost:"), ParseResult::INCOMPLETE);
}
