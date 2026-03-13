#ifndef TIBIA_CREATURE_MAIN_H_
#define TIBIA_CREATURE_MAIN_H_ 1

#include "creature/creature/creature.h"
#include "creature/race/race.h"
#include "containers.h"

// AttackWave
// =============================================================================
struct AttackWave {
	AttackWave(void);
	~AttackWave(void);

	// DATA
	// =================
	int x;
	int y;
	int z;
	int Spread;
	int Race;
	int MinCount;
	int MaxCount;
	int Radius;
	int Lifetime;
	uint32 Message;
	int ExtraItems;
	vector<ItemData> ExtraItem;
};

// Global data
#define MAX_RACES 512
extern RaceData race_data[MAX_RACES];
extern priority_queue<uint32, uint32> ToDoQueue;

// Creature chain/hash management
void insert_chain_creature(TCreature *Creature, int CoordX, int CoordY);
void delete_chain_creature(TCreature *Creature);
void move_chain_creature(TCreature *Creature, int CoordX, int CoordY);
void process_creatures(void);
void process_skills(void);
void move_creatures(int Delay);

// Kill statistics
void add_kill_statistics(int AttackerRace, int DefenderRace);
void write_kill_statistics(void);
void init_kill_statistics(void);
void exit_kill_statistics(void);

// Monster raids
void load_monster_raid(const char *FileName, int Start,
		bool *Type, int *Date, int *Interval, int *Duration);
void load_monster_raids(void);
void process_monster_raids(void);

// Init/Exit
void init_cr(void);
void exit_cr(void);

#endif // TIBIA_CREATURE_MAIN_H_
