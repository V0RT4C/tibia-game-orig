#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "network/connection/connection.h"

#include <cstring>

// Helper: create a zero-initialized TConnection without calling the constructor.
// This avoids linking connection.cpp which has heavy transitive deps.
static TConnection *make_zeroed_connection() {
    static uint8 storage[sizeof(TConnection)];
    memset(storage, 0, sizeof(storage));
    return reinterpret_cast<TConnection*>(storage);
}

TEST_CASE("TConnection: struct layout — compile-time sizes") {
    // Buffer sizes via offsetof/sizeof on member types
    CHECK(sizeof(TConnection::InData) == 2048);
    CHECK(sizeof(TConnection::OutData) == 16384);
    CHECK(sizeof(TConnection::IPAddress) == 16);
    CHECK(sizeof(TConnection::Name) == 31);
    CHECK(sizeof(TConnection::KnownCreatureTable) / sizeof(TKnownCreature) == 150);
}

TEST_CASE("TConnection: InGame inline method") {
    TConnection *conn = make_zeroed_connection();
    conn->State = CONNECTION_FREE;
    CHECK(conn->in_game() == false);
    conn->State = CONNECTION_GAME;
    CHECK(conn->in_game() == true);
    conn->State = CONNECTION_DEAD;
    CHECK(conn->in_game() == true);
    conn->State = CONNECTION_LOGOUT;
    CHECK(conn->in_game() == false);
}

TEST_CASE("TConnection: Live inline method") {
    TConnection *conn = make_zeroed_connection();
    conn->State = CONNECTION_FREE;
    CHECK(conn->live() == false);
    conn->State = CONNECTION_ASSIGNED;
    CHECK(conn->live() == false);
    conn->State = CONNECTION_CONNECTED;
    CHECK(conn->live() == false);
    conn->State = CONNECTION_LOGIN;
    CHECK(conn->live() == true);
    conn->State = CONNECTION_GAME;
    CHECK(conn->live() == true);
    conn->State = CONNECTION_DEAD;
    CHECK(conn->live() == true);
    conn->State = CONNECTION_LOGOUT;
    CHECK(conn->live() == true);
    conn->State = CONNECTION_DISCONNECTED;
    CHECK(conn->live() == false);
}

TEST_CASE("TKnownCreature: struct layout") {
    CHECK(sizeof(TKnownCreature) > 0);
    TKnownCreature kc;
    memset(&kc, 0, sizeof(kc));
    kc.State = KNOWNCREATURE_FREE;
    kc.CreatureID = 0;
    kc.Next = nullptr;
    kc.Connection = nullptr;
    CHECK(kc.State == KNOWNCREATURE_FREE);
}
