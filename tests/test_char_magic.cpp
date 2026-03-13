#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "game_data/magic/magic.h"

// Characterization tests for magic enums.
// Impact virtual methods are defined in magic.cpp (not inline), so we do not
// instantiate Impact here to avoid requiring magic.cpp linkage.

TEST_CASE("Field type enum values") {
	CHECK(FIELD_TYPE_FIRE == 1);
	CHECK(FIELD_TYPE_POISON == 2);
	CHECK(FIELD_TYPE_ENERGY == 3);
	CHECK(FIELD_TYPE_MAGICWALL == 4);
	CHECK(FIELD_TYPE_WILDGROWTH == 5);
}
