#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/binary_file/binary_file.h"
#include <cstdio>
#include <cstring>

TEST_CASE("TWriteBinaryFile + TReadBinaryFile roundtrip") {
    const char *path = "/tmp/test_binary_file.bin";
    {
        TWriteBinaryFile wf;
        REQUIRE(wf.open(path));
        wf.writeByte(0x42);
        wf.writeByte(0xFF);
        wf.close();
    }
    {
        TReadBinaryFile rf;
        REQUIRE(rf.open(path));
        CHECK(rf.readByte() == 0x42);
        CHECK(rf.readByte() == 0xFF);
        CHECK(rf.eof());
        rf.close();
    }
    remove(path);
}

TEST_CASE("TReadBinaryFile: open nonexistent file fails") {
    TReadBinaryFile rf;
    CHECK_FALSE(rf.open("/tmp/nonexistent_test_file_xyz.bin"));
}
