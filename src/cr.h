#ifndef TIBIA_CREATURE_H_
#define TIBIA_CREATURE_H_ 1

#include "common.h"
#include "connections.h"
#include "containers.h"
#include "enums.h"
#include "map.h"

struct TMonster;
struct TNPC;
struct TPlayer;

#include "creature/creature/creature.h"

#include "creature/race/race.h"

// TPlayerData, TPlayerIndex*, TPlayer — extracted to creature/player/player.h

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

#include "creature/nonplayer/nonplayer.h"

#include "creature/npc/npc.h"

#include "creature/monster/monster.h"

#include "creature/player/player.h"

// crmain.cc
// =============================================================================
#define MAX_RACES 512
extern TRaceData RaceData[MAX_RACES];
extern priority_queue<uint32, uint32> ToDoQueue;

void InsertChainCreature(TCreature *Creature, int CoordX, int CoordY);
void DeleteChainCreature(TCreature *Creature);
void MoveChainCreature(TCreature *Creature, int CoordX, int CoordY);
void ProcessCreatures(void);
void ProcessSkills(void);
void MoveCreatures(int Delay);

void AddKillStatistics(int AttackerRace, int DefenderRace);
void WriteKillStatistics(void);
void InitKillStatistics(void);
void ExitKillStatistics(void);

void LoadMonsterRaid(const char *FileName, int Start,
		bool *Type, int *Date, int *Interval, int *Duration);
void LoadMonsterRaids(void);
void ProcessMonsterRaids(void);

void InitCr(void);
void ExitCr(void);

#endif //TIBIA_CREATURE_H_
