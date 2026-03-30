// gameserver/tests/test_npc_action.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "creature/npc/npc_action.h"

#include <cstring>

TEST_CASE("FindActionByName returns correct action for deletemoney") {
    NpcActionDef *def = FindActionByName("deletemoney");
    REQUIRE(def != nullptr);
    CHECK(def->Type == NPC_ACTION_DELETE_MONEY);
    CHECK(def->ParamCount == 0);
    CHECK(def->Execute != nullptr);
}

TEST_CASE("FindActionByName returns nullptr for unknown action") {
    NpcActionDef *def = FindActionByName("nonexistent");
    CHECK(def == nullptr);
}

TEST_CASE("FindActionByType returns correct action for teleport") {
    NpcActionDef *def = FindActionByType(NPC_ACTION_TELEPORT);
    REQUIRE(def != nullptr);
    CHECK(strcmp(def->Name, "teleport") == 0);
    CHECK(def->ParamCount == 3);
    CHECK(def->Execute != nullptr);
}

TEST_CASE("FindActionByType returns nullptr for unknown type") {
    NpcActionDef *def = FindActionByType((NpcActionType)999);
    CHECK(def == nullptr);
}

TEST_CASE("All registry entries round-trip through both lookup functions") {
    const char *names[] = {
        "topic", "price", "amount", "type", "data",
        "hp", "poison", "burning",
        "effectme", "effectopp",
        "profession", "teachspell", "summon", "create", "delete",
        "createmoney", "deletemoney", "queue",
        "setquestvalue",
        "teleport", "startposition",
    };

    for (const char *name : names) {
        INFO("action: ", name);
        NpcActionDef *byName = FindActionByName(name);
        REQUIRE(byName != nullptr);
        CHECK(byName->Execute != nullptr);
        CHECK(byName->ParamCount >= -1);
        CHECK(byName->ParamCount <= 3);

        NpcActionDef *byType = FindActionByType(byName->Type);
        REQUIRE(byType != nullptr);
        CHECK(byType == byName);
    }
}

TEST_CASE("ParamCount values match expected parameter counts") {
    // 0-param actions
    CHECK(FindActionByName("createmoney")->ParamCount == 0);
    CHECK(FindActionByName("deletemoney")->ParamCount == 0);
    CHECK(FindActionByName("queue")->ParamCount == 0);

    // 1-param actions
    CHECK(FindActionByName("effectme")->ParamCount == 1);
    CHECK(FindActionByName("effectopp")->ParamCount == 1);
    CHECK(FindActionByName("profession")->ParamCount == 1);
    CHECK(FindActionByName("teachspell")->ParamCount == 1);
    CHECK(FindActionByName("summon")->ParamCount == 1);
    CHECK(FindActionByName("create")->ParamCount == 1);
    CHECK(FindActionByName("delete")->ParamCount == 1);

    // 2-param actions
    CHECK(FindActionByName("setquestvalue")->ParamCount == 2);
    CHECK(FindActionByName("poison")->ParamCount == 2);
    CHECK(FindActionByName("burning")->ParamCount == 2);

    // 3-param actions
    CHECK(FindActionByName("teleport")->ParamCount == 3);
    CHECK(FindActionByName("startposition")->ParamCount == 3);

    // "= expr" actions
    CHECK(FindActionByName("topic")->ParamCount == -1);
    CHECK(FindActionByName("price")->ParamCount == -1);
    CHECK(FindActionByName("amount")->ParamCount == -1);
    CHECK(FindActionByName("type")->ParamCount == -1);
    CHECK(FindActionByName("data")->ParamCount == -1);
    CHECK(FindActionByName("hp")->ParamCount == -1);
}
