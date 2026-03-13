#ifndef TIBIA_MAP_H_
#define TIBIA_MAP_H_ 1

#include "common.h"
#include "objects.h"
#include "script.h"

// NOTE(fusion): This is used by hash table entries and sectors to tell whether
// they're currently loaded or swapped out to disk.
enum : uint8 {
	STATUS_FREE = 0,
	STATUS_LOADED = 1,
	STATUS_SWAPPED = 2,

	// TODO(fusion): It seems this is only used with the `NONE` entry in the
	// hash table. I haven't seen it used **yet** but It may have a purpose
	// aside from preventing swap outs.
	STATUS_PERMANENT = 255,
};

// NOTE(fusion): This is used to determine precedence order of different objects
// in a map container.
enum : int {
	PRIORITY_BANK = 0,
	PRIORITY_CLIP = 1,
	PRIORITY_BOTTOM = 2,
	PRIORITY_TOP = 3,
	PRIORITY_CREATURE = 4,
	PRIORITY_LOW = 5,
};

struct Object {
	constexpr Object(void) : ObjectID(0) {}
	constexpr explicit Object(uint32 ObjectID): ObjectID(ObjectID) {}

	bool exists(void);
	ObjectType get_object_type(void);
	void set_object_type(ObjectType Type);
	Object get_next_object(void);
	void set_next_object(Object NextObject);
	Object get_container(void);
	void set_container(Object Con);
	uint32 get_creature_id(void);
	uint32 get_attribute(INSTANCEATTRIBUTE Attribute);
	void set_attribute(INSTANCEATTRIBUTE Attribute, uint32 Value);

	constexpr bool operator==(const Object &Other) const {
		return this->ObjectID == Other.ObjectID;
	}

	constexpr bool operator!=(const Object &Other) const {
		return this->ObjectID != Other.ObjectID;
	}

	// DATA
	// =================
	uint32 ObjectID;
};

constexpr Object NONE;

struct MapObject {
	uint32 ObjectID;
	Object NextObject;
	Object Container;
	ObjectType Type;
	uint32 Attributes[4];
};

struct ObjectBlock {
	MapObject Object[32768];
};

struct Sector {
	Object MapCon[32][32];
	uint32 TimeStamp;
	uint8 Status;
	uint8 MapFlags;
};

struct DepotInfo {
	char Town[20];
	int Size;
};

struct Mark {
	char Name[20];
	int x;
	int y;
	int z;
};

struct CronEntry {
	Object Obj;
	uint32 RoundNr;
	int Previous;
	int Next;
};

// NOTE(fusion): Map config values.
extern int SectorXMin;
extern int SectorXMax;
extern int SectorYMin;
extern int SectorYMax;
extern int SectorZMin;
extern int SectorZMax;
extern int RefreshedCylinders;
extern int NewbieStartPositionX;
extern int NewbieStartPositionY;
extern int NewbieStartPositionZ;
extern int VeteranStartPositionX;
extern int VeteranStartPositionY;
extern int VeteranStartPositionZ;

// NOTE(fusion): Cron management functions. Most for internal use.
Object cron_check(void);
void cron_expire(Object Obj, int Delay);
void cron_change(Object Obj, int NewDelay);
uint32 cron_info(Object Obj, bool delete_op);
uint32 cron_stop(Object Obj);

// NOTE(fusion): Map management functions. Most for internal use.
void swap_object(TWriteBinaryFile *File, Object Obj, uintptr FileNumber);
void swap_sector(void);
void unswap_sector(uintptr FileNumber);
void delete_swapped_sectors(void);
void load_objects(ReadScriptFile *Script, TWriteStream *Stream, bool Skip);
void load_objects(TReadStream *Stream, Object Con);
void init_sector(int SectorX, int SectorY, int SectorZ);
void load_sector(const char *FileName, int SectorX, int SectorY, int SectorZ);
void load_map(void);
void save_objects(Object Obj, TWriteStream *Stream, bool Stop);
void save_objects(TReadStream *Stream, WriteScriptFile *Script);
void save_sector(char *FileName, int SectorX, int SectorY, int SectorZ);
void save_map(void);
void refresh_sector(int SectorX, int SectorY, int SectorZ, TReadStream *Stream);
void patch_sector(int SectorX, int SectorY, int SectorZ, bool FullSector,
		ReadScriptFile *Script, bool SaveHouses);
void init_map(void);
void exit_map(bool Save);

// NOTE(fusion): Object related functions.
MapObject *access_object(Object Obj);
Object create_object(void);
void delete_object(Object Obj);
void change_object(Object Obj, ObjectType NewType);
void change_object(Object Obj, INSTANCEATTRIBUTE Attribute, uint32 Value);
int get_object_priority(Object Obj);
void place_object(Object Obj, Object Con, bool Append);
void cut_object(Object Obj);
void move_object(Object Obj, Object Con);
Object append_object(Object Con, ObjectType Type);
Object set_object(Object Con, ObjectType Type, uint32 CreatureID);
Object copy_object(Object Con, Object Source);
Object split_object(Object Obj, int Count);
void merge_objects(Object Obj, Object Dest);
Object get_first_container_object(Object Con);
Object get_container_object(Object Con, int Index);
Object get_map_container(int x, int y, int z);
Object get_map_container(Object Obj);
Object get_first_object(int x, int y, int z);
Object get_first_spec_object(int x, int y, int z, ObjectType Type);
uint8 get_map_container_flags(Object Obj);
void get_object_coordinates(Object Obj, int *x, int *y, int *z);
bool coordinate_flag(int x, int y, int z, FLAG Flag);
bool is_on_map(int x, int y, int z);
bool is_premium_area(int x, int y, int z);
bool is_no_logout_field(int x, int y, int z);
bool is_protection_zone(int x, int y, int z);
bool is_house(int x, int y, int z);
uint16 get_house_id(int x, int y, int z);
void set_house_id(int x, int y, int z, uint16 ID);
int get_depot_number(const char *Town);
const char *get_depot_name(int DepotNumber);
int get_depot_size(int DepotNumber, bool PremiumAccount);
bool get_mark_position(const char *Name, int *x, int *y, int *z);
void get_start_position(int *x, int *y, int *z, bool Newbie);

#endif //TIBIA_MAP_H_
