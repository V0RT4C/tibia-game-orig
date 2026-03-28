#include "config/config.h"

#include <cstdlib>
#include <cstring>

char BINPATH[4096];
char DATAPATH[4096];
char LOGPATH[4096];
char MAPPATH[4096];
char MONSTERPATH[4096];
char NPCPATH[4096];
char ORIGMAPPATH[4096];
char SAVEPATH[4096];
char USERPATH[4096];

int SHMKey;
int GamePort;

int DebugLevel;
bool PrivateWorld;
TWorldType WorldType;
char WorldName[30];
int MaxPlayers;
int MaxNewbies;
int PremiumPlayerBuffer;
int PremiumNewbieBuffer;
int Beat;
int RebootTime;

char GameAddress[16];

int NumberOfQueryManagers;
QueryManagerSettings QUERY_MANAGER[10];

TRANSPORTMODE TransportMode;
int WebSocketPort;
char WebSocketAddress[16];

bool ChallengeEnabled;
uint8 ChallengeSecret[16];
int ChallengeInterval;
int ChallengeTimeout;
int ChallengeBanMinutes;

static const char *EnvStr(const char *Name, const char *Default){
	const char *Value = getenv(Name);
	return (Value != NULL) ? Value : Default;
}

static int EnvInt(const char *Name, int Default){
	const char *Value = getenv(Name);
	return (Value != NULL) ? atoi(Value) : Default;
}

static void EnvCopy(char *Dest, usize DestSize, const char *Name, const char *Default){
	strncpy(Dest, EnvStr(Name, Default), DestSize - 1);
	Dest[DestSize - 1] = '\0';
}

void ReadConfig(void){
	// Paths
	EnvCopy(BINPATH, sizeof(BINPATH), "SENJA_GAMESERVER_BINPATH", ".");
	EnvCopy(MAPPATH, sizeof(MAPPATH), "SENJA_GAMESERVER_MAPPATH", "./map");
	EnvCopy(ORIGMAPPATH, sizeof(ORIGMAPPATH), "SENJA_GAMESERVER_ORIGMAPPATH", "./origmap");
	EnvCopy(DATAPATH, sizeof(DATAPATH), "SENJA_GAMESERVER_DATAPATH", "./dat");
	EnvCopy(USERPATH, sizeof(USERPATH), "SENJA_GAMESERVER_USERPATH", "./usr");
	EnvCopy(LOGPATH, sizeof(LOGPATH), "SENJA_GAMESERVER_LOGPATH", "./log");
	EnvCopy(SAVEPATH, sizeof(SAVEPATH), "SENJA_GAMESERVER_SAVEPATH", "./save");
	EnvCopy(MONSTERPATH, sizeof(MONSTERPATH), "SENJA_GAMESERVER_MONSTERPATH", "./mon");
	EnvCopy(NPCPATH, sizeof(NPCPATH), "SENJA_GAMESERVER_NPCPATH", "./npc");

	// Server settings
	SHMKey = EnvInt("SENJA_GAMESERVER_SHM", 10011);
	DebugLevel = EnvInt("SENJA_GAMESERVER_DEBUG_LEVEL", 2);
	Beat = EnvInt("SENJA_GAMESERVER_BEAT", 50);
	GamePort = EnvInt("SENJA_GAMESERVER_TCP_PORT", 7172);

	// World
	EnvCopy(WorldName, sizeof(WorldName), "SENJA_WORLD_NAME", "Senja");
	PrivateWorld = (strcmp(EnvStr("SENJA_GAMESERVER_WORLD_STATE", "public"), "private") == 0);

	// Globals initialized to 0 — overwritten by LoadWorldConfig()
	WorldType = NORMAL;
	MaxPlayers = 0;
	MaxNewbies = 0;
	PremiumPlayerBuffer = 0;
	PremiumNewbieBuffer = 0;
	RebootTime = 0;
	GameAddress[0] = '\0';

	// Query manager
	const char *QmPassword = getenv("SENJA_QM_PASSWORD");
	if(QmPassword == NULL){
		throw "SENJA_QM_PASSWORD environment variable is required";
	}
	EnvCopy(QUERY_MANAGER[0].Host, sizeof(QUERY_MANAGER[0].Host), "SENJA_QM_HOST", "127.0.0.1");
	QUERY_MANAGER[0].Port = EnvInt("SENJA_QM_PORT", 7173);
	strncpy(QUERY_MANAGER[0].Password, QmPassword, sizeof(QUERY_MANAGER[0].Password) - 1);
	QUERY_MANAGER[0].Password[sizeof(QUERY_MANAGER[0].Password) - 1] = '\0';
	NumberOfQueryManagers = 1;

	// Transport
	const char *Mode = EnvStr("SENJA_TRANSPORT_MODE", "both");
	if(strcmp(Mode, "tcp") == 0){
		TransportMode = TRANSPORT_TCP;
	}else if(strcmp(Mode, "websocket") == 0){
		TransportMode = TRANSPORT_WEBSOCKET;
	}else{
		TransportMode = TRANSPORT_BOTH;
	}
	WebSocketPort = EnvInt("SENJA_GAMESERVER_WS_PORT", 7979);
	EnvCopy(WebSocketAddress, sizeof(WebSocketAddress), "SENJA_GAMESERVER_WS_ADDRESS", "0.0.0.0");

	// Challenge-response anti-bot
	ChallengeEnabled = (EnvInt("SENJA_GAMESERVER_CHALLENGE_ENABLED", 0) != 0);
	ChallengeInterval = EnvInt("SENJA_GAMESERVER_CHALLENGE_INTERVAL", 60);
	ChallengeTimeout = EnvInt("SENJA_GAMESERVER_CHALLENGE_TIMEOUT", 10);
	ChallengeBanMinutes = EnvInt("SENJA_GAMESERVER_CHALLENGE_BAN_MINUTES", 0);
	memset(ChallengeSecret, 0, sizeof(ChallengeSecret));
	if(ChallengeEnabled){
		const char *SecretHex = getenv("SENJA_GAMESERVER_CHALLENGE_SECRET");
		if(SecretHex == NULL || strlen(SecretHex) != 32){
			error("SENJA_GAMESERVER_CHALLENGE_SECRET must be 32 hex chars; disabling challenges.\n");
			ChallengeEnabled = false;
		}else{
			for(int i = 0; i < 16; i++){
				unsigned int Byte;
				if(sscanf(SecretHex + i * 2, "%2x", &Byte) != 1){
					error("SENJA_GAMESERVER_CHALLENGE_SECRET contains invalid hex; disabling challenges.\n");
					ChallengeEnabled = false;
					memset(ChallengeSecret, 0, sizeof(ChallengeSecret));
					break;
				}
				ChallengeSecret[i] = (uint8)Byte;
			}
		}
	}
}
