#ifndef TIBIA_CREATURE_RACE_H_
#define TIBIA_CREATURE_RACE_H_ 1

#include "creature/creature/creature.h"

struct ReadScriptFile;
struct WriteScriptFile;

struct SkillData {
	int Nr;
	int Actual;
	int Minimum;
	int Maximum;
	int NextLevel;
	int FactorPercent;
	int AddLevel;
};

struct ItemData {
	ObjectType Type;
	int Maximum;
	int Probability;
};

struct SpellData {
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

struct RaceData {
	RaceData(void);

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
	vector<SkillData> Skill;
	int Talks;
	vector<uint32> Talk; // POINTER? Probably a reference from `AddDynamicString`?
	int Items;
	vector<ItemData> Item;
	int Spells;
	vector<SpellData> Spell;
};

// Race API
bool is_race_valid(int Race);
int get_race_by_name(const char *RaceName);
const char *get_race_name(int Race);
TOutfit get_race_outfit(int Race);
bool get_race_no_summon(int Race);
bool get_race_no_convince(int Race);
bool get_race_no_illusion(int Race);
bool get_race_no_paralyze(int Race);
int get_race_summon_cost(int Race);
int get_race_poison(int Race);
bool get_race_unpushable(int Race);
TOutfit read_outfit(ReadScriptFile *Script);
void write_outfit(WriteScriptFile *Script, TOutfit Outfit);
void load_race(const char *FileName);
void load_races(void);

#endif // TIBIA_CREATURE_RACE_H_
