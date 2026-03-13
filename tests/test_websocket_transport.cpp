#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "network/transport/websocket_transport/websocket_transport.h"

#include <sys/eventfd.h>
#include <cstring>
#include <cerrno>

// These tests exercise the transport's ring buffers and eventfd without
// a real uWebSockets event loop. We pass nullptr for both ws and event_loop
// since we won't call write() (which defers to the event loop).

TEST_CASE("WebSocketTransport: eventfd creation and get_fd") {
    WebSocketTransport transport(nullptr, nullptr, "192.168.1.1");
    CHECK(transport.get_fd() >= 0);
    CHECK(transport.is_connected() == true);
}

TEST_CASE("WebSocketTransport: get_remote_address") {
    WebSocketTransport transport(nullptr, nullptr, "10.0.0.1");
    CHECK(strcmp(transport.get_remote_address(), "10.0.0.1") == 0);
}

TEST_CASE("WebSocketTransport: push_inbound and read roundtrip") {
    WebSocketTransport transport(nullptr, nullptr, "127.0.0.1");

    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02};
    CHECK(transport.push_inbound(data, 6) == true);

    uint8_t out[6] = {};
    int n = transport.read(out, 6);
    CHECK(n == 6);
    CHECK(memcmp(data, out, 6) == 0);
}

TEST_CASE("WebSocketTransport: read returns -1 with EAGAIN when empty") {
    WebSocketTransport transport(nullptr, nullptr, "127.0.0.1");

    uint8_t out[4];
    int n = transport.read(out, 4);
    CHECK(n == -1);
    CHECK(errno == EAGAIN);
}

TEST_CASE("WebSocketTransport: push_inbound signals eventfd") {
    WebSocketTransport transport(nullptr, nullptr, "127.0.0.1");
    int efd = transport.get_fd();

    const uint8_t data[] = {0x42};
    transport.push_inbound(data, 1);

    // eventfd should be readable (non-blocking)
    uint64_t val = 0;
    int r = ::read(efd, &val, sizeof(val));
    CHECK(r == 8);
    CHECK(val > 0);
}

TEST_CASE("WebSocketTransport: close marks disconnected") {
    WebSocketTransport transport(nullptr, nullptr, "127.0.0.1");
    CHECK(transport.is_connected() == true);
    transport.close();
    CHECK(transport.is_connected() == false);
    // read should fail after close
    uint8_t out[4];
    CHECK(transport.read(out, 4) == -1);
}

TEST_CASE("WebSocketTransport: push_inbound fails when buffer full") {
    WebSocketTransport transport(nullptr, nullptr, "127.0.0.1");

    // Fill the 65536-byte ring buffer (usable capacity = 65535)
    uint8_t fill[8192];
    memset(fill, 0xAA, sizeof(fill));
    for (int i = 0; i < 7; i++) {
        CHECK(transport.push_inbound(fill, 8192) == true);
    }
    // 7 * 8192 = 57344, need 65535 - 57344 = 8191 more
    CHECK(transport.push_inbound(fill, 8191) == true);

    // Buffer is now full (65535 bytes used)
    uint8_t extra = 0xFF;
    CHECK(transport.push_inbound(&extra, 1) == false);
}

TEST_CASE("WebSocketTransport: set_thread_id and get_thread_id") {
    WebSocketTransport transport(nullptr, nullptr, "127.0.0.1");
    CHECK(transport.get_thread_id() == 0);
    transport.set_thread_id(12345);
    CHECK(transport.get_thread_id() == 12345);
}
