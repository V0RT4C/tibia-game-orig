#ifndef TIBIA_WRITER_H_
#define TIBIA_WRITER_H_ 1

#include "common.h"
#include "containers.h"

struct TCreature;
struct TPlayer;
struct ReportedStatement;

enum WriterThreadOrderType : int {
	WRITER_ORDER_TERMINATE = 0,
	WRITER_ORDER_LOGOUT = 1,
	WRITER_ORDER_PLAYERLIST = 2,
	WRITER_ORDER_KILLSTATISTICS = 3,
	WRITER_ORDER_PUNISHMENT = 4,
	WRITER_ORDER_CHARACTERDEATH = 5,
	WRITER_ORDER_ADDBUDDY = 6,
	WRITER_ORDER_REMOVEBUDDY = 7,
	WRITER_ORDER_DECREMENTISONLINE = 8,
	WRITER_ORDER_SAVEPLAYERDATA = 9,
};

enum WriterThreadReplyType : int {
	WRITER_REPLY_BROADCAST = 0,
	WRITER_REPLY_DIRECT = 1,
	WRITER_REPLY_LOGOUT = 2,
};

struct ProtocolThreadOrder {
	char ProtocolName[20];
	char Text[256];
};

struct WriterThreadOrder {
	WriterThreadOrderType OrderType;
	const void *Data;
};

struct LogoutOrderData {
	uint32 CharacterID;
	int Level;
	int Profession;
	time_t LastLoginTime;
	int TutorActivities;
	char Residence[30];
};

struct PlayerlistOrderData {
	int NumberOfPlayers;
	const char *PlayerNames;
	int *PlayerLevels;
	int *PlayerProfessions;
};

struct KillStatisticsOrderData {
	int NumberOfRaces;
	const char *RaceNames;
	int *KilledPlayers;
	int *KilledCreatures;
};

struct PunishmentOrderData {
	uint32 GamemasterID;
	char GamemasterName[30];
	char CriminalName[30];
	char CriminalIPAddress[16];
	int Reason;
	int Action;
	char Comment[200];
	int NumberOfStatements;
	vector<ReportedStatement> *ReportedStatements;
	uint32 StatementID;
	bool ip_banishment;
};

struct CharacterDeathOrderData {
	uint32 CharacterID;
	int Level;
	uint32 Offender;
	char Remark[30];
	bool Unjustified;
	time_t Time;
};

struct BuddyOrderData {
	uint32 AccountID;
	uint32 Buddy;
};

struct WriterThreadReply {
	WriterThreadReplyType ReplyType;
	const void *Data;
};

struct BroadcastReplyData {
	char Message[100];
};

struct DirectReplyData {
	uint32 CharacterID;
	char Message[100];
};

void init_protocol(void);
void insert_protocol_order(const char *ProtocolName, const char *Text);
void get_protocol_order(ProtocolThreadOrder *Order);
void write_protocol(const char *ProtocolName, const char *Text);
int protocol_thread_loop(void *Unused);
void init_log(const char *ProtocolName);
void log_message(const char *ProtocolName, const char *Text, ...) ATTR_PRINTF(2, 3);

void init_writer_buffers(void);
int get_order_buffer_space(void);
void insert_order(WriterThreadOrderType OrderType, const void *Data);
void get_order(WriterThreadOrder *Order);
void terminate_writer_order(void);
void logout_order(TPlayer *Player);
void playerlist_order(int NumberOfPlayers, const char *PlayerNames, int *PlayerLevels, int *PlayerProfessions);
void kill_statistics_order(int NumberOfRaces, const char *RaceNames, int *KilledPlayers, int *KilledCreatures);
void punishment_order(TCreature *Gamemaster, const char *Name, const char *IPAddress, int Reason, int Action,
					  const char *Comment, int NumberOfStatements, vector<ReportedStatement> *ReportedStatements,
					  uint32 StatementID, bool ip_banishment);
void character_death_order(TCreature *Creature, int OldLevel, uint32 OffenderID, const char *Remark, bool Unjustified);
void add_buddy_order(TCreature *Creature, uint32 BuddyID);
void remove_buddy_order(TCreature *Creature, uint32 BuddyID);
void decrement_is_online_order(uint32 CharacterID);
void save_player_data_order(void);
void process_logout_order(LogoutOrderData *Data);
void process_playerlist_order(PlayerlistOrderData *Data);
void process_kill_statistics_order(KillStatisticsOrderData *Data);
void process_punishment_order(PunishmentOrderData *Data);
void process_character_death_order(CharacterDeathOrderData *Data);
void process_add_buddy_order(BuddyOrderData *Data);
void process_remove_buddy_order(BuddyOrderData *Data);
void process_decrement_is_online_order(uint32 CharacterID);
int writer_thread_loop(void *Unused);

void insert_reply(WriterThreadReplyType ReplyType, const void *Data);
void broadcast_reply(const char *Text, ...) ATTR_PRINTF(1, 2);
void direct_reply(uint32 CharacterID, const char *Text, ...) ATTR_PRINTF(2, 3);
void logout_reply(const char *PlayerName);
bool get_reply(WriterThreadReply *Reply);
void process_broadcast_reply(BroadcastReplyData *Data);
void process_direct_reply(DirectReplyData *Data);
void process_logout_reply(const char *Name);
void process_writer_thread_replies(void);

void clear_players(void);
void init_writer(void);
void abort_writer(void);
void exit_writer(void);

#endif // TIBIA_WRITER_H_
