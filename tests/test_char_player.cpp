#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "creature/player/player.h"

TEST_CASE("TPlayerData array sizes") {
	CHECK(sizeof(TPlayerData::Actual) / sizeof(int) == 25);
	CHECK(sizeof(TPlayerData::Maximum) / sizeof(int) == 25);
	CHECK(sizeof(TPlayerData::Minimum) / sizeof(int) == 25);
	CHECK(sizeof(TPlayerData::SpellList) == 256);
	CHECK(sizeof(TPlayerData::QuestValues) / sizeof(int) == 500);
	CHECK(sizeof(TPlayerData::Buddy) / sizeof(uint32) == 100);
}

TEST_CASE("PlayerIndexEntry name capacity") {
	PlayerIndexEntry e{};
	CHECK(sizeof(e.Name) == 30);
}

TEST_CASE("PlayerIndexLeafNode entry count") {
	CHECK(sizeof(PlayerIndexLeafNode::Entry) / sizeof(PlayerIndexEntry) == 10);
}

TEST_CASE("PlayerIndexInternalNode child count") {
	CHECK(sizeof(PlayerIndexInternalNode::Child) / sizeof(PlayerIndexNode *) == 37);
}
