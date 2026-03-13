#ifndef TIBIA_MAGIC_H_
#define TIBIA_MAGIC_H_ 1

#include "common.h"
#include "cr.h"
#include "map.h"

enum : int {
	FIELD_TYPE_FIRE = 1,
	FIELD_TYPE_POISON = 2,
	FIELD_TYPE_ENERGY = 3,
	FIELD_TYPE_MAGICWALL = 4,
	FIELD_TYPE_WILDGROWTH = 5,
};

struct Impact {
	// VIRTUAL FUNCTIONS
	// =========================================================================
	virtual void handleField(int x, int y, int z);  // VTABLE[0]
	virtual void handleCreature(TCreature *Victim); // VTABLE[1]
	virtual bool isAggressive(void);                // VTABLE[2]

	// NOTE(fusion): I don't think the original version had a destructor declared
	// here but the compiler complains when calling delete (which seems to only be
	// used in `TMonster::IdleStimulus`).
	virtual ~Impact(void) {
		// no-op
	}

	// DATA
	// =========================================================================
	// void *VTABLE;	// IMPLICIT
};

struct DamageImpact : Impact {
	DamageImpact(TCreature *Actor, int DamageType, int Power, bool AllowDefense);
	void handleCreature(TCreature *Victim) override;

	TCreature *Actor;
	int DamageType;
	int Power;
	bool AllowDefense;
};

struct FieldImpact : Impact {
	FieldImpact(TCreature *Actor, int FieldType);
	void handleField(int x, int y, int z) override;

	TCreature *Actor;
	int FieldType;
};

struct HealingImpact : Impact {
	HealingImpact(TCreature *Actor, int Power);
	void handleCreature(TCreature *Victim) override;
	bool isAggressive(void) override;

	TCreature *Actor;
	int Power;
};

struct SpeedImpact : Impact {
	SpeedImpact(TCreature *Actor, int Percent, int Duration);
	void handleCreature(TCreature *Victim) override;

	TCreature *Actor;
	int Percent;
	int Duration;
};

struct DrunkenImpact : Impact {
	DrunkenImpact(TCreature *Actor, int Power, int Duration);
	void handleCreature(TCreature *Victim) override;

	TCreature *Actor;
	int Power;
	int Duration;
};

struct StrengthImpact : Impact {
	StrengthImpact(TCreature *Actor, int Skills, int Percent, int Duration);
	void handleCreature(TCreature *Victim) override;

	TCreature *Actor;
	int Skills;
	int Percent;
	int Duration;
};

struct OutfitImpact : Impact {
	OutfitImpact(TCreature *Actor, TOutfit Outfit, int Duration);
	void handleCreature(TCreature *Victim) override;

	TCreature *Actor;
	TOutfit Outfit;
	int Duration;
};

struct SummonImpact : Impact {
	SummonImpact(TCreature *Actor, int Race, int Maximum);
	void handleField(int x, int y, int z) override;

	TCreature *Actor;
	int Race;
	int Maximum;
};

void actor_shape_spell(TCreature *Actor, Impact *Impact, int Effect);
void victim_shape_spell(TCreature *Actor, TCreature *Victim, int Range, int Animation, Impact *Impact, int Effect);
void origin_shape_spell(TCreature *Actor, int Radius, Impact *Impact, int Effect);
void circle_shape_spell(TCreature *Actor, int DestX, int DestY, int DestZ, int Range, int Animation, int Radius,
						Impact *Impact, int Effect);
void destination_shape_spell(TCreature *Actor, TCreature *Victim, int Range, int Animation, int Radius, Impact *Impact,
							 int Effect);
void angle_shape_spell(TCreature *Actor, int Angle, int Range, Impact *Impact, int Effect);
void check_spellbook(TCreature *Actor, int SpellNr);
void check_account(TCreature *Actor, int SpellNr);
void check_level(TCreature *Actor, int SpellNr);
void check_rune_level(TCreature *Actor, int SpellNr);
void check_magic_item(TCreature *Actor, ObjectType Type);
void check_ring(TCreature *Actor, int SpellNr);
void check_affected_players(TCreature *Actor, int x, int y, int z);
void check_mana(TCreature *Actor, int ManaPoints, int SoulPoints, int Delay);
int compute_damage(TCreature *Actor, int SpellNr, int Damage, int Variation);
bool is_aggressive_spell(int SpellNr);
void mass_combat(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints, int Damage, int Effect, int Radius,
				 int DamageType, int Animation);
void angle_combat(TCreature *Actor, int ManaPoints, int SoulPoints, int Damage, int Effect, int Range, int Angle,
				  int DamageType);
void combat(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints, int Damage, int Effect, int Animation,
			int DamageType);
int get_direction(int dx, int dy); // TODO(fusion): Move this one elsewhere? Maybe `info.cc`.
void kill_all_monsters(TCreature *Actor, int Effect, int Radius);
void create_field(int x, int y, int z, int FieldType, uint32 Owner, bool Peaceful);
void create_field(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints, int FieldType);
void create_field(TCreature *Actor, int ManaPoints, int SoulPoints, int FieldType);
void mass_create_field(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints, int FieldType, int Radius);
void create_field_wall(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints, int FieldType, int Width);
void delete_field(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints);
void cleanup_field(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints);
void cleanup_field(TCreature *Actor);
void teleport(TCreature *Actor, const char *Param);
void teleport_to_creature(TCreature *Actor, const char *Name);
void teleport_player_to_me(TCreature *Actor, const char *Name);
void magic_rope(TCreature *Actor, int ManaPoints, int SoulPoints);
void magic_climbing(TCreature *Actor, int ManaPoints, int SoulPoints, const char *Param);
void magic_climbing(TCreature *Actor, const char *Param);
void create_thing(TCreature *Actor, const char *Param1, const char *Param2);
void create_money(TCreature *Actor, const char *Param);
void create_food(TCreature *Actor, int ManaPoints, int SoulPoints);
void create_arrows(TCreature *Actor, int ManaPoints, int SoulPoints, int ArrowType, int Count);
void summon_creature(TCreature *Actor, int ManaPoints, int Race, bool God);
void summon_creature(TCreature *Actor, int ManaPoints, const char *RaceName, bool God);
void start_monsterraid(TCreature *Actor, const char *RaidName);
void raise_dead(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints);
void mass_raise_dead(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints, int Radius);
void heal(TCreature *Actor, int ManaPoints, int SoulPoints, int Amount);
void mass_heal(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints, int Amount, int Radius);
void heal_friend(TCreature *Actor, const char *TargetName, int ManaPoints, int SoulPoints, int Amount);
void refresh_mana(TCreature *Actor, int ManaPoints, int SoulPoints, int Amount);
void magic_go_strength(TCreature *Actor, TCreature *Target, int ManaPoints, int SoulPoints, int Percent, int Duration);
void shielding(TCreature *Actor, int ManaPoints, int SoulPoints, int Duration);
void negate_poison(TCreature *Actor, TCreature *Target, int ManaPoints, int SoulPoints);
void enlight(TCreature *Actor, int ManaPoints, int SoulPoints, int Radius, int Duration);
void invisibility(TCreature *Actor, int ManaPoints, int SoulPoints, int Duration);
void cancel_invisibility(TCreature *Actor, Object Target, int ManaPoints, int SoulPoints, int Radius);
void creature_illusion(TCreature *Actor, int ManaPoints, int SoulPoints, const char *RaceName, int Duration);
void object_illusion(TCreature *Actor, int ManaPoints, int SoulPoints, Object Target, int Duration);
void change_data(TCreature *Actor, const char *Param);
void enchant_object(TCreature *Actor, int ManaPoints, int SoulPoints, ObjectType OldType, ObjectType NewType);
void convince(TCreature *Actor, TCreature *Target);
void challenge(TCreature *Actor, int ManaPoints, int SoulPoints, int Radius);
void find_person(TCreature *Actor, int ManaPoints, int SoulPoints, const char *TargetName);
void get_position(TCreature *Actor);
void get_quest_value(TCreature *Actor, const char *Param);
void set_quest_value(TCreature *Actor, const char *Param1, const char *Param2);
void clear_quest_values(TCreature *Actor);
void create_knowledge(TCreature *Actor, const char *Param1, const char *Param2);
void change_profession(TCreature *Actor, const char *Param);
void edit_guests(TCreature *Actor);
void edit_subowners(TCreature *Actor);
void edit_name_door(TCreature *Actor);
void kick_guest(TCreature *Actor, const char *GuestName);
void notation(TCreature *Actor, const char *Name, const char *Comment);
void name_lock(TCreature *Actor, const char *Name);
void banish_account(TCreature *Actor, const char *Name, int Duration, const char *Reason);
void delete_account(TCreature *Actor, const char *Name, const char *Reason);
void banish_character(TCreature *Actor, const char *Name, int Duration, const char *Reason);
void delete_character(TCreature *Actor, const char *Name, const char *Reason);
void ip_banishment(TCreature *Actor, const char *Name, const char *Reason);
void set_name_rule(TCreature *Actor, const char *Name);
void kick_player(TCreature *Actor, const char *Name);
void home_teleport(TCreature *Actor, const char *Name);

// TODO(fusion): These are unsafe like strcpy.
void get_magic_item_description(Object Obj, char *SpellString, int *MagicLevel);
void get_spellbook(uint32 CharacterID, char *Buffer);

int get_spell_level(int SpellNr);

int check_for_spell(uint32 CreatureID, const char *Text);
void use_magic_item(uint32 CreatureID, Object Obj, Object Dest);
void drink_potion(uint32 CreatureID, Object Obj);

void init_magic(void);
void exit_magic(void);

#endif // TIBIA_MAGIC_H_
