#ifndef TIBIA_NETWORK_CONNECTION_H_
#define TIBIA_NETWORK_CONNECTION_H_ 1

#include "common.h"
#include "crypto.h"
#include "enums.h"

struct TConnection;
struct TPlayer;

struct TKnownCreature {
	KNOWNCREATURESTATE State;
	uint32 CreatureID;
	TKnownCreature *Next;
	TConnection *Connection;
};

struct TConnection {
	TConnection(void);
	void Process(void);
	void ResetTimer(int Command);
	void EmergencyPing(void);
	pid_t GetThreadID(void);
	bool SetLoginTimer(int Timeout);
	void StopLoginTimer(void);
	int GetSocket(void);
	const char *GetIPAddress(void);
	void Free(void);
	void Assign(void);
	void Connect(int Socket);
	void Login(void);
	bool JoinGame(TReadBuffer *Buffer);
	void EnterGame(void);
	void Die(void);
	void Logout(int Delay, bool StopFight);
	void Close(bool Delay);
	void Disconnect(void);
	TPlayer *get_player(void);
	const char *get_name(void);
	void get_position(int *x, int *y, int *z);
	bool IsVisible(int x, int y, int z);
	KNOWNCREATURESTATE KnownCreature(uint32 ID, bool UpdateFollows);
	uint32 NewKnownCreature(uint32 NewID);
	void ClearKnownCreatureTable(bool Unchain);
	void UnchainKnownCreature(uint32 ID);

	bool InGame(void) const {
		return this->State == CONNECTION_GAME
			|| this->State == CONNECTION_DEAD;
	}

	bool Live(void) const {
		return this->State == CONNECTION_LOGIN
			|| this->State == CONNECTION_GAME
			|| this->State == CONNECTION_DEAD
			|| this->State == CONNECTION_LOGOUT;
	}

	// DATA
	// =================
	uint8 InData[2048];
	int InDataSize;
	bool SigIOPending;
	bool WaitingForACK;
	uint8 OutData[16384];
	int NextToSend;
	int NextToCommit;
	int NextToWrite;
	bool Overflow;
	bool WillingToSend;
	TConnection *NextSendingConnection;
	uint32 RandomSeed;
	CONNECTIONSTATE State;
	pid_t ThreadID;
	timer_t LoginTimer;
	int Socket;
	char IPAddress[16];
	TXTEASymmetricKey SymmetricKey;
	bool ConnectionIsOk;
	bool ClosingIsDelayed;
	uint32 TimeStamp;
	uint32 TimeStampAction;
	int TerminalType;
	int TerminalVersion;
	int TerminalOffsetX;
	int TerminalOffsetY;
	int TerminalWidth;
	int TerminalHeight;
	uint32 CharacterID;
	char Name[31];
	TKnownCreature KnownCreatureTable[150];
};

#endif // TIBIA_NETWORK_CONNECTION_H_
