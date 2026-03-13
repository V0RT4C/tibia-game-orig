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

TEST_CASE("ObjectType::isMapContainer") {
    ObjectType map_con(TYPEID_MAP_CONTAINER);
    ObjectType head(TYPEID_HEAD_CONTAINER);
    CHECK(map_con.isMapContainer());
    CHECK_FALSE(head.isMapContainer());
}

TEST_CASE("ObjectType::isBodyContainer") {
    CHECK(ObjectType(TYPEID_HEAD_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_NECK_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_BAG_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_TORSO_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_RIGHTHAND_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_LEFTHAND_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_LEGS_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_FEET_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_FINGER_CONTAINER).isBodyContainer());
    CHECK(ObjectType(TYPEID_AMMO_CONTAINER).isBodyContainer());
    CHECK_FALSE(ObjectType(TYPEID_MAP_CONTAINER).isBodyContainer());
    CHECK_FALSE(ObjectType(TYPEID_CREATURE_CONTAINER).isBodyContainer());
    CHECK_FALSE(ObjectType(500).isBodyContainer());
}

TEST_CASE("ObjectType::isCreatureContainer") {
    CHECK(ObjectType(TYPEID_CREATURE_CONTAINER).isCreatureContainer());
    CHECK_FALSE(ObjectType(TYPEID_MAP_CONTAINER).isCreatureContainer());
    CHECK_FALSE(ObjectType(TYPEID_HEAD_CONTAINER).isCreatureContainer());
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
