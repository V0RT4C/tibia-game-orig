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

struct TPlayerData {
	uint32 CharacterID;
	pid_t Locked;
	int Sticky;
	bool Dirty;
	int Race;
	TOutfit OriginalOutfit;
	TOutfit CurrentOutfit;
	time_t LastLoginTime;
	time_t LastLogoutTime;
	int startx;
	int starty;
	int startz;
	int posx;
	int posy;
	int posz;
	int Profession;
	int PlayerkillerEnd;
	int Actual[25];
	int Maximum[25];
	int Minimum[25];
	int DeltaAct[25];
	int MagicDeltaAct[25];
	int Cycle[25];
	int MaxCycle[25];
	int Count[25];
	int MaxCount[25];
	int AddLevel[25];
	int Experience[25];
	int FactorPercent[25];
	int NextLevel[25];
	int Delta[25];
	uint8 SpellList[256];
	int QuestValues[500];
	int MurderTimestamps[20];
	uint8 *Inventory;
	int InventorySize;
	uint8 *Depot[MAX_DEPOTS];
	int DepotSize[MAX_DEPOTS];
	uint32 AccountID;
	int Sex;
	char Name[30];
	uint8 Rights[12];
	char Guild[31];
	char Rank[31];
	char Title[31];
	int Buddies;
	uint32 Buddy[100];
	char BuddyName[100][30];
	uint32 EarliestYellRound;
	uint32 EarliestTradeChannelRound;
	uint32 EarliestSpellTime;
	uint32 EarliestMultiuseTime;
	uint32 TalkBufferFullTime;
	uint32 MutingEndRound;
	uint32 Addressees[20];
	uint32 AddresseesTimes[20];
	int NumberOfMutings;
};

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

// TNonPlayer
// =============================================================================
struct TNonplayer: TCreature {
	TNonplayer(void);
	~TNonplayer(void) override;
	void SetInList(void);
	void DelInList(void);

	// DATA
	// =================
	//TCreature super_TCreature;	// IMPLICIT
	STATE State;
};

// TNPC
// =============================================================================
struct TBehaviourNode {
	int Type;
	int Data;
	TBehaviourNode *Left;
	TBehaviourNode *Right;
};

struct TBehaviourCondition {
	bool set(int Type, void *Data);
	void clear(void);

	// DATA
	// =================
	int Type;
	uint32 Text;
	TBehaviourNode *Expression;
	int Property;
	int Number;
};

struct TBehaviourAction {
	bool set(int Type, void *Data, void *Data2, void *Data3, void *Data4);
	void clear(void);

	// DATA
	// =================
	int Type;
	uint32 Text;
	int Number;
	TBehaviourNode *Expression;
	TBehaviourNode *Expression2;
	TBehaviourNode *Expression3;
};

struct TBehaviour {
	TBehaviour(void);
	~TBehaviour(void);
	bool addCondition(int Type, void *Data);
	bool addAction(int Type, void *Data, void *Data2, void *Data3, void *Data4);

	// TODO(fusion): Same as `Channel` in `operate.hh`.
	TBehaviour(const TBehaviour &Other);
	void operator=(const TBehaviour &Other);

	// DATA
	// =================
	vector<TBehaviourCondition> Condition;
	vector<TBehaviourAction> Action;
	int Conditions;
	int Actions;
};

struct TBehaviourDatabase {
	TBehaviourDatabase(ReadScriptFile *Script);

	// TODO(fusion): These could/should be standalone functions.
	TBehaviourNode *readValue(ReadScriptFile *Script);
	TBehaviourNode *readFactor(ReadScriptFile *Script);
	TBehaviourNode *readTerm(ReadScriptFile *Script);
	int evaluate(TNPC *Npc, TBehaviourNode *Node, int *Parameters);

	void react(TNPC *Npc, const char *Text, SITUATION Situation);

	// DATA
	// =================
	vector<TBehaviour> Behaviour;
	int Behaviours;
};

struct TNPC: TNonplayer {
	TNPC(const char *FileName);
	void GiveTo(ObjectType Type, int Amount);
	void GetFrom(ObjectType Type, int Amount);
	void GiveMoney(int Amount);
	void GetMoney(int Amount);
	void Enqueue(uint32 InterlocutorID, const char *Text);
	void TurnToInterlocutor(void);
	void ChangeState(STATE NewState, bool Stimulus);

	// VIRTUAL FUNCTIONS
	// =================
	~TNPC(void) override;
	bool MovePossible(int x, int y, int z, bool Execute, bool Jump) override;
	void TalkStimulus(uint32 SpeakerID, const char *Text) override;
	void DamageStimulus(uint32 AttackerID, int Damage, int DamageType) override;
	void IdleStimulus(void) override;
	void CreatureMoveStimulus(uint32 CreatureID, int Type) override;

	// DATA
	// =================
	//TNonplayer super_TNonplayer; 	// IMPLICIT
	uint32 Interlocutor;
	int Topic;
	int Price;
	int Amount;
	int TypeID;
	uint32 Data;
	uint32 LastTalk;
	vector<uint32> QueuedPlayers;
	vector<uint32> QueuedAddresses;
	int QueueLength;
	TBehaviourDatabase *Behaviour;
};

// TMonster
// =============================================================================
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
	// =================
	~TMonster(void) override;
	bool MovePossible(int x, int y, int z, bool Execute, bool Jump) override;
	bool IsPeaceful(void) override;
	uint32 GetMaster(void) override;
	void DamageStimulus(uint32 AttackerID, int Damage, int DamageType) override;
	void IdleStimulus(void) override;
	void CreatureMoveStimulus(uint32 CreatureID, int Type) override;

	// DATA
	//TNonplayer super_TNonplayer;	// IMPLICIT
	int Home;
	uint32 Master;
	uint32 Target;
};

// TPlayer
// =============================================================================
struct TPlayerIndexNode {
	bool InternalNode;
};

struct TPlayerIndexInternalNode: TPlayerIndexNode {
	TPlayerIndexNode *Child[27];
};

struct TPlayerIndexEntry {
	char Name[30];
	uint32 CharacterID;
};

struct TPlayerIndexLeafNode: TPlayerIndexNode {
	int Count;
	TPlayerIndexEntry Entry[10];
};

struct TPlayer: TCreature {
	TPlayer(TConnection *Connection, uint32 CharacterID);
	void SetInList(void);
	void DelInList(void);
	void ClearRequest(void);
	void ClearConnection(void);
	void LoadData(void);
	void SaveData(void);
	void LoadInventory(bool SetStandardInventory);
	void SaveInventory(void);
	void StartCoordinates(void);
	void TakeOver(TConnection *Connection);
	void SetOpenContainer(int ContainerNr, Object Con);
	Object GetOpenContainer(int ContainerNr);
	void CloseAllContainers(void);
	Object InspectTrade(bool OwnOffer, int Position);
	void AcceptTrade(void);
	void RejectTrade(void);
	void ClearProfession(void);
	void SetProfession(uint8 Profession);
	uint8 GetRealProfession(void);
	uint8 GetEffectiveProfession(void);
	uint8 GetActiveProfession(void);
	bool GetActivePromotion(void);
	bool SpellKnown(int SpellNr);
	void LearnSpell(int SpellNr);
	int GetQuestValue(int QuestNr);
	void SetQuestValue(int QuestNr, int Value);
	void CheckOutfit(void);
	void CheckState(void);
	void AddBuddy(const char *Name);
	void RemoveBuddy(uint32 CharacterID);
	void SendBuddies(void);
	void Regenerate(void);
	bool IsAttacker(uint32 VictimID, bool CheckFormer);
	bool IsAggressor(bool CheckFormer);
	bool IsAttackJustified(uint32 VictimID);
	void RecordAttack(uint32 VictimID);
	void RecordMurder(uint32 VictimID);
	void RecordDeath(uint32 AttackerID, int OldLevel, const char *Remark);
	int CheckPlayerkilling(int Now);
	void ClearAttacker(uint32 VictimID);
	void ClearPlayerkillingMarks(void);
	int GetPlayerkillingMark(TPlayer *Observer);
	uint32 GetPartyLeader(bool CheckFormer);
	bool InPartyWith(TPlayer *Other, bool CheckFormer);
	void JoinParty(uint32 LeaderID);
	void LeaveParty(void);
	int GetPartyMark(TPlayer *Observer);
	int RecordTalk(void);
	int RecordMessage(uint32 AddresseeID);
	int CheckForMuting(void);

	// VIRTUAL FUNCTIONS
	// =================
	~TPlayer(void) override;
	void Death(void) override;
	bool MovePossible(int x, int y, int z, bool Execute, bool Jump) override;
	void DamageStimulus(uint32 AttackerID, int Damage, int DamageType) override;
	void IdleStimulus(void) override;
	void AttackStimulus(uint32 AttackerID) override;

	// DATA
	// =================
	//TCreature super_TCreature;	// IMPLICIT
	uint32 AccountID;
	char Guild[31];
	char Rank[31];
	char Title[31];
	char IPAddress[16];
	uint8 Rights[12];
	Object Depot;
	int DepotNr;
	int DepotSpace;
	RESULT ConstructError;
	TPlayerData *PlayerData;
	Object TradeObject;
	uint32 TradePartner;
	bool TradeAccepted;
	int OldState;
	uint32 Request;
	int RequestTimestamp;
	uint32 RequestProcessingGamemaster;
	int TutorActivities;
	uint8 SpellList[256];
	int QuestValues[500];
	Object OpenContainer[16];
	vector<uint32> AttackedPlayers;
	int NumberOfAttackedPlayers;
	bool Aggressor;
	vector<uint32> FormerAttackedPlayers;
	int NumberOfFormerAttackedPlayers;
	bool FormerAggressor;
	uint32 FormerLogoutRound;
	uint32 PartyLeader;
	uint32 PartyLeavingRound;
	uint32 TalkBufferFullTime;
	uint32 MutingEndRound;
	int NumberOfMutings;
	uint32 Addressees[20];
	uint32 AddresseesTimes[20];
};

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

void LoadMonsterRaid(const char *FileName, int Start,
		bool *Type, int *Date, int *Interval, int *Duration);
void LoadMonsterRaids(void);
void ProcessMonsterRaids(void);

void InitCr(void);
void ExitCr(void);

// crnonpl.cc
// =============================================================================
void StartMonsterhomeTimer(int Nr);
void LoadMonsterhomes(void);
void ProcessMonsterhomes(void);
void NotifyMonsterhomeOfDeath(int Nr);
bool MonsterhomeInRange(int Nr, int x, int y, int z);

void ChangeNPCState(TCreature *Npc, int NewState, bool Stimulus);

TCreature *CreateMonster(int Race, int x, int y, int z,
		int Home, uint32 MasterID, bool ShowEffect);
void ConvinceMonster(TCreature *Master, TCreature *Slave);
void ChallengeMonster(TCreature *Challenger, TCreature *Monster);

void InitNPCs(void);
void InitNonplayer(void);
void ExitNonplayer(void);

// crplayer.cc
// =============================================================================
int GetNumberOfPlayers(void);
TPlayer *GetPlayer(uint32 CharacterID);
TPlayer *GetPlayer(const char *Name);
bool IsPlayerOnline(const char *Name);
int IdentifyPlayer(const char *Name, bool ExactMatch, bool IgnoreGamemasters, TPlayer **OutPlayer);
void LogoutAllPlayers(void);
void CloseProcessedRequests(uint32 CharacterID);
void NotifyBuddies(uint32 CharacterID, const char *Name, bool Login);
void CreatePlayerList(bool Online);
void PrintPlayerPositions(void);
void LoadDepot(TPlayerData *PlayerData, int DepotNr, Object Con);
void SaveDepot(TPlayerData *PlayerData, int DepotNr, Object Con);
void GetProfessionName(char *Buffer, int Profession, bool Article, bool Capitals);
void SendExistingRequests(TConnection *Connection);

void SavePlayerPoolSlot(TPlayerData *Slot);
void FreePlayerPoolSlot(TPlayerData *Slot);
TPlayerData *GetPlayerPoolSlot(uint32 CharacterID);
TPlayerData *AssignPlayerPoolSlot(uint32 CharacterID, bool DontWait);
TPlayerData *AttachPlayerPoolSlot(uint32 CharacterID, bool DontWait);
void AttachPlayerPoolSlot(TPlayerData *Slot, bool DontWait);
void IncreasePlayerPoolSlotSticky(TPlayerData *Slot);
void DecreasePlayerPoolSlotSticky(TPlayerData *Slot);
void DecreasePlayerPoolSlotSticky(uint32 CharacterID);
void ReleasePlayerPoolSlot(TPlayerData *Slot);
void SavePlayerPoolSlots(void);
void InitPlayerPool(void);
void ExitPlayerPool(void);

int GetPlayerIndexEntryNumber(const char *Name, int Position);
void InsertPlayerIndex(TPlayerIndexInternalNode *Node,
		int Position, const char *Name, uint32 CharacterID);
TPlayerIndexEntry *SearchPlayerIndex(const char *Name);
bool PlayerExists(const char *Name);
uint32 GetCharacterID(const char *Name);
const char *GetCharacterName(const char *Name);
void InitPlayerIndex(void);
void ExitPlayerIndex(void);

void InitPlayer(void);
void ExitPlayer(void);

#endif //TIBIA_CREATURE_H_
