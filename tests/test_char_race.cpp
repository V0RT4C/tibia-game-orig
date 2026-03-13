#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "creature/race/race.h"

TEST_CASE("SkillData struct layout") {
    SkillData s{};
    CHECK(s.Nr == 0);
    CHECK(s.Actual == 0);
    CHECK(s.Minimum == 0);
    CHECK(s.Maximum == 0);
}

TEST_CASE("ItemData struct layout") {
    ItemData i{};
    CHECK(i.Maximum == 0);
    CHECK(i.Probability == 0);
}

TEST_CASE("SpellData struct layout") {
    SpellData s{};
    CHECK(s.ShapeParam1 == 0);
    CHECK(s.ImpactParam1 == 0);
    CHECK(s.Delay == 0);
}
