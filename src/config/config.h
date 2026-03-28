#ifndef GAMESERVER_CONFIG_H
#define GAMESERVER_CONFIG_H

#include "common.h"
#include "enums.h"

struct QueryManagerSettings {
    char Host[50];
    int Port;
    char Password[30];
};

extern char BINPATH[4096];
extern char DATAPATH[4096];
extern char LOGPATH[4096];
extern char MAPPATH[4096];
extern char MONSTERPATH[4096];
extern char NPCPATH[4096];
extern char ORIGMAPPATH[4096];
extern char SAVEPATH[4096];
extern char USERPATH[4096];
extern int SHMKey;
extern int GamePort;
extern char GameAddress[16];
extern int DebugLevel;
extern bool PrivateWorld;
extern TWorldType WorldType;
extern char WorldName[30];
extern int MaxPlayers;
extern int MaxNewbies;
extern int PremiumPlayerBuffer;
extern int PremiumNewbieBuffer;
extern int Beat;
extern int RebootTime;
extern int NumberOfQueryManagers;
extern QueryManagerSettings QUERY_MANAGER[10];

enum TRANSPORTMODE { TRANSPORT_TCP, TRANSPORT_WEBSOCKET, TRANSPORT_BOTH };

extern TRANSPORTMODE TransportMode;
extern int WebSocketPort;
extern char WebSocketAddress[16];

extern bool ChallengeEnabled;
extern uint8 ChallengeSecret[16];
extern int ChallengeInterval;
extern int ChallengeTimeout;
extern int ChallengeBanMinutes;

void ReadConfig(void);

#endif // GAMESERVER_CONFIG_H
