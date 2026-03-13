#ifndef TIBIA_CREATURE_CREATURE_H_
#define TIBIA_CREATURE_CREATURE_H_ 1

#include "common.h"
#include "connections.h"
#include "containers.h"
#include "enums.h"
#include "map.h"
#include "creature/skill/skill.h"
#include "creature/combat/combat.h"

// Creature Data
// =============================================================================
struct TOutfit{
	int OutfitID;
	union{
		int ObjectType;
		uint8 Colors[4];
	};

	constexpr bool operator==(const TOutfit &Other) const {
		// IMPORTANT(fusion): We don't need to have special comparisson cases
		// if all members of the union have the same size. This is not true
		// otherwise when smaller active fields leave trailing bytes of the
		// union filled with whatever data was there before.
		static_assert(sizeof(this->ObjectType) == sizeof(this->Colors));
		return this->OutfitID == Other.OutfitID
			&& this->ObjectType == Other.ObjectType;
	}

	static constexpr TOutfit Invisible(void){
		return TOutfit{};
	}
};

// FindCreatures
// =============================================================================
enum : int {
	FIND_PLAYERS	= 0x01,
	FIND_NPCS		= 0x02,
	FIND_MONSTERS	= 0x04,
	FIND_ALL		= FIND_PLAYERS | FIND_NPCS | FIND_MONSTERS,
};

struct FindCreatures {
	FindCreatures(int RadiusX, int RadiusY, int CenterX, int CenterY, int Mask);
	FindCreatures(int RadiusX, int RadiusY, uint32 CreatureID, int Mask);
	FindCreatures(int RadiusX, int RadiusY, Object Obj, int Mask);
	void initSearch(int RadiusX, int RadiusY, int CenterX, int CenterY, int Mask);
	uint32 getNext(void);

	// DATA
	// =================
	int startx;
	int starty;
	int endx;
	int endy;
	int blockx;
	int blocky;
	uint32 ActID;
	uint32 SkipID;
	int Mask;
	bool finished;
};

// TCreature
// =============================================================================
enum : int {
	LOSE_INVENTORY_NONE		= 0,
	LOSE_INVENTORY_SOME		= 1,
	LOSE_INVENTORY_ALL		= 2,
};

struct ToDoEntry {
	ToDoType Code;
	union{
		struct{
			uint32 Time;
		} Wait;

		struct{
			int x;
			int y;
			int z;
		} Go;

		struct{
			int Direction;
		} Rotate;

		struct{
			uint32 Obj;
			int x;
			int y;
			int z;
			int Count;
		} Move;

		struct{
			uint32 Obj;
			uint32 Partner;
		} Trade;

		struct{
			uint32 Obj1;
			uint32 Obj2;
			int Dummy;
		} Use;

		struct{
			uint32 Obj;
		} Turn;

		struct{
			uint32 Text;
			int Mode;
			uint32 Addressee;
			bool CheckSpamming;
		} Talk;

		struct{
			int NewState;
		} ChangeState;
	};
};

struct TCreature: SkillBase {
	// crmain.cc
	TCreature(void);
	void SetID(uint32 CharacterID);
	void DelID(void);
	void SetInCrList(void);
	void DelInCrList(void);
	void StartLogout(bool Force, bool StopFight);
	int LogoutPossible(void);
	void BlockLogout(int Delay, bool BlockProtectionZone);
	int GetHealth(void);
	int GetSpeed(void);
	int Damage(TCreature *Attacker, int Damage, int DamageType);

	// cract.cc
	bool SetOnMap(void);
	bool DelOnMap(void);
	void Go(int DestX, int DestY, int DestZ);
	void Rotate(int Direction);
	void Rotate(TCreature *Target);
	void Move(Object Obj, int DestX, int DestY, int DestZ, uint8 Count);
	void Trade(Object Obj, uint32 TradePartner);
	void Use(Object Obj1, Object Obj2, uint8 Dummy);
	void Turn(Object Obj);
	void Attack(void);
	void Execute(void);
	uint32 CalculateDelay(void);
	bool ToDoClear(void);
	void ToDoAdd(ToDoEntry TD);
	void ToDoStop(void);
	void ToDoStart(void);
	void ToDoYield(void);
	void ToDoWait(int Delay);
	void ToDoWaitUntil(uint32 Time);
	void ToDoGo(int DestX, int DestY, int DestZ, bool MustReach, int MaxSteps);
	void ToDoRotate(int Direction);
	void ToDoMove(int ObjX, int ObjY, int ObjZ, ObjectType Type, uint8 RNum,
				int DestX, int DestY, int DestZ, uint8 Count);
	void ToDoMove(Object Obj, int DestX, int DestY, int DestZ, uint8 Count);
	void ToDoTrade(int ObjX, int ObjY, int ObjZ, ObjectType Type, uint8 RNum,
				uint32 TradePartner);
	void ToDoUse(uint8 Count, int ObjX1, int ObjY1, int ObjZ1, ObjectType Type1, uint8 RNum1,
				uint8 Dummy, int ObjX2, int ObjY2, int ObjZ2, ObjectType Type2, uint8 RNum2);
	void ToDoUse(uint8 Count, Object Obj1, Object Obj2);
	void ToDoTurn(int ObjX, int ObjY, int ObjZ, ObjectType Type, uint8 RNum);
	void ToDoAttack(void);
	void ToDoTalk(int Mode, const char *Addressee, const char *Text, bool CheckSpamming);
	void ToDoChangeState(int NewState);
	void NotifyGo(void);
	void NotifyTurn(Object DestCon);
	void NotifyCreate(void);
	void NotifyDelete(void);
	void NotifyChangeInventory(void);

	void Kill(void){
		this->Skills[SKILL_HITPOINTS]->Set(0);
		this->Death();
	}

	bool IsInvisible(void){
		return this->Outfit == TOutfit::Invisible();
	}

	bool CanSeeFloor(int FloorZ){
		if(this->posz <= 7){
			return FloorZ <= 7;
		}else{
			return std::abs(this->posz - FloorZ) <= 2;
		}
	}

	// VIRTUAL FUNCTIONS
	// =================
	virtual ~TCreature(void);															// VTABLE[ 0]
	// Duplicate destructor that also calls operator delete.							// VTABLE[ 1]
	virtual void Death(void);															// VTABLE[ 2]
	virtual bool MovePossible(int x, int y, int z, bool Execute, bool Jump);			// VTABLE[ 3]
	virtual bool IsPeaceful(void);														// VTABLE[ 4]
	virtual uint32 GetMaster(void);														// VTABLE[ 5]
	virtual void TalkStimulus(uint32 SpeakerID, const char *Text);						// VTABLE[ 6]
	virtual void DamageStimulus(uint32 AttackerID, int Damage, int DamageType);			// VTABLE[ 7]
	virtual void IdleStimulus(void);													// VTABLE[ 8]
	virtual void CreatureMoveStimulus(uint32 CreatureID, int Type);						// VTABLE[ 9]
	virtual void AttackStimulus(uint32 AttackerID);										// VTABLE[10]

	// DATA
	// =================
	//void *VTABLE;					// IMPLICIT
	//SkillBase super_TSkillBase;	// IMPLICIT
	TCombat Combat;
	uint32 ID;
	TCreature *NextHashEntry;
	uint32 NextChainCreature;
	char Name[31];
	char Murderer[31];
	TOutfit OrgOutfit;
	TOutfit Outfit;
	int startx;
	int starty;
	int startz;
	int posx;
	int posy;
	int posz;
	int Sex;
	int Race;
	int Direction;
	int Radius;
	CreatureType Type;
	bool IsDead;
	int LoseInventory;
	bool LoggingOut;
	bool LogoutAllowed;
	uint32 EarliestLogoutRound;
	uint32 EarliestProtectionZoneRound;
	uint32 EarliestYellRound;
	uint32 EarliestTradeChannelRound;
	uint32 EarliestSpellTime;
	uint32 EarliestMultiuseTime;
	uint32 EarliestWalkTime;
	uint32 LifeEndRound;
	TKnownCreature *FirstKnowingConnection;
	int SummonedCreatures;
	uint32 FireDamageOrigin;
	uint32 PoisonDamageOrigin;
	uint32 EnergyDamageOrigin;
	Object CrObject;
	vector<ToDoEntry> ToDoList;
	int ActToDo;
	int NrToDo;
	uint32 NextWakeup;
	bool Stop;
	bool LockToDo;
	uint8 Profession;
	TConnection *Connection;
};

bool is_creature_player(uint32 CreatureID);
TCreature *get_creature(uint32 CreatureID);
TCreature *get_creature(Object Obj);

#endif // TIBIA_CREATURE_CREATURE_H_
