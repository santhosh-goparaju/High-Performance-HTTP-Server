#include "router/router.hpp"
#include "http/request.hpp"
#include "http/response.hpp"
#include "http/status.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

using namespace httpserver;

TEST(RouterTest, ExactPathMatch) {
    router::Router r;
    r.add_route("GET", "/api/data", [](const http::Request&) {
        return http::Response::ok("data");
    });

    http::Request req;
    req.method = "GET";
    req.target = "/api/data";
    req.version = "HTTP/1.1";

    auto res = r.route(req);
    EXPECT_EQ(res.status, http::StatusCode::OK);
    EXPECT_EQ(res.body, "data");
}

TEST(RouterTest, RouteNotFound) {
    router::Router r;
    http::Request req;
    req.method = "GET";
    req.target = "/unknown";
    req.version = "HTTP/1.1";

    auto res = r.route(req);
    EXPECT_EQ(res.status, http::StatusCode::NotFound);
}

TEST(RouterTest, MethodNotAllowed) {
    router::Router r;
    r.add_route("GET", "/api/data", [](const http::Request&) {
        return http::Response::ok("ok");
    });

    http::Request req;
    req.method = "POST";
    req.target = "/api/data";
    req.version = "HTTP/1.1";

    auto res = r.route(req);
    EXPECT_EQ(res.status, http::StatusCode::MethodNotAllowed);
    // Check Allow header is present
    auto it = res.headers.find("Allow");
    ASSERT_NE(it, res.headers.end());
    EXPECT_NE(it->second.find("GET"), std::string::npos);
}

TEST(RouterTest, MultipleMethodsOnSamePath) {
    router::Router r;
    r.add_route("GET", "/api/data", [](const http::Request&) {
        return http::Response::ok("GET");
    });
    r.add_route("POST", "/api/data", [](const http::Request&) {
        return http::Response::ok("POST");
    });

    http::Request req_get;
    req_get.method = "GET";
    req_get.target = "/api/data";
    req_get.version = "HTTP/1.1";
    auto res_get = r.route(req_get);
    EXPECT_EQ(res_get.body, "GET");

    http::Request req_post;
    req_post.method = "POST";
    req_post.target = "/api/data";
    req_post.version = "HTTP/1.1";
    auto res_post = r.route(req_post);
    EXPECT_EQ(res_post.body, "POST");
}

TEST(RouterTest, StaticFileServing) {
    auto temp_dir = std::filesystem::temp_directory_path() / "httpserver_test_static";
    std::filesystem::create_directories(temp_dir);

    std::ofstream out(temp_dir / "index.html");
    out << "<html></html>";
    out.close();

    router::Router r;
    r.add_static("/static", temp_dir.string());

    http::Request req;
    req.method = "GET";
    req.target = "/static/index.html";
    req.version = "HTTP/1.1";

    auto res = r.route(req);
    EXPECT_EQ(res.status, http::StatusCode::OK);
    EXPECT_EQ(res.body, "<html></html>");
    auto ct = res.headers.find("Content-Type");
    ASSERT_NE(ct, res.headers.end());
    EXPECT_EQ(ct->second, "text/html");

    std::filesystem::remove_all(temp_dir);
}

TEST(RouterTest, StaticFileMimeTypes) {
    auto temp_dir = std::filesystem::temp_directory_path() / "httpserver_test_mime";
    std::filesystem::create_directories(temp_dir);

    std::ofstream(temp_dir / "file.txt") << "txt";
    std::ofstream(temp_dir / "data.json") << "{}";

    router::Router r;
    r.add_static("/files", temp_dir.string());

    http::Request req1;
    req1.method = "GET";
    req1.target = "/files/file.txt";
    req1.version = "HTTP/1.1";
    auto res1 = r.route(req1);
    EXPECT_EQ(res1.headers["Content-Type"], "text/plain");

    http::Request req2;
    req2.method = "GET";
    req2.target = "/files/data.json";
    req2.version = "HTTP/1.1";
    auto res2 = r.route(req2);
    EXPECT_EQ(res2.headers["Content-Type"], "application/json");

    std::filesystem::remove_all(temp_dir);
}

TEST(RouterTest, PathTraversalBlocked) {
    auto temp_dir = std::filesystem::temp_directory_path() / "httpserver_test_traversal";
    std::filesystem::create_directories(temp_dir);

    router::Router r;
    r.add_static("/static", temp_dir.string());

    http::Request req;
    req.method = "GET";
    req.target = "/static/../etc/passwd";
    req.version = "HTTP/1.1";

    auto res = r.route(req);
    EXPECT_EQ(res.status, http::StatusCode::Forbidden);

    std::filesystem::remove_all(temp_dir);
}

TEST(RouterTest, NullByteInPathBlocked) {
    auto temp_dir = std::filesystem::temp_directory_path() / "httpserver_test_null";
    std::filesystem::create_directories(temp_dir);

    router::Router r;
    r.add_static("/static", temp_dir.string());

    http::Request req;
    req.method = "GET";
    std::string target = std::string("/static/file") + '\0' + ".txt";
    req.target = target;
    req.version = "HTTP/1.1";

    auto res = r.route(req);
    EXPECT_EQ(res.status, http::StatusCode::Forbidden);

    std::filesystem::remove_all(temp_dir);
}
