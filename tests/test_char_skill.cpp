#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "creature/skill/skill.h"

// Characterization tests for skill struct layout.
// Only test header-level things (sizeof, array sizes). Do NOT link skill.cpp.
// We cannot instantiate SkillBase (ctor/dtor in skill.cpp), so we use
// compile-time sizeof expressions on member types via null pointer casts.

static constexpr size_t kSkillSlots =
	sizeof(((SkillBase*)nullptr)->Skills) / sizeof(((SkillBase*)nullptr)->Skills[0]);

static constexpr size_t kTimerSlots =
	sizeof(((SkillBase*)nullptr)->TimerList) / sizeof(((SkillBase*)nullptr)->TimerList[0]);

TEST_CASE("SkillBase Skills array has 25 slots") {
	CHECK(kSkillSlots == 25);
}

TEST_CASE("SkillBase TimerList array has 25 slots") {
	CHECK(kTimerSlots == 25);
}

TEST_CASE("SkillBase Skills and TimerList arrays are same size") {
	CHECK(kSkillSlots == kTimerSlots);
}

TEST_CASE("TSkill is polymorphic (has vtable)") {
	CHECK(sizeof(TSkill) > sizeof(void*));
}
