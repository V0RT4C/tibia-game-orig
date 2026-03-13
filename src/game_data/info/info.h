#ifndef TIBIA_INFO_H_
#define TIBIA_INFO_H_ 1

#include "common.h"
#include "map.h"

const char *get_liquid_name(int LiquidType);
uint8 get_liquid_color(int LiquidType);
const char *get_name(Object Obj);
const char *get_info(Object Obj);
int get_weight(Object Obj, int Count);
int get_complete_weight(Object Obj);
int get_row_weight(Object Obj);
uint32 get_object_creature_id(Object Obj);
int get_object_body_position(Object Obj);
int get_object_rnum(Object Obj);
bool object_in_range(uint32 CreatureID, Object Obj, int Range);
bool object_accessible(uint32 CreatureID, Object Obj, int Range);
int object_distance(Object Obj1, Object Obj2);
Object get_body_container(uint32 CreatureID, int Position);
Object get_body_object(uint32 CreatureID, int Position);
Object get_top_object(int x, int y, int z, bool Move);
Object get_container(uint32 CreatureID, int x, int y, int z);
Object get_object(uint32 CreatureID, int x, int y, int z, int RNum, ObjectType Type);
Object get_row_object(Object Obj, ObjectType Type, uint32 Value, bool Recurse);
Object get_inventory_object(uint32 CreatureID, ObjectType Type, uint32 Value);
bool is_held_by_container(Object Obj, Object Con);
int count_objects_in_container(Object Con);
int count_objects(Object Obj);
int count_objects(Object Obj, ObjectType Type, uint32 Value);
int count_inventory_objects(uint32 CreatureID, ObjectType Type, uint32 Value);
int count_money(Object Obj);
int count_inventory_money(uint32 CreatureID);
void calculate_change(int Amount, int *Gold, int *Platinum, int *Crystal);
int get_height(int x, int y, int z);
bool jump_possible(int x, int y, int z, bool AvoidPlayers);
bool field_possible(int x, int y, int z, int FieldType);
bool search_free_field(int *x, int *y, int *z, int Distance, uint16 HouseID, bool Jump);
bool search_login_field(int *x, int *y, int *z, int Distance, bool Player);
bool search_spawn_field(int *x, int *y, int *z, int Distance, bool Player);
bool search_flight_field(uint32 FugitiveID, uint32 PursuerID, int *x, int *y, int *z);
bool search_summon_field(int *x, int *y, int *z, int Distance);
bool throw_possible(int OrigX, int OrigY, int OrigZ,
			int DestX, int DestY, int DestZ, int Power);
void get_creature_light(uint32 CreatureID, int *Brightness, int *Color);
int get_inventory_weight(uint32 CreatureID);
bool check_right(uint32 CharacterID, RIGHT Right);
bool check_banishment_right(uint32 CharacterID, int Reason, int Action);
const char *get_banishment_reason(int Reason);
void init_info(void);
void exit_info(void);

#endif //TIBIA_INFO_H_
