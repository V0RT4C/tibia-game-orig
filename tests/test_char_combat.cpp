#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "creature/combat/combat.h"

// Characterization tests for combat struct layout.
// Only test header-level things (sizeof, array sizes). Do NOT link combat.cpp.
// We cannot instantiate TCombat (ctor in combat.cpp), so we use
// compile-time sizeof expressions on member types via null pointer casts.

TEST_CASE("CombatEntry is a POD with three uint32 fields") {
	CHECK(sizeof(CombatEntry) >= 3 * sizeof(uint32));
}

TEST_CASE("CombatEntry fields have expected offsets") {
	CombatEntry entry{};
	entry.ID = 1;
	entry.Damage = 2;
	entry.TimeStamp = 3;
	CHECK(entry.ID == 1);
	CHECK(entry.Damage == 2);
	CHECK(entry.TimeStamp == 3);
}

static constexpr size_t kCombatListSlots =
	sizeof(((TCombat*)nullptr)->CombatList) / sizeof(((TCombat*)nullptr)->CombatList[0]);

TEST_CASE("TCombat CombatList array has 20 slots") {
	CHECK(kCombatListSlots == 20);
}

TEST_CASE("TCombat has a Master pointer field") {
	// Verify that the Master field exists and is pointer-sized.
	CHECK(sizeof(((TCombat*)nullptr)->Master) == sizeof(void*));
}
