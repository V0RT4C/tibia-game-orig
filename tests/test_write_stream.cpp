#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/write_stream/write_stream.h"
#include <cstring>

TEST_CASE("TWriteBuffer: writeByte") {
    uint8 data[4] = {};
    TWriteBuffer buf(data, 4);
    buf.writeByte(0x42);
    CHECK(data[0] == 0x42);
    CHECK(buf.Position == 1);
}

TEST_CASE("TWriteBuffer: writeWord little-endian") {
    uint8 data[4] = {};
    TWriteBuffer buf(data, 4);
    buf.writeWord(0x1234);
    CHECK(data[0] == 0x34);
    CHECK(data[1] == 0x12);
}

TEST_CASE("TWriteBuffer: writeQuad little-endian") {
    uint8 data[4] = {};
    TWriteBuffer buf(data, 4);
    buf.writeQuad(0x12345678);
    CHECK(data[0] == 0x78);
    CHECK(data[1] == 0x56);
    CHECK(data[2] == 0x34);
    CHECK(data[3] == 0x12);
}

TEST_CASE("TWriteBuffer: throws on overflow") {
    uint8 data[1] = {};
    TWriteBuffer buf(data, 1);
    buf.writeByte(0xFF);
    CHECK_THROWS(buf.writeByte(0x01));
}

TEST_CASE("TWriteStream: writeFlag") {
    uint8 data[2] = {};
    TWriteBuffer buf(data, 2);
    buf.writeFlag(true);
    buf.writeFlag(false);
    CHECK(data[0] == 1);
    CHECK(data[1] == 0);
}
