#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/byte_order/byte_order.h"

TEST_CASE("read_u16_le reads little-endian uint16") {
    uint8 buf[] = {0x34, 0x12};
    CHECK(read_u16_le(buf) == 0x1234);
}

TEST_CASE("read_u32_le reads little-endian uint32") {
    uint8 buf[] = {0x78, 0x56, 0x34, 0x12};
    CHECK(read_u32_le(buf) == 0x12345678);
}

TEST_CASE("write_u16_le writes little-endian uint16") {
    uint8 buf[2] = {};
    write_u16_le(buf, 0xABCD);
    CHECK(buf[0] == 0xCD);
    CHECK(buf[1] == 0xAB);
}

TEST_CASE("write_u32_le writes little-endian uint32") {
    uint8 buf[4] = {};
    write_u32_le(buf, 0xDEADBEEF);
    CHECK(buf[0] == 0xEF);
    CHECK(buf[1] == 0xBE);
    CHECK(buf[2] == 0xAD);
    CHECK(buf[3] == 0xDE);
}

TEST_CASE("roundtrip u16") {
    uint8 buf[2];
    write_u16_le(buf, 0);
    CHECK(read_u16_le(buf) == 0);
    write_u16_le(buf, 0xFFFF);
    CHECK(read_u16_le(buf) == 0xFFFF);
    write_u16_le(buf, 1);
    CHECK(read_u16_le(buf) == 1);
}

TEST_CASE("roundtrip u32") {
    uint8 buf[4];
    write_u32_le(buf, 0);
    CHECK(read_u32_le(buf) == 0);
    write_u32_le(buf, 0xFFFFFFFF);
    CHECK(read_u32_le(buf) == 0xFFFFFFFF);
    write_u32_le(buf, 1);
    CHECK(read_u32_le(buf) == 1);
}
