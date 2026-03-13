#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "game_data/moveuse/moveuse.h"

// Characterization tests for moveuse enums and structs.

TEST_CASE("MoveUseActionType enum values") {
    CHECK(MOVEUSE_ACTION_CREATEONMAP == 0);
    CHECK(MOVEUSE_ACTION_CREATE == 1);
    CHECK(MOVEUSE_ACTION_NOP == 37);
}

TEST_CASE("MoveUseConditionType enum values") {
    CHECK(MOVEUSE_CONDITION_ISPOSITION == 0);
    CHECK(MOVEUSE_CONDITION_RANDOM == 25);
}

TEST_CASE("MoveUseEventType enum values") {
    CHECK(MOVEUSE_EVENT_USE == 0);
    CHECK(MOVEUSE_EVENT_MULTIUSE == 1);
    CHECK(MOVEUSE_EVENT_MOVEMENT == 2);
    CHECK(MOVEUSE_EVENT_COLLISION == 3);
    CHECK(MOVEUSE_EVENT_SEPARATION == 4);
}

TEST_CASE("MoveUseModifierType enum values") {
    CHECK(MOVEUSE_MODIFIER_NORMAL == 0);
    CHECK(MOVEUSE_MODIFIER_INVERT == 1);
    CHECK(MOVEUSE_MODIFIER_TRUE == 2);
}

TEST_CASE("MoveUseParameterType enum values") {
    CHECK(MOVEUSE_PARAMETER_OBJECT == 0);
    CHECK(MOVEUSE_PARAMETER_TEXT == 10);
    CHECK(MOVEUSE_PARAMETER_COMPARISON == 11);
}

TEST_CASE("MOVEUSE_MAX_PARAMETERS") {
    CHECK(MOVEUSE_MAX_PARAMETERS == 5);
}

TEST_CASE("TMoveUseAction struct layout") {
    TMoveUseAction a{};
    CHECK(sizeof(a.Parameters) == MOVEUSE_MAX_PARAMETERS * sizeof(int));
}

TEST_CASE("TMoveUseCondition struct layout") {
    TMoveUseCondition c{};
    CHECK(sizeof(c.Parameters) == MOVEUSE_MAX_PARAMETERS * sizeof(int));
}

TEST_CASE("TMoveUseRule struct layout") {
    TMoveUseRule r{};
    CHECK(r.FirstCondition == 0);
    CHECK(r.LastCondition == 0);
    CHECK(r.FirstAction == 0);
    CHECK(r.LastAction == 0);
}

TEST_CASE("TMoveUseDatabase default construction") {
    TMoveUseDatabase db;
    CHECK(db.NumberOfRules == 0);
}

TEST_CASE("TDelayedMail struct layout") {
    TDelayedMail m{};
    CHECK(m.CharacterID == 0);
    CHECK(m.DepotNumber == 0);
    CHECK(m.Packet == nullptr);
    CHECK(m.PacketSize == 0);
}
