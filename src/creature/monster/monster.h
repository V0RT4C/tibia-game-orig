#ifndef TIBIA_CREATURE_MONSTER_H_
#define TIBIA_CREATURE_MONSTER_H_ 1

#include "creature/nonplayer/nonplayer.h"

struct Monsterhome {
	int Race;
	int x;
	int y;
	int z;
	int Radius;
	int MaxMonsters;
	int ActMonsters;
	int RegenerationTime;
	int Timer;
};

struct TMonster: Nonplayer {
	TMonster(int Race, int x, int y, int z, int Home, uint32 MasterID);
	bool CanKickBoxes(void);
	void KickBoxes(Object Obj);
	bool KickCreature(TCreature *Creature);
	void Convince(TCreature *NewMaster);
	void SetTarget(TCreature *NewTarget);
	bool IsPlayerControlled(void);
	bool IsFleeing(void);

	// VIRTUAL FUNCTIONS
	~TMonster(void) override;
	bool MovePossible(int x, int y, int z, bool Execute, bool Jump) override;
	bool IsPeaceful(void) override;
	uint32 GetMaster(void) override;
	void DamageStimulus(uint32 AttackerID, int Damage, int DamageType) override;
	void IdleStimulus(void) override;
	void CreatureMoveStimulus(uint32 CreatureID, int Type) override;

	// DATA
	int Home;
	uint32 Master;
	uint32 Target;
};

// Monsterhome API
void start_monsterhome_timer(int Nr);
void load_monsterhomes(void);
void process_monsterhomes(void);
void notify_monsterhome_of_death(int Nr);
bool monsterhome_in_range(int Nr, int x, int y, int z);

// Monster creation API
TCreature *create_monster(int Race, int x, int y, int z,
		int Home, uint32 MasterID, bool ShowEffect);
void convince_monster(TCreature *Master, TCreature *Slave);
void challenge_monster(TCreature *Challenger, TCreature *Monster);

#endif // TIBIA_CREATURE_MONSTER_H_
