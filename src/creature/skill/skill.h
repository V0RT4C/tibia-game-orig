#ifndef TIBIA_CREATURE_SKILL_H_
#define TIBIA_CREATURE_SKILL_H_ 1

#include "common.h"

struct TCreature;

// SkillBase
// =============================================================================
struct TSkill{
	TSkill(int SkNr, TCreature *Master);
	int Get(void);
	int GetProgress(void);
	void Check(void);
	void Change(int Amount);
	void SetMax(void);
	void DecreasePercent(int Percent);
	void SetMDAct(int MDAct);
	void Load(int Act, int Max, int Min, int DAct, int MDAct,
			int Cycle, int MaxCycle, int Count, int MaxCount, int AddLevel,
			int Exp, int FactorPercent, int NextLevel, int Delta);
	void Save(int *Act, int *Max, int *Min, int *DAct, int *MDAct,
			int *Cycle, int *MaxCycle, int *Count, int *MaxCount, int *AddLevel,
			int *Exp, int *FactorPercent, int *NextLevel, int *Delta);

	// VIRTUAL FUNCTIONS
	// =================
	virtual ~TSkill(void);																// VTABLE[ 0]
	// Duplicate destructor that also calls operator delete.							// VTABLE[ 1]
	virtual void Set(int Value);														// VTABLE[ 2]
	virtual void Increase(int Amount);													// VTABLE[ 3]
	virtual void Decrease(int Amount);													// VTABLE[ 4]
	virtual int GetExpForLevel(int Level);												// VTABLE[ 5]
	virtual void Advance(int Range);													// VTABLE[ 6]
	virtual void ChangeSkill(int FactorPercent, int Delta);								// VTABLE[ 7]
	virtual int ProbeValue(int Max, bool Increase);										// VTABLE[ 8]
	virtual bool Probe(int Diff, int Prob, bool Increase);								// VTABLE[ 9]
	virtual bool Process(void);															// VTABLE[10]
	virtual bool SetTimer(int Cycle, int Count, int MaxCount, int AdditionalValue);		// VTABLE[11]
	virtual bool DelTimer(void);														// VTABLE[12]
	virtual int TimerValue(void);														// VTABLE[13]
	virtual bool Jump(int Range);														// VTABLE[14]
	virtual void Event(int Range);														// VTABLE[15]
	virtual void Reset(void);															// VTABLE[16]

	// DATA
	// =================
	//void *VTABLE;		// IMPLICIT
	int DAct;			// Delta Value - Probably from equipment.
	int MDAct;			// Delta Magic Value - Probably from spells.
	uint16 SkNr;
	TCreature *Master;
	int Act;			// Actual Value (?)
	int Max;
	int Min;
	int FactorPercent;
	int LastLevel;
	int NextLevel;
	int Delta;
	int Exp;
	int Cycle;
	int MaxCycle;
	int Count;
	int MaxCount;
	int AddLevel;
};

struct SkillLevel: TSkill {
	SkillLevel(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	void Increase(int Amount) override;
	void Decrease(int Amount) override;
	int GetExpForLevel(int Level) override;
	bool Jump(int Range) override;
};

struct SkillProbe: TSkill {
	SkillProbe(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	void Increase(int Amount) override;
	void Decrease(int Amount) override;
	int GetExpForLevel(int Level) override;
	void ChangeSkill(int FactorPercent, int Delta) override;
	int ProbeValue(int Max, bool Increase) override;
	bool Probe(int Diff, int Prob, bool Increase) override;
	bool SetTimer(int Cycle, int Count, int MaxCount, int AdditionalValue) override;
	bool Jump(int Range) override;
	void Event(int Range) override;
};

struct SkillAdd: TSkill {
	SkillAdd(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	void Advance(int Range) override;
};

struct SkillHitpoints: SkillAdd {
	SkillHitpoints(int SkNr, TCreature *Master) : SkillAdd(SkNr, Master) {}
	void Set(int Value) override;
};

struct SkillMana: SkillAdd {
	SkillMana(int SkNr, TCreature *Master) : SkillAdd(SkNr, Master) {}
	void Set(int Value) override;
};

struct SkillGoStrength: SkillAdd {
	SkillGoStrength(int SkNr, TCreature *Master) : SkillAdd(SkNr, Master) {}
	bool SetTimer(int Cycle, int Count, int MaxCount, int AdditionalValue) override;
	void Event(int Range) override;
};

struct SkillCarryStrength: SkillAdd {
	SkillCarryStrength(int SkNr, TCreature *Master) : SkillAdd(SkNr, Master) {}
	void Set(int Value) override;
};

struct SkillSoulpoints: SkillAdd {
	SkillSoulpoints(int SkNr, TCreature *Master) : SkillAdd(SkNr, Master) {}
	void Set(int Value) override;
	int TimerValue(void) override;
	void Event(int Range) override;
};

struct SkillFed: TSkill {
	SkillFed(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	void Event(int Range) override;
};

struct SkillLight: TSkill {
	SkillLight(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	bool SetTimer(int Cycle, int Count, int MaxCount, int AdditionalValue) override;
	void Event(int Range) override;
};

struct SkillIllusion: TSkill {
	SkillIllusion(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	bool SetTimer(int Cycle, int Count, int MaxCount, int AdditionalValue) override;
	void Event(int Range) override;
};

struct SkillPoison: TSkill {
	SkillPoison(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	bool Process(void) override;
	bool SetTimer(int Cycle, int Count, int MaxCount, int AdditionalValue) override;
	void Event(int Range) override;
	void Reset(void) override;
};

struct SkillBurning: TSkill {
	SkillBurning(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	void Event(int Range) override;
};

struct SkillEnergy: TSkill {
	SkillEnergy(int SkNr, TCreature *Master) : TSkill(SkNr, Master) {}
	void Event(int Range) override;
};

struct SkillBase{
	SkillBase(void);
	~SkillBase(void);
	bool NewSkill(uint16 SkillNo, TCreature *Creature);
	bool SetSkills(int Race);
	void ProcessSkills(void);
	bool SetTimer(uint16 SkNr, int Cycle, int Count, int MaxCount, int AdditionalValue);
	void DelTimer(uint16 SkNr);

	// DATA
	// =================
	TSkill *Skills[25];
	TSkill *TimerList[25];
	uint16 FirstFreeTimer;
};

// API declarations
int get_skill_by_name(const char *Name);
void init_crskill(void);
void exit_crskill(void);

#endif //TIBIA_CREATURE_SKILL_H_
