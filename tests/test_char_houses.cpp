#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "game_data/houses/houses.h"

// Characterization tests for houses structs — layout and default values.

TEST_CASE("THelpDepot struct layout") {
    THelpDepot d{};
    CHECK(d.CharacterID == 0);
    CHECK(d.Box == NONE);
    CHECK(d.DepotNr == 0);
}

TEST_CASE("THouseArea struct layout") {
    THouseArea a{};
    CHECK(a.ID == 0);
    CHECK(a.SQMPrice == 0);
    CHECK(a.DepotNr == 0);
}

TEST_CASE("THouseGuest name capacity") {
    THouseGuest g{};
    CHECK(sizeof(g.Name) == MAX_HOUSE_GUEST_NAME);
    CHECK(MAX_HOUSE_GUEST_NAME == 60);
}

// THouse constructor and copy tests omitted — THouse has non-inline
// constructors in houses.cpp which pulls in heavy game-wide dependencies.
// These will be testable after Phase 3 decomposes cr.h.
