#include "network/connection/connection.h"
#include "connections.h"
#include "cr.h"
#include "info.h"
#include "threads.h"
#include "writer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

// TConnection
// =============================================================================
TConnection::TConnection(void) {
	this->State = CONNECTION_FREE;
	this->Transport = nullptr;
}

void TConnection::process(void) {
	if (this->in_game()) {
		uint32 LastCommand = (RoundNr - this->TimeStamp);
		if (LastCommand == 30 || LastCommand == 60) {
			send_ping(this);
		}

		uint32 LastAction = (RoundNr - this->TimeStampAction);
		if (LastAction == 900 && !check_right(this->CharacterID, NO_LOGOUT_BLOCK)) {
			send_message(this, TALK_ADMIN_MESSAGE,
						 "You have been idle for %d minutes. You will be disconnected"
						 " in one minute if you are still idle then.",
						 15);
		}

		if (LastAction >= 960 && !check_right(this->CharacterID, NO_LOGOUT_BLOCK)) {
			this->logout(0, true);
		} else if (!GameRunning() || !this->ConnectionIsOk || (LastCommand >= 90)) {
			this->logout(0, false);
		}
	} else if (this->State == CONNECTION_LOGIN) {
		if (!GameRunning() || !this->ConnectionIsOk) {
			this->disconnect();
		}
	} else if (this->State == CONNECTION_LOGOUT) {
		// NOTE(fusion): `TimeStamp` has the logout round which is set by
		// `TConnection::logout`.
		if (!GameRunning() || !this->ConnectionIsOk || this->TimeStamp <= RoundNr) {
			this->disconnect();
		}
	}
}

void TConnection::reset_timer(int Command) {
	if (this->in_game()) {
		this->TimeStamp = RoundNr;
		if (Command != CL_CMD_PING && Command != CL_CMD_GO_STOP && Command != CL_CMD_CANCEL &&
			Command != CL_CMD_REFRESH_FIELD && Command != CL_CMD_REFRESH_CONTAINER) {
			this->TimeStampAction = RoundNr;
		}
	}
}

void TConnection::emergency_ping(void) {
	if (this->in_game()) {
		uint32 LastCommand = (RoundNr - this->TimeStamp);
		if (LastCommand < 80) {
			// TODO(fusion): This is only called by `NetLoadCheck`, when it detects
			// lag, which can only happen after some set number of rounds, usually
			// higher than 100. This is all to say this subtraction below is very
			// unlikely to wrap. Nevertheless, we should also have some helper
			// functions to do saturating addition or subtraction for both signed
			// and unsigned integers.
			this->TimeStamp = RoundNr - 100;
		}
		send_ping(this);
	}
}

pid_t TConnection::get_thread_id(void) {
	if (this->State == CONNECTION_FREE) {
		error("TConnection::GetThreadID: Connection is not assigned.\n");
		return 0;
	}

	return this->ThreadID;
}

bool TConnection::set_login_timer(int Timeout) {
	if (this->State == CONNECTION_FREE) {
		error("TConnection::SetLoginTimer: Connection is not assigned.\n");
		return false;
	}

	if (this->LoginTimer != 0) {
		error("TConnection::SetLoginTimer: Timer already set.\n");
		return false;
	}

	struct sigevent SigEvent = {};
	SigEvent.sigev_notify = SIGEV_THREAD_ID;
	SigEvent.sigev_signo = SIGALRM;
	SigEvent.sigev_notify_thread_id = this->ThreadID;
	if (timer_create(CLOCK_MONOTONIC, &SigEvent, &this->LoginTimer) == -1) {
		error("TConnection::SetLoginTimer: Failed to create timer: (%d) %s\n", errno, strerror(errno));
		return false;
	}

	struct itimerspec TimerSpec = {};
	TimerSpec.it_value.tv_sec = Timeout;
	if (timer_settime(this->LoginTimer, 0, &TimerSpec, NULL) == -1) {
		error("TConnection::SetLoginTimer: Failed to start timer: (%d) %s\n", errno, strerror(errno));
		return false;
	}

	return true;
}

void TConnection::stop_login_timer(void) {
	if (this->State == CONNECTION_FREE) {
		error("TConnection::StopLoginTimer: Connection is not assigned.\n");
		return;
	}

	if (this->LoginTimer == 0) {
		error("TConnection::StopLoginTimer: Timer not set.\n");
		return;
	}

	if (timer_delete(this->LoginTimer) == -1) {
		error("TConnection::StopLoginTimer: Failed to delete timer: (%d) %s\n", errno, strerror(errno));
	}

	this->LoginTimer = 0;
}

int TConnection::get_socket(void) {
	if (this->State == CONNECTION_FREE || this->State == CONNECTION_ASSIGNED) {
		error("TConnection::GetSocket: Connection is not connected.\n");
		return -1;
	}

	return this->Socket;
}

const char *TConnection::get_ip_address(void) {
	if (this->State == CONNECTION_FREE || this->State == CONNECTION_ASSIGNED) {
		error("TConnection::GetIPAddress: Connection is not connected.\n");
		return "Unknown";
	}

	return this->IPAddress;
}

void TConnection::free_connection(void) {
	this->State = CONNECTION_FREE;
}

void TConnection::assign(void) {
	if (this->State != CONNECTION_FREE) {
		error("TConnection::Assign: Connection is not free.\n");
	}

	this->State = CONNECTION_ASSIGNED;
	this->ThreadID = gettid();
	this->LoginTimer = 0;
}

void TConnection::connect(int Socket, ITransport *Transport) {
	if (this->State != CONNECTION_ASSIGNED) {
		error("TConnection::Connect: Connection is not assigned to any thread.\n");
	}

	this->State = CONNECTION_CONNECTED;
	this->Socket = Socket;
	this->Transport = Transport;
	this->ConnectionIsOk = true;
	this->ClosingIsDelayed = true;
	this->RandomSeed = rand();

	// Use transport's address instead of getpeername
	strcpy(this->IPAddress, Transport->get_remote_address());
}

void TConnection::login(void) {
	if (this->State != CONNECTION_CONNECTED) {
		error("TConnection::Connect: Invalid connection state %d.\n", this->State);
	}

	this->State = CONNECTION_LOGIN;
}

bool TConnection::join_game(TReadBuffer *Buffer) {
	if (this->State != CONNECTION_LOGIN) {
		error("TConnection::JoinGame: Invalid connection state %d.\n", this->State);
	}

	this->clear_known_creature_table(false);

	try {
		this->TerminalType = (int)Buffer->readWord();
		this->TerminalVersion = (int)Buffer->readWord();
		this->CharacterID = Buffer->readQuad();
	} catch (const char *str) {
		error("TConnection::JoinGame: Error while reading the buffer (%s).\n", str);
	}

	if (this->TerminalType != 1 && this->TerminalType != 2) {
		error("TConnection::JoinGame: Unknown terminal type %d.\n", this->TerminalType);
		return false;
	}

	this->TerminalOffsetX = 8;
	this->TerminalOffsetY = 6;
	this->TerminalWidth = 18;
	this->TerminalHeight = 14;

	TPlayer *Player = ::get_player(this->CharacterID);
	if (Player == NULL) {
		Player = new TPlayer(this, this->CharacterID);
		if (Player->ConstructError != NOERROR) {
			delete Player;
			return false;
		}
	} else {
		if (Player->IsDead) {
			log_message("game", "Player %s is currently dying - login failed.\n", Player->Name);
			decrease_player_pool_slot_sticky(this->CharacterID);
			return false;
		}

		if (Player->LoggingOut && Player->LogoutPossible() == 0) {
			log_message("game", "Player %s is currently logging out - login failed.\n", Player->Name);
			decrease_player_pool_slot_sticky(this->CharacterID);
			return false;
		}

		TConnection *OldConnection = Player->Connection;
		Player->ClearConnection();
		if (OldConnection != NULL) {
			OldConnection->CharacterID = 0;
			OldConnection->logout(0, true);
		}

		decrement_is_online_order(this->CharacterID);
		Player->TakeOver(this);
	}

	strcpy(this->Name, Player->Name);
	this->TimeStamp = RoundNr;
	this->TimeStampAction = RoundNr;
	return true;
}

void TConnection::enter_game(void) {
	if (this->State != CONNECTION_LOGIN) {
		error("TConnection::EnterGame: Invalid connection state %d.\n", this->State);
	}

	this->State = CONNECTION_GAME;
}

void TConnection::die(void) {
	if (this->State == CONNECTION_GAME) {
		this->State = CONNECTION_DEAD;
	}
}

void TConnection::logout(int Delay, bool StopFight) {
	if (!this->in_game() && this->State != CONNECTION_LOGOUT) {
		error("TConnection::Logout: Invalid connection state %d.\n", this->State);
	}

	this->State = CONNECTION_LOGOUT;
	if (this->CharacterID != 0) {
		TPlayer *Player = ::get_player(this->CharacterID);
		if (Player != NULL) {
			Player->ClearConnection();
			Player->StartLogout(false, StopFight);
		}
	}

	if (Delay < 0) {
		Delay = 0;
	}

	this->CharacterID = 0;
	this->TimeStamp = RoundNr + (uint32)Delay;
	this->ClosingIsDelayed = false;
}

void TConnection::close_connection(bool Delay) {
	if (this->State == CONNECTION_FREE || this->State == CONNECTION_ASSIGNED) {
		error("TConnection::Close: Invalid connection state %d.\n", this->State);
	}

	if (this->State == CONNECTION_CONNECTED) {
		this->State = CONNECTION_DISCONNECTED;
	}

	this->ConnectionIsOk = false;
	this->ClosingIsDelayed = Delay;
}

void TConnection::disconnect(void) {
	if (this->State == CONNECTION_FREE || this->State == CONNECTION_ASSIGNED) {
		error("TConnection::Close: Invalid connection state %d.\n", this->State);
	}

	this->clear_known_creature_table(true);
	this->ConnectionIsOk = false;
	this->State = CONNECTION_DISCONNECTED;
	tgkill(GetGameProcessID(), this->ThreadID, SIGHUP);
}

TPlayer *TConnection::get_player(void) {
	if (!this->live()) {
		error("TConnection::get_player: Invalid connection state %d.\n", this->State);
		return NULL;
	}

	TPlayer *Player = NULL;
	if (this->CharacterID != 0) {
		Player = ::get_player(this->CharacterID);
	}
	return Player;
}

const char *TConnection::get_name(void) {
	if (!this->live()) {
		error("TConnection::get_name: Invalid connection state %d.\n", this->State);
		return "";
	}

	return this->Name;
}

void TConnection::get_position(int *x, int *y, int *z) {
	TPlayer *Player = this->get_player();
	if (Player != NULL) {
		*x = Player->posx;
		*y = Player->posy;
		*z = Player->posz;
	} else {
		*x = 0;
		*y = 0;
		*z = 0;
	}
}

bool TConnection::is_visible(int x, int y, int z) {
	int PlayerX, PlayerY, PlayerZ;
	this->get_position(&PlayerX, &PlayerY, &PlayerZ);

	// TODO(fusion): Have a standalone version of `TCreature::CanSeeFloor`?
	if (PlayerZ <= 7) {
		if (z > 7) {
			return false;
		}
	} else {
		if (std::abs(PlayerZ - z) > 2) {
			return false;
		}
	}

	int MinX = (PlayerX - this->TerminalOffsetX) + (PlayerZ - z);
	int MinY = (PlayerY - this->TerminalOffsetY) + (PlayerZ - z);
	int MaxX = MinX + this->TerminalWidth - 1;
	int MaxY = MinY + this->TerminalHeight - 1;
	return x >= MinX && x <= MaxX && y >= MinY && y <= MaxY;
}

KNOWNCREATURESTATE TConnection::known_creature(uint32 ID, bool UpdateFollows) {
	int EntryIndex = -1;
	for (int i = 0; i < NARRAY(this->KnownCreatureTable); i += 1) {
		if (this->KnownCreatureTable[i].CreatureID == ID) {
			EntryIndex = i;
			break;
		}
	}

	if (EntryIndex == -1) {
		return KNOWNCREATURE_FREE;
	}

	KNOWNCREATURESTATE Result = this->KnownCreatureTable[EntryIndex].State;
	if (Result == KNOWNCREATURE_OUTDATED && UpdateFollows) {
		this->KnownCreatureTable[EntryIndex].State = KNOWNCREATURE_UPTODATE;
	}
	return Result;
}

uint32 TConnection::new_known_creature(uint32 NewID) {
	uint32 OldID = 0;
	int EntryIndex = -1;
	for (int i = 0; i < NARRAY(this->KnownCreatureTable); i += 1) {
		if (this->KnownCreatureTable[i].CreatureID == NewID) {
			OldID = NewID;
			EntryIndex = i;
			break;
		}
	}

	if (EntryIndex == -1) {
		for (int i = 0; i < NARRAY(this->KnownCreatureTable); i += 1) {
			if (this->KnownCreatureTable[i].State == KNOWNCREATURE_FREE) {
				OldID = this->KnownCreatureTable[i].CreatureID;
				EntryIndex = i;
				break;
			}
		}
	}

	if (EntryIndex == -1) {
		for (int i = 0; i < NARRAY(this->KnownCreatureTable); i += 1) {
			TCreature *Creature = get_creature(this->KnownCreatureTable[i].CreatureID);
			if (Creature == NULL || !this->is_visible(Creature->posx, Creature->posy, Creature->posz)) {
				OldID = this->KnownCreatureTable[i].CreatureID;
				EntryIndex = i;
				this->unchain_known_creature(OldID);
				break;
			}
		}
	}

	if (EntryIndex == -1) {
		print(3, "KnownCreatureTable is full.\n");
		return 0;
	}

	if (this->KnownCreatureTable[EntryIndex].State != KNOWNCREATURE_FREE) {
		error("TUserCom::NewKnownCreature: Slot is not cleared.\n");
	}

	this->KnownCreatureTable[EntryIndex].State = KNOWNCREATURE_UPTODATE;
	this->KnownCreatureTable[EntryIndex].CreatureID = NewID;

	TCreature *Creature = get_creature(NewID);
	if (Creature != NULL) {
		this->KnownCreatureTable[EntryIndex].Next = Creature->FirstKnowingConnection;
		Creature->FirstKnowingConnection = &this->KnownCreatureTable[EntryIndex];
	} else {
		error("TUserCom::NewKnownCreature: Creature %u does not exist.\n", NewID);
	}

	return OldID;
}

void TConnection::clear_known_creature_table(bool Unchain) {
	for (int i = 0; i < NARRAY(this->KnownCreatureTable); i += 1) {
		if (Unchain && this->KnownCreatureTable[i].State != KNOWNCREATURE_FREE) {
			this->unchain_known_creature(this->KnownCreatureTable[i].CreatureID);
		}
		this->KnownCreatureTable[i].State = KNOWNCREATURE_FREE;
		this->KnownCreatureTable[i].CreatureID = 0;
		this->KnownCreatureTable[i].Connection = this;
	}
}

void TConnection::unchain_known_creature(uint32 ID) {
	TCreature *Creature = get_creature(ID);
	if (Creature == NULL) {
		error("TUserCom::UnchainKnownCreature: Creature %u does not exist.\n", ID);
		return;
	}

	if (Creature->FirstKnowingConnection == NULL) {
		error("TUserCom::UnchainKnownCreature: Creature %u is not known by anyone.\n", ID);
		return;
	}

	TKnownCreature *Prev = NULL;
	TKnownCreature *Cur = Creature->FirstKnowingConnection;
	while (Cur != NULL) {
		if (Cur->Connection == this) {
			break;
		}
		Prev = Cur;
		Cur = Cur->Next;
	}

	if (Cur == NULL) {
		error("TUserCom::UnchainKnownCreature: Creature %u is not known.\n", ID);
		return;
	}

	Cur->State = KNOWNCREATURE_FREE;
	if (Prev == NULL) {
		Creature->FirstKnowingConnection = Cur->Next;
	} else {
		Prev->Next = Cur->Next;
	}
}
