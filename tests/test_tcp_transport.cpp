// tests/test_tcp_transport.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "network/transport/tcp_transport/tcp_transport.h"

#include <sys/socket.h>
#include <cstring>

TEST_CASE("TcpTransport: socketpair roundtrip") {
    int fds[2];
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    int saved_fd0 = fds[0];  // Save fd before transport.close() discards it

    TcpTransport transport(fds[0], "127.0.0.1");

    SUBCASE("write and read roundtrip") {
        const uint8 data[] = {0xDE, 0xAD, 0xBE, 0xEF};
        int written = transport.write(data, 4);
        CHECK(written == 4);

        uint8 buf[4] = {};
        // Read from the other end of the socketpair
        int n = ::read(fds[1], buf, 4);
        CHECK(n == 4);
        CHECK(memcmp(data, buf, 4) == 0);
    }

    SUBCASE("read receives data written to other end") {
        const uint8 data[] = {0x01, 0x02, 0x03};
        ::write(fds[1], data, 3);

        uint8 buf[3] = {};
        int n = transport.read(buf, 3);
        CHECK(n == 3);
        CHECK(memcmp(data, buf, 3) == 0);
    }

    SUBCASE("get_fd returns socket fd") {
        CHECK(transport.get_fd() == fds[0]);
    }

    SUBCASE("get_remote_address returns address") {
        CHECK(strcmp(transport.get_remote_address(), "127.0.0.1") == 0);
    }

    SUBCASE("is_connected initially true") {
        CHECK(transport.is_connected() == true);
    }

    SUBCASE("close transitions to disconnected") {
        transport.close();
        CHECK(transport.is_connected() == false);
        CHECK(transport.get_fd() == -1);
    }

    // Cleanup — close both fds. saved_fd0 ensures we can close even
    // after transport.close() discards the fd value.
    ::close(saved_fd0);
    ::close(fds[1]);
}
