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
struct PlayerIndexNode {
	bool InternalNode;
};

struct PlayerIndexInternalNode : PlayerIndexNode {
	PlayerIndexNode *Child[37]; // 26 letters + 10 digits + 1 catch-all
};

struct PlayerIndexEntry {
	char Name[30];
	uint32 CharacterID;
};

struct PlayerIndexLeafNode : PlayerIndexNode {
	int Count;
	PlayerIndexEntry Entry[10];
};

// TPlayer
// =============================================================================
struct TPlayer : TCreature {
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
	// TCreature super_TCreature;	// IMPLICIT
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
int get_number_of_players(void);
TPlayer *get_player(uint32 CharacterID);
TPlayer *get_player(const char *Name);
bool is_player_online(const char *Name);
int identify_player(const char *Name, bool ExactMatch, bool IgnoreGamemasters, TPlayer **OutPlayer);
void logout_all_players(void);
void close_processed_requests(uint32 CharacterID);
void notify_buddies(uint32 CharacterID, const char *Name, bool Login);
void create_player_list(bool Online);
void print_player_positions(void);
void load_depot(TPlayerData *PlayerData, int DepotNr, Object Con);
void save_depot(TPlayerData *PlayerData, int DepotNr, Object Con);
void get_profession_name(char *Buffer, int Profession, bool Article, bool Capitals);
void send_existing_requests(TConnection *Connection);

void save_player_pool_slot(TPlayerData *Slot);
void free_player_pool_slot(TPlayerData *Slot);
TPlayerData *get_player_pool_slot(uint32 CharacterID);
TPlayerData *assign_player_pool_slot(uint32 CharacterID, bool DontWait);
TPlayerData *attach_player_pool_slot(uint32 CharacterID, bool DontWait);
void attach_player_pool_slot(TPlayerData *Slot, bool DontWait);
void increase_player_pool_slot_sticky(TPlayerData *Slot);
void decrease_player_pool_slot_sticky(TPlayerData *Slot);
void decrease_player_pool_slot_sticky(uint32 CharacterID);
void release_player_pool_slot(TPlayerData *Slot);
void save_player_pool_slots(void);
void init_player_pool(void);
void exit_player_pool(void);

int get_player_index_entry_number(const char *Name, int Position);
void insert_player_index(PlayerIndexInternalNode *Node, int Position, const char *Name, uint32 CharacterID);
PlayerIndexEntry *search_player_index(const char *Name);
bool player_exists(const char *Name);
uint32 get_character_id(const char *Name);
const char *get_character_name(const char *Name);
void init_player_index(void);
void exit_player_index(void);

void init_player(void);
void exit_player(void);

#endif // TIBIA_CREATURE_PLAYER_H_
