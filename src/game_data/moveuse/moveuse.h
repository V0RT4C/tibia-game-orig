#ifndef TIBIA_MOVEUSE_H_
#define TIBIA_MOVEUSE_H_ 1

#include "common.h"
#include "containers.h"
#include "map.h"
#include "script.h"

#define MOVEUSE_MAX_PARAMETERS 5

struct TPlayerData;

enum MoveUseActionType : int {
	MOVEUSE_ACTION_CREATEONMAP = 0,
	MOVEUSE_ACTION_CREATE = 1,
	MOVEUSE_ACTION_MONSTERONMAP = 2,
	MOVEUSE_ACTION_MONSTER = 3,
	MOVEUSE_ACTION_EFFECTONMAP = 4,
	MOVEUSE_ACTION_EFFECT = 5,
	MOVEUSE_ACTION_TEXTONMAP = 6,
	MOVEUSE_ACTION_TEXT = 7,
	MOVEUSE_ACTION_CHANGEONMAP = 8,
	MOVEUSE_ACTION_CHANGE = 9,
	MOVEUSE_ACTION_CHANGEREL = 10,
	MOVEUSE_ACTION_SETATTRIBUTE = 11,
	MOVEUSE_ACTION_CHANGEATTRIBUTE = 12,
	MOVEUSE_ACTION_SETQUESTVALUE = 13,
	MOVEUSE_ACTION_DAMAGE = 14,
	MOVEUSE_ACTION_SETSTART = 15,
	MOVEUSE_ACTION_WRITENAME = 16,
	MOVEUSE_ACTION_WRITETEXT = 17,
	MOVEUSE_ACTION_LOGOUT = 18,
	MOVEUSE_ACTION_MOVEALLONMAP = 19,
	MOVEUSE_ACTION_MOVEALL = 20,
	MOVEUSE_ACTION_MOVEALLREL = 21,
	MOVEUSE_ACTION_MOVETOPONMAP = 22,
	MOVEUSE_ACTION_MOVETOP = 23,
	MOVEUSE_ACTION_MOVETOPREL = 24,
	MOVEUSE_ACTION_MOVE = 25,
	MOVEUSE_ACTION_MOVEREL = 26,
	MOVEUSE_ACTION_RETRIEVE = 27,
	MOVEUSE_ACTION_DELETEALLONMAP = 28,
	MOVEUSE_ACTION_DELETETOPONMAP = 29,
	MOVEUSE_ACTION_DELETEONMAP = 30,
	MOVEUSE_ACTION_DELETE = 31,
	MOVEUSE_ACTION_DELETEININVENTORY = 32,
	MOVEUSE_ACTION_DESCRIPTION = 33,
	MOVEUSE_ACTION_LOADDEPOT = 34,
	MOVEUSE_ACTION_SAVEDEPOT = 35,
	MOVEUSE_ACTION_SENDMAIL = 36,
	MOVEUSE_ACTION_NOP = 37,
};

enum MoveUseConditionType : int {
	MOVEUSE_CONDITION_ISPOSITION = 0,
	MOVEUSE_CONDITION_ISTYPE = 1,
	MOVEUSE_CONDITION_ISCREATURE = 2,
	MOVEUSE_CONDITION_ISPLAYER = 3,
	MOVEUSE_CONDITION_HASFLAG = 4,
	MOVEUSE_CONDITION_HASTYPEATTRIBUTE = 5,
	MOVEUSE_CONDITION_HASINSTANCEATTRIBUTE = 6,
	MOVEUSE_CONDITION_HASTEXT = 7,
	MOVEUSE_CONDITION_ISPEACEFUL = 8,
	MOVEUSE_CONDITION_MAYLOGOUT = 9,
	MOVEUSE_CONDITION_HASPROFESSION = 10,
	MOVEUSE_CONDITION_HASLEVEL = 11,
	MOVEUSE_CONDITION_HASRIGHT = 12,
	MOVEUSE_CONDITION_HASQUESTVALUE = 13,
	MOVEUSE_CONDITION_TESTSKILL = 14,
	MOVEUSE_CONDITION_COUNTOBJECTS = 15,
	MOVEUSE_CONDITION_COUNTOBJECTSONMAP = 16,
	MOVEUSE_CONDITION_ISOBJECTTHERE = 17,
	MOVEUSE_CONDITION_ISCREATURETHERE = 18,
	MOVEUSE_CONDITION_ISPLAYERTHERE = 19,
	MOVEUSE_CONDITION_ISOBJECTININVENTORY = 20,
	MOVEUSE_CONDITION_ISPROTECTIONZONE = 21,
	MOVEUSE_CONDITION_ISHOUSE = 22,
	MOVEUSE_CONDITION_ISHOUSEOWNER = 23,
	MOVEUSE_CONDITION_ISDRESSED = 24,
	MOVEUSE_CONDITION_RANDOM = 25,
};

enum MoveUseEventType : int {
	MOVEUSE_EVENT_USE = 0,
	MOVEUSE_EVENT_MULTIUSE = 1,
	MOVEUSE_EVENT_MOVEMENT = 2,
	MOVEUSE_EVENT_COLLISION = 3,
	MOVEUSE_EVENT_SEPARATION = 4,
};

enum MoveUseModifierType : int {
	MOVEUSE_MODIFIER_NORMAL = 0,
	MOVEUSE_MODIFIER_INVERT = 1,
	MOVEUSE_MODIFIER_TRUE = 2,
};

enum MoveUseParameterType : int {
	MOVEUSE_PARAMETER_OBJECT = 0,
	MOVEUSE_PARAMETER_TYPE = 1,
	MOVEUSE_PARAMETER_FLAG = 2,
	MOVEUSE_PARAMETER_TYPEATTRIBUTE = 3,
	MOVEUSE_PARAMETER_INSTANCEATTRIBUTE = 4,
	MOVEUSE_PARAMETER_COORDINATE = 5,
	MOVEUSE_PARAMETER_VECTOR = 6,
	MOVEUSE_PARAMETER_RIGHT = 7,
	MOVEUSE_PARAMETER_SKILL = 8,
	MOVEUSE_PARAMETER_NUMBER = 9,
	MOVEUSE_PARAMETER_TEXT = 10,
	MOVEUSE_PARAMETER_COMPARISON = 11,
};

struct MoveUseAction {
	MoveUseActionType Action;
	int Parameters[MOVEUSE_MAX_PARAMETERS];
};

struct MoveUseRule {
	int FirstCondition;
	int LastCondition;
	int FirstAction;
	int LastAction;
};

struct MoveUseCondition {
	MoveUseModifierType Modifier;
	MoveUseConditionType Condition;
	int Parameters[MOVEUSE_MAX_PARAMETERS];
};

struct MoveUseDatabase {
	MoveUseDatabase(void) : Rules(1, 100, 100), NumberOfRules(0) {}

	vector<MoveUseRule> Rules;
	int NumberOfRules;
};

struct DelayedMail {
	uint32 CharacterID;
	int DepotNumber;
	uint8 *Packet;
	int PacketSize;
};

int pack_absolute_coordinate(int x, int y, int z);
void unpack_absolute_coordinate(int Packed, int *x, int *y, int *z);
int pack_relative_coordinate(int x, int y, int z);
void unpack_relative_coordinate(int Packed, int *x, int *y, int *z);

Object get_event_object(int Nr, Object User, Object Obj1, Object Obj2, Object Temp);
bool compare(int Value1, int Operator, int Value2);
bool check_condition(MoveUseEventType EventType, MoveUseCondition *Condition, Object User, Object Obj1, Object Obj2,
					 Object *Temp);
Object create_object(Object Con, ObjectType Type, uint32 Value);
void change_object(Object Obj, ObjectType NewType, uint32 Value);
void move_one_object(Object Obj, Object Con);
void move_all_objects(Object Obj, Object Dest, Object Exclude, bool MoveUnmovable);
void delete_all_objects(Object Obj, Object Exclude, bool DeleteUnmovable);
void clear_field(Object Obj, Object Exclude);
void load_depot_box(uint32 CreatureID, int Nr, Object Con);
void save_depot_box(uint32 CreatureID, int Nr, Object Con);
void send_mail(Object Obj);
void send_mails(TPlayerData *PlayerData);
void text_effect(const char *Text, int x, int y, int z, int Radius);
void execute_action(MoveUseEventType EventType, MoveUseAction *Action, Object User, Object Obj1, Object Obj2,
					Object *Temp);
bool handle_event(MoveUseEventType EventType, Object User, Object Obj1, Object Obj2);

void use_container(uint32 CreatureID, Object Con, int NextContainerNr);
void use_chest(uint32 CreatureID, Object Chest);
void use_liquid_container(uint32 CreatureID, Object Obj, Object Dest);
void use_food(uint32 CreatureID, Object Obj);
void use_text_object(uint32 CreatureID, Object Obj);
void use_announcer(uint32 CreatureID, Object Obj);
void use_key_door(uint32 CreatureID, Object Key, Object Door);
void use_name_door(uint32 CreatureID, Object Door);
void use_level_door(uint32 CreatureID, Object Door);
void use_quest_door(uint32 CreatureID, Object Door);
void use_weapon(uint32 CreatureID, Object Weapon, Object Target);
void use_change_object(uint32 CreatureID, Object Obj);
void use_object(uint32 CreatureID, Object Obj);
void use_objects(uint32 CreatureID, Object Obj1, Object Obj2);
void movement_event(Object Obj, Object Start, Object Dest);
void separation_event(Object Obj, Object Start);
void collision_event(Object Obj, Object Dest);

void load_parameters(ReadScriptFile *Script, int *Parameters, int NumberOfParameters, ...);
void load_condition(ReadScriptFile *Script, MoveUseCondition *Condition);
void load_action(ReadScriptFile *Script, MoveUseAction *Action);
void load_database(void);

void init_move_use(void);
void exit_move_use(void);

#endif // TIBIA_MOVEUSE_H_
