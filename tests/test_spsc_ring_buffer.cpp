#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/spsc_ring_buffer/spsc_ring_buffer.h"

#include <cstring>
#include <thread>

TEST_CASE("SPSCRingBuffer: basic write and read") {
    SPSCRingBuffer<1024> buf;

    SUBCASE("empty buffer returns 0 on read") {
        uint8_t out[64];
        CHECK(buf.read(out, 64) == 0);
    }

    SUBCASE("write then read returns same data") {
        const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
        CHECK(buf.write(data, 4) == true);
        uint8_t out[4] = {};
        CHECK(buf.read(out, 4) == 4);
        CHECK(memcmp(data, out, 4) == 0);
    }

    SUBCASE("partial read drains available data") {
        const uint8_t data[] = {1, 2, 3, 4, 5};
        buf.write(data, 5);
        uint8_t out[3] = {};
        CHECK(buf.read(out, 3) == 3);
        CHECK(out[0] == 1);
        CHECK(out[1] == 2);
        CHECK(out[2] == 3);
        // remaining 2 bytes still available
        uint8_t out2[2] = {};
        CHECK(buf.read(out2, 2) == 2);
        CHECK(out2[0] == 4);
        CHECK(out2[1] == 5);
    }

    SUBCASE("available_bytes tracks correctly") {
        CHECK(buf.available_bytes() == 0);
        const uint8_t data[] = {1, 2, 3};
        buf.write(data, 3);
        CHECK(buf.available_bytes() == 3);
        uint8_t out[2];
        buf.read(out, 2);
        CHECK(buf.available_bytes() == 1);
    }
}

TEST_CASE("SPSCRingBuffer: write rejects when full") {
    SPSCRingBuffer<16> buf;
    uint8_t data[16];
    memset(data, 0xAA, sizeof(data));

    // Fill buffer (capacity is Size-1 = 15 usable bytes)
    CHECK(buf.write(data, 15) == true);
    // One more byte should fail
    uint8_t extra = 0xFF;
    CHECK(buf.write(&extra, 1) == false);
}

TEST_CASE("SPSCRingBuffer: wrap-around write and read") {
    SPSCRingBuffer<16> buf;
    // Write 10 bytes, read 10 bytes (moves read pointer forward)
    uint8_t fill[10];
    memset(fill, 0xAA, sizeof(fill));
    buf.write(fill, 10);
    uint8_t discard[10];
    buf.read(discard, 10);

    // Now write 12 bytes — wraps around the 16-byte buffer
    uint8_t wrap_data[12];
    for (int i = 0; i < 12; i++) wrap_data[i] = (uint8_t)i;
    CHECK(buf.write(wrap_data, 12) == true);

    uint8_t out[12] = {};
    CHECK(buf.read(out, 12) == 12);
    CHECK(memcmp(wrap_data, out, 12) == 0);
}

TEST_CASE("SPSCRingBuffer: concurrent producer-consumer") {
    SPSCRingBuffer<4096> buf;
    constexpr int TOTAL = 100000;

    std::thread producer([&]() {
        for (int i = 0; i < TOTAL; i++) {
            uint8_t val = (uint8_t)(i & 0xFF);
            while (!buf.write(&val, 1)) {
                // spin until space available
            }
        }
    });

    int received = 0;
    int errors = 0;
    while (received < TOTAL) {
        uint8_t val;
        int n = buf.read(&val, 1);
        if (n == 1) {
            if (val != (uint8_t)(received & 0xFF)) {
                errors++;
            }
            received++;
        }
    }

    producer.join();
    CHECK(received == TOTAL);
    CHECK(errors == 0);
}
