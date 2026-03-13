#include "game_data/writer/writer.h"
#include "game_data/operate/operate.h"
#include "config.h"
#include "cr.h"
#include "info.h"
#include "query.h"
#include "threads.h"

static ThreadHandle ProtocolThread;
static ThreadHandle WriterThread;

static ProtocolThreadOrder ProtocolBuffer[1000];
static int ProtocolPointerWrite;
static int ProtocolPointerRead;
static Semaphore ProtocolMutex(1);
static Semaphore ProtocolBufferEmpty(NARRAY(ProtocolBuffer));
static Semaphore ProtocolBufferFull(0);

static WriterThreadOrder OrderBuffer[2000];
static int OrderPointerWrite;
static int OrderPointerRead;
static Semaphore OrderBufferEmpty(NARRAY(OrderBuffer));
static Semaphore OrderBufferFull(0);

static WriterThreadReply ReplyBuffer[100];
static int ReplyPointerWrite;
static int ReplyPointerRead;

static TQueryManagerConnection *QueryManagerConnection;

// Protocol Orders
// =============================================================================
void init_protocol(void){
	ProtocolPointerWrite = 0;
	ProtocolPointerRead = 0;
}

void insert_protocol_order(const char *ProtocolName, const char *Text){
	if(ProtocolName == NULL){
		error("insert_protocol_order: Protocol name not specified.\n");
		return;
	}

	if(Text == NULL){
		error("insert_protocol_order: Text not specified.\n");
		return;
	}

	int Orders = (ProtocolPointerWrite - ProtocolPointerRead);
	if(Orders >= NARRAY(ProtocolBuffer) && strcmp(ProtocolName, "error") != 0){
		error("insert_protocol_order: Protocol buffer is full => increase size.\n");
	}

	ProtocolMutex.down();
	ProtocolBufferEmpty.down();
	int WritePos = ProtocolPointerWrite % NARRAY(ProtocolBuffer);
	strcpy(ProtocolBuffer[WritePos].ProtocolName, ProtocolName);
	strcpy(ProtocolBuffer[WritePos].Text, Text);
	ProtocolPointerWrite += 1;
	ProtocolBufferFull.up();
	ProtocolMutex.up();
}

void get_protocol_order(ProtocolThreadOrder *Order){
	ProtocolBufferFull.down();
	int ReadPos = ProtocolPointerRead % NARRAY(ProtocolBuffer);
	*Order = ProtocolBuffer[ReadPos];
	ProtocolPointerRead += 1;
	ProtocolBufferEmpty.up();
}

void write_protocol(const char *ProtocolName, const char *Text){
	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/%s.log", LOGPATH, ProtocolName);

	FILE *File = fopen(FileName, "at");
	if(File != NULL){
		fprintf(File, "%s", Text);
		if(fclose(File) != 0){
			error("write_protocol: Error %d closing the file.\n", errno);
		}
	}
}

int protocol_thread_loop(void *Unused){
	ProtocolThreadOrder Order = {};
	while(true){
		get_protocol_order(&Order);
		if(Order.ProtocolName[0] == 0){
			break;
		}

		write_protocol(Order.ProtocolName, Order.Text);
	}
	return 0;
}

void init_log(const char *ProtocolName){
	if(ProtocolName == NULL){
		error("init_log: Protocol name not specified.\n");
		return;
	}

	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/%s.log", LOGPATH, ProtocolName);

	FILE *File = fopen(FileName, "at");
	if(File ==NULL){
		error("init_log: Cannot create protocol %s.\n", ProtocolName);
		return;
	}

	// NOTE(fusion): `ctime` will already add a new line character.
	time_t Time = time(NULL);
	fprintf(File, "-------------------------------------------------------------------------------\n");
	fprintf(File, "Tibia - Graphical Multi-User-Dungeon\n");
	fprintf(File, "%s.log - started %s", ProtocolName, ctime(&Time));
	fclose(File);
}

void log_message(const char *ProtocolName, const char *Text, ...){
	if(ProtocolName == NULL || ProtocolName[0] == 0){
		error("log_message: Protocol name not specified.\n");
		return;
	}

	bool WriteDate = (strcmp(ProtocolName, "bugreport") != 0)
			&& (strcmp(ProtocolName, "client-error") != 0)
			&& (strcmp(ProtocolName, "load") != 0);

	char Output[256];
	va_list ap;
	va_start(ap, Text);
	vsnprintf(Output, sizeof(Output), Text, ap);
	va_end(ap);

	char Line[256];
	if(WriteDate){
		print(2, "%s.log: %s", ProtocolName, Output);
		struct tm LocalTime = GetLocalTimeTM(time(NULL));
		snprintf(Line, sizeof(Line), "%02d.%02d.%04d %02d:%02d:%02d (%u): %s",
				LocalTime.tm_mday, LocalTime.tm_mon + 1, LocalTime.tm_year + 1900,
				LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec, RoundNr, Output);
	}else{
		strcpy(Line, Output);
	}

	if(Line[0] != 0){
		int LineEnd = (int)strlen(Line);
		if(Line[LineEnd - 1] != '\n'){
			if(LineEnd < (int)(sizeof(Line) - 1)){
				Line[LineEnd] = '\n';
				Line[LineEnd + 1] = 0;
			}else{
				Line[LineEnd - 1] = '\n';
			}
		}

		if(ProtocolThread != INVALID_THREAD_HANDLE){
			insert_protocol_order(ProtocolName, Line);
		}else{
			write_protocol(ProtocolName, Line);
		}
	}
}

// Writer Orders
// =============================================================================
void init_writer_buffers(void){
	OrderPointerWrite = 0;
	ReplyPointerWrite = 0;
	OrderPointerRead = 0;
	ReplyPointerRead = 0;
}

int get_order_buffer_space(void){
	int Result = INT_MAX;
	if(WriterThread != INVALID_THREAD_HANDLE){
		int Orders = (OrderPointerWrite - OrderPointerRead);
		Result = NARRAY(OrderBuffer) - Orders;
	}
	return Result;
}

void insert_order(WriterThreadOrderType OrderType, const void *Data){
	if(WriterThread != INVALID_THREAD_HANDLE){
		int Orders = (OrderPointerWrite - OrderPointerRead);
		if(Orders >= NARRAY(OrderBuffer)){
			error("insert_order (Writer): Order buffer is full => increase size.\n");
		}

		OrderBufferEmpty.down();
		int WritePos = OrderPointerWrite % NARRAY(OrderBuffer);
		OrderBuffer[WritePos].OrderType = OrderType;
		OrderBuffer[WritePos].Data = Data;
		OrderPointerWrite += 1;
		OrderBufferFull.up();
	}
}

void get_order(WriterThreadOrder *Order){
	OrderBufferFull.down();
	*Order = OrderBuffer[OrderPointerRead % NARRAY(OrderBuffer)];
	OrderPointerRead += 1;
	OrderBufferEmpty.up();
}

void terminate_writer_order(void){
	insert_order(WRITER_ORDER_TERMINATE, NULL);
}

void logout_order(TPlayer *Player){
	if(Player == NULL){
		error("logout_order: Provided player does not exist.\n");
		return;
	}

	LogoutOrderData *Data = new LogoutOrderData;
	Data->CharacterID = Player->ID;
	Data->Level = Player->Skills[SKILL_LEVEL]->Get();
	Data->Profession = Player->GetActiveProfession();
	if(Player->PlayerData == NULL){
		error("logout_order: PlayerData is NULL.\n");
		Data->LastLoginTime = 0;
	}else{
		Data->LastLoginTime = Player->PlayerData->LastLoginTime;
	}
	Data->TutorActivities = Player->TutorActivities;

	// TODO(fusion): LOL.
	if(Player->startx == 32097){
		strcpy(Data->Residence, "Rookgaard");
	}else if(Player->startx == 32360){
		strcpy(Data->Residence, "Carlin");
	}else if(Player->startx == 32369){
		strcpy(Data->Residence, "Thais");
	}else if(Player->startx == 32595){
		strcpy(Data->Residence, "Port Hope");
	}else if(Player->startx == 32649){
		strcpy(Data->Residence, "Kazordoon");
	}else if(Player->startx == 32732){
		strcpy(Data->Residence, "Ab'Dendriel");
	}else if(Player->startx == 32957){
		strcpy(Data->Residence, "Venore");
	}else if(Player->startx == 33194){
		strcpy(Data->Residence, "Ankrahmun");
	}else if(Player->startx == 33213){
		strcpy(Data->Residence, "Darashia");
	}else if(Player->startx == 33217){
		strcpy(Data->Residence, "Edron");
	}else{
		error("logout_order: Unknown start coordinate [%d,%d,%d] for player %s.\n",
				Player->startx, Player->starty, Player->startz, Player->Name);
		strcpy(Data->Residence, "Unknown");
	}

	insert_order(WRITER_ORDER_LOGOUT, Data);
}

void playerlist_order(int NumberOfPlayers, const char *PlayerNames,
		int *PlayerLevels, int *PlayerProfessions){
	if(PlayerNames == NULL){
		error("playerlist_order: PlayerNames is NULL.\n");
		return;
	}

	if(PlayerLevels == NULL){
		error("playerlist_order: PlayerLevels is NULL.\n");
		return;
	}

	if(PlayerProfessions == NULL){
		error("playerlist_order: PlayerProfessions is NULL.\n");
		return;
	}

	PlayerlistOrderData *Data = new PlayerlistOrderData;
	Data->NumberOfPlayers = NumberOfPlayers;
	Data->PlayerNames = PlayerNames;
	Data->PlayerLevels = PlayerLevels;
	Data->PlayerProfessions = PlayerProfessions;

	insert_order(WRITER_ORDER_PLAYERLIST, Data);
}

void kill_statistics_order(int NumberOfRaces, const char *RaceNames,
		int *KilledPlayers, int *KilledCreatures){
	if(RaceNames == NULL){
		error("kill_statistics_order: RaceNames is NULL.\n");
		return;
	}

	if(KilledPlayers == NULL){
		error("kill_statistics_order: KilledPlayers is NULL.\n");
		return;
	}

	if(KilledCreatures == NULL){
		error("kill_statistics_order: KilledCreatures is NULL.\n");
		return;
	}

	KillStatisticsOrderData *Data = new KillStatisticsOrderData;
	Data->NumberOfRaces = NumberOfRaces;
	Data->RaceNames = RaceNames;
	Data->KilledPlayers = KilledPlayers;
	Data->KilledCreatures = KilledCreatures;

	insert_order(WRITER_ORDER_KILLSTATISTICS, Data);
}

void punishment_order(TCreature *Gamemaster, const char *Name, const char *IPAddress,
		int Reason, int Action, const char *Comment, int NumberOfStatements,
		vector<ReportedStatement> *ReportedStatements, uint32 StatementID,
		bool ip_banishment){
	if(Name == NULL){
		error("punishment_order: Name is NULL.\n");
		return;
	}

	if(Comment == NULL){
		error("punishment_order: Comment is NULL.\n");
		return;
	}

	PunishmentOrderData *Data = new PunishmentOrderData;
	if(Gamemaster != NULL){
		Data->GamemasterID = Gamemaster->ID;
		strcpy(Data->GamemasterName, Gamemaster->Name);
	}else{
		Data->GamemasterID = 0;
		strcpy(Data->GamemasterName, "automatic");
	}
	strcpy(Data->CriminalName, Name);
	strcpy(Data->CriminalIPAddress, (IPAddress != NULL ? IPAddress : ""));
	Data->Reason = Reason;
	Data->Action = Action;
	strcpy(Data->Comment, Comment);
	Data->NumberOfStatements = NumberOfStatements;
	Data->ReportedStatements = ReportedStatements;
	Data->StatementID = StatementID;
	Data->ip_banishment = ip_banishment;

	insert_order(WRITER_ORDER_PUNISHMENT, Data);
}

void character_death_order(TCreature *Creature, int OldLevel,
		uint32 OffenderID, const char *Remark, bool Unjustified){
	if(Creature == NULL){
		error("character_death_order: cr is NULL.\n");
		return;
	}

	if(Remark == NULL){
		error("character_death_order: Remark is NULL.\n");
		return;
	}

	CharacterDeathOrderData *Data = new CharacterDeathOrderData;
	Data->CharacterID = Creature->ID;
	Data->Level = OldLevel;
	Data->Offender = OffenderID;
	strcpy(Data->Remark, Remark);
	Data->Unjustified = Unjustified;
	Data->Time = time(NULL);

	insert_order(WRITER_ORDER_CHARACTERDEATH, Data);
}

void add_buddy_order(TCreature *Creature, uint32 BuddyID){
	if(Creature == NULL){
		error("add_buddy_order: cr is NULL.\n");
		return;
	}

	if(Creature->Type != PLAYER){
		error("add_buddy_order: Creature is not a player.\n");
		return;
	}

	BuddyOrderData *Data = new BuddyOrderData;
	Data->AccountID = ((TPlayer*)Creature)->AccountID;
	Data->Buddy = BuddyID;

	insert_order(WRITER_ORDER_ADDBUDDY, Data);
}

void remove_buddy_order(TCreature *Creature, uint32 BuddyID){
	if(Creature == NULL){
		error("remove_buddy_order: cr is NULL.\n");
		return;
	}

	if(Creature->Type != PLAYER){
		error("remove_buddy_order: Creature is not a player.\n");
		return;
	}

	BuddyOrderData *Data = new BuddyOrderData;
	Data->AccountID = ((TPlayer*)Creature)->AccountID;
	Data->Buddy = BuddyID;

	insert_order(WRITER_ORDER_REMOVEBUDDY, Data);
}

void decrement_is_online_order(uint32 CharacterID){
	void *Data = (void*)((uintptr)CharacterID);
	insert_order(WRITER_ORDER_DECREMENTISONLINE, Data);
}

void save_player_data_order(void){
	insert_order(WRITER_ORDER_SAVEPLAYERDATA, NULL);
}

void process_logout_order(LogoutOrderData *Data){
	if(Data == NULL){
		error("process_logout_order: No data provided.\n");
		return;
	}

	if(Data->TutorActivities > 0){
		print(3, "%d tutor points for player %u.\n",
				Data->TutorActivities, Data->CharacterID);
	}

	char ProfessionName[30];
	GetProfessionName(ProfessionName, Data->Profession, false, true);
	int Ret = QueryManagerConnection->logoutGame(Data->CharacterID, Data->Level,
			ProfessionName, Data->Residence, Data->LastLoginTime, Data->TutorActivities);
	if(Ret != 0){
		error("process_logout_order: Logout for player %u failed.\n",
				Data->CharacterID);
	}

	delete Data;
}

void process_playerlist_order(PlayerlistOrderData *Data){
	if(Data == NULL){
		error("process_playerlist_order: No data provided.\n");
		return;
	}

	bool NewRecord = false;
	if(Data->NumberOfPlayers <= 0){
		int Ret = QueryManagerConnection->createPlayerlist(Data->NumberOfPlayers,
				NULL, NULL, NULL, &NewRecord);
		if(Ret != 0){
			error("process_playerlist_order: Request failed (1).\n");
		}
	}else{
		const char **Names      = (const char**)alloca(Data->NumberOfPlayers * sizeof(const char*));
		int *Levels             = (int*)alloca(Data->NumberOfPlayers * sizeof(int));
		char (*Professions)[30] = (char(*)[30])alloca(Data->NumberOfPlayers * 30);
		for(int PlayerNr = 0; PlayerNr < Data->NumberOfPlayers; PlayerNr += 1){
			Names[PlayerNr] = &Data->PlayerNames[PlayerNr * 30];
			Levels[PlayerNr] = Data->PlayerLevels[PlayerNr];
			GetProfessionName(Professions[PlayerNr],
					Data->PlayerProfessions[PlayerNr], false, true);
		}

		int Ret = QueryManagerConnection->createPlayerlist(Data->NumberOfPlayers,
				Names, Levels, Professions, &NewRecord);
		if(Ret != 0){
			error("process_playerlist_order: Request failed (2).\n");
		}
	}

	if(NewRecord){
		broadcast_reply("New record: %d players are logged in.", Data->NumberOfPlayers);
	}

	delete[] Data->PlayerNames;
	delete[] Data->PlayerLevels;
	delete[] Data->PlayerProfessions;
	delete Data;
}

void process_kill_statistics_order(KillStatisticsOrderData *Data){
	if(Data == NULL){
		error("process_kill_statistics_order: No data provided.\n");
		return;
	}

	const char **RaceNames = (const char**)alloca(Data->NumberOfRaces * sizeof(const char*));
	int *KilledPlayers = (int*)alloca(Data->NumberOfRaces * sizeof(int));
	int *KilledCreatures = (int*)alloca(Data->NumberOfRaces * sizeof(int));
	for(int RaceNr = 0; RaceNr < Data->NumberOfRaces; RaceNr += 1){
		RaceNames[RaceNr] = &Data->RaceNames[RaceNr * 30];
		KilledPlayers[RaceNr] = Data->KilledPlayers[RaceNr];
		KilledCreatures[RaceNr] = Data->KilledCreatures[RaceNr];
	}

	int Ret = QueryManagerConnection->logKilledCreatures(Data->NumberOfRaces,
			RaceNames, KilledPlayers, KilledCreatures);
	if(Ret != 0){
		error("process_kill_statistics_order: Request failed.\n");
	}

	delete[] Data->RaceNames;
	delete[] Data->KilledPlayers;
	delete[] Data->KilledCreatures;
	delete Data;
}

static const char *GetStatementOutputChannel(int Mode, int Channel){
	const char *Result;
	switch(Mode){
		case TALK_SAY:						Result = "Say"; break;
		case TALK_WHISPER:					Result = "Whisper"; break;
		case TALK_YELL:						Result = "Yell"; break;
		case TALK_PRIVATE_MESSAGE:			Result = "Private Message"; break;
		case TALK_GAMEMASTER_REQUEST:		Result = "Gamemaster Request"; break;
		case TALK_GAMEMASTER_ANSWER:		Result = "Gamemaster Answer"; break;
		case TALK_PLAYER_ANSWER:			Result = "Player Answer"; break;
		case TALK_GAMEMASTER_BROADCAST:		Result = "Broadcast"; break;
		case TALK_GAMEMASTER_MESSAGE:		Result = "Private Message"; break;

		case TALK_CHANNEL_CALL:
		case TALK_GAMEMASTER_CHANNELCALL:
		case TALK_HIGHLIGHT_CHANNELCALL:{
			switch(Channel){
				case CHANNEL_GUILD:			Result = "Guild Channel"; break;
				case CHANNEL_GAMEMASTER:	Result = "Gamemaster Channel"; break;
				case CHANNEL_TUTOR:			Result = "Tutor Channel"; break;
				case CHANNEL_GAMECHAT:		Result = "Game Chat"; break;
				case CHANNEL_TRADE:			Result = "Trade Channel"; break;
				case CHANNEL_RLCHAT:		Result = "Reallife Chat"; break;
				case CHANNEL_HELP:			Result = "Help Channel"; break;
				default:					Result = "Private Chat Channel"; break;
			}
			break;
		}

		default:{
			error("process_punishment_order: Invalid mode %d.\n", Mode);
			Result = "Unknown";
			break;
		}
	}

	return Result;
}

void process_punishment_order(PunishmentOrderData *Data){
	// TODO(fusion): I feel this it too complex for handling a simple banishment system.
	if(Data == NULL){
		error("process_punishment_order: No data provided.\n");
		return;
	}

	bool Ok = true;
	const char *Reason = get_banishment_reason(Data->Reason);
	uint32 BanishmentID = 0;

	if(Ok && Data->Action == 0){
		int Ret = QueryManagerConnection->setNotation(Data->GamemasterID,
				Data->CriminalName, Data->CriminalIPAddress, Reason,
				Data->Comment, &BanishmentID);
		switch(Ret){
			case 0:{
				direct_reply(Data->GamemasterID, "notation for player %s inserted.", Data->CriminalName);
				log_message("banish", "%s noted about %s: %s.\n", Data->GamemasterName, Data->CriminalName, Data->Comment);
				break;
			}

			case 1:{
				direct_reply(Data->GamemasterID, "A player with this name does not exist. Perhaps he/she has been renamed?");
				Ok = false;
				break;
			}

			case 2:{
				direct_reply(Data->GamemasterID, "You may not report a god or gamemaster.");
				Ok = false;
				break;
			}
		}
	}

	if(Ok && (Data->Action == 1 || Data->Action == 3 || Data->Action == 5)){
		int Ret = QueryManagerConnection->setNamelock(Data->GamemasterID,
				Data->CriminalName, Data->CriminalIPAddress, Reason,
				Data->Comment);
		switch(Ret){
			case 0:{
				direct_reply(Data->GamemasterID, "Player %s reported for renaming.", Data->CriminalName);
				log_message("banish", "%s reported %s for name change.\n", Data->GamemasterName, Data->CriminalName);
				break;
			}

			case 3:{
				direct_reply(Data->GamemasterID, "This player has already been reported.");
				break;
			}

			case 1:{
				direct_reply(Data->GamemasterID, "A player with this name does not exist. Perhaps he/she has already been renamed?");
				Ok = false;
				break;
			}

			case 2:
			case 4:{
				direct_reply(Data->GamemasterID, "This name has already been approved.");
				Ok = false;
				break;
			}
		}
	}

	if(Ok && (Data->Action == 2 || Data->Action == 3 || Data->Action == 4 || Data->Action == 5)){
		int Days;
		bool FinalWarning = (Data->Action == 4 || Data->Action == 5);
		int Ret = QueryManagerConnection->banishAccount(Data->GamemasterID,
				Data->CriminalName, Data->CriminalIPAddress, Reason,
				Data->Comment, &FinalWarning, &Days, &BanishmentID);
		switch(Ret){
			case 0:{
				if(Days == -1){
					direct_reply(Data->GamemasterID, "Account of player %s banished infinitely.", Data->CriminalName);
				}else if(FinalWarning){
					direct_reply(Data->GamemasterID, "Account of player %s banished for %d days with final warning.", Data->CriminalName, Days);
				}else{
					direct_reply(Data->GamemasterID, "Account of player %s banished for %d days.", Data->CriminalName, Days);
				}
				logout_reply(Data->CriminalName);
				log_message("banish", "%s banished account of player %s.\n", Data->GamemasterName, Data->CriminalName);
				break;
			}

			case 3:{
				direct_reply(Data->GamemasterID, "Player %s has already been banished.", Data->CriminalName);
				logout_reply(Data->CriminalName);
				break;
			}

			case 1:{
				direct_reply(Data->GamemasterID, "A player with this name does not exist. Perhaps he/she has been renamed?");
				Ok = false;
				break;
			}

			case 2:{
				direct_reply(Data->GamemasterID, "You may not report a god or gamemaster.");
				Ok = false;
				break;
			}
		}
	}

	if(Ok && Data->StatementID != 0){
		if(Data->NumberOfStatements > 0 && Data->ReportedStatements != NULL){
			uint32 *StatementIDs = (uint32*)alloca(Data->NumberOfStatements * sizeof(uint32));
			int *TimeStamps      = (int*)alloca(Data->NumberOfStatements * sizeof(int));
			uint32 *CharacterIDs = (uint32*)alloca(Data->NumberOfStatements * sizeof(uint32));
			char (*Channels)[30] = (char(*)[30])alloca(Data->NumberOfStatements * 30);
			char (*Texts)[256]   = (char(*)[256])alloca(Data->NumberOfStatements * 256);
			for(int StatementNr = 0; StatementNr < Data->NumberOfStatements; StatementNr += 1){
				ReportedStatement *Statement = Data->ReportedStatements->at(StatementNr);
				StatementIDs[StatementNr] = Statement->StatementID;
				TimeStamps[StatementNr] = Statement->TimeStamp;
				CharacterIDs[StatementNr] = Statement->CharacterID;
				strcpy(Channels[StatementNr], GetStatementOutputChannel(Statement->Mode, Statement->Channel));
				strcpy(Texts[StatementNr], Statement->Text);
			}

			int Ret = QueryManagerConnection->reportStatement(Data->GamemasterID,
					Data->CriminalName, Reason, Data->Comment, BanishmentID,
					Data->StatementID, Data->NumberOfStatements, StatementIDs,
					TimeStamps, CharacterIDs, Channels, Texts);
			switch(Ret){
				case 0:{
					if(Data->Action == 6){
						direct_reply(Data->GamemasterID, "Statement of %s reported.", Data->CriminalName);
					}
					log_message("banish", "%s reported statement of player %s.\n", Data->GamemasterName, Data->CriminalName);
					break;
				}

				case 1:{
					if(Data->Action == 6){
						direct_reply(Data->GamemasterID, "A player with this name does not exist. Perhaps he/she has been renamed?");
					}
					Ok = false;
					break;
				}

				case 2:{
					if(Data->Action == 6){
						direct_reply(Data->GamemasterID, "Statement has already been reported.");
					}
					Ok = false;
					break;
				}
			}
		}else{
			error("process_punishment_order: Statements do not exist.\n");
		}
	}

	if(Ok && Data->ip_banishment){
		int Ret = QueryManagerConnection->banishIPAddress(Data->GamemasterID,
				Data->CriminalName, Data->CriminalIPAddress, Reason, Data->Comment);
		switch(Ret){
			case 0:{
				direct_reply(Data->GamemasterID, "IP address of %s banished.", Data->CriminalName);
				logout_reply(Data->CriminalName);
				log_message("banish", "%s blocked the IP address of %s.\n", Data->GamemasterName, Data->CriminalName);
				break;
			}

			case 1:{
				direct_reply(Data->GamemasterID, "A player with this name does not exist. Perhaps he/she has been renamed?");
				Ok = false;
				break;
			}

			case 2:{
				direct_reply(Data->GamemasterID, "You may not report a god or gamemaster.");
				Ok = false;
				break;
			}
		}
	}

	delete Data->ReportedStatements;
	delete Data;
}

void process_character_death_order(CharacterDeathOrderData *Data){
	if(Data == NULL){
		error("process_character_death_order: No data provided.\n");
		return;
	}

	int Ret = QueryManagerConnection->logCharacterDeath(Data->CharacterID,
			Data->Level, Data->Offender, Data->Remark, Data->Unjustified,
			Data->Time);
	if(Ret != 0){
		error("process_character_death_order: Logging failed.\n");
	}

	delete Data;
}

void process_add_buddy_order(BuddyOrderData *Data){
	if(Data == NULL){
		error("process_add_buddy_order: No data provided.\n");
		return;
	}

	int Ret = QueryManagerConnection->addBuddy(Data->AccountID, Data->Buddy);
	if(Ret != 0){
		error("process_add_buddy_order: Addition failed.\n");
	}

	delete Data;
}

void process_remove_buddy_order(BuddyOrderData *Data){
	if(Data == NULL){
		error("process_remove_buddy_order: No data provided.\n");
		return;
	}

	int Ret = QueryManagerConnection->removeBuddy(Data->AccountID, Data->Buddy);
	if(Ret != 0){
		error("process_remove_buddy_order: Removal failed.\n");
	}

	delete Data;
}

void process_decrement_is_online_order(uint32 CharacterID){
	int Ret = QueryManagerConnection->decrementIsOnline(CharacterID);
	if(Ret != 0){
		error("process_decrement_is_online_order: Decrement failed.\n");
	}
}

int writer_thread_loop(void *Unused){
	WriterThreadOrder Order = {};
	while(true){
		get_order(&Order);
		if(Order.OrderType == WRITER_ORDER_TERMINATE){
			break;
		}

		switch(Order.OrderType){
			case WRITER_ORDER_LOGOUT:{
				process_logout_order((LogoutOrderData*)Order.Data);
				break;
			}

			case WRITER_ORDER_PLAYERLIST:{
				process_playerlist_order((PlayerlistOrderData*)Order.Data);
				break;
			}

			case WRITER_ORDER_KILLSTATISTICS:{
				process_kill_statistics_order((KillStatisticsOrderData*)Order.Data);
				break;
			}

			case WRITER_ORDER_PUNISHMENT:{
				process_punishment_order((PunishmentOrderData*)Order.Data);
				break;
			}

			case WRITER_ORDER_CHARACTERDEATH:{
				process_character_death_order((CharacterDeathOrderData*)Order.Data);
				break;
			}

			case WRITER_ORDER_ADDBUDDY:{
				process_add_buddy_order((BuddyOrderData*)Order.Data);
				break;
			}

			case WRITER_ORDER_REMOVEBUDDY:{
				process_remove_buddy_order((BuddyOrderData*)Order.Data);
				break;
			}

			case WRITER_ORDER_DECREMENTISONLINE:{
				uint32 CharacterID = (uint32)((uintptr)Order.Data);
				process_decrement_is_online_order(CharacterID);
				break;
			}

			case WRITER_ORDER_SAVEPLAYERDATA:{
				SavePlayerPoolSlots();
				break;
			}

			default:{
				error("writer_thread_loop: Unknown command %d.\n", Order.OrderType);
				break;
			}
		}
	}

	return 0;
}

// Writer Replies
// =============================================================================
void insert_reply(WriterThreadReplyType ReplyType, const void *Data){
	int Replies = (ReplyPointerWrite - ReplyPointerRead);
	if(Replies >= NARRAY(ReplyBuffer)){
		error("insert_reply (Writer): Buffer is full; reply will be discarded.\n");
		return;
	}

	int WritePos = ReplyPointerWrite % NARRAY(ReplyBuffer);
	ReplyBuffer[WritePos].ReplyType = ReplyType;
	ReplyBuffer[WritePos].Data = Data;
	ReplyPointerWrite += 1;
}

bool get_reply(WriterThreadReply *Reply){
	bool Result = (ReplyPointerRead < ReplyPointerWrite);
	if(Result){
		*Reply = ReplyBuffer[ReplyPointerRead % NARRAY(ReplyBuffer)];
		ReplyPointerRead += 1;
	}
	return Result;
}

void broadcast_reply(const char *Text, ...){
	if(Text == NULL){
		error("broadcast_reply: No text specified.\n");
		return;
	}

	BroadcastReplyData *Data = new BroadcastReplyData;

	va_list ap;
	va_start(ap, Text);
	vsnprintf(Data->Message, sizeof(Data->Message), Text, ap);
	va_end(ap);

	insert_reply(WRITER_REPLY_BROADCAST, Data);
}

void direct_reply(uint32 CharacterID, const char *Text, ...){
	if(CharacterID == 0){
		return;
	}

	if(Text == NULL){
		error("SendDirectReply: No text specified.\n");
		return;
	}

	DirectReplyData *Data = new DirectReplyData;
	Data->CharacterID = CharacterID;

	va_list ap;
	va_start(ap, Text);
	vsnprintf(Data->Message, sizeof(Data->Message), Text, ap);
	va_end(ap);

	insert_reply(WRITER_REPLY_DIRECT, Data);
}

void logout_reply(const char *PlayerName){
	if(PlayerName == NULL){
		return;
	}

	// TODO(fusion): Probably some string dup function?
	char *Data = new char[strlen(PlayerName) + 1];
	strcpy(Data, PlayerName);
	insert_reply(WRITER_REPLY_LOGOUT, Data);
}

void process_broadcast_reply(BroadcastReplyData *Data){
	if(Data == NULL){
		error("process_broadcast_reply: No data provided.\n");
		return;
	}

	BroadcastMessage(TALK_STATUS_MESSAGE, Data->Message);

	delete Data;
}

void process_direct_reply(DirectReplyData *Data){
	if(Data == NULL){
		error("process_direct_reply: No data provided.\n");
		return;
	}

	TPlayer *Player = GetPlayer(Data->CharacterID);
	if(Player != NULL){
		SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s", Data->Message);
	}

	delete Data;
}

void process_logout_reply(const char *Name){
	if(Name == NULL){
		error("process_logout_reply: No data provided.\n");
		return;
	}

	TPlayer *Player = GetPlayer(Name);
	if(Player != NULL){
		graphical_effect(Player->CrObject, EFFECT_MAGIC_GREEN);
		Player->StartLogout(true, true);
	}

	delete[] Name;
}

void process_writer_thread_replies(void){
	WriterThreadReply Reply = {};
	while(get_reply(&Reply)){
		switch(Reply.ReplyType){
			case WRITER_REPLY_BROADCAST:{
				process_broadcast_reply((BroadcastReplyData*)Reply.Data);
				break;
			}

			case WRITER_REPLY_DIRECT:{
				process_direct_reply((DirectReplyData*)Reply.Data);
				break;
			}

			case WRITER_REPLY_LOGOUT:{
				process_logout_reply((const char*)Reply.Data);
				break;
			}

			default:{
				error("process_writer_thread_replies: Unknown reply %d.\n", Reply.ReplyType);
				break;
			}
		}
	}
}

// Initialization
// =============================================================================
void clear_players(void){
	int NumberOfAffectedPlayers;
	int Ret = QueryManagerConnection->clearIsOnline(&NumberOfAffectedPlayers);
	if(Ret != 0){
		error("clear_players: Cannot clear IsOnline flags.\n");
	}else if(NumberOfAffectedPlayers != 0){
		error("clear_players: %d players were marked as logged in.\n",
				NumberOfAffectedPlayers);
	}
}

void init_writer(void){
	// TODO(fusion): No idea what's this about.
	int QueryBufferSize = std::max<int>(kb(16), MaxPlayers * 66 + 2);
	QueryManagerConnection = new TQueryManagerConnection(QueryBufferSize);
	if(!QueryManagerConnection->isConnected()){
		throw "cannot connect to query manager";
	}

	clear_players();

	init_protocol();
	ProtocolThread = StartThread(protocol_thread_loop, NULL, false);
	if(ProtocolThread == INVALID_THREAD_HANDLE){
		throw "cannot start protocol thread";
	}

	init_writer_buffers();
	WriterThread = StartThread(writer_thread_loop, NULL, false);
	if(WriterThread == INVALID_THREAD_HANDLE){
		throw "cannot start writer thread";
	}
}

void abort_writer(void){
	// TODO(fusion): The original function was calling `pthread_cancel` with
	// an argument of 0 which is probably wrong?. I feel something is missing
	// here.
	if(WriterThread != INVALID_THREAD_HANDLE){
		pthread_cancel(WriterThread);
		WriterThread = INVALID_THREAD_HANDLE;
		OrderBufferEmpty.up();
	}
}

void exit_writer(void){
	if(ProtocolThread != INVALID_THREAD_HANDLE){
		insert_protocol_order("", "");
		JoinThread(ProtocolThread);
		ProtocolThread = INVALID_THREAD_HANDLE;
	}

	if(WriterThread != INVALID_THREAD_HANDLE){
		terminate_writer_order();
		JoinThread(WriterThread);
		WriterThread = INVALID_THREAD_HANDLE;
	}

	delete QueryManagerConnection;
}
