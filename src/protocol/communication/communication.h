#ifndef TIBIA_PROTOCOL_COMMUNICATION_H_
#define TIBIA_PROTOCOL_COMMUNICATION_H_ 1

#include "network/connection/connection.h"
#include "protocol/protocol_enums.h"

struct TPlayerData;

struct TWaitinglistEntry {
    TWaitinglistEntry *Next;
    char Name[30];
    uint32 NextTry;
    bool FreeAccount;
    bool Newbie;
    bool Sleeping;
};

void GetCommunicationThreadStack(int *StackNumber, void **Stack);
void AttachCommunicationThreadStack(int StackNumber);
void ReleaseCommunicationThreadStack(int StackNumber);
void InitCommunicationThreadStacks(void);
void ExitCommunicationThreadStacks(void);

bool LagDetected(void);
void NetLoad(int Amount, bool Send);
void NetLoadSummary(void);
void NetLoadCheck(void);
void InitLoadHistory(void);
void ExitLoadHistory(void);

bool WriteToSocket(TConnection *Connection, uint8 *Buffer, int Size, int MaxSize);
bool SendLoginMessage(TConnection *Connection, int Type, const char *Message, int WaitingTime);
bool SendData(TConnection *Connection);

bool GetWaitinglistEntry(const char *Name, uint32 *NextTry, bool *FreeAccount, bool *Newbie);
void InsertWaitinglistEntry(const char *Name, uint32 NextTry, bool FreeAccount, bool Newbie);
void DeleteWaitinglistEntry(const char *Name);
int GetWaitinglistPosition(const char *Name, bool FreeAccount, bool Newbie);
int CheckWaitingTime(const char *Name, TConnection *Connection, bool FreeAccount, bool Newbie);

int ReadFromSocket(TConnection *Connection, uint8 *Buffer, int Size);
bool CallGameThread(TConnection *Connection);
bool CheckConnection(TConnection *Connection);
TPlayerData *PerformRegistration(TConnection *Connection, char *PlayerName,
		uint32 AccountID, const char *PlayerPassword, bool GamemasterClient);
bool HandleLogin(TConnection *Connection);
bool ReceiveCommand(TConnection *Connection);

void IncrementActiveConnections(void);
void DecrementActiveConnections(void);
void CommunicationThread(int Socket);
int HandleConnection(void *Data);
bool OpenSocket(void);
int AcceptorThreadLoop(void *Unused);

void CheckThreadlibVersion(void);
void InitCommunication(void);
void ExitCommunication(void);

#endif // TIBIA_PROTOCOL_COMMUNICATION_H_
