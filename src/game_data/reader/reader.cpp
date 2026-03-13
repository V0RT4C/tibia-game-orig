#include "game_data/reader/reader.h"
#include "config.h"
#include "cr.h"
#include "map.h"
#include "threads.h"

static ThreadHandle ReaderThread;

static ReaderThreadOrder OrderBuffer[200];
static int OrderPointerWrite;
static int OrderPointerRead;
static Semaphore OrderBufferEmpty(NARRAY(OrderBuffer));
static Semaphore OrderBufferFull(0);

static ReaderThreadReply ReplyBuffer[200];
static int ReplyPointerWrite;
static int ReplyPointerRead;

static TDynamicWriteBuffer HelpBuffer(kb(64));

// Reader Orders
// =============================================================================
void init_reader_buffers(void) {
	OrderPointerWrite = 0;
	OrderPointerRead = 0;
	ReplyPointerWrite = 0;
	ReplyPointerRead = 0;
}

void insert_order(ReaderThreadOrderType OrderType, int SectorX, int SectorY, int SectorZ, uint32 CharacterID) {
	int Orders = (OrderPointerWrite - OrderPointerRead);
	if (Orders >= NARRAY(OrderBuffer)) {
		error("insert_order (Reader): Order buffer is full => needs resizing.\n");
	}

	OrderBufferEmpty.down();
	int WritePos = OrderPointerWrite % NARRAY(OrderBuffer);
	OrderBuffer[WritePos].OrderType = OrderType;
	OrderBuffer[WritePos].SectorX = SectorX;
	OrderBuffer[WritePos].SectorY = SectorY;
	OrderBuffer[WritePos].SectorZ = SectorZ;
	OrderBuffer[WritePos].CharacterID = CharacterID;
	OrderPointerWrite += 1;
	OrderBufferFull.up();
}

void get_order(ReaderThreadOrder *Order) {
	OrderBufferFull.down();
	*Order = OrderBuffer[OrderPointerRead % NARRAY(OrderBuffer)];
	OrderPointerRead += 1;
	OrderBufferEmpty.up();
}

void terminate_reader_order(void) {
	insert_order(READER_ORDER_TERMINATE, 0, 0, 0, 0);
}

void load_sector_order(int SectorX, int SectorY, int SectorZ) {
	insert_order(READER_ORDER_LOADSECTOR, SectorX, SectorY, SectorZ, 0);
}

void load_character_order(uint32 CharacterID) {
	insert_order(READER_ORDER_LOADCHARACTER, 0, 0, 0, CharacterID);
}

void process_load_sector_order(int SectorX, int SectorY, int SectorZ) {
	// TODO(fusion): We parsed sector files way too many times now. And there
	// is also a drop in loader quality.
	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/%04d-%04d-%02d.sec", ORIGMAPPATH, SectorX, SectorY, SectorZ);
	if (!FileExists(FileName)) {
		return;
	}

	int OffsetX = -1;
	int OffsetY = -1;
	bool Refreshable = false;
	HelpBuffer.Position = 0;

	ReadScriptFile Script;
	Script.open(FileName);
	while (true) {
		Script.next_token();
		if (Script.Token == ENDOFFILE) {
			Script.close();
			break;
		}

		if (Script.Token == SPECIAL && Script.get_special() == ',') {
			continue;
		}

		if (Script.Token == BYTES) {
			uint8 *Offset = Script.get_bytesequence();
			OffsetX = (int)Offset[0];
			OffsetY = (int)Offset[1];
			Refreshable = false;
			Script.read_symbol(':');
		} else if (Script.Token == IDENTIFIER) {
			if (OffsetX == -1 || OffsetY == -1) {
				Script.error("coordinate expected");
			}

			const char *Identifier = Script.get_identifier();
			if (strcmp(Identifier, "refresh") == 0) {
				Refreshable = true;
			} else if (strcmp(Identifier, "content") == 0) {
				Script.read_symbol('=');

				if (Refreshable) {
					HelpBuffer.writeByte((uint8)OffsetX);
					HelpBuffer.writeByte((uint8)OffsetY);
				}

				load_objects(&Script, &HelpBuffer, !Refreshable);
			}
		}
	}

	int Size = HelpBuffer.Position;
	if (Size > 0) {
		uint8 *Data = new uint8[Size];
		memcpy(Data, HelpBuffer.Data, Size);
		sector_reply(SectorX, SectorY, SectorZ, Data, Size);
	}
}

void process_load_character_order(uint32 CharacterID) {
	while (true) {
		TPlayerData *Slot = assign_player_pool_slot(CharacterID, true);
		if (Slot == NULL) {
			error("process_load_character_order: Cannot assign a slot for player data.\n");
			break;
		}

		if (Slot->Locked == GetGameThreadID()) {
			break;
		}

		if (Slot->Locked == gettid()) {
			increase_player_pool_slot_sticky(Slot);
			release_player_pool_slot(Slot);
			character_reply(CharacterID);
			break;
		}

		DelayThread(1, 0);
	}
}

int reader_thread_loop(void *Unused) {
	ReaderThreadOrder Order = {};
	while (true) {
		get_order(&Order);
		if (Order.OrderType == READER_ORDER_TERMINATE) {
			break;
		}

		switch (Order.OrderType) {
		case READER_ORDER_LOADSECTOR: {
			process_load_sector_order(Order.SectorX, Order.SectorY, Order.SectorZ);
			break;
		}

		case READER_ORDER_LOADCHARACTER: {
			process_load_character_order(Order.CharacterID);
			break;
		}

		default: {
			error("reader_thread_loop: Unknown command %d.\n", Order.OrderType);
			break;
		}
		}
	}

	return 0;
}

// Reader Replies
// =============================================================================
void insert_reply(ReaderThreadReplyType ReplyType, int SectorX, int SectorY, int SectorZ, uint8 *Data, int Size) {
	int Replies = (ReplyPointerWrite - ReplyPointerRead);
	while (Replies > NARRAY(ReplyBuffer)) {
		error("insert_reply (Reader): Buffer is full; waiting...\n");
		DelayThread(5, 0);
	}

	int WritePos = ReplyPointerWrite % NARRAY(ReplyBuffer);
	ReplyBuffer[WritePos].ReplyType = ReplyType;
	ReplyBuffer[WritePos].SectorX = SectorX;
	ReplyBuffer[WritePos].SectorY = SectorY;
	ReplyBuffer[WritePos].SectorZ = SectorZ;
	ReplyBuffer[WritePos].Data = Data;
	ReplyBuffer[WritePos].Size = Size;
	ReplyPointerWrite += 1;
}

bool get_reply(ReaderThreadReply *Reply) {
	bool Result = (ReplyPointerRead < ReplyPointerWrite);
	if (Result) {
		*Reply = ReplyBuffer[ReplyPointerRead % NARRAY(ReplyBuffer)];
		ReplyPointerRead += 1;
	}
	return Result;
}

void sector_reply(int SectorX, int SectorY, int SectorZ, uint8 *Data, int Size) {
	insert_reply(READER_REPLY_SECTORDATA, SectorX, SectorY, SectorZ, Data, Size);
}

void character_reply(uint32 CharacterID) {
	insert_reply(READER_REPLY_CHARACTERDATA, 0, 0, 0, NULL, (int)CharacterID);
}

void process_sector_reply(RefreshSectorFunction *refresh_sector, int SectorX, int SectorY, int SectorZ, uint8 *Data,
						  int Size) {
	refresh_sector(SectorX, SectorY, SectorZ, Data, Size);
	delete[] Data;
}

void process_character_reply(SendMailsFunction *send_mails, uint32 CharacterID) {
	TPlayerData *Slot = attach_player_pool_slot(CharacterID, true);
	if (Slot == NULL) {
		decrease_player_pool_slot_sticky(Slot);
		return;
	}

	send_mails(Slot);
	decrease_player_pool_slot_sticky(Slot);
	release_player_pool_slot(Slot);
}

void process_reader_thread_replies(RefreshSectorFunction *refresh_sector, SendMailsFunction *send_mails) {
	ReaderThreadReply Reply = {};
	while (get_reply(&Reply)) {
		switch (Reply.ReplyType) {
		case READER_REPLY_SECTORDATA: {
			process_sector_reply(refresh_sector, Reply.SectorX, Reply.SectorY, Reply.SectorZ, Reply.Data, Reply.Size);
			break;
		}

		case READER_REPLY_CHARACTERDATA: {
			process_character_reply(send_mails, (uint32)Reply.Size);
			break;
		}

		default: {
			error("process_reader_thread_replies: Unknown reply type %d.\n", Reply.ReplyType);
			break;
		}
		}
	}
}

// Initialization
// =============================================================================
void init_reader(void) {
	init_reader_buffers();
	ReaderThread = StartThread(reader_thread_loop, NULL, false);
	if (ReaderThread == INVALID_THREAD_HANDLE) {
		throw "cannot start reader thread";
	}
}

void exit_reader(void) {
	if (ReaderThread != INVALID_THREAD_HANDLE) {
		terminate_reader_order();
		JoinThread(ReaderThread);
		ReaderThread = INVALID_THREAD_HANDLE;
	}
}
