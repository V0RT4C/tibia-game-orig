#ifndef TIBIA_CREATURE_COMBAT_H_
#define TIBIA_CREATURE_COMBAT_H_ 1

#include "common.h"
#include "enums.h"
#include "map.h"

struct TCreature;

// TCombat
// =============================================================================
struct CombatEntry{
	uint32 ID;
	uint32 Damage;
	uint32 TimeStamp;
};

struct TCombat{
	TCombat(void);
	void GetWeapon(void);
	void GetAmmo(void);
	void CheckCombatValues(void);
	void GetAttackValue(int *Value, int *SkillNr);
	void GetDefendValue(int *Value, int *SkillNr);
	int GetAttackDamage(void);
	int GetDefendDamage(void);
	int GetArmorStrength(void);
	int GetDistance(void);
	void ActivateLearning(void);
	void SetAttackMode(uint8 AttackMode);
	void SetChaseMode(uint8 ChaseMode);
	void SetSecureMode(uint8 SecureMode);
	void SetAttackDest(uint32 TargetID, bool Follow);
	void CanToDoAttack(void);
	void StopAttack(int Delay);
	void DelayAttack(int Milliseconds);
	void Attack(void);
	void CloseAttack(TCreature *Target);
	void DistanceAttack(TCreature *Target);
	void WandAttack(TCreature *Target);
	void AddDamageToCombatList(uint32 Attacker, uint32 Damage);
	uint32 GetDamageByCreature(uint32 CreatureID);
	uint32 GetMostDangerousAttacker(void);
	void DistributeExperiencePoints(uint32 Exp);


	// DATA
	// =================
	TCreature *Master;
	uint32 EarliestAttackTime;
	uint32 EarliestDefendTime;
	uint32 LastDefendTime;
	uint32 LatestAttackTime;
	uint8 AttackMode;
	uint8 ChaseMode;
	uint8 SecureMode;
	uint32 AttackDest;
	bool Following;
	Object Shield;
	Object Close;
	Object Missile;
	Object Throw;
	Object Wand;
	Object Ammo;
	bool Fist;
	uint32 CombatDamage;
	int ActCombatEntry;
	CombatEntry CombatList[20];
	int LearningPoints;
};

#endif //TIBIA_CREATURE_COMBAT_H_
