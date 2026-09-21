#include "net/connection.hpp"
#include "net/socket.hpp"
#include <gtest/gtest.h>

using namespace httpserver::net;

TEST(ConnectionTest, BasicConstruction) {
    Socket sock(-1);
    Connection conn(std::move(sock));
    EXPECT_FALSE(conn.is_open());
}

TEST(ConnectionTest, MoveSemantics) {
    Socket sock(-1);
    Connection conn1(std::move(sock));
    Connection conn2(std::move(conn1));
    EXPECT_FALSE(conn2.is_open());
}

TEST(ConnectionTest, WriteAllEmpty) {
    Socket sock(-1);
    Connection conn(std::move(sock));
    // Writing empty data to invalid fd should return true (nothing to write)
    EXPECT_TRUE(conn.write_all(""));
}
