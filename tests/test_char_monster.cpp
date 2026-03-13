#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "creature/monster/monster.h"

TEST_CASE("Monsterhome struct layout") {
	Monsterhome m{};
	CHECK(m.Race == 0);
	CHECK(m.x == 0);
	CHECK(m.y == 0);
	CHECK(m.z == 0);
	CHECK(m.Radius == 0);
	CHECK(m.MaxMonsters == 0);
	CHECK(m.ActMonsters == 0);
	CHECK(m.RegenerationTime == 0);
	CHECK(m.Timer == 0);
}
