#include "game_data/objects/objects.h"
#include "config.h"
#include "containers.h"
#include "script.h"

#include <fstream>

static vector<TObjectType> ObjectTypes(0, 5000, 1000);
static ObjectType SpecialObjects[49];
static uint8 OldGroup[8192];
static uint8 OldNumber[8192];
static int NewType[65536];

static FLAG TypeAttributeFlags[62] = {
	BANK,               // WAYPOINTS
	CONTAINER,          // CAPACITY
	CHANGEUSE,          // CHANGETARGET
	KEYDOOR,            // KEYDOORTARGET
	NAMEDOOR,           // NAMEDOORTARGET
	LEVELDOOR,          // LEVELDOORTARGET
	QUESTDOOR,          // QUESTDOORTARGET
	FOOD,               // NUTRITION
	INFORMATION,        // INFORMATIONTYPE
	TEXT,               // FONTSIZE
	WRITE,              // MAXLENGTH
	WRITEONCE,          // MAXLENGTHONCE
	LIQUIDSOURCE,       // SOURCELIQUIDTYPE
	TELEPORTABSOLUTE,   // ABSTELEPORTEFFECT
	TELEPORTRELATIVE,   // RELTELEPORTDISPLACEMENT
	TELEPORTRELATIVE,   // RELTELEPORTEFFECT
	AVOID,              // AVOIDDAMAGETYPES
	RESTRICTLEVEL,      // MINIMUMLEVEL
	RESTRICTPROFESSION, // PROFESSIONS
	TAKE,               // WEIGHT
	ROTATE,             // ROTATETARGET
	DESTROY,            // DESTROYTARGET
	CLOTHES,            // BODYPOSITION
	SKILLBOOST,         // SKILLNUMBER
	SKILLBOOST,         // SKILLMODIFICATION
	PROTECTION,         // PROTECTIONDAMAGETYPES
	PROTECTION,         // DAMAGEREDUCTION
	LIGHT,              // BRIGHTNESS
	LIGHT,              // LIGHTCOLOR
	CORPSE,             // CORPSETYPE
	EXPIRE,             // TOTALEXPIRETIME
	EXPIRE,             // EXPIRETARGET
	WEAROUT,            // TOTALUSES
	WEAROUT,            // WEAROUTTARGET
	WEAPON,             // WEAPONTYPE
	WEAPON,             // WEAPONATTACKVALUE
	WEAPON,             // WEAPONDEFENDVALUE
	SHIELD,             // SHIELDDEFENDVALUE
	BOW,                // BOWRANGE
	BOW,                // BOWAMMOTYPE
	THROW,              // THROWRANGE
	THROW,              // THROWATTACKVALUE
	THROW,              // THROWDEFENDVALUE
	THROW,              // THROWMISSILE
	THROW,              // THROWSPECIALEFFECT
	THROW,              // THROWEFFECTSTRENGTH
	THROW,              // THROWFRAGILITY
	WAND,               // WANDRANGE
	WAND,               // WANDMANACONSUMPTION
	WAND,               // WANDATTACKSTRENGTH
	WAND,               // WANDATTACKVARIATION
	WAND,               // WANDDAMAGETYPE
	WAND,               // WANDMISSILE
	AMMO,               // AMMOTYPE
	AMMO,               // AMMOATTACKVALUE
	AMMO,               // AMMOMISSILE
	AMMO,               // AMMOSPECIALEFFECT
	AMMO,               // AMMOEFFECTSTRENGTH
	ARMOR,              // ARMORVALUE
	HEIGHT,             // ELEVATION
	DISGUISE,           // DISGUISETARGET
	BANK,               // MEANING
};

static FLAG InstanceAttributeFlags[18] = {
	CONTAINER,        // CONTENT
	CHEST,            // CHESTQUESTNUMBER
	CUMULATIVE,       // AMOUNT
	KEY,              // KEYNUMBER
	KEYDOOR,          // KEYHOLENUMBER
	LEVELDOOR,        // DOORLEVEL
	QUESTDOOR,        // DOORQUESTNUMBER
	QUESTDOOR,        // DOORQUESTVALUE
	RUNE,             // CHARGES
	TEXT,             // TEXTSTRING
	TEXT,             // EDITOR
	LIQUIDCONTAINER,  // CONTAINERLIQUIDTYPE
	LIQUIDPOOL,       // POOLLIQUIDTYPE
	TELEPORTABSOLUTE, // ABSTELEPORTDESTINATION
	MAGICFIELD,       // RESPONSIBLE
	EXPIRE,           // REMAININGEXPIRETIME
	EXPIRESTOP,       // SAVEDEXPIRETIME
	WEAROUT,          // REMAININGUSES
};

static const char FlagNames[66][30] = {
	"Bank",               // BANK
	"Clip",               // CLIP
	"Bottom",             // BOTTOM
	"Top",                // TOP
	"Container",          // CONTAINER
	"Chest",              // CHEST
	"Cumulative",         // CUMULATIVE
	"UseEvent",           // USEEVENT
	"ChangeUse",          // CHANGEUSE
	"ForceUse",           // FORCEUSE
	"MultiUse",           // MULTIUSE
	"DistUse",            // DISTUSE
	"movement_event",     // MOVEMENTEVENT
	"collision_event",    // COLLISIONEVENT
	"separation_event",   // SEPARATIONEVENT
	"Key",                // KEY
	"KeyDoor",            // KEYDOOR
	"NameDoor",           // NAMEDOOR
	"LevelDoor",          // LEVELDOOR
	"QuestDoor",          // QUESTDOOR
	"Bed",                // BED
	"Food",               // FOOD
	"Rune",               // RUNE
	"Information",        // INFORMATION
	"Text",               // TEXT
	"Write",              // WRITE
	"WriteOnce",          // WRITEONCE
	"LiquidContainer",    // LIQUIDCONTAINER
	"LiquidSource",       // LIQUIDSOURCE
	"LiquidPool",         // LIQUIDPOOL
	"TeleportAbsolute",   // TELEPORTABSOLUTE
	"TeleportRelative",   // TELEPORTRELATIVE
	"Unpass",             // UNPASS
	"Unmove",             // UNMOVE
	"Unthrow",            // UNTHROW
	"Unlay",              // UNLAY
	"Avoid",              // AVOID
	"MagicField",         // MAGICFIELD
	"RestrictLevel",      // RESTRICTLEVEL
	"RestrictProfession", // RESTRICTPROFESSION
	"Take",               // TAKE
	"Hang",               // HANG
	"HookSouth",          // HOOKSOUTH
	"HookEast",           // HOOKEAST
	"Rotate",             // ROTATE
	"Destroy",            // DESTROY
	"Clothes",            // CLOTHES
	"SkillBoost",         // SKILLBOOST
	"Protection",         // PROTECTION
	"Light",              // LIGHT
	"RopeSpot",           // ROPESPOT
	"Corpse",             // CORPSE
	"Expire",             // EXPIRE
	"ExpireStop",         // EXPIRESTOP
	"WearOut",            // WEAROUT
	"Weapon",             // WEAPON
	"Shield",             // SHIELD
	"Bow",                // BOW
	"Throw",              // THROW
	"Wand",               // WAND
	"Ammo",               // AMMO
	"Armor",              // ARMOR
	"Height",             // HEIGHT
	"Disguise",           // DISGUISE
	"ShowDetail",         // SHOWDETAIL
	"Special",            // SPECIALOBJECT
};

static const char TypeAttributeNames[62][30] = {
	"Waypoints",               // WAYPOINTS
	"Capacity",                // CAPACITY
	"ChangeTarget",            // CHANGETARGET
	"KeydoorTarget",           // KEYDOORTARGET
	"NamedoorTarget",          // NAMEDOORTARGET
	"LeveldoorTarget",         // LEVELDOORTARGET
	"QuestdoorTarget",         // QUESTDOORTARGET
	"Nutrition",               // NUTRITION
	"InformationType",         // INFORMATIONTYPE
	"FontSize",                // FONTSIZE
	"MaxLength",               // MAXLENGTH
	"MaxLengthOnce",           // MAXLENGTHONCE
	"SourceLiquidType",        // SOURCELIQUIDTYPE
	"AbsTeleportEffect",       // ABSTELEPORTEFFECT
	"RelTeleportDisplacement", // RELTELEPORTDISPLACEMENT
	"RelTeleportEffect",       // RELTELEPORTEFFECT
	"AvoidDamageTypes",        // AVOIDDAMAGETYPES
	"MinimumLevel",            // MINIMUMLEVEL
	"Professions",             // PROFESSIONS
	"Weight",                  // WEIGHT
	"RotateTarget",            // ROTATETARGET
	"DestroyTarget",           // DESTROYTARGET
	"BodyPosition",            // BODYPOSITION
	"SkillNumber",             // SKILLNUMBER
	"SkillModification",       // SKILLMODIFICATION
	"ProtectionDamageTypes",   // PROTECTIONDAMAGETYPES
	"DamageReduction",         // DAMAGEREDUCTION
	"Brightness",              // BRIGHTNESS
	"LightColor",              // LIGHTCOLOR
	"CorpseType",              // CORPSETYPE
	"TotalExpireTime",         // TOTALEXPIRETIME
	"ExpireTarget",            // EXPIRETARGET
	"TotalUses",               // TOTALUSES
	"WearoutTarget",           // WEAROUTTARGET
	"WeaponType",              // WEAPONTYPE
	"WeaponAttackValue",       // WEAPONATTACKVALUE
	"WeaponDefendValue",       // WEAPONDEFENDVALUE
	"ShieldDefendValue",       // SHIELDDEFENDVALUE
	"BowRange",                // BOWRANGE
	"BowAmmoType",             // BOWAMMOTYPE
	"ThrowRange",              // THROWRANGE
	"ThrowAttackValue",        // THROWATTACKVALUE
	"ThrowDefendValue",        // THROWDEFENDVALUE
	"ThrowMissile",            // THROWMISSILE
	"ThrowSpecialEffect",      // THROWSPECIALEFFECT
	"ThrowEffectStrength",     // THROWEFFECTSTRENGTH
	"ThrowFragility",          // THROWFRAGILITY
	"WandRange",               // WANDRANGE
	"WandManaConsumption",     // WANDMANACONSUMPTION
	"WandAttackStrength",      // WANDATTACKSTRENGTH
	"WandAttackVariation",     // WANDATTACKVARIATION
	"WandDamageType",          // WANDDAMAGETYPE
	"WandMissile",             // WANDMISSILE
	"AmmoType",                // AMMOTYPE
	"AmmoAttackValue",         // AMMOATTACKVALUE
	"AmmoMissile",             // AMMOMISSILE
	"AmmoSpecialEffect",       // AMMOSPECIALEFFECT
	"AmmoEffectStrength",      // AMMOEFFECTSTRENGTH
	"ArmorValue",              // ARMORVALUE
	"Elevation",               // ELEVATION
	"DisguiseTarget",          // DISGUISETARGET
	"Meaning",                 // MEANING
};

static const char InstanceAttributeNames[18][30] = {
	"Content",                // CONTENT
	"ChestQuestNumber",       // CHESTQUESTNUMBER
	"Amount",                 // AMOUNT
	"KeyNumber",              // KEYNUMBER
	"KeyholeNumber",          // KEYHOLENUMBER
	"Level",                  // DOORLEVEL
	"DoorQuestNumber",        // DOORQUESTNUMBER
	"DoorQuestValue",         // DOORQUESTVALUE
	"Charges",                // CHARGES
	"String",                 // TEXTSTRING
	"Editor",                 // EDITOR
	"ContainerLiquidType",    // CONTAINERLIQUIDTYPE
	"PoolLiquidType",         // POOLLIQUIDTYPE
	"AbsTeleportDestination", // ABSTELEPORTDESTINATION
	"Responsible",            // RESPONSIBLE
	"RemainingExpireTime",    // REMAININGEXPIRETIME
	"SavedExpireTime",        // SAVEDEXPIRETIME
	"RemainingUses",          // REMAININGUSES
};

// ObjectType
// =============================================================================
void ObjectType::set_type_id(int TypeID) {
	if (!object_type_exists(TypeID)) {
		error("ObjectType::set_type_id: Invalid type %d.\n", TypeID);
		TypeID = 0;
	}

	this->TypeID = TypeID;
}

bool ObjectType::get_flag(FLAG Flag) {
	TObjectType *TypeP = ObjectTypes.at(this->TypeID);
	return CheckBit(TypeP->Flags, (int)Flag);
}

uint32 ObjectType::get_attribute(TYPEATTRIBUTE Attribute) {
	if (!this->get_flag(TypeAttributeFlags[Attribute])) {
		error("ObjectType::get_attribute: Type %d does not have flag %d for attribute %d.\n", this->TypeID,
			  TypeAttributeFlags[Attribute], Attribute);
		return 0;
	}

	TObjectType *TypeP = ObjectTypes.at(this->TypeID);
	return TypeP->Attributes[Attribute];
}

int ObjectType::get_attribute_offset(INSTANCEATTRIBUTE Attribute) {
	if (!this->get_flag(InstanceAttributeFlags[Attribute])) {
		// NOTE(fusion): The CONTENT attribute seems to be the only one that maps
		// to two object flags, which is why we have this extra check here.
		if (!(Attribute == CONTENT && this->get_flag(CHEST))) {
			return -1;
		}
	}

	TObjectType *TypeP = ObjectTypes.at(this->TypeID);
	return TypeP->AttributeOffsets[Attribute];
}

const char *ObjectType::get_name(int Count) {
	if (this->is_creature_container()) {
		error("ObjectType::get_name: The creature type has no name.\n");
		return NULL;
	}

	TObjectType *TypeP = ObjectTypes.at(this->TypeID);
	const char *Name = TypeP->Name;
	if (Name == NULL) {
		Name = "";
	}

	return Plural(Name, Count);
}

const char *ObjectType::get_description(void) {
	TObjectType *TypeP = ObjectTypes.at(this->TypeID);
	return TypeP->Description;
}

// Object Type Related Functions
// =============================================================================
int get_flag_by_name(const char *Name) {
	int Result = -1;
	for (int i = 0; i < NARRAY(FlagNames); i += 1) {
		if (stricmp(FlagNames[i], Name) == 0) {
			Result = i;
			break;
		}
	}
	return Result;
}

int get_type_attribute_by_name(const char *Name) {
	int Result = -1;
	for (int i = 0; i < NARRAY(TypeAttributeNames); i += 1) {
		if (stricmp(TypeAttributeNames[i], Name) == 0) {
			Result = i;
			break;
		}
	}
	return Result;
}

int get_instance_attribute_by_name(const char *Name) {
	int Result = -1;
	for (int i = 0; i < NARRAY(InstanceAttributeNames); i += 1) {
		if (stricmp(InstanceAttributeNames[i], Name) == 0) {
			Result = i;
			break;
		}
	}
	return Result;
}

const char *get_flag_name(int Flag) {
	ASSERT(Flag >= 0 && Flag < NARRAY(FlagNames));
	return FlagNames[Flag];
}

const char *get_type_attribute_name(int Attribute) {
	ASSERT(Attribute >= 0 && Attribute < NARRAY(TypeAttributeFlags));
	return TypeAttributeNames[Attribute];
}

const char *get_instance_attribute_name(int Attribute) {
	ASSERT(Attribute >= 0 && Attribute < NARRAY(InstanceAttributeNames));
	return InstanceAttributeNames[Attribute];
}

bool object_type_exists(int TypeID) {
	return ObjectTypes.min <= TypeID && TypeID <= ObjectTypes.max;
}

// NOTE(fusion): Small helper to compute the index into the `NewType` array
// from old object type values.
static uint16 GetNewTypeIndex(uint8 Group, uint8 Number) {
	return ((uint16)Group << 8) | (uint16)Number;
}

bool object_type_exists(uint8 Group, uint8 Number) {
	// TODO(fusion): Check bounds?
	return NewType[GetNewTypeIndex(Group, Number)] >= 0;
}

ObjectType get_new_object_type(uint8 Group, uint8 Number) {
	int TypeID = NewType[GetNewTypeIndex(Group, Number)];
	if (TypeID < 0) {
		error("get_new_object_type: Object type %u/%u does not exist.\n", Group, Number);
		TypeID = 0;
	}

	return ObjectType(TypeID);
}

void get_old_object_type(ObjectType Type, uint8 *Group, uint8 *Number) {
	if (!object_type_exists(Type.TypeID)) {
		error("get_old_object_type: Invalid type %d.\n", Type.TypeID);
		return;
	}

	// TODO(fusion): Check bounds?
	*Group = OldGroup[Type.TypeID];
	*Number = OldNumber[Type.TypeID];
}

ObjectType get_special_object(SPECIALMEANING Meaning) {
	int TypeID = 0;
	if (Meaning > 0 && Meaning < NARRAY(SpecialObjects)) {
		TypeID = SpecialObjects[Meaning].TypeID;
		if (TypeID == 0) {
			error("get_special_object: No object type with meaning %d defined.\n", Meaning);
		}
	} else {
		error("get_special_object: Invalid meaning %d.\n", Meaning);
	}
	return ObjectType(TypeID);
}

ObjectType get_object_type_by_name(const char *SearchName, bool Movable) {
	if (SearchName == NULL) {
		error("get_object_type_by_name: SearchName is NULL.\n");
		return ObjectType();
	}

	// NOTE(fusion): We add spaces around the search term to match whole words
	// or sentences. This requires use to also add spaces around the searching
	// text, in case the search term is at edge.
	ObjectType BestMatch;
	char Help[50];
	char Pattern[50];
	snprintf(Pattern, sizeof(Pattern), " %s ", SearchName);
	for (int TypeID = TYPEID_CREATURE_CONTAINER + 1; TypeID <= ObjectTypes.max; TypeID += 1) {
		ObjectType Type = TypeID;
		if (!Movable || !Type.get_flag(UNMOVE)) {
			const char *TypeName = Type.get_name(-1);
			if (TypeName && TypeName[0] != 0) {
				if (stricmp(SearchName, TypeName) == 0) {
					BestMatch = Type;
					break;
				}

				if (BestMatch.TypeID == 0) {
					snprintf(Help, sizeof(Help), " %s ", TypeName);
					if (strstr(Help, Pattern) != NULL) {
						BestMatch = Type;
					}
				}
			}
		}
	}

	return BestMatch;
}

static void load_objects(void) {
	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/objects.srv", DATAPATH);

	int TypeID = -1;
	ReadScriptFile Script;
	Script.open(FileName);
	while (true) {
		Script.next_token();
		if (Script.Token == ENDOFFILE) {
			Script.close();
			break;
		}

		if (Script.Token != IDENTIFIER) {
			Script.error("Identifier expected");
		}

		char Identifier[MAX_IDENT_LENGTH];
		strcpy(Identifier, Script.get_identifier());
		Script.read_symbol('=');

		if (strcmp(Identifier, "typeid") == 0) {
			TypeID = Script.read_number();
			// NOTE(fusion): Initialize object type.
			TObjectType *TypeP = ObjectTypes.at(TypeID);
			memset(TypeP, 0, sizeof(TObjectType));
			for (int i = 0; i < NARRAY(TypeP->AttributeOffsets); i += 1) {
				TypeP->AttributeOffsets[i] = -1;
			}
		} else {
			if (TypeID < 0) {
				Script.error("Invalid type id");
			}

			TObjectType *TypeP = ObjectTypes.at(TypeID);
			if (strcmp(Identifier, "name") == 0) {
				TypeP->Name = AddStaticString(Script.read_string());
			} else if (strcmp(Identifier, "description") == 0) {
				TypeP->Description = AddStaticString(Script.read_string());
			} else if (strcmp(Identifier, "flags") == 0) {
				Script.read_symbol('{');
				while (true) {
					Script.next_token();
					if (Script.Token == SPECIAL) {
						char Special = Script.get_special();
						if (Special == ',')
							continue;
						if (Special == '}')
							break;
					}

					int Flag = get_flag_by_name(Script.get_identifier());
					if (Flag == -1) {
						Script.error("Unknown flag");
					}

					SetBit(TypeP->Flags, Flag);
				}

				// NOTE(fusion): Assign instance attribute offsets after parsing flags.
				int InstanceAttributeCount = 0;
				for (int InstanceAttribute = 0; InstanceAttribute < NARRAY(TypeP->AttributeOffsets);
					 InstanceAttribute += 1) {
					// TODO(fusion): This thing with CONTENT, CONTAINER, and CHEST have
					// been a trend so my guess it that CONTENT is the only instance
					// attribute that has two flags (CONTAINER and CHEST), and we need
					// to check it here.
					if (CheckBit(TypeP->Flags, InstanceAttributeFlags[InstanceAttribute]) ||
						(InstanceAttribute == CONTENT && CheckBit(TypeP->Flags, CHEST))) {
						TypeP->AttributeOffsets[InstanceAttribute] = InstanceAttributeCount;
						InstanceAttributeCount += 1;
					}
				}
			} else if (strcmp(Identifier, "attributes") == 0) {
				Script.read_symbol('{');
				while (true) {
					Script.next_token();
					if (Script.Token == SPECIAL) {
						char Special = Script.get_special();
						if (Special == ',')
							continue;
						if (Special == '}')
							break;
					}

					int TypeAttribute = get_type_attribute_by_name(Script.get_identifier());
					if (TypeAttribute == -1) {
						Script.error("Unknown attribute");
					}

					Script.read_symbol('=');
					uint32 Value = (uint32)Script.read_number();
					TypeP->Attributes[TypeAttribute] = Value;
					if (TypeAttribute == MEANING) {
						if (Value == 0 || Value >= NARRAY(SpecialObjects)) {
							Script.error("Unknown meaning");
						}

						if (SpecialObjects[Value].TypeID != 0) {
							Script.error("Special object already defined");
						}

						SpecialObjects[Value].set_type_id(TypeID);
					}
				}
			} else {
				Script.error("Unknown object type property");
			}
		}
	}
}

static void LoadConversionList(void) {
	static_assert(NARRAY(OldGroup) == NARRAY(OldNumber));

	for (int i = 0; i < NARRAY(OldGroup); i += 1) {
		OldGroup[i] = 0;
		OldNumber[i] = 0;
	}

	for (int i = 0; i < NARRAY(NewType); i += 1) {
		NewType[i] = -1;
	}

	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/conversion.lst", DATAPATH);
	std::ifstream IN(FileName, std::ios_base::in);
	if (IN.fail()) {
		error("LoadConversionList: Cannot open file %s.\n", FileName);
		throw "Cannot open conversion.lst";
	}

	while (true) {
		int Group, Number, TypeID;
		IN >> Group >> Number >> TypeID;
		if (IN.fail()) {
			// TODO(fusion): if(!IN.eof()) { error(...); }
			IN.close();
			break;
		}

		// TODO(fusion): Review this?
		if (TypeID <= 0 || TypeID >= NARRAY(OldGroup)) {
			error("LoadConversionList: Invalid type %d.\n", TypeID);
			throw "Error while loading conversion.lst";
		}

		OldGroup[TypeID] = Group;
		OldNumber[TypeID] = Number;
		NewType[GetNewTypeIndex(Group, Number)] = TypeID;
	}

	// NOTE(fusion): Map and body containers.
	for (int TypeID = 0; TypeID <= 10; TypeID += 1) {
		OldGroup[TypeID] = 0;
		OldNumber[TypeID] = (uint8)TypeID;
		NewType[GetNewTypeIndex(0, (uint8)TypeID)] = TypeID;
	}

	// NOTE(fusion): Creature container.
	OldGroup[99] = 250;
	OldNumber[99] = 0;
	NewType[GetNewTypeIndex(250, 0)] = 99;
}

void init_objects(void) {
	load_objects();
	LoadConversionList();
}

void exit_objects(void) {
	// no-op
}
