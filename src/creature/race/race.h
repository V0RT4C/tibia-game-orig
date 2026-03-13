#ifndef TIBIA_CREATURE_RACE_H_
#define TIBIA_CREATURE_RACE_H_ 1

#include "creature/creature/creature.h"

struct ReadScriptFile;
struct WriteScriptFile;

struct TSkillData {
	int Nr;
	int Actual;
	int Minimum;
	int Maximum;
	int NextLevel;
	int FactorPercent;
	int AddLevel;
};

struct TItemData {
	ObjectType Type;
	int Maximum;
	int Probability;
};

struct TSpellData {
	SpellShapeType Shape;
	int ShapeParam1;
	int ShapeParam2;
	int ShapeParam3;
	int ShapeParam4;
	SpellImpactType Impact;
	int ImpactParam1;
	int ImpactParam2;
	int ImpactParam3;
	int ImpactParam4;
	int Delay;
};

struct TRaceData {
	TRaceData(void);

	// DATA
	// =================
	char Name[30];
	char Article[3];
	TOutfit Outfit;
	ObjectType MaleCorpse;
	ObjectType FemaleCorpse;
	BloodType Blood;
	int ExperiencePoints;
	int FleeThreshold;
	int Attack;
	int Defend;
	int Armor;
	int Poison;
	int SummonCost;
	int LoseTarget;
	int Strategy[4];
	bool KickBoxes;
	bool KickCreatures;
	bool SeeInvisible;
	bool Unpushable;
	bool DistanceFighting;
	bool NoSummon;
	bool NoIllusion;
	bool NoConvince;
	bool NoBurning;
	bool NoPoison;
	bool NoEnergy;
	bool NoHit;
	bool NoLifeDrain;
	bool NoParalyze;
	int Skills;
	vector<TSkillData> Skill;
	int Talks;
	vector<uint32> Talk; // POINTER? Probably a reference from `AddDynamicString`?
	int Items;
	vector<TItemData> Item;
	int Spells;
	vector<TSpellData> Spell;
};

// Race API
bool IsRaceValid(int Race);
int GetRaceByName(const char *RaceName);
const char *GetRaceName(int Race);
TOutfit GetRaceOutfit(int Race);
bool GetRaceNoSummon(int Race);
bool GetRaceNoConvince(int Race);
bool GetRaceNoIllusion(int Race);
bool GetRaceNoParalyze(int Race);
int GetRaceSummonCost(int Race);
int GetRacePoison(int Race);
bool GetRaceUnpushable(int Race);
TOutfit ReadOutfit(ReadScriptFile *Script);
void WriteOutfit(WriteScriptFile *Script, TOutfit Outfit);
void LoadRace(const char *FileName);
void LoadRaces(void);

#endif // TIBIA_CREATURE_RACE_H_
