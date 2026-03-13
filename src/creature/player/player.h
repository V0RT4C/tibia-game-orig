#ifndef TIBIA_CREATURE_PLAYER_H_
#define TIBIA_CREATURE_PLAYER_H_ 1

#include "creature/creature/creature.h"

// TPlayerData
// =============================================================================
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

// TPlayerIndex
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

// TPlayer
// =============================================================================
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

// crplayer.cc API
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

#endif // TIBIA_CREATURE_PLAYER_H_
