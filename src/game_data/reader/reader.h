#ifndef TIBIA_READER_H_
#define TIBIA_READER_H_ 1

#include "common.h"

struct TPlayerData;

typedef void RefreshSectorFunction(int SectorX, int SectorY, int SectorZ,
									const uint8 *Data, int Size);
typedef void SendMailsFunction(TPlayerData *PlayerData);

enum ReaderThreadOrderType: int {
	READER_ORDER_TERMINATE		= 0,
	READER_ORDER_LOADSECTOR		= 1,
	READER_ORDER_LOADCHARACTER	= 2,
};

enum ReaderThreadReplyType: int {
	READER_REPLY_SECTORDATA		= 0,
	READER_REPLY_CHARACTERDATA	= 1,
};

struct ReaderThreadOrder {
	ReaderThreadOrderType OrderType;
	int SectorX;
	int SectorY;
	int SectorZ;
	uint32 CharacterID;
};

struct ReaderThreadReply {
	ReaderThreadReplyType ReplyType;
	int SectorX;
	int SectorY;
	int SectorZ;
	uint8 *Data;
	int Size;
};

void init_reader_buffers(void);
void insert_order(ReaderThreadOrderType OrderType,
		int SectorX, int SectorY, int SectorZ, uint32 CharacterID);
void get_order(ReaderThreadOrder *Order);
void terminate_reader_order(void);
void load_sector_order(int SectorX, int SectorY, int SectorZ);
void load_character_order(uint32 CharacterID);
void process_load_sector_order(int SectorX, int SectorY, int SectorZ);
void process_load_character_order(uint32 CharacterID);
int reader_thread_loop(void *Unused);

void insert_reply(ReaderThreadReplyType ReplyType,
		int SectorX, int SectorY, int SectorZ, uint8 *Data, int Size);
bool get_reply(ReaderThreadReply *Reply);
void sector_reply(int SectorX, int SectorY, int SectorZ, uint8 *Data, int Size);
void character_reply(uint32 CharacterID);
void process_sector_reply(RefreshSectorFunction *refresh_sector,
		int SectorX, int SectorY, int SectorZ, uint8 *Data, int Size);
void process_character_reply(SendMailsFunction *send_mails, uint32 CharacterID);
void process_reader_thread_replies(RefreshSectorFunction *refresh_sector, SendMailsFunction *send_mails);

void init_reader(void);
void exit_reader(void);

#endif //TIBIA_READER_HH
