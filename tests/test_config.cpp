#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "config/config.h"
#include <cstring>

TEST_CASE("DatabaseSettings struct has correct field sizes") {
    DatabaseSettings ds;
    CHECK(sizeof(ds.Product) == 30);
    CHECK(sizeof(ds.Database) == 30);
    CHECK(sizeof(ds.Login) == 30);
    CHECK(sizeof(ds.Password) == 30);
    CHECK(sizeof(ds.Host) == 30);
    CHECK(sizeof(ds.Port) == 6);
}

TEST_CASE("QueryManagerSettings struct has correct field sizes") {
    QueryManagerSettings qs;
    CHECK(sizeof(qs.Host) == 50);
    CHECK(sizeof(qs.Password) == 30);
}

TEST_CASE("Config path buffers are 4096 bytes") {
    CHECK(sizeof(BINPATH) == 4096);
    CHECK(sizeof(DATAPATH) == 4096);
    CHECK(sizeof(MAPPATH) == 4096);
}
