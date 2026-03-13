#ifndef TIBIA_CREATURE_MONSTER_H_
#define TIBIA_CREATURE_MONSTER_H_ 1

#include "creature/nonplayer/nonplayer.h"

struct TMonsterhome {
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

struct TMonster: TNonplayer {
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
void StartMonsterhomeTimer(int Nr);
void LoadMonsterhomes(void);
void ProcessMonsterhomes(void);
void NotifyMonsterhomeOfDeath(int Nr);
bool MonsterhomeInRange(int Nr, int x, int y, int z);

// Monster creation API
TCreature *CreateMonster(int Race, int x, int y, int z,
		int Home, uint32 MasterID, bool ShowEffect);
void ConvinceMonster(TCreature *Master, TCreature *Slave);
void ChallengeMonster(TCreature *Challenger, TCreature *Monster);

#endif // TIBIA_CREATURE_MONSTER_H_
