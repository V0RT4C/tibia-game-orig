#ifndef TIBIA_PROTOCOL_COMMUNICATION_H_
#define TIBIA_PROTOCOL_COMMUNICATION_H_ 1

#include "network/connection/connection.h"
#include "protocol/protocol_enums.h"

#define MAX_COMMUNICATION_THREADS 1100
#define COMMUNICATION_THREAD_STACK_SIZE ((int)kb(64))

struct TPlayerData;

struct TWaitinglistEntry {
	TWaitinglistEntry *Next;
	char Name[30];
	uint32 NextTry;
	bool FreeAccount;
	bool Newbie;
	bool Sleeping;
};

void get_communication_thread_stack(int *StackNumber, void **Stack);
void attach_communication_thread_stack(int StackNumber);
void release_communication_thread_stack(int StackNumber);
void init_communication_thread_stacks(void);
void exit_communication_thread_stacks(void);

bool lag_detected(void);
void net_load(int Amount, bool Send);
void net_load_summary(void);
void net_load_check(void);
void init_load_history(void);
void exit_load_history(void);

bool write_to_socket(TConnection *Connection, uint8 *Buffer, int Size, int MaxSize);
bool send_login_message(TConnection *Connection, int Type, const char *Message, int WaitingTime);
bool send_data(TConnection *Connection);

bool get_waitinglist_entry(const char *Name, uint32 *NextTry, bool *FreeAccount, bool *Newbie);
void insert_waitinglist_entry(const char *Name, uint32 NextTry, bool FreeAccount, bool Newbie);
void delete_waitinglist_entry(const char *Name);
int get_waitinglist_position(const char *Name, bool FreeAccount, bool Newbie);
int check_waiting_time(const char *Name, TConnection *Connection, bool FreeAccount, bool Newbie);

int read_from_socket(TConnection *Connection, uint8 *Buffer, int Size);
bool call_game_thread(TConnection *Connection);
bool check_connection(TConnection *Connection);
TPlayerData *perform_registration(TConnection *Connection, char *PlayerName, uint32 AccountID,
								  const char *PlayerPassword, bool GamemasterClient);
bool handle_login(TConnection *Connection);
bool receive_command(TConnection *Connection);

int get_active_connections(void);
void increment_active_connections(void);
void decrement_active_connections(void);
void communication_thread(int Socket);
int handle_connection(void *Data);

class WebSocketTransport;
void communication_thread_ws(WebSocketTransport *Transport);
int handle_ws_connection(void *Data);

bool open_socket(void);
int acceptor_thread_loop(void *Unused);

void check_threadlib_version(void);
void init_communication(void);
void exit_communication(void);

#endif // TIBIA_PROTOCOL_COMMUNICATION_H_
