#ifndef TIBIA_OBJECTS_H_
#define TIBIA_OBJECTS_H_ 1

#include "common.h"
#include "enums.h"

enum : int {
	TYPEID_MAP_CONTAINER = 0,
	TYPEID_HEAD_CONTAINER = 1,
	TYPEID_NECK_CONTAINER = 2,
	TYPEID_BAG_CONTAINER = 3,
	TYPEID_TORSO_CONTAINER = 4,
	TYPEID_RIGHTHAND_CONTAINER = 5,
	TYPEID_LEFTHAND_CONTAINER = 6,
	TYPEID_LEGS_CONTAINER = 7,
	TYPEID_FEET_CONTAINER = 8,
	TYPEID_FINGER_CONTAINER = 9,
	TYPEID_AMMO_CONTAINER = 10,
	TYPEID_CREATURE_CONTAINER = 99,
};

struct ObjectType {
	ObjectType(void) { this->set_type_id(0); }
	ObjectType(int TypeID) { this->set_type_id(TypeID); }
	void set_type_id(int TypeID);
	bool get_flag(FLAG Flag);
	uint32 get_attribute(TYPEATTRIBUTE Attribute);
	int get_attribute_offset(INSTANCEATTRIBUTE Attribute);
	const char *get_name(int Count);
	const char *get_description(void);

	bool is_map_container(void) { return this->TypeID == TYPEID_MAP_CONTAINER; }

	bool is_body_container(void) {
		return this->TypeID == TYPEID_HEAD_CONTAINER || this->TypeID == TYPEID_NECK_CONTAINER ||
			   this->TypeID == TYPEID_BAG_CONTAINER || this->TypeID == TYPEID_TORSO_CONTAINER ||
			   this->TypeID == TYPEID_RIGHTHAND_CONTAINER || this->TypeID == TYPEID_LEFTHAND_CONTAINER ||
			   this->TypeID == TYPEID_LEGS_CONTAINER || this->TypeID == TYPEID_FEET_CONTAINER ||
			   this->TypeID == TYPEID_FINGER_CONTAINER || this->TypeID == TYPEID_AMMO_CONTAINER;
	}

	bool is_creature_container(void) { return this->TypeID == TYPEID_CREATURE_CONTAINER; }

	bool is_two_handed(void) { return this->get_flag(CLOTHES) && this->get_attribute(BODYPOSITION) == 0; }

	bool is_weapon(void) {
		return this->get_flag(WEAPON) || this->get_flag(BOW) || this->get_flag(THROW) || this->get_flag(WAND);
	}

	bool is_close_weapon(void) {
		if (!this->get_flag(WEAPON)) {
			return false;
		}

		int WeaponType = this->get_attribute(WEAPONTYPE);
		return WeaponType == WEAPON_SWORD || WeaponType == WEAPON_CLUB || WeaponType == WEAPON_AXE;
	}

	ObjectType get_disguise(void) {
		if (this->get_flag(DISGUISE)) {
			return (int)this->get_attribute(DISGUISETARGET);
		} else {
			return *this;
		}
	}

	bool operator==(const ObjectType &Other) const { return this->TypeID == Other.TypeID; }

	bool operator!=(const ObjectType &Other) const { return this->TypeID != Other.TypeID; }

	// DATA
	// =================
	int TypeID;
};

struct TObjectType {
	const char *Name;
	const char *Description;
	uint8 Flags[9];
	uint32 Attributes[62];
	int AttributeOffsets[18];
};

int get_flag_by_name(const char *Name);
int get_type_attribute_by_name(const char *Name);
int get_instance_attribute_by_name(const char *Name);
const char *get_flag_name(int Flag);
const char *get_type_attribute_name(int Attribute);
const char *get_instance_attribute_name(int Attribute);
bool object_type_exists(int TypeID);
bool object_type_exists(uint8 Group, uint8 Number);
ObjectType get_new_object_type(uint8 Group, uint8 Number);
void get_old_object_type(ObjectType Type, uint8 *Group, uint8 *Number);
ObjectType get_special_object(SPECIALMEANING Meaning);
ObjectType get_object_type_by_name(const char *SearchName, bool Movable);
void init_objects(void);
void exit_objects(void);

#endif // TIBIA_OBJECTS_H_
