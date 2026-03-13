#ifndef TIBIA_CONNECTIONS_H_
#define TIBIA_CONNECTIONS_H_ 1

#include "common.h"
#include "crypto.h"
#include "enums.h"
#include "map.h"

struct TConnection;
struct TPlayer;

#include "protocol/protocol_enums.h"

// TODO(fusion): The maximum number of connections should probably be kept in
// sync with the maximum number of communication threads, or maybe it is the
// same constant.
#define MAX_CONNECTIONS 1100

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

// connections.cc
TConnection *AssignFreeConnection(void);
TConnection *GetFirstConnection(void);
TConnection *GetNextConnection(void);
void ProcessConnections(void);
void InitConnections(void);
void ExitConnections(void);

// sending.cc
void SendAll(void);
bool BeginSendData(TConnection *Connection);
void FinishSendData(TConnection *Connection);
void SkipFlush(TConnection *Connection);
void SendMapObject(TConnection *Connection, Object Obj);
void SendMapPoint(TConnection *Connection, int x, int y, int z);
void SendResult(TConnection *Connection, RESULT r);
void SendRefresh(TConnection *Connection);
void SendInitGame(TConnection *Connection, uint32 CreatureID);
void SendRights(TConnection *Connection);
void SendPing(TConnection *Connection);
void SendFullScreen(TConnection *Connection);
void SendRow(TConnection *Connection, int Direction);
void SendFloors(TConnection *Connection, bool Up);
void SendFieldData(TConnection *Connection, int x, int y, int z);
void SendAddField(TConnection *Connection, int x, int y, int z, Object Obj);
void SendChangeField(TConnection *Connection, int x, int y, int z, Object Obj);
void SendDeleteField(TConnection *Connection, int x, int y, int z, Object Obj);
void SendMoveCreature(TConnection *Connection,
		uint32 CreatureID, int DestX, int DestY, int DestZ);
void SendContainer(TConnection *Connection, int ContainerNr);
void SendCloseContainer(TConnection *Connection, int ContainerNr);
void SendCreateInContainer(TConnection *Connection, int ContainerNr, Object Obj);
void SendChangeInContainer(TConnection *Connection, int ContainerNr, Object Obj);
void SendDeleteInContainer(TConnection *Connection, int ContainerNr, Object Obj);
void SendBodyInventory(TConnection *Connection, uint32 CreatureID);
void SendSetInventory(TConnection *Connection, int Position, Object Obj);
void SendDeleteInventory(TConnection *Connection, int Position);
void SendTradeOffer(TConnection *Connection, const char *Name, bool OwnOffer, Object Obj);
void SendCloseTrade(TConnection *Connection);
void SendAmbiente(TConnection *Connection);
void SendGraphicalEffect(TConnection *Connection, int x, int y, int z, int Type);
void SendTextualEffect(TConnection *Connection, int x, int y, int z, int Color, const char *Text);
void SendMissileEffect(TConnection *Connection, int OrigX, int OrigY, int OrigZ,
		int DestX, int DestY, int DestZ, int Type);
void SendMarkCreature(TConnection *Connection, uint32 CreatureID, int Color);
void SendCreatureHealth(TConnection *Connection, uint32 CreatureID);
void SendCreatureLight(TConnection *Connection, uint32 CreatureID);
void SendCreatureOutfit(TConnection *Connection, uint32 CreatureID);
void SendCreatureSpeed(TConnection *Connection, uint32 CreatureID);
void SendCreatureSkull(TConnection *Connection, uint32 CreatureID);
void SendCreatureParty(TConnection *Connection, uint32 CreatureID);
void SendEditText(TConnection *Connection, Object Obj);
void SendEditList(TConnection *Connection, uint8 Type, uint32 ID, const char *Text);
void SendPlayerData(TConnection *Connection);
void SendPlayerSkills(TConnection *Connection);
void SendPlayerState(TConnection *Connection, uint8 State);
void SendClearTarget(TConnection *Connection);
void SendTalk(TConnection *Connection, uint32 StatementID,
		const char *Sender, int Mode, const char *Text, int Data);
void SendTalk(TConnection *Connection, uint32 StatementID,
		const char *Sender, int Mode, int Channel, const char *Text);
void SendTalk(TConnection *Connection, uint32 StatementID,
		const char *Sender, int Mode, int x, int y, int z, const char *Text);
void SendChannels(TConnection *Connection);
void SendOpenChannel(TConnection *Connection, int Channel);
void SendPrivateChannel(TConnection *Connection, const char *Name);
void SendOpenRequestQueue(TConnection *Connection);
void SendDeleteRequest(TConnection *Connection, const char *Name);
void SendFinishRequest(TConnection *Connection, const char *Name);
void SendCloseRequest(TConnection *Connection);
void SendOpenOwnChannel(TConnection *Connection, int Channel);
void SendCloseChannel(TConnection *Connection, int Channel);
void SendMessage(TConnection *Connection, int Mode, const char *Text, ...) ATTR_PRINTF(3, 4);
void SendSnapback(TConnection *Connection);
void SendOutfit(TConnection *Connection);
void SendBuddyData(TConnection *Connection, uint32 CharacterID, const char *Name, bool Online);
void SendBuddyStatus(TConnection *Connection, uint32 CharacterID, bool Online);
void BroadcastMessage(int Mode, const char *Text, ...) ATTR_PRINTF(2, 3);
void CreateGamemasterRequest(const char *Name, const char *Text);
void DeleteGamemasterRequest(const char *Name);
void InitSending(void);
void ExitSending(void);

// receiving.cc
void ReceiveData(TConnection *Connection);
void ReceiveData(void);
void InitReceiving(void);
void ExitReceiving(void);

#endif // TIBIA_CONNECTIONS_H_
