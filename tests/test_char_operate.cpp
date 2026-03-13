#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "game_data/operate/operate.h"

// Characterization tests for operate structs and enums.

TEST_CASE("Creature change type enums") {
    CHECK(CREATURE_HEALTH_CHANGED == 1);
    CHECK(CREATURE_LIGHT_CHANGED == 2);
    CHECK(CREATURE_OUTFIT_CHANGED == 3);
    CHECK(CREATURE_SPEED_CHANGED == 4);
    CHECK(CREATURE_SKULL_CHANGED == 5);
    CHECK(CREATURE_PARTY_CHANGED == 6);
}

TEST_CASE("Object event type enums") {
    CHECK(OBJECT_DELETED == 0);
    CHECK(OBJECT_CREATED == 1);
    CHECK(OBJECT_CHANGED == 2);
    CHECK(OBJECT_MOVED == 3);
}

TEST_CASE("Channel ID enums") {
    CHECK(CHANNEL_GUILD == 0);
    CHECK(CHANNEL_GAMEMASTER == 1);
    CHECK(CHANNEL_TUTOR == 2);
    CHECK(CHANNEL_RULEVIOLATIONS == 3);
    CHECK(CHANNEL_GAMECHAT == 4);
    CHECK(CHANNEL_TRADE == 5);
    CHECK(CHANNEL_RLCHAT == 6);
    CHECK(CHANNEL_HELP == 7);
    CHECK(PUBLIC_CHANNELS == 8);
    CHECK(FIRST_PRIVATE_CHANNEL == PUBLIC_CHANNELS);
    CHECK(MAX_CHANNELS == 0xFFFF);
}

TEST_CASE("Statement struct layout") {
    Statement s{};
    CHECK(s.StatementID == 0);
    CHECK(s.TimeStamp == 0);
    CHECK(s.CharacterID == 0);
    CHECK(s.Mode == 0);
    CHECK(s.Channel == 0);
    CHECK(s.Text == 0);
    CHECK(s.Reported == false);
}

TEST_CASE("Listener struct layout") {
    Listener l{};
    CHECK(l.StatementID == 0);
    CHECK(l.CharacterID == 0);
}

TEST_CASE("ReportedStatement struct layout") {
    ReportedStatement r{};
    CHECK(r.StatementID == 0);
    CHECK(r.TimeStamp == 0);
    CHECK(r.CharacterID == 0);
    CHECK(sizeof(r.Text) == 256);
}

// Channel and Party constructor/copy tests omitted — Channel and Party
// have non-inline constructors in operate.cpp which pulls in heavy game-wide
// dependencies. These will be testable after Phase 3 decomposes cr.h.
