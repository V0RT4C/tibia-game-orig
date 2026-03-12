#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/dynamic_buffer/dynamic_buffer.h"

TEST_CASE("TDynamicWriteBuffer: auto-resize on overflow") {
    TDynamicWriteBuffer buf(4);
    for (int i = 0; i < 10; i++) {
        buf.writeByte((uint8)i);
    }
    CHECK(buf.Position == 10);
    CHECK(buf.Size >= 10);
    CHECK(buf.Data[0] == 0);
    CHECK(buf.Data[9] == 9);
}

TEST_CASE("TDynamicWriteBuffer: writeWord triggers resize") {
    TDynamicWriteBuffer buf(1);
    buf.writeWord(0xABCD);
    CHECK(buf.Position == 2);
    CHECK(buf.Data[0] == 0xCD);
    CHECK(buf.Data[1] == 0xAB);
}

TEST_CASE("TDynamicWriteBuffer: writeQuad triggers resize") {
    TDynamicWriteBuffer buf(2);
    buf.writeQuad(0xDEADBEEF);
    CHECK(buf.Position == 4);
}

TEST_CASE("TDynamicWriteBuffer: destructor frees memory") {
    auto *buf = new TDynamicWriteBuffer(16);
    buf->writeByte(1);
    delete buf;
}
