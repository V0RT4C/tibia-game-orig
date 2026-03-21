#include "protocol/communication/communication.h"
#include "protocol/websocket_acceptor/websocket_acceptor.h"
#include "config.h"
#include "connections.h"
#include "containers.h"
#include "cr.h"
#include "crypto.h"
#include "network/transport/tcp_transport/tcp_transport.h"
#include "network/transport/websocket_transport/websocket_transport.h"
#include "query.h"
#include "threads.h"
#include "writer.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>

// NOTE(fusion): We seem to add this value of 48 every time `NetLoad` is called,
// and I assume it is to account for IPv4 (20 bytes) and TCP (20 bytes) headers
// although there are still 8 bytes on the table that I'm not sure where are
// coming from.
#define PACKET_AVERAGE_SIZE_OVERHEAD 48

// MAX_COMMUNICATION_THREADS and COMMUNICATION_THREAD_STACK_SIZE are defined in communication.h

#if TIBIA772
static const int TERMINALVERSION[] = {772, 772, 772};
#else
static const int TERMINALVERSION[] = {770, 770, 770};
#endif

static int TCPSocket;
static ThreadHandle AcceptorThread;
static pid_t AcceptorThreadID;
static int ActiveConnections;

static Semaphore RSAMutex(1);
static TRSAPrivateKey PrivateKey;

static TQueryManagerConnectionPool QueryManagerConnectionPool(10);
static int LoadHistory[360];
static int LoadHistoryPointer;
static int TotalLoad;
static int TotalSend;
static int TotalRecv;
static uint32 LagEnd;
static uint32 EarliestFreeAccountAdmissionRound;
static store<TWaitinglistEntry, 100> Waitinglist;
static TWaitinglistEntry *WaitinglistHead;

static Semaphore CommunicationThreadMutex(1);
static bool UseOwnStacks;
static uint8 CommunicationThreadStacks[MAX_COMMUNICATION_THREADS][COMMUNICATION_THREAD_STACK_SIZE];
static pid_t LastUsingCommunicationThread[MAX_COMMUNICATION_THREADS];
static int FreeCommunicationThreadStacks[MAX_COMMUNICATION_THREADS];
static int NumberOfFreeCommunicationThreadStacks;

// Communication Thread Stacks
// =============================================================================
void get_communication_thread_stack(int *StackNumber, void **Stack) {
	*StackNumber = -1;
	*Stack = NULL;

	if (!UseOwnStacks) {
		error("GetCommunicationThreadStack: Library does not support custom stacks.\n");
		return;
	}

	CommunicationThreadMutex.down();
	for (int i = 0; i < NumberOfFreeCommunicationThreadStacks; i += 1) {
		int FreeStack = FreeCommunicationThreadStacks[i];
		if (LastUsingCommunicationThread[FreeStack] == 0 ||
			tgkill(GetGameProcessID(), LastUsingCommunicationThread[FreeStack], 0) == -1) {
			// NOTE(fusion): A little swap and pop action.
			NumberOfFreeCommunicationThreadStacks -= 1;
			FreeCommunicationThreadStacks[i] = FreeCommunicationThreadStacks[NumberOfFreeCommunicationThreadStacks];

			*StackNumber = FreeStack;
			*Stack = CommunicationThreadStacks[FreeStack];
			break;
		}
	}
	CommunicationThreadMutex.up();
}

void attach_communication_thread_stack(int StackNumber) {
	LastUsingCommunicationThread[StackNumber] = gettid();
}

void release_communication_thread_stack(int StackNumber) {
	CommunicationThreadMutex.down();
	FreeCommunicationThreadStacks[NumberOfFreeCommunicationThreadStacks] = StackNumber;
	NumberOfFreeCommunicationThreadStacks += 1;
	CommunicationThreadMutex.up();
}

void init_communication_thread_stacks(void) {
	if (UseOwnStacks) {
		memset(CommunicationThreadStacks, 0xAA, sizeof(CommunicationThreadStacks));
		for (int i = 0; i < MAX_COMMUNICATION_THREADS; i += 1) {
			LastUsingCommunicationThread[i] = 0;
			FreeCommunicationThreadStacks[i] = i;
		}
		NumberOfFreeCommunicationThreadStacks = MAX_COMMUNICATION_THREADS;
	}
}

void exit_communication_thread_stacks(void) {
	if (UseOwnStacks) {
		if (NumberOfFreeCommunicationThreadStacks != MAX_COMMUNICATION_THREADS) {
			error("FreeCommunicationThreadStacks: Not all stacks have been released.\n");
		}

		int HighestStackAddress = -1;
		int LowestStackAddress = COMMUNICATION_THREAD_STACK_SIZE;
		for (int i = 0; i < MAX_COMMUNICATION_THREADS; i += 1) {
			for (int Addr = 0; Addr < COMMUNICATION_THREAD_STACK_SIZE; Addr += 1) {
				if (CommunicationThreadStacks[i][Addr] != 0xAA) {
					if (Addr < LowestStackAddress) {
						LowestStackAddress = Addr;
					}

					if (Addr > HighestStackAddress) {
						HighestStackAddress = Addr;
					}
				}
			}
		}

		// NOTE(fusion): It seems we want to track whether the stack size is
		// too small but I'd argue the method is not very robust.
		if ((HighestStackAddress - LowestStackAddress) > (COMMUNICATION_THREAD_STACK_SIZE / 2)) {
			error("Maximum stack extent: %d..%d\n", LowestStackAddress, HighestStackAddress);
		}
	}
}

// Load History
// =============================================================================
bool lag_detected(void) {
	return RoundNr <= LagEnd;
}

void net_load(int Amount, bool Send) {
	CommunicationThreadMutex.down();
	if (Send) {
		TotalSend += Amount;
	} else {
		TotalRecv += Amount;
	}
	CommunicationThreadMutex.up();
}

void net_load_summary(void) {
	CommunicationThreadMutex.down();
	log_message("netload", "sent:     %d bytes.\n", TotalSend);
	log_message("netload", "received: %d bytes.\n", TotalRecv);
	TotalSend = 0;
	TotalRecv = 0;
	CommunicationThreadMutex.up();
}

void net_load_check(void) {
	static int LastRecv;

	int DeltaRecv = TotalRecv - LastRecv;
	LastRecv = TotalRecv;
	if (DeltaRecv < 0) {
		return;
	}

	int DeltaRecvPerPlayer = 0;
	int PlayersOnline = GetPlayersOnline();
	if (PlayersOnline > 0) {
		DeltaRecvPerPlayer = DeltaRecv / PlayersOnline;
	}

	TotalLoad -= LoadHistory[LoadHistoryPointer];
	TotalLoad += DeltaRecvPerPlayer;
	LoadHistory[LoadHistoryPointer] = DeltaRecvPerPlayer;
	LoadHistoryPointer += 1;
	if (LoadHistoryPointer >= NARRAY(LoadHistory)) {
		LoadHistoryPointer = 0;
	}

	// NOTE(fusion): Running this lag check only makes sense if `LoadHistory`
	// is filled up which won't be the case until this function executes at
	// least as many times as there are entries in `LoadHistory`.
	//	Looking at `AdvanceGame`, we see that this function is called every
	// 10 rounds, giving us the value of `EarliestLagCheckRound`.
	constexpr uint32 EarliestLagCheckRound = 10 * NARRAY(LoadHistory);
	if (RoundNr >= EarliestLagCheckRound && PlayersOnline >= 50) {
		int AvgDeltaRecvPerPlayer = (TotalLoad / NARRAY(LoadHistory));
		if (DeltaRecvPerPlayer < (AvgDeltaRecvPerPlayer / 2)) {
			log_message("game", "Lag detected!\n");
			LagEnd = RoundNr + 30;

			// NOTE(fusion): This formula looks weird but when we take `MaxPlayers`
			// and `PremiumPlayerBuffer` to be 950 and 150 (taken from the config
			// file, although their values are now loaded from the database), we
			// get a line with negative values when the number of players online
			// is less than 650, and exactly 60 when it is 950. Since the delay
			// is 60 when `PremiumPlayerBuffer` is zero, I don't think this is a
			// coincidence.
			int FreeAccountAdmissionDelay = 60;
			if (PremiumPlayerBuffer != 0) {
				FreeAccountAdmissionDelay = (PlayersOnline - (MaxPlayers - PremiumPlayerBuffer * 2));
				FreeAccountAdmissionDelay = (FreeAccountAdmissionDelay * 30) / PremiumPlayerBuffer;
				if (FreeAccountAdmissionDelay < 0) {
					FreeAccountAdmissionDelay = 0;
				}
			}

			uint32 FreeAccountAdmissionRound = RoundNr + (uint32)FreeAccountAdmissionDelay;
			if (EarliestFreeAccountAdmissionRound < FreeAccountAdmissionRound) {
				EarliestFreeAccountAdmissionRound = FreeAccountAdmissionRound;
			}

			TConnection *Connection = get_first_connection();
			while (Connection != NULL) {
				if (Connection->live()) {
					Connection->emergency_ping();
				}
				Connection = get_next_connection();
			}
		}
	}
}

void init_load_history(void) {
	for (int i = 0; i < NARRAY(LoadHistory); i += 1) {
		LoadHistory[i] = 0;
	}
	LoadHistoryPointer = 0;
	TotalLoad = 0;
	TotalSend = 0;
	TotalRecv = 0;
	LagEnd = 0;
	EarliestFreeAccountAdmissionRound = 0;
	init_log("netload");
}

void exit_load_history(void) {
	// no-op
}

// Connection Output
// =============================================================================
static constexpr int GetPacketSize(int DataSize) {
	// NOTE(fusion): This can be annoying but we're compiling with C++11 which
	// means constexpr functions can only have a single return statement.
#if __cplusplus > 201103L
	int EncryptedSize = ((DataSize + 2) + 7) & ~7;
	int PacketSize = EncryptedSize + 2;
	return PacketSize;
#else
	return (((DataSize + 2) + 7) & ~7) + 2;
#endif
}

bool write_to_socket(TConnection *Connection, uint8 *Buffer, int Size, int MaxSize) {
	// IMPORTANT(fusion): The final packet will have the following layout:
	//	PLAIN:
	//		0 .. 2 => Encrypted Size
	//	ENCRYPTED:
	//		2 .. 4 => Data Size
	//		4 ..   => Data + Padding
	//
	//	The caller must ensure `Buffer` has four extra bytes at the beginning so
	// the packet and payload sizes can be written. It should also ensure that
	// `(MaxSize % 8) == 2` so we can always add the necessary amount of padding
	// for encryption.
	ASSERT(Size >= 4 && Size <= MaxSize && MaxSize <= UINT16_MAX);

	int DataSize = Size - 4;
	while ((Size % 8) != 2 && Size < MaxSize) {
		Buffer[Size] = rand_r(&Connection->RandomSeed);
		Size += 1;
	}

	if ((Size % 8) != 2) {
		error("WriteToSocket: Failed to add padding (Size: %d, MaxSize: %d)\n", Size, MaxSize);
		return false;
	}

	TWriteBuffer WriteBuffer(Buffer, 4);
	WriteBuffer.writeWord((uint16)(Size - 2));
	WriteBuffer.writeWord((uint16)(DataSize));
	for (int i = 2; i < Size; i += 8) {
		Connection->SymmetricKey.encrypt(&Buffer[i]);
	}

	int Attempts = 50;
	int BytesToWrite = Size;
	uint8 *WritePtr = Buffer;
	while (BytesToWrite > 0) {
		int BytesWritten = Connection->Transport->write(WritePtr, BytesToWrite);
		if (BytesWritten > 0) {
			BytesToWrite -= BytesWritten;
			WritePtr += BytesWritten;
		} else if (BytesWritten == 0) {
			// TODO(fusion): Can this even happen without an error?
			error("WriteToSocket: Error %d while sending to socket %d.\n", errno, Connection->get_socket());
			return false;
		} else if (errno != EINTR) {
			if (errno != EAGAIN || Attempts <= 0) {
				if (errno == ECONNRESET || errno == EPIPE || errno == EAGAIN) {
					log_message("game", "Connection on socket %d has broken down.\n", Connection->get_socket());
				} else {
					error("WriteToSocket: Error %d while sending to socket %d.\n", errno, Connection->get_socket());
				}
				return false;
			}

			DelayThread(0, 100000);
			Attempts -= 1;
		}
	}

	net_load(PACKET_AVERAGE_SIZE_OVERHEAD + Size, true);
	return true;
}

bool send_login_message(TConnection *Connection, int Type, const char *Message, int WaitingTime) {
	if (Type != LOGIN_MESSAGE_ERROR && Type != LOGIN_MESSAGE_PREMIUM && Type != LOGIN_MESSAGE_WAITINGLIST) {
		error("SendLoginMessage: Invalid message type %d.\n", Type);
		return false;
	}

	if (Message == NULL) {
		error("SendLoginMessage: Message is NULL.\n");
		return false;
	}

	if (Type == LOGIN_MESSAGE_WAITINGLIST && (WaitingTime < 0 || WaitingTime > UINT8_MAX)) {
		error("SendLoginMessage: Invalid waiting time %d.\n", WaitingTime);
		return false;
	}

	if (strlen(Message) > 290) {
		error("SendLoginMessage: Message too long (%s).\n", Message);
		return false;
	}

	// IMPORTANT(fusion): This is doing the same thing as `SendData` but in a
	// smaller scale and building the packet directly instead of using the
	// connection's write buffer.
	try {
		uint8 Data[GetPacketSize(300)];
		TWriteBuffer WriteBuffer(Data, sizeof(Data));
		WriteBuffer.writeWord(0); // EncryptedSize
		WriteBuffer.writeWord(0); // DataSize
		WriteBuffer.writeByte((uint8)Type);
		WriteBuffer.write_string(Message);
		if (Type == LOGIN_MESSAGE_WAITINGLIST) {
			WriteBuffer.writeByte(WaitingTime);
		}

		return write_to_socket(Connection, Data, WriteBuffer.Position, WriteBuffer.Size);
	} catch (const char *str) {
		error("SendLoginMessage: Error while filling the buffer (%s)\n", str);
		return false;
	}
}

bool send_data(TConnection *Connection) {
	if (Connection == NULL) {
		error("SendData: Connection is NULL.\n");
		return false;
	}

	int DataSize = Connection->NextToCommit - Connection->NextToSend;
	int PacketSize = GetPacketSize(DataSize);

	// TODO(fusion): The maximum size of a packet depends on the size of
	// `Connection->OutData` and I don't think we'd have a problem with
	// having a constant size buffer on the stack here although with such
	// a small stack size (64KB with our own stacks) it could become a
	// problem.
	uint8 *Buffer = (uint8 *)alloca(PacketSize);
	TWriteBuffer WriteBuffer(Buffer, PacketSize);
	WriteBuffer.writeWord(0); // EncryptedSize
	WriteBuffer.writeWord(0); // DataSize

	// NOTE(fusion): `Connection->OutData` is a ring buffer so we need to check
	// if the data we're currently sending is wrapping around, in which case we'd
	// need to copy two separate regions instead of a single contiguous one.
	constexpr int OutDataSize = sizeof(Connection->OutData);
	int DataStart = Connection->NextToSend % OutDataSize;
	int DataEnd = DataStart + DataSize;
	if (DataEnd < OutDataSize) {
		WriteBuffer.writeBytes(&Connection->OutData[DataStart], DataSize);
	} else {
		WriteBuffer.writeBytes(&Connection->OutData[DataStart], OutDataSize - DataStart);
		WriteBuffer.writeBytes(&Connection->OutData[0], DataEnd - OutDataSize);
	}

	bool Result = write_to_socket(Connection, Buffer, WriteBuffer.Position, WriteBuffer.Size);
	if (Result) {
		Connection->NextToSend += DataSize;
	}
	return Result;
}

// Waiting List
// =============================================================================
bool get_waitinglist_entry(const char *Name, uint32 *NextTry, bool *FreeAccount, bool *Newbie) {
	bool Result = false;
	CommunicationThreadMutex.down();
	TWaitinglistEntry *Entry = WaitinglistHead;
	while (Entry != NULL) {
		if (stricmp(Entry->Name, Name) == 0) {
			break;
		}
		Entry = Entry->Next;
	}

	if (Entry != NULL) {
		*NextTry = Entry->NextTry;
		*FreeAccount = Entry->FreeAccount;
		*Newbie = Entry->Newbie;
		Result = true;
	}
	CommunicationThreadMutex.up();
	return Result;
}

void insert_waitinglist_entry(const char *Name, uint32 NextTry, bool FreeAccount, bool Newbie) {
	bool NewEntry = false;
	CommunicationThreadMutex.down();
	TWaitinglistEntry *Prev = NULL;
	TWaitinglistEntry *Entry = WaitinglistHead;
	while (Entry != NULL) {
		if (stricmp(Entry->Name, Name) == 0) {
			break;
		}
		Prev = Entry;
		Entry = Entry->Next;
	}

	if (Entry == NULL) {
		Entry = Waitinglist.getFreeItem();
		Entry->Next = NULL;
		strcpy(Entry->Name, Name);
		Entry->NextTry = NextTry;
		Entry->FreeAccount = FreeAccount;
		Entry->Newbie = Newbie;
		Entry->Sleeping = false;
		if (Prev != NULL) {
			Prev->Next = Entry;
		} else {
			WaitinglistHead = Entry;
		}
		NewEntry = true;
	} else {
		Entry->NextTry = NextTry;
		Entry->FreeAccount = FreeAccount;
		Entry->Newbie = Newbie;
	}
	CommunicationThreadMutex.up();

	if (NewEntry) {
		log_message("queue", "Adding %s to the waiting list.\n", Name);
	}
}

void delete_waitinglist_entry(const char *Name) {
	CommunicationThreadMutex.down();
	TWaitinglistEntry *Prev = NULL;
	TWaitinglistEntry *Entry = WaitinglistHead;
	while (Entry != NULL) {
		if (stricmp(Entry->Name, Name) == 0) {
			break;
		}
		Prev = Entry;
		Entry = Entry->Next;
	}

	if (Entry != NULL) {
		if (Prev != NULL) {
			Prev->Next = Entry->Next;
		} else {
			WaitinglistHead = Entry->Next;
		}
		Waitinglist.putFreeItem(Entry);
	}
	CommunicationThreadMutex.up();
}

int get_waitinglist_position(const char *Name, bool FreeAccount, bool Newbie) {
	int FreeNewbies = 0;
	int FreeVeterans = 0;
	int PremiumNewbies = 0;
	int PremiumVeterans = 0;

	CommunicationThreadMutex.down();
	// NOTE(fusion): Remove inactive entries from the start of the list.
	while (WaitinglistHead != NULL && RoundNr > (WaitinglistHead->NextTry + 60)) {
		TWaitinglistEntry *Next = WaitinglistHead->Next;
		Waitinglist.putFreeItem(WaitinglistHead);
		WaitinglistHead = Next;
	}

	// NOTE(fusion): Count players up until we find the player's entry or reach
	// the end of the queue.
	TWaitinglistEntry *Entry = WaitinglistHead;
	while (Entry != NULL) {
		if (stricmp(Entry->Name, Name) == 0) {
			break;
		}

		if (!Entry->Sleeping) {
			if (RoundNr > (Entry->NextTry + 5)) {
				Entry->Sleeping = true;
			} else if (Entry->FreeAccount) {
				if (Entry->Newbie) {
					FreeNewbies += 1;
				} else {
					FreeVeterans += 1;
				}
			} else {
				if (Entry->Newbie) {
					PremiumNewbies += 1;
				} else {
					PremiumVeterans += 1;
				}
			}
		}

		Entry = Entry->Next;
	}
	CommunicationThreadMutex.up();

	int Result = 1;
	if (FreeAccount) {
		Result += PremiumVeterans + FreeVeterans;
		if (Newbie) {
			Result += PremiumNewbies + FreeNewbies;
		} else if (GetNewbiesOnline() < (MaxNewbies - PremiumNewbieBuffer)) {
			Result += FreeNewbies;
		}
	} else {
		Result += PremiumVeterans;
		if (Newbie || GetNewbiesOnline() < MaxNewbies) {
			Result += PremiumNewbies;
		}
	}
	return Result;
}

int check_waiting_time(const char *Name, TConnection *Connection, bool FreeAccount, bool Newbie) {
	int WaitingTime = 0;
	const char *Reason = NULL;
	int Position = get_waitinglist_position(Name, FreeAccount, Newbie);
	int PlayersOnline = GetPlayersOnline();
	int NewbiesOnline = GetNewbiesOnline();
	if ((PlayersOnline + Position) > get_order_buffer_space()) {
		print(3, "Order buffer is almost full.\n");
		Reason = "The server is overloaded.";
		WaitingTime = (Position / 2) + 10;
	} else if (FreeAccount) {
		if (EarliestFreeAccountAdmissionRound > RoundNr) {
			print(3, "No free accounts allowed after mass kick.\n");
			Reason = "The server is overloaded.\n"
					 "Only players with premium accounts\n"
					 "are admitted at the moment.";
			WaitingTime = (int)(EarliestFreeAccountAdmissionRound - RoundNr);
			WaitingTime += Position / 2;
		} else if ((PlayersOnline + Position) > (MaxPlayers - PremiumPlayerBuffer)) {
			print(3, "No more room for free accounts.\n");
			Reason = "Too many players online.\n"
					 "Only players with premium accounts\n"
					 "are admitted at the moment.";
			WaitingTime = Position / 2 + 5;
		} else if (Newbie && (NewbiesOnline + Position) > (MaxNewbies - PremiumNewbieBuffer)) {
			print(3, "No more room for newbies with free accounts.\n");
			Reason = "There are too many players online\n"
					 "on the beginners' island, Rookgaard.\n"
					 "Only players with premium accounts\n"
					 "are admitted at the moment.";
			WaitingTime = Position / 2 + 5;
		}
	} else {
		if ((PlayersOnline + Position) > MaxPlayers) {
			print(3, "Too many players online.\n");
			Reason = "There are too many players online.";
			WaitingTime = Position / 2 + 3;
		} else if (Newbie && (NewbiesOnline + Position) > MaxNewbies) {
			print(3, "No more room for newbies.\n");
			Reason = "There are too many players online\n"
					 "on the beginners' island, Rookgaard.";
			WaitingTime = Position / 2 + 3;
		}
	}

	if (WaitingTime > 240) {
		WaitingTime = 240;
	}

	if (WaitingTime > 0) {
		char Message[250];
		snprintf(Message, sizeof(Message), "%s\n\nYou are at place %d on the waiting list.", Reason, Position);
		send_login_message(Connection, LOGIN_MESSAGE_WAITINGLIST, Message, WaitingTime);
	}

	return WaitingTime;
}

// Connection Input
// =============================================================================
int read_from_socket(TConnection *Connection, uint8 *Buffer, int Size) {
	int Attempts = 50;
	int BytesToRead = Size;
	uint8 *ReadPtr = Buffer;
	while (BytesToRead > 0) {
		int BytesRead = Connection->Transport->read(ReadPtr, BytesToRead);
		if (BytesRead > 0) {
			BytesToRead -= BytesRead;
			ReadPtr += BytesRead;
		} else if (BytesRead == 0) {
			// NOTE(fusion): TCP FIN with no more data to read.
			break;
		} else if (errno != EINTR) {
			// TODO(fusion): This is probably overkill. We only need a way to
			// differentiate between an ERROR/TIMEOUT and NODATA.
			if (errno != EAGAIN) {
				return -errno;
			} else if (Attempts <= 0) {
				return -ETIMEDOUT;
			} else if (BytesToRead == Size) {
				return -EAGAIN;
			}

			DelayThread(0, 100000);
			Attempts -= 1;
		}
	}
	return Size - BytesToRead;
}

bool call_game_thread(TConnection *Connection) {
	if (GameRunning()) {
		Connection->WaitingForACK = true;
		if (tgkill(GetGameProcessID(), GetGameThreadID(), SIGUSR1) == -1) {
			error("CallGameThread: Can't send SIGUSR1 to thread %d/%d: (%d) %s\n", GetGameProcessID(),
				  GetGameThreadID(), errno, strerror(errno));
			send_login_message(Connection, LOGIN_MESSAGE_ERROR, "The server is not online.\nPlease try again later.",
							   -1);
			return false;
		}
	}
	return true;
}

bool check_connection(TConnection *Connection) {
	// For WebSocket: eventfd POLLIN means "data waiting", not "peer closed".
	// Use the transport's connected state instead.
	if (!Connection->Transport->is_connected()) {
		return false;
	}

	// For TCP: poll to check if the peer closed the connection.
	// For WebSocket: the is_connected() check above is sufficient;
	// poll on eventfd would give false negatives when data is queued.
	struct pollfd pollfd = {};
	pollfd.fd = Connection->Transport->get_fd();
	pollfd.events = POLLIN;
	int ret = poll(&pollfd, 1, 0);
	if (ret < 0) return false;

	// For eventfd (WebSocket), POLLIN just means data is ready — not disconnect.
	// Only treat POLLIN as disconnect for real sockets (when POLLHUP is also set).
	if ((pollfd.revents & POLLIN) && (pollfd.revents & POLLHUP)) {
		return false;
	}

	return true;
}

// NOTE(fusion): `PlayerName` is an input and output parameter. It will contain
// the correct player name with upper and lower case letters if the player is
// actually logged in.
TPlayerData *perform_registration(TConnection *Connection, char *PlayerName, const char *Email,
								  const char *PlayerPassword, bool GamemasterClient) {
	TQueryManagerPoolConnection QueryManagerConnection(&QueryManagerConnectionPool);
	if (!QueryManagerConnection) {
		error("PerformRegistration: Cannot establish connection to the query manager.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Internal error, closing connection.", -1);
		return NULL;
	}

	if (!check_connection(Connection)) {
		return NULL;
	}

	// TODO(fusion): What a disaster. The size of these arrays are are hardcoded
	// and should be declared constants somewhere.
	uint32 CharacterID;
	int Sex;
	char Guild[31]; // MAX_NAME_LENGTH ?
	char Rank[31];  // MAX_NAME_LENGTH ?
	char Title[31]; // MAX_NAME_LENGTH ?
	int NumberOfBuddies;
	uint32 BuddyIDs[100];     // MAX_BUDDIES ?
	char BuddyNames[100][30]; // MAX_BUDDIES, MAX_NAME_LENGTH ?
	uint8 Rights[12];         // MAX_RIGHT_BYTES ?
	bool PremiumAccountActivated;
	uint32 AccountID = 0;
	int ResolveCode = QueryManagerConnection->resolveEmail(Email, &AccountID);
	if(ResolveCode != 0){
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Email or password is not correct.", -1);
		return NULL;
	}

	int LoginCode =
		QueryManagerConnection->loginGame(AccountID, PlayerName, PlayerPassword, Connection->get_ip_address(),
										  PrivateWorld, false, GamemasterClient, &CharacterID, &Sex, Guild, Rank, Title,
										  &NumberOfBuddies, BuddyIDs, BuddyNames, Rights, &PremiumAccountActivated);
	switch (LoginCode) {
	case 0: {
		// NOTE(fusion): Login successful.
		break;
	}

	case 1: {
		print(3, "Player does not exist.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "Character doesn't exist.\n"
						   "Create a new character on the Tibia website\n"
						   "at \"www.tibia.com\".",
						   -1);
		return NULL;
	}

	case 2: {
		print(3, "Player has been deleted.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "Character doesn't exist.\n"
						   "Create a new character on the Tibia website.",
						   -1);
		return NULL;
	}

	case 3: {
		print(3, "Player does not live on this world.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "Character doesn't live on this world.\n"
						   "Please login on the right world.",
						   -1);
		return NULL;
	}

	case 4: {
		print(3, "Player is not invited.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "This world is private and you have not been invited to play on it.", -1);
		return NULL;
	}

	case 6: {
		log_message("game", "Wrong password for player %s; login from %s.\n", PlayerName, Connection->get_ip_address());
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Accountnumber or password is not correct.", -1);
		return NULL;
	}

	case 7: {
		log_message("game", "Player %s blocked; login from %s.\n", PlayerName, Connection->get_ip_address());
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Account disabled for five minutes. Please wait.", -1);
		return NULL;
	}

	case 8: {
		log_message("game", "Account of player %s has been deleted.\n", PlayerName);
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Accountnumber or password is not correct.", -1);
		return NULL;
	}

	case 9: {
		log_message("game", "IP address %s blocked for player %s.\n", Connection->get_ip_address(), PlayerName);
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "IP address blocked for 30 minutes. Please wait.", -1);
		return NULL;
	}

	case 10: {
		print(3, "Account is banished.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Your account is banished.", -1);
		return NULL;
	}

	case 11: {
		print(3, "Character must be renamed.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Your character is banished because of his/her name.", -1);
		return NULL;
	}

	case 12: {
		print(3, "IP address is blocked.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Your IP address is banished.", -1);
		return NULL;
	}

	case 13: {
		print(3, "Other characters of the same account are already logged in.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "You may only login with one character\n"
						   "of your account at the same time.",
						   -1);
		return NULL;
	}

	case 14: {
		print(3, "Login with gamemaster client on non-gamemaster account.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "You may only login with a Gamemaster account.", -1);
		return NULL;
	}

	case 15: {
		print(3, "Wrong account number %u for %s.\n", AccountID, PlayerName);
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Login failed due to corrupt data.", -1);
		return NULL;
	}

	case -1: {
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Internal error, closing connection.", -1);
		return NULL;
	}

	default: {
		error("PerformRegistration: Unknown return value from QueryManager.\n");
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Internal error, closing connection.", -1);
		return NULL;
	}
	}

	// TODO(fusion): Again?
	if (!check_connection(Connection)) {
		QueryManagerConnection->decrementIsOnline(CharacterID);
		return NULL;
	}

	// TODO(fusion): This should probably checked beforehand or also dispatch
	// `decrementIsOnline`.
	if (AccountID == 0) {
		error("PerformRegistration: Player %s has not been assigned to an account yet.\n", PlayerName);
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "Character is not assigned to an account.\n"
						   "Perform this on the Tibia website\n"
						   "at \"www.tibia.com\".",
						   -1);
		return NULL;
	}

	// TODO(fusion): We were also adding a null terminator to `PlayerName` here
	// for whatever reason. I assume it is a compiler artifact because we already
	// use it in the condition block just above so...
	// PlayerName[29] = 0;

	if (PremiumAccountActivated) {
		send_login_message(Connection, LOGIN_MESSAGE_PREMIUM,
						   "Your Premium Account is now activated.\n"
						   "Have a lot of fun in Tibia.",
						   -1);
	}

	log_message("game", "Player %s logging in on socket %d from %s.\n", PlayerName, Connection->get_socket(),
				Connection->get_ip_address());

	TPlayerData *PlayerData = assign_player_pool_slot(CharacterID, true);
	if (PlayerData == NULL) {
		error("PerformRegistration: Cannot assign a slot for player data.\n");
		QueryManagerConnection->decrementIsOnline(CharacterID);
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "There are too many players online.\n"
						   "Please try again later.",
						   -1);
		return NULL;
	}

	bool Locked = (PlayerData->Locked == gettid());

	// TODO(fusion): How come this isn't a race condition? Perhaps these are
	// constant and can only change when loaded from the database?
	if (Locked || PlayerData->AccountID == 0) {
		PlayerData->AccountID = AccountID;
		PlayerData->Sex = Sex;
		strcpy(PlayerData->Name, PlayerName);
		memcpy(PlayerData->Rights, Rights, sizeof(Rights));
		strcpy(PlayerData->Guild, Guild);
		strcpy(PlayerData->Rank, Rank);
		strcpy(PlayerData->Title, Title);
	}

	if (Locked && PlayerData->Buddies == 0) {
		PlayerData->Buddies = NumberOfBuddies;
		for (int i = 0; i < NumberOfBuddies; i += 1) {
			PlayerData->Buddy[i] = BuddyIDs[i];
			strcpy(PlayerData->BuddyName[i], BuddyNames[i]);
		}
	}

	return PlayerData;
}

bool handle_login(TConnection *Connection) {
	// TODO(fusion): Exception handling keeps getting crazier.

	TReadBuffer InputBuffer(Connection->InData, Connection->InDataSize);
	try {
		uint8 Command = InputBuffer.readByte();
		if (Command != CL_CMD_LOGIN_REQUEST) {
			print(3, "Invalid init command %d.\n", Command);
			return false;
		}
	} catch (const char *str) {
		error("HandleLogin: Error while reading the command (%s).\n", str);
		return false;
	}

	int TerminalType;
	int TerminalVersion;
	bool GamemasterClient;
	char Email[51];
	char PlayerName[30];
	char PlayerPassword[30];
	try {
#if TIBIA772
		// IMPORTANT(fusion): With 7.72, the terminal type and version are brought
		// outside the asymmetric data. This is probably to maintain some level of
		// backwards compatibility with versions before 7.7, given that it was the
		// first encrypted protocol.
		TerminalType = (int)InputBuffer.readWord();
		TerminalVersion = (int)InputBuffer.readWord();
#endif

		// IMPORTANT(fusion): Without a checksum, there is no way of validating the
		// asymmetric data. The best we can do is to verify that the first plaintext
		// byte is ZERO, but that alone isn't enough.
		// TODO(fusion): Using `SendLoginMessage` before initializing the symmetric
		// key will result in gibberish being sent back to the client.
		uint8 AsymmetricData[128];
		InputBuffer.readBytes(AsymmetricData, 128);
		RSAMutex.down();
		if (!PrivateKey.decrypt(AsymmetricData) || AsymmetricData[0] != 0) {
			RSAMutex.up();
			error("HandleLogin: Error during decryption.\n");
			send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Login failed due to corrupt data.", -1);
			return false;
		}
		RSAMutex.up();

		TReadBuffer ReadBuffer(AsymmetricData, 128);
		ReadBuffer.readByte(); // always zero
		Connection->SymmetricKey.init(&ReadBuffer);
#if !TIBIA772
		TerminalType = (int)ReadBuffer.readWord();
		TerminalVersion = (int)ReadBuffer.readWord();
#endif
		GamemasterClient = ReadBuffer.readByte() != 0;
		ReadBuffer.read_string(Email, sizeof(Email));
		ReadBuffer.read_string(PlayerName, sizeof(PlayerName));
		ReadBuffer.read_string(PlayerPassword, sizeof(PlayerPassword));
	} catch (const char *str) {
		print(3, "Error while reading the login data (%s).\n", str);
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Login failed due to corrupt data.", -1);
		return false;
	}

	if (PlayerName[0] == 0) {
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "You must enter a character name.", -1);
		return false;
	}

	if (TerminalType < 0 || TerminalType >= NARRAY(TERMINALVERSION) ||
		TERMINALVERSION[TerminalType] != TerminalVersion) {
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "Your terminal version is too old.\n"
						   "Please get a new version at\n"
						   "http://www.tibia.com.",
						   -1);
		return false;
	}

	if (!GameRunning()) {
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "The server is not online.\n"
						   "Please try again later.",
						   -1);
		return false;
	}

	if (GameStarting()) {
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "The game is just starting.\n"
						   "Please try again later.",
						   -1);
		return false;
	}

	if (GameEnding()) {
		send_login_message(Connection, LOGIN_MESSAGE_ERROR,
						   "The game is just going down.\n"
						   "Please try again later.",
						   -1);
		return false;
	}

	// NOTE(fusion): Check waitlist entry.
	bool WasInWaitingList = false;
	while (true) {
		uint32 NextTry;
		bool FreeAccount;
		bool Newbie;
		if (!get_waitinglist_entry(PlayerName, &NextTry, &FreeAccount, &Newbie)) {
			print(3, "Player not on waiting list.\n");
			break;
		}

		print(3, "Player on waiting list.\n");
		if (NextTry > RoundNr) {
			int WaitingTime = (int)(NextTry - RoundNr);
			log_message("queue", "%s is logging in %d seconds too early.\n", PlayerName, WaitingTime);
			send_login_message(Connection, LOGIN_MESSAGE_WAITINGLIST, "It's not your turn yet.", WaitingTime);
			return false;
		}

		if (RoundNr > (NextTry + 60)) {
			log_message("queue", "%s is logging in %d seconds too late.\n", PlayerName, (RoundNr - NextTry));
			delete_waitinglist_entry(PlayerName);
			continue;
		}

		int WaitingTime = check_waiting_time(PlayerName, Connection, FreeAccount, Newbie);
		if (WaitingTime > 0) {
			NextTry = RoundNr + (uint32)WaitingTime;
			insert_waitinglist_entry(PlayerName, NextTry, FreeAccount, Newbie);
			return false;
		}

		delete_waitinglist_entry(PlayerName);
		WasInWaitingList = true;
		break;
	}

	TPlayerData *Slot = perform_registration(Connection, PlayerName, Email, PlayerPassword, GamemasterClient);
	if (Slot == NULL) {
		return false;
	}

	bool SlotLocked = (Slot->Locked == gettid());

	// NOTE(fusion): These checks would have been already made if the player was
	// in the waiting list so they'd be redundant.
	if (!WasInWaitingList) {
		bool BlockLogin = false;
		bool FreeAccount = !CheckBit(Slot->Rights, PREMIUM_ACCOUNT);
		bool Newbie = Slot->Profession == PROFESSION_NONE && !CheckBit(Slot->Rights, NO_LOGOUT_BLOCK);

		bool PremiumOnly = (MaxPlayers == PremiumPlayerBuffer) || (Newbie && MaxNewbies == PremiumNewbieBuffer);
		if (!BlockLogin && FreeAccount && PremiumOnly) {
			send_login_message(Connection, LOGIN_MESSAGE_ERROR,
							   "Only players with premium accounts\n"
							   "are allowed to enter this world.",
							   -1);
			BlockLogin = true;
		}

		if (!BlockLogin && !is_player_online(PlayerName)) {
			int WaitingTime = check_waiting_time(PlayerName, Connection, FreeAccount, Newbie);
			if (WaitingTime > 0) {
				uint32 NextTry = RoundNr + (uint32)WaitingTime;
				insert_waitinglist_entry(PlayerName, NextTry, FreeAccount, Newbie);
				BlockLogin = true;
			}
		}

		if (BlockLogin) {
			// TODO(fusion): This is probably an inlined function.
			TQueryManagerPoolConnection QueryManagerConnection(&QueryManagerConnectionPool);
			if (QueryManagerConnection) {
				QueryManagerConnection->decrementIsOnline(Slot->CharacterID);
			} else {
				error("HandleLogin: Cannot establish connection to the query manager.\n");
			}

			if (SlotLocked) {
				release_player_pool_slot(Slot);
			} else {
				decrease_player_pool_slot_sticky(Slot);
			}
			return false;
		}
	}

	if (SlotLocked) {
		increase_player_pool_slot_sticky(Slot);
		release_player_pool_slot(Slot);
	}

	// NOTE(fusion): Rewrite packet in a different format, ready for the main
	// thread to interpret.
	TWriteBuffer WriteBuffer(Connection->InData + 2, sizeof(Connection->InData) - 2);
	try {
		WriteBuffer.writeByte(CL_CMD_LOGIN);
		WriteBuffer.writeWord((int)TerminalType);
		WriteBuffer.writeWord((int)TerminalVersion);
		WriteBuffer.writeQuad(Slot->CharacterID);
	} catch (const char *str) {
		error("HandleLogin: Error while assembling the login packet (%s).\n", str);
		send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Internal error, closing connection.", -1);
		return false;
	}

	Connection->NextToSend = 0;
	Connection->NextToCommit = 0;
	Connection->InDataSize = WriteBuffer.Position;
	Connection->NextToWrite = 0;

	Connection->login();
	return call_game_thread(Connection);
}

bool receive_command(TConnection *Connection) {
	// IMPORTANT(fusion): The return value of this function is used to determine
	// whether the connection should be closed. Returning true will maintain it
	// open. It is a weird convention but w/e.

	if (Connection == NULL) {
		error("ReceiveCommand: Connection is NULL.\n");
		return false;
	}

	while (!Connection->WaitingForACK) {
		uint8 Help[2];
		int BytesRead = read_from_socket(Connection, Help, 2);
		if (BytesRead == 0) {
			// NOTE(fusion): Peer has closed the connection and there was no
			// more data to read.
			return false;
		} else if (BytesRead < 0) {
			// NOTE(fusion): There was either no data, some data then a timeout,
			// or a connection error. We only want to keep the connection open
			// in case there was no data, to allow us to return to the connection
			// loop in a consistent state.
			return (BytesRead == -EAGAIN);
		}

		// NOTE(fusion): It doesn't make sense to continue if we couldn't read
		// the packet's length completely. We're already using TCP which doesn't
		// drop data so it would only compound into more errors.
		if (BytesRead != 2) {
			net_load(PACKET_AVERAGE_SIZE_OVERHEAD + BytesRead, false);
			print(3, "Too little data on socket %d.\n", BytesRead);
			return false;
		}

		// TODO(fusion): Size is encoded as a little endian uint16. We should
		// have a few helper functions to assist with buffer reading.
		int Size = ((uint16)Help[0] | ((uint16)Help[1] << 8));

		// NOTE(fusion): It doesn't make sense to continue if the client didn't
		// correctly size its packet.
		if (Size == 0 || Size > (int)sizeof(Connection->InData)) {
			print(3, "Packet on socket %d too large or empty, discarding (%d bytes)\n", Connection->get_socket(), Size);
			return false;
		}

		BytesRead = read_from_socket(Connection, &Connection->InData[0], Size);
		if (BytesRead < 0) {
			// NOTE(fusion): It doesn't make sense to call `SendLoginMessage` before
			// the key exchange has completed, which happens after login.
			if (Connection->State != CONNECTION_CONNECTED) {
				send_login_message(Connection, LOGIN_MESSAGE_ERROR, "Internal error, closing connection.", -1);
			}
			return false;
		}

		net_load(PACKET_AVERAGE_SIZE_OVERHEAD + BytesRead, false);

		// NOTE(fusion): It doesn't make sense to continue if we didn't receive
		// the whole packet. We're already using TCP which doesn't drop data so
		// it would only compound into more errors.
		if (BytesRead != Size) {
			return false;
		}

		if (Connection->State == CONNECTION_CONNECTED) {
			Connection->stop_login_timer();
			Connection->InDataSize = Size;
			if (!handle_login(Connection)) {
				return false;
			}
		} else {
			// NOTE(fusion): It doesn't make sense to continue if the client didn't
			// correctly size its packet.
			if ((Size % 8) != 0) {
				print(3, "Invalid packet length %d for encrypted packet from %s.\n", Size, Connection->get_name());
				return false;
			}

			for (int i = 0; i < Size; i += 8) {
				Connection->SymmetricKey.decrypt(&Connection->InData[i]);
			}

			// NOTE(fusion): It doesn't make sense to continue if the client didn't
			// correctly size its payload.
			int PlainSize = ((uint16)Connection->InData[0]) | ((uint16)Connection->InData[1] << 8);
			if (PlainSize == 0 || (PlainSize + 2) > Size) {
				print(3, "Payload (%d bytes) of packet on socket %d too large or empty.\n", PlainSize,
					  Connection->get_socket());
				return false;
			}

			Connection->InDataSize = PlainSize;
			if (!call_game_thread(Connection)) {
				return false;
			}
		}
	}

	// NOTE(fusion): We set `SigIOPending` here to signal that there could be more
	// packets already queued up, in which case `CommunicationThread` will attempt
	// to read without waiting for another `SIGIO`.
	//	The only way to know there is no more data on a socket's receive buffer is
	// when `read` returns `EAGAIN` which is handled when `ReadFromSocket` returns
	// a negative value.
	Connection->SigIOPending = true;
	return true;
}

// Communication Thread
// =============================================================================
int get_active_connections(void) {
	CommunicationThreadMutex.down();
	int Result = ActiveConnections;
	CommunicationThreadMutex.up();
	return Result;
}

void increment_active_connections(void) {
	CommunicationThreadMutex.down();
	ActiveConnections += 1;
	CommunicationThreadMutex.up();
}

void decrement_active_connections(void) {
	CommunicationThreadMutex.down();
	ActiveConnections -= 1;
	CommunicationThreadMutex.up();
}

static void communication_thread_common(TConnection *Connection, bool OwnsRawSocket, int RawSocket) {
	sigset_t SignalSet;
	sigfillset(&SignalSet);
	sigprocmask(SIG_SETMASK, &SignalSet, NULL);
	if (!Connection->set_login_timer(5)) {
		error("CommunicationThread: Failed to set login timer.\n");
		goto cleanup;
	}

	if (!receive_command(Connection)) {
		Connection->close_connection(true);
	}

	Connection->SigIOPending = false;
	while (GameRunning() && Connection->ConnectionIsOk) {
		int Signal;
		sigwait(&SignalSet, &Signal);
		switch (Signal) {
		case SIGHUP:
		case SIGPIPE: {
			Connection->close_connection(false);
			break;
		}

		case SIGUSR1:
		case SIGIO: {
			if (Signal == SIGIO || Connection->SigIOPending) {
				if (!Connection->WaitingForACK) {
					Connection->SigIOPending = false;
					if (!receive_command(Connection)) {
						Connection->close_connection(true);
					}
				} else {
					Connection->SigIOPending = true;
				}
			}
			break;
		}

		case SIGUSR2: {
			if (!send_data(Connection)) {
				Connection->close_connection(false);
			}
			break;
		}

		case SIGALRM: {
			Connection->stop_login_timer();
			if (Connection->State == CONNECTION_CONNECTED) {
				print(2, "Login timeout for socket %d.\n", Connection->Transport->get_fd());
				Connection->close_connection(false);
			}
			break;
		}

		default: break;
		}
	}

	while (Connection->live()) {
		DelayThread(1, 0);
	}

	if (Connection->ClosingIsDelayed) {
		DelayThread(2, 0);
	}

cleanup:
	if (OwnsRawSocket) {
		delete Connection->Transport;
	} else {
		// WebSocket: defer deletion to the event loop thread to ensure all
		// previously queued defer() callbacks have completed first.
		WebSocketTransport::deferred_delete(
			static_cast<WebSocketTransport *>(Connection->Transport));
	}
	Connection->Transport = nullptr;

	if (OwnsRawSocket && RawSocket >= 0) {
		if (close(RawSocket) == -1) {
			error("CommunicationThread: Error %d while closing socket.\n", errno);
		}
	}

	Connection->free_connection();
}

void communication_thread(int Socket) {
	TConnection *Connection = assign_free_connection();
	if (Connection == NULL) {
		print(2, "No more connections available.\n");
		if (close(Socket) == -1) {
			error("CommunicationThread: Error %d while closing socket (1).\n", errno);
		}
		return;
	}

	ASSERT(Connection->ThreadID == gettid());

	// Resolve remote address before creating transport
	struct sockaddr_in RemoteAddr;
	socklen_t RemoteAddrLen = sizeof(RemoteAddr);
	char RemoteIP[16] = "Unknown";
	if (getpeername(Socket, (struct sockaddr *)&RemoteAddr, &RemoteAddrLen) == 0) {
		strcpy(RemoteIP, inet_ntoa(RemoteAddr.sin_addr));
	}

	TcpTransport *Transport = new TcpTransport(Socket, RemoteIP);
	Connection->connect(Socket, Transport);
	Connection->WaitingForACK = false;

	{
		struct f_owner_ex FOwnerEx = {};
		FOwnerEx.type = F_OWNER_TID;
		FOwnerEx.pid = Connection->ThreadID;
		if (fcntl(Socket, F_SETOWN_EX, &FOwnerEx) == -1) {
			error("CommunicationThread: F_SETOWN_EX failed for socket %d.\n", Socket);
			delete Connection->Transport;
			Connection->Transport = nullptr;
			if (close(Socket) == -1) {
				error("CommunicationThread: Error %d while closing socket (2).\n", errno);
			}
			Connection->free_connection();
			return;
		}
	}

	{
		int SocketFlags = fcntl(Socket, F_GETFL);
		if (SocketFlags == -1 || fcntl(Socket, F_SETFL, (SocketFlags | O_NONBLOCK | O_ASYNC)) == -1) {
			error("ConnectionThread: F_SETFL failed for socket %d.\n", Socket);
			delete Connection->Transport;
			Connection->Transport = nullptr;
			if (close(Socket) == -1) {
				error("CommunicationThread: Error %d while closing socket (3).\n", errno);
			}
			Connection->free_connection();
			return;
		}
	}

	// NOTE(fusion): In some systems, the accepted socket will inherit TCP_NODELAY
	// from the acceptor, making this next call redundant then. Nevertheless it is
	// probably better to set it anyways to be sure.
	{
		int NoDelay = 1;
		if (setsockopt(Socket, IPPROTO_TCP, TCP_NODELAY, &NoDelay, sizeof(NoDelay)) == -1) {
			error("ConnectionThread: Failed to set TCP_NODELAY=1 on socket %d.\n", Socket);
			delete Connection->Transport;
			Connection->Transport = nullptr;
			if (close(Socket) == -1) {
				error("CommunicationThread: Error %d while closing socket (3.5).\n", errno);
			}
			Connection->free_connection();
			return;
		}
	}

	communication_thread_common(Connection, true, Socket);
}

int handle_connection(void *Data) {
	int Socket = (uint16)((uintptr)Data);
	int StackNumber = (uint16)((uintptr)Data >> 16);

	if (UseOwnStacks) {
		attach_communication_thread_stack(StackNumber);
	}

	try {
		communication_thread(Socket);
	} catch (RESULT r) {
		error("HandleConnection: Uncaught exception %d.\n", r);
	} catch (const char *str) {
		error("HandleConnection: Uncaught exception \"%s\".\n", str);
	} catch (const std::exception &e) {
		error("HandleConnection: Uncaught exception %s.\n", e.what());
	} catch (...) {
		error("HandleConnection: Uncaught exception of unknown type.\n");
	}

	decrement_active_connections();

	if (UseOwnStacks) {
		release_communication_thread_stack(StackNumber);
	}

	return 0;
}

void communication_thread_ws(WebSocketTransport *Transport) {
	TConnection *Connection = assign_free_connection();
	if (Connection == NULL) {
		print(2, "No more connections available (WS).\n");
		delete Transport;
		return;
	}

	ASSERT(Connection->ThreadID == gettid());

	int EventFD = Transport->get_fd();
	if (EventFD == -1) {
		error("CommunicationThreadWS: Transport has invalid eventfd.\n");
		delete Transport;
		Connection->free_connection();
		return;
	}

	// Pass eventfd as the "socket" — used only for logging/identification.
	Connection->connect(EventFD, Transport);
	Connection->WaitingForACK = false;

	// Publish our thread ID so the event loop's .close callback can SIGHUP us.
	Transport->set_thread_id(gettid());

	// Set up async I/O signaling on the eventfd (same as TCP does on the socket).
	{
		struct f_owner_ex FOwnerEx = {};
		FOwnerEx.type = F_OWNER_TID;
		FOwnerEx.pid = Connection->ThreadID;
		if (fcntl(EventFD, F_SETOWN_EX, &FOwnerEx) == -1) {
			error("CommunicationThreadWS: F_SETOWN_EX failed for eventfd %d.\n", EventFD);
			delete Transport;
			Connection->free_connection();
			return;
		}
	}

	{
		int Flags = fcntl(EventFD, F_GETFL);
		if (Flags == -1 || fcntl(EventFD, F_SETFL, (Flags | O_NONBLOCK | O_ASYNC)) == -1) {
			error("CommunicationThreadWS: F_SETFL failed for eventfd %d.\n", EventFD);
			delete Transport;
			Connection->free_connection();
			return;
		}
	}

	// No TCP_NODELAY for WebSocket — not a raw TCP socket.
	communication_thread_common(Connection, false, -1);
}

int handle_ws_connection(void *Data) {
	WebSocketTransport *Transport = (WebSocketTransport *)Data;

	try {
		communication_thread_ws(Transport);
	} catch (RESULT r) {
		error("HandleWSConnection: Uncaught exception %d.\n", r);
	} catch (const char *str) {
		error("HandleWSConnection: Uncaught exception \"%s\".\n", str);
	} catch (const std::exception &e) {
		error("HandleWSConnection: Uncaught exception %s.\n", e.what());
	} catch (...) {
		error("HandleWSConnection: Uncaught exception of unknown type.\n");
	}

	decrement_active_connections();
	return 0;
}

// Acceptor Thread
// =============================================================================
bool open_socket(void) {
	print(1, "Starting game server...\n");
	print(1, "Pid=%d, Tid=%d - listening on port %d\n", getpid(), gettid(), GamePort);
	TCPSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (TCPSocket == -1) {
		error("LaunchServer: Cannot open socket.\n");
		return false;
	}

	struct linger Linger = {};
	Linger.l_onoff = 0;
	Linger.l_linger = 0;
	if (setsockopt(TCPSocket, SOL_SOCKET, SO_LINGER, &Linger, sizeof(Linger)) == -1) {
		error("LaunchServer: Failed to set SO_LINGER=(0, 0): (%d) %s.\n", errno, strerror(errno));
		return false;
	}

	int ReuseAddr = 1;
	if (setsockopt(TCPSocket, SOL_SOCKET, SO_REUSEADDR, &ReuseAddr, sizeof(ReuseAddr)) == -1) {
		error("LaunchServer: Failed to set SO_REUSEADDR=1: (%d) %s.\n", errno, strerror(errno));
		return false;
	}

	int NoDelay = 1;
	if (setsockopt(TCPSocket, IPPROTO_TCP, TCP_NODELAY, &NoDelay, sizeof(NoDelay)) == -1) {
		error("LaunchServer: Failed to set TCP_NODELAY=1: (%d) %s.\n", errno, strerror(errno));
		return false;
	}

	struct sockaddr_in ServerAddress = {};
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_port = htons(GamePort);
#if BIND_ACCEPTOR_TO_GAME_ADDRESS
	ServerAddress.sin_addr.s_addr = inet_addr(GameAddress);
#else
	ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	if (bind(TCPSocket, (struct sockaddr *)&ServerAddress, sizeof(ServerAddress)) == -1) {
		error("LaunchServer: Failed to bind to acceptor to %s:%d: (%d) %s.\n", inet_ntoa(ServerAddress.sin_addr),
			  GamePort, errno, strerror(errno));
		return false;
	}

	if (listen(TCPSocket, 512) == -1) {
		error("LaunchServer: Error %d on listen.\n", errno);
		return false;
	}

	return true;
}

int acceptor_thread_loop(void *Unused) {
	AcceptorThreadID = gettid();
	print(1, "Waiting for clients...\n");
	while (GameRunning()) {
		int Socket = accept(TCPSocket, NULL, NULL);
		if (Socket == -1) {
			error("AcceptorThreadLoop: Error %d on accept.\n", errno);
			continue;
		}

		// TODO(fusion): I don't think anything in here can throw any exception.
		try {
			if (UseOwnStacks) {
				int StackNumber;
				void *Stack;
				get_communication_thread_stack(&StackNumber, &Stack);
				if (Stack == NULL) {
					print(3, "No more stack space available.\n");
					if (close(Socket) == -1) {
						error("AcceptorThreadLoop: Error %d while closing socket (1).\n", errno);
					}
				} else {
					increment_active_connections();
					void *Argument = (void *)(((uintptr)Socket & 0xFFFF) | (((uintptr)StackNumber & 0xFFFF) << 16));
					ThreadHandle ConnectionThread =
						StartThread(handle_connection, Argument, Stack, COMMUNICATION_THREAD_STACK_SIZE, true);
					if (ConnectionThread == INVALID_THREAD_HANDLE) {
						decrement_active_connections();
						release_communication_thread_stack(StackNumber);
						if (close(Socket) == -1) {
							error("AcceptorThreadLoop: Error %d while closing socket (2).\n", errno);
						}
					}
				}
			} else {
				if (ActiveConnections >= MAX_COMMUNICATION_THREADS) {
					print(3, "No more connections available.\n");
					if (close(Socket) == -1) {
						error("AcceptorThreadLoop: Error %d while closing socket (3).\n", errno);
					}
				} else {
					increment_active_connections();
					void *Argument = (void *)((uintptr)Socket & 0xFFFF);
					ThreadHandle ConnectionThread =
						StartThread(handle_connection, Argument, COMMUNICATION_THREAD_STACK_SIZE, true);
					if (ConnectionThread == INVALID_THREAD_HANDLE) {
						print(3, "Cannot create new thread.\n");
						decrement_active_connections();
						if (close(Socket) == -1) {
							error("AcceptorThreadLoop: Error %d while closing socket (4).\n", errno);
						}
					}
				}
			}
		} catch (RESULT r) {
			error("AcceptorThreadLoop: Uncaught exception %d.\n", r);
		} catch (const char *str) {
			error("AcceptorThreadLoop: Uncaught exception \"%s\".\n", str);
		} catch (...) {
			error("AcceptorThreadLoop: Uncaught exception of unknown type.\n");
		}
	}

	AcceptorThreadID = 0;
	if (ActiveConnections > 0) {
		print(3, "Waiting for %d communication threads to finish...\n", ActiveConnections);
		while (ActiveConnections > 0) {
			DelayThread(1, 0);
		}
	}

	return 0;
}

// Initialization
// =============================================================================
void check_threadlib_version(void) {
	// TODO(fusion): We'll probably remove `OwnStacks` support anyway but it
	// seems to be using this file `/etc/image-release` as an heuristic to
	// enable it. Not sure what this file is about.
	UseOwnStacks = FileExists("/etc/image-release");
	if (UseOwnStacks) {
		print(2, "Using custom stacks.\n");
	} else {
		print(2, "Using reduced library stacks.\n");
	}
}

void init_communication(void) {
	check_threadlib_version();
	init_communication_thread_stacks();
	init_load_history();

	WaitinglistHead = NULL;
	TCPSocket = -1;
	AcceptorThread = INVALID_THREAD_HANDLE;
	AcceptorThreadID = 0;
	ActiveConnections = 0;
	QueryManagerConnectionPool.init();

	// TODO(fusion): This is arbitrary, should probably be set in the config.
	if (!PrivateKey.initFromFile("tibia.pem")) {
		throw "cannot load RSA key";
	}

	if (TransportMode == TRANSPORT_TCP || TransportMode == TRANSPORT_BOTH) {
		if (!open_socket()) {
			throw "cannot open socket";
		}

		AcceptorThread = StartThread(acceptor_thread_loop, NULL, false);
		if (AcceptorThread == INVALID_THREAD_HANDLE) {
			throw "cannot start acceptor thread";
		}
	}

	if (TransportMode == TRANSPORT_WEBSOCKET || TransportMode == TRANSPORT_BOTH) {
		start_websocket_thread();
	}
}

void exit_communication(void) {
	// NOTE(fusion): `SIGHUP` is used to signal the connection thread to close
	// the connection and terminate.
	print(3, "Closing all connections...\n");
	TConnection *Connection = get_first_connection();
	while (Connection != NULL) {
		tgkill(GetGameProcessID(), Connection->get_thread_id(), SIGHUP);
		Connection = get_next_connection();
	}

	process_connections();
	print(3, "All connections closed.\n");

	if (TCPSocket != -1) {
		if (close(TCPSocket) == -1) {
			error("ExitCommunication: Error %d while closing socket.\n", errno);
		}
		TCPSocket = -1;
	}

	if (AcceptorThread != INVALID_THREAD_HANDLE) {
		if (AcceptorThreadID != 0) {
			tgkill(GetGameProcessID(), AcceptorThreadID, SIGHUP);
		}
		JoinThread(AcceptorThread);
		AcceptorThread = INVALID_THREAD_HANDLE;
	}

	if (TransportMode == TRANSPORT_WEBSOCKET || TransportMode == TRANSPORT_BOTH) {
		stop_websocket_thread();
	}

	QueryManagerConnectionPool.exit();
	exit_load_history();
	exit_communication_thread_stacks();
}
