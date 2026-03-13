#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "game_data/script/script.h"

TEST_CASE("MAX_IDENT_LENGTH constant") {
    CHECK(MAX_IDENT_LENGTH == 30);
}

TEST_CASE("TReadScriptFile default construction") {
    TReadScriptFile r;
    // Constructor sets RecursionDepth to -1 (closed/no file open)
    CHECK(r.RecursionDepth == -1);
    // Constructor sets Bytes to point at the String buffer
    CHECK(r.Bytes == (uint8*)r.String);
}

TEST_CASE("TReadScriptFile buffer capacities") {
    TReadScriptFile r;
    CHECK(sizeof(r.String) == 4000);
    CHECK(sizeof(r.Filename[0]) == 4096);
    CHECK(sizeof(r.Filename[1]) == 4096);
    CHECK(sizeof(r.Filename[2]) == 4096);
    CHECK(sizeof(r.File) / sizeof(r.File[0]) == 3);
}

TEST_CASE("TWriteScriptFile default construction") {
    TWriteScriptFile w;
    // Constructor sets File to NULL
    CHECK(w.File == nullptr);
    CHECK(sizeof(w.Filename) == 4096);
}
