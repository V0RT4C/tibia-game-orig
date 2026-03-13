#ifndef TIBIA_NETWORK_CONNECTION_H_
#define TIBIA_NETWORK_CONNECTION_H_ 1

#include "common.h"
#include "crypto.h"
#include "enums.h"
#include "network/transport/transport.h"

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
	void process(void);
	void reset_timer(int Command);
	void emergency_ping(void);
	pid_t get_thread_id(void);
	bool set_login_timer(int Timeout);
	void stop_login_timer(void);
	int get_socket(void);
	const char *get_ip_address(void);
	void free_connection(void);
	void assign(void);
	void connect(int Socket, ITransport *Transport);
	void login(void);
	bool join_game(TReadBuffer *Buffer);
	void enter_game(void);
	void die(void);
	void logout(int Delay, bool StopFight);
	void close_connection(bool Delay);
	void disconnect(void);
	TPlayer *get_player(void);
	const char *get_name(void);
	void get_position(int *x, int *y, int *z);
	bool is_visible(int x, int y, int z);
	KNOWNCREATURESTATE known_creature(uint32 ID, bool UpdateFollows);
	uint32 new_known_creature(uint32 NewID);
	void clear_known_creature_table(bool Unchain);
	void unchain_known_creature(uint32 ID);

	bool in_game(void) const { return this->State == CONNECTION_GAME || this->State == CONNECTION_DEAD; }

	bool live(void) const {
		return this->State == CONNECTION_LOGIN || this->State == CONNECTION_GAME || this->State == CONNECTION_DEAD ||
			   this->State == CONNECTION_LOGOUT;
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
	ITransport *Transport;
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
