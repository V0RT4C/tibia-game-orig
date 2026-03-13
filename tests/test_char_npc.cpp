#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "creature/npc/npc.h"

TEST_CASE("BehaviourNode struct layout") {
	BehaviourNode n{};
	CHECK(n.Type == 0);
	CHECK(n.Data == 0);
	CHECK(n.Left == nullptr);
	CHECK(n.Right == nullptr);
}
