#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "creature/race/race.h"

TEST_CASE("TSkillData struct layout") {
    TSkillData s{};
    CHECK(s.Nr == 0);
    CHECK(s.Actual == 0);
    CHECK(s.Minimum == 0);
    CHECK(s.Maximum == 0);
}

TEST_CASE("TItemData struct layout") {
    TItemData i{};
    CHECK(i.Maximum == 0);
    CHECK(i.Probability == 0);
}

TEST_CASE("TSpellData struct layout") {
    TSpellData s{};
    CHECK(s.ShapeParam1 == 0);
    CHECK(s.ImpactParam1 == 0);
    CHECK(s.Delay == 0);
}
