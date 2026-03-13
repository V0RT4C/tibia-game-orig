#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "game_data/objects/objects.h"

TEST_CASE("ObjectType default constructor sets TypeID to 0") {
    ObjectType t;
    CHECK(t.TypeID == 0);
}

TEST_CASE("ObjectType int constructor sets TypeID") {
    ObjectType t(42);
    CHECK(t.TypeID == 42);
}

TEST_CASE("ObjectType equality operators") {
    ObjectType a(10);
    ObjectType b(10);
    ObjectType c(20);
    CHECK(a == b);
    CHECK(a != c);
    CHECK_FALSE(a != b);
    CHECK_FALSE(a == c);
}

TEST_CASE("ObjectType::is_map_container") {
    ObjectType map_con(TYPEID_MAP_CONTAINER);
    ObjectType head(TYPEID_HEAD_CONTAINER);
    CHECK(map_con.is_map_container());
    CHECK_FALSE(head.is_map_container());
}

TEST_CASE("ObjectType::is_body_container") {
    CHECK(ObjectType(TYPEID_HEAD_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_NECK_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_BAG_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_TORSO_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_RIGHTHAND_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_LEFTHAND_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_LEGS_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_FEET_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_FINGER_CONTAINER).is_body_container());
    CHECK(ObjectType(TYPEID_AMMO_CONTAINER).is_body_container());
    CHECK_FALSE(ObjectType(TYPEID_MAP_CONTAINER).is_body_container());
    CHECK_FALSE(ObjectType(TYPEID_CREATURE_CONTAINER).is_body_container());
    CHECK_FALSE(ObjectType(500).is_body_container());
}

TEST_CASE("ObjectType::is_creature_container") {
    CHECK(ObjectType(TYPEID_CREATURE_CONTAINER).is_creature_container());
    CHECK_FALSE(ObjectType(TYPEID_MAP_CONTAINER).is_creature_container());
    CHECK_FALSE(ObjectType(TYPEID_HEAD_CONTAINER).is_creature_container());
}

TEST_CASE("TYPEID enum values are correct") {
    CHECK(TYPEID_MAP_CONTAINER == 0);
    CHECK(TYPEID_HEAD_CONTAINER == 1);
    CHECK(TYPEID_NECK_CONTAINER == 2);
    CHECK(TYPEID_BAG_CONTAINER == 3);
    CHECK(TYPEID_TORSO_CONTAINER == 4);
    CHECK(TYPEID_RIGHTHAND_CONTAINER == 5);
    CHECK(TYPEID_LEFTHAND_CONTAINER == 6);
    CHECK(TYPEID_LEGS_CONTAINER == 7);
    CHECK(TYPEID_FEET_CONTAINER == 8);
    CHECK(TYPEID_FINGER_CONTAINER == 9);
    CHECK(TYPEID_AMMO_CONTAINER == 10);
    CHECK(TYPEID_CREATURE_CONTAINER == 99);
}
