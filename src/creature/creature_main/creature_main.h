#ifndef TIBIA_CREATURE_MAIN_H_
#define TIBIA_CREATURE_MAIN_H_ 1

#include "creature/creature/creature.h"
#include "creature/race/race.h"
#include "containers.h"

// TAttackWave
// =============================================================================
struct TAttackWave {
	TAttackWave(void);
	~TAttackWave(void);

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
	vector<TItemData> ExtraItem;
};

// Global data
#define MAX_RACES 512
extern TRaceData RaceData[MAX_RACES];
extern priority_queue<uint32, uint32> ToDoQueue;

// Creature chain/hash management
void InsertChainCreature(TCreature *Creature, int CoordX, int CoordY);
void DeleteChainCreature(TCreature *Creature);
void MoveChainCreature(TCreature *Creature, int CoordX, int CoordY);
void ProcessCreatures(void);
void ProcessSkills(void);
void MoveCreatures(int Delay);

// Kill statistics
void AddKillStatistics(int AttackerRace, int DefenderRace);
void WriteKillStatistics(void);
void InitKillStatistics(void);
void ExitKillStatistics(void);

// Monster raids
void LoadMonsterRaid(const char *FileName, int Start,
		bool *Type, int *Date, int *Interval, int *Duration);
void LoadMonsterRaids(void);
void ProcessMonsterRaids(void);

// Init/Exit
void InitCr(void);
void ExitCr(void);

#endif // TIBIA_CREATURE_MAIN_H_
