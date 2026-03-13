#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "game_data/map/map.h"

// Characterization tests for Object struct and map enums/constants.
// These test inline methods and compile-time values only — no global state.

TEST_CASE("Object default constructor") {
    Object obj;
    CHECK(obj.ObjectID == 0);
}

TEST_CASE("Object explicit constructor") {
    Object obj(Object(42));
    CHECK(obj.ObjectID == 42);
}

TEST_CASE("Object equality operators") {
    Object a(Object(10));
    Object b(Object(10));
    Object c(Object(20));
    CHECK(a == b);
    CHECK(a != c);
    CHECK_FALSE(a != b);
    CHECK_FALSE(a == c);
}

TEST_CASE("NONE sentinel is ObjectID 0") {
    CHECK(NONE.ObjectID == 0);
    CHECK(NONE == Object());
}

TEST_CASE("Status enum values") {
    CHECK(STATUS_FREE == 0);
    CHECK(STATUS_LOADED == 1);
    CHECK(STATUS_SWAPPED == 2);
    CHECK(STATUS_PERMANENT == 255);
}

TEST_CASE("Priority enum values") {
    CHECK(PRIORITY_BANK == 0);
    CHECK(PRIORITY_CLIP == 1);
    CHECK(PRIORITY_BOTTOM == 2);
    CHECK(PRIORITY_TOP == 3);
    CHECK(PRIORITY_CREATURE == 4);
    CHECK(PRIORITY_LOW == 5);
    // Bank < Clip < Bottom < Top < Creature < Low
    CHECK(PRIORITY_BANK < PRIORITY_CLIP);
    CHECK(PRIORITY_CLIP < PRIORITY_BOTTOM);
    CHECK(PRIORITY_BOTTOM < PRIORITY_TOP);
    CHECK(PRIORITY_TOP < PRIORITY_CREATURE);
    CHECK(PRIORITY_CREATURE < PRIORITY_LOW);
}

TEST_CASE("TSector struct layout") {
    TSector s{};
    CHECK(s.Status == STATUS_FREE);
    CHECK(s.TimeStamp == 0);
    CHECK(s.MapFlags == 0);
    // MapCon is 32x32 grid of Object
    CHECK(s.MapCon[0][0] == NONE);
    CHECK(s.MapCon[31][31] == NONE);
}

TEST_CASE("TObject struct layout") {
    TObject obj{};
    CHECK(obj.ObjectID == 0);
    CHECK(obj.NextObject == NONE);
    CHECK(obj.Container == NONE);
    CHECK(obj.Type.TypeID == 0);
}

TEST_CASE("TDepotInfo struct layout") {
    TDepotInfo d{};
    CHECK(d.Size == 0);
}

TEST_CASE("TMark struct layout") {
    TMark m{};
    CHECK(m.x == 0);
    CHECK(m.y == 0);
    CHECK(m.z == 0);
}

TEST_CASE("TCronEntry struct layout") {
    TCronEntry c{};
    CHECK(c.Obj == NONE);
    CHECK(c.RoundNr == 0);
}
