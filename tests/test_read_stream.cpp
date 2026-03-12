#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/read_stream/read_stream.h"
#include <cstring>

TEST_CASE("TReadBuffer: readByte") {
    uint8 data[] = {0x42};
    TReadBuffer buf(data, 1);
    CHECK(buf.readByte() == 0x42);
    CHECK(buf.eof());
}

TEST_CASE("TReadBuffer: readWord little-endian") {
    uint8 data[] = {0x34, 0x12};
    TReadBuffer buf(data, 2);
    CHECK(buf.readWord() == 0x1234);
}

TEST_CASE("TReadBuffer: readQuad little-endian") {
    uint8 data[] = {0x78, 0x56, 0x34, 0x12};
    TReadBuffer buf(data, 4);
    CHECK(buf.readQuad() == 0x12345678);
}

TEST_CASE("TReadBuffer: readString") {
    uint8 data[] = {0x05, 0x00, 'h', 'e', 'l', 'l', 'o'};
    TReadBuffer buf(data, 7);
    char str[32];
    buf.readString(str, sizeof(str));
    CHECK(strcmp(str, "hello") == 0);
}

TEST_CASE("TReadBuffer: throws on overread") {
    uint8 data[] = {0x01};
    TReadBuffer buf(data, 1);
    buf.readByte();
    CHECK_THROWS(buf.readByte());
}

TEST_CASE("TReadBuffer: skip") {
    uint8 data[] = {1, 2, 3, 4, 5};
    TReadBuffer buf(data, 5);
    buf.skip(3);
    CHECK(buf.readByte() == 4);
}

TEST_CASE("TReadBuffer: readFlag") {
    uint8 data[] = {0, 1};
    TReadBuffer buf(data, 2);
    CHECK_FALSE(buf.readFlag());
    CHECK(buf.readFlag());
}
