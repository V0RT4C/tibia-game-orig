#include "protocol/receiving/receiving.h"
#include "connections.h"
#include "cr.h"
#include "game_data/operate/operate.h"
#include "houses.h"
#include "info.h"
#include "writer.h"

#include <signal.h>

bool CommandAllowed(TConnection *Connection, int Command) {
	if (Connection == NULL) {
		error("CommandAllowed: Connection is NULL.\n");
		return false;
	}

	if (Connection->State == CONNECTION_GAME) {
		return Command != CL_CMD_LOGIN_REQUEST && Command != CL_CMD_LOGIN;
	} else if (Connection->State == CONNECTION_DEAD || Connection->State == CONNECTION_LOGOUT) {
		return Command == CL_CMD_LOGOUT || Command == CL_CMD_PING || Command == CL_CMD_BUG_REPORT ||
			   Command == CL_CMD_ERROR_FILE_ENTRY;
	} else {
		error("CommandAllowed: Invalid connection state %d for command %d.\n", Connection->State, Command);
		return false;
	}
}

bool CheckSpecialCoordinates(int Command, int x, int y, int z, bool AllowInventoryAny) {
	if (x == 0xFFFF) { // SPECIAL_COORDINATE ?
		if (y == INVENTORY_ANY) {
			return AllowInventoryAny;
		} else if ((y >= INVENTORY_FIRST && y <= INVENTORY_LAST) || (y >= CONTAINER_FIRST && y <= CONTAINER_LAST)) {
			return true;
		} else {
			print(3,
				  "CheckSpecialCoordinates: Invalid coordinates"
				  " [%d,%d,%d] for command %d.\n",
				  x, y, z, Command);
			return false;
		}
	} else {
		return is_on_map(x, y, z);
	}
}

bool CheckVisibility(int Command, TConnection *Connection, int x, int y, int z) {
	if (Connection == NULL) {
		error("CheckVisibility: Connection is NULL.\n");
		return false;
	}

	bool Result = Connection->is_visible(x, y, z);
	if (!Result) {
		int PlayerX, PlayerY, PlayerZ;
		Connection->get_position(&PlayerX, &PlayerY, &PlayerZ);
		print(3,
			  "CheckVisibility: Field [%d,%d,%d] is not visible from [%d,%d,%d]"
			  " for command %d.\n",
			  x, y, z, PlayerX, PlayerY, PlayerZ, Command);
	}
	return Result;
}

bool CheckObjectType(int Command, int TypeID) {
	bool Result = object_type_exists(TypeID);
	if (!Result) {
		print(3, "CheckObjectType: Invalid object type %d for command %d.\n", TypeID, Command);
	}
	return Result;
}

ObjectType GetObjectType(int Command, int TypeID) {
	// TODO(fusion): Isn't this already done by `ObjectType::set_type_id` ?
	ObjectType ObjType(0);
	if (CheckObjectType(Command, TypeID)) {
		ObjType.set_type_id(TypeID);
	}
	return ObjType;
}

void CQuitGame(TConnection *Connection, TReadBuffer *Buffer) {
	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		Connection->logout(0, true);
		return;
	}

	switch (Player->LogoutPossible()) {
	case 0: {
		Player->ToDoClear();
		Connection->logout(0, true);
		break;
	}

	case 1: {
		send_message(Connection, TALK_FAILURE_MESSAGE, "You may not logout during or immediately after a fight!");
		break;
	}
	case 2: {
		send_message(Connection, TALK_FAILURE_MESSAGE, "You may not logout here!");
		break;
	}

	default: {
		// TODO(fusion): The original function would also logout the player
		// on this case which feels weird, specially when you can't have any
		// other result from `LogoutPossible`.
		error("CQuitGame: Invalid return value from LogoutPossible.\n");
		break;
	}
	}
}

void CPing(TConnection *Connection, TReadBuffer *Buffer) {
	// NOTE(fusion): Its a no-op because it's already encoded in `Connection::TimeStamp`.
	// Echo the ping back so the client can measure RTT (rate-limited).
	if (RoundNr > Connection->LastPingEcho) {
		Connection->LastPingEcho = RoundNr;
		send_ping(Connection);
	}
}

void CGoPath(TConnection *Connection, TReadBuffer *Buffer) {
	// TODO(fusion): Sometimes we check, sometimes we don't. It should always be
	// valid since these functions are all called from within `ReceiveData` which
	// also checks it.
	if (Connection == NULL) {
		error("CGoPath: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	if (Player->ToDoClear()) {
		send_snapback(Connection);
	}

	int Steps = (int)Buffer->readByte();
	ToDoEntry TD = {};
	TD.Code = TDGo;
	TD.Go.x = Player->posx;
	TD.Go.y = Player->posy;
	TD.Go.z = Player->posz;
	try {
		for (int i = 0; i < Steps; i += 1) {
			// NOTE(fusion): For whatever reason these directions are not the same
			// we use internally.
			switch (Buffer->readByte()) {
			case 1:
				TD.Go.x += 1;
				break; // EAST
			case 2:
				TD.Go.x += 1;
				TD.Go.y -= 1;
				break; // NORTHEAST
			case 3:
				TD.Go.y -= 1;
				break; // NORTH
			case 4:
				TD.Go.x -= 1;
				TD.Go.y -= 1;
				break; // NORTHWEST
			case 5:
				TD.Go.x -= 1;
				break; // WEST
			case 6:
				TD.Go.x -= 1;
				TD.Go.y += 1;
				break; // SOUTHWEST
			case 7:
				TD.Go.y += 1;
				break; // SOUTH
			case 8:
				TD.Go.x += 1;
				TD.Go.y += 1;
				break; // SOUTHEAST
			default: {
				continue;
			}
			}

			Player->ToDoAdd(TD);
		}

		Player->ToDoStart();
	} catch (RESULT r) {
		send_result(Connection, r);
		Player->ToDoYield();
	}
}

void CGoDirection(TConnection *Connection, int OffsetX, int OffsetY) {
	if (Connection == NULL) {
		error("CGoDirection: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	try {
		if (Player->ToDoClear()) {
			send_snapback(Connection);
		}

		ToDoEntry TD = {};
		TD.Code = TDGo;
		TD.Go.x = Player->posx + OffsetX;
		TD.Go.y = Player->posy + OffsetY;
		TD.Go.z = Player->posz;
		Player->ToDoAdd(TD);
		Player->ToDoStart();
	} catch (RESULT r) {
		send_result(Connection, r);
		Player->ToDoYield();
	}
}

void CGoStop(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CGoStop: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player != NULL) {
		Player->ToDoStop();
	}
}

void CRotate(TConnection *Connection, int Direction) {
	if (Connection == NULL) {
		error("CRotate: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	try {
		Player->ToDoRotate(Direction);
		Player->ToDoStart();
	} catch (RESULT r) {
		send_result(Connection, r);
		Player->ToDoYield();
	}
}

void CMoveObject(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CMoveObject: Connection is NULL.\n");
		return;
	}

	// TODO(fusion): I'm just following the convention being used. There is a
	// comment in `ReceiveData` but the fact that there is only one command in
	// a packet makes it unnecessary to "consume" all command's parameters.
	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int OrigX = Buffer->readWord();
	int OrigY = Buffer->readWord();
	int OrigZ = Buffer->readByte();
	int TypeID = Buffer->readWord();
	uint8 RNum = Buffer->readByte();
	int DestX = Buffer->readWord();
	int DestY = Buffer->readWord();
	int DestZ = Buffer->readByte();
	uint8 Count = Buffer->readByte();

	ObjectType Type = GetObjectType(CL_CMD_MOVE_OBJECT, TypeID);
	if (Type.is_map_container() || (Type.get_flag(CUMULATIVE) && Count == 0)) {
		return;
	}

	if (!CheckSpecialCoordinates(CL_CMD_MOVE_OBJECT, OrigX, OrigY, OrigZ, false) ||
		!CheckSpecialCoordinates(CL_CMD_MOVE_OBJECT, DestX, DestY, DestZ, true)) {
		return;
	}

	if ((OrigX != 0xFFFF && !CheckVisibility(CL_CMD_MOVE_OBJECT, Connection, OrigX, OrigY, OrigZ)) ||
		(DestX != 0xFFFF && !CheckVisibility(CL_CMD_MOVE_OBJECT, Connection, DestX, DestY, DestZ))) {
		return;
	}

	// TODO(fusion): `RNum` is only used inside `ToDoMove` > `get_object` to index
	// into containers. For some reason I thought the Z coordinate was also used
	// for that. Perhaps they're set to the same value.
	if (OrigX != 0xFFFF) {
		RNum = 1;
	}

	try {
		Player->ToDoMove(OrigX, OrigY, OrigZ, Type, RNum, DestX, DestY, DestZ, Count);
		Player->ToDoStart();
	} catch (RESULT r) {
		if (r != NOERROR) {
			send_result(Connection, r);
			Player->ToDoYield();
		}
	}
}

void CTradeObject(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CTradeObject: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ObjX = Buffer->readWord();
	int ObjY = Buffer->readWord();
	int ObjZ = Buffer->readByte();
	int TypeID = Buffer->readWord();
	uint8 RNum = Buffer->readByte();
	uint32 TradePartner = Buffer->readQuad();

	ObjectType Type = GetObjectType(CL_CMD_TRADE_OBJECT, TypeID);
	if (Type.is_map_container()) {
		return;
	}

	if (!CheckSpecialCoordinates(CL_CMD_TRADE_OBJECT, ObjX, ObjY, ObjZ, false)) {
		return;
	}

	if (ObjX != 0xFFFF && !CheckVisibility(CL_CMD_TRADE_OBJECT, Connection, ObjX, ObjY, ObjZ)) {
		return;
	}

	// TODO(fusion): Same as `CMoveObject`.
	if (ObjX != 0xFFFF) {
		RNum = 1;
	}

	try {
		Player->ToDoTrade(ObjX, ObjY, ObjZ, Type, RNum, TradePartner);
		Player->ToDoStart();
	} catch (RESULT r) {
		if (r != NOERROR) {
			send_result(Connection, r);
			Player->ToDoYield();
		}
	}
}

void CInspectTrade(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CInspectTrade: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	bool OwnOffer = (Buffer->readByte() == 0);
	int Position = Buffer->readByte();
	Object Obj = Player->InspectTrade(OwnOffer, Position);
	if (Obj.exists()) {
		try {
			look(Player->ID, Obj);
		} catch (RESULT r) {
			send_result(Connection, r);
		}
	}
}

void CAcceptTrade(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CAcceptTrade: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player != NULL) {
		Player->AcceptTrade();
	}
}

void CRejectTrade(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CRejectTrade: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player != NULL) {
		Player->RejectTrade();
	}
}

void CUseObject(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CUseObject: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ObjX = Buffer->readWord();
	int ObjY = Buffer->readWord();
	int ObjZ = Buffer->readByte();
	int TypeID = Buffer->readWord();
	uint8 RNum = Buffer->readByte();
	uint8 Dummy = Buffer->readByte();

	ObjectType Type = GetObjectType(CL_CMD_USE_OBJECT, TypeID);
	if (Type.is_map_container() || Type.get_flag(MULTIUSE)) {
		return;
	}

	if (Type.get_flag(CONTAINER) && Dummy >= NARRAY(TPlayer::OpenContainer)) {
		return;
	}

	if (!CheckSpecialCoordinates(CL_CMD_USE_OBJECT, ObjX, ObjY, ObjZ, false)) {
		return;
	}

	if (ObjX != 0xFFFF && !CheckVisibility(CL_CMD_USE_OBJECT, Connection, ObjX, ObjY, ObjZ)) {
		return;
	}

	try {
		Player->ToDoWait(100);
		Player->ToDoUse(1, ObjX, ObjY, ObjZ, Type, RNum, Dummy, 0, 0, 0, 0, 0);
		Player->ToDoStart();
	} catch (RESULT r) {
		send_result(Connection, r);
		Player->ToDoClear();
		Player->ToDoYield();
	}
}

void CUseTwoObjects(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CUseTwoObjects: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ObjX1 = Buffer->readWord();
	int ObjY1 = Buffer->readWord();
	int ObjZ1 = Buffer->readByte();
	int TypeID1 = Buffer->readWord();
	uint8 RNum1 = Buffer->readByte();
	int ObjX2 = Buffer->readWord();
	int ObjY2 = Buffer->readWord();
	int ObjZ2 = Buffer->readByte();
	int TypeID2 = Buffer->readWord();
	uint8 RNum2 = Buffer->readByte();

	ObjectType Type1 = GetObjectType(CL_CMD_USE_TWO_OBJECTS, TypeID1);
	ObjectType Type2 = GetObjectType(CL_CMD_USE_TWO_OBJECTS, TypeID2);
	if (Type1.is_map_container() || Type2.is_map_container() || !Type1.get_flag(MULTIUSE)) {
		return;
	}

	if (!CheckSpecialCoordinates(CL_CMD_USE_TWO_OBJECTS, ObjX1, ObjY1, ObjZ1, false) ||
		!CheckSpecialCoordinates(CL_CMD_USE_TWO_OBJECTS, ObjX2, ObjY2, ObjZ2, false)) {
		return;
	}

	if ((ObjX1 != 0xFFFF && !CheckVisibility(CL_CMD_USE_TWO_OBJECTS, Connection, ObjX1, ObjY1, ObjZ1)) ||
		(ObjX2 != 0xFFFF && !CheckVisibility(CL_CMD_USE_TWO_OBJECTS, Connection, ObjX2, ObjY2, ObjZ2))) {
		return;
	}

	try {
		Player->ToDoWait(100);
		Player->ToDoUse(2, ObjX1, ObjY1, ObjZ1, Type1, RNum1, 0, ObjX2, ObjY2, ObjZ2, Type2, RNum2);
		Player->ToDoStart();
	} catch (RESULT r) {
		send_result(Connection, r);
		Player->ToDoClear();
		Player->ToDoYield();
	}
}

void CUseOnCreature(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CUseOnCreature: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ObjX = Buffer->readWord();
	int ObjY = Buffer->readWord();
	int ObjZ = Buffer->readByte();
	int TypeID = Buffer->readWord();
	uint8 RNum = Buffer->readByte();
	uint32 TargetID = Buffer->readQuad();

	// TODO(fusion): Check `Type.get_flag(MULTIUSE)`?
	ObjectType Type = GetObjectType(CL_CMD_USE_ON_CREATURE, TypeID);
	if (Type.is_map_container()) {
		return;
	}

	if (!CheckSpecialCoordinates(CL_CMD_USE_ON_CREATURE, ObjX, ObjY, ObjZ, false)) {
		return;
	}

	if (ObjX != 0xFFFF && !CheckVisibility(CL_CMD_USE_ON_CREATURE, Connection, ObjX, ObjY, ObjZ)) {
		return;
	}

	TCreature *Target = get_creature(TargetID);
	if (Target == NULL || Target->Type == PLAYER) {
		return;
	}

	if (!CheckVisibility(CL_CMD_USE_ON_CREATURE, Connection, Target->posx, Target->posy, Target->posz)) {
		return;
	}

	// TODO(fusion): Isn't this already checked with `CheckVisibility`? It also
	// checks against the terminal offset which is not a distance semantically.
	int DistanceX = std::abs(Target->posx - Player->posx);
	int DistanceY = std::abs(Target->posy - Player->posy);
	if (DistanceX >= Connection->TerminalOffsetX || DistanceY >= Connection->TerminalOffsetY) {
		send_result(Connection, OUTOFRANGE);
		return;
	}

	Object Obj = get_object(Player->ID, ObjX, ObjY, ObjZ, RNum, Type);
	if (!Obj.exists() || !Target->CrObject.exists()) {
		send_result(Connection, NOTACCESSIBLE);
		return;
	}

	try {
		Player->ToDoWait(100);
		Player->ToDoUse(2, Obj, Target->CrObject);
		Player->ToDoStart();
	} catch (RESULT r) {
		send_result(Connection, r);
		Player->ToDoClear();
		Player->ToDoYield();
	}
}

void CTurnObject(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CTurnObject: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ObjX = Buffer->readWord();
	int ObjY = Buffer->readWord();
	int ObjZ = Buffer->readByte();
	int TypeID = Buffer->readWord();
	uint8 RNum = Buffer->readByte();

	// TODO(fusion): Check `Type.get_flag(ROTATE)`?
	ObjectType Type = GetObjectType(CL_CMD_TURN_OBJECT, TypeID);
	if (Type.is_map_container()) {
		return;
	}

	if (!CheckSpecialCoordinates(CL_CMD_TURN_OBJECT, ObjX, ObjY, ObjZ, false)) {
		return;
	}

	if (ObjX != 0xFFFF && !CheckVisibility(CL_CMD_TURN_OBJECT, Connection, ObjX, ObjY, ObjZ)) {
		return;
	}

	try {
		Player->ToDoWait(100);
		Player->ToDoTurn(ObjX, ObjY, ObjZ, Type, RNum);
		Player->ToDoStart();
	} catch (RESULT r) {
		send_result(Connection, r);
		Player->ToDoClear();
		Player->ToDoYield();
	}
}

void CCloseContainer(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CCloseContainer: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ContainerNr = Buffer->readByte();
	if (ContainerNr < NARRAY(TPlayer::OpenContainer)) {
		Player->SetOpenContainer(ContainerNr, NONE);
		send_close_container(Connection, ContainerNr);
	}
}

void CUpContainer(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CUpContainer: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ContainerNr = Buffer->readByte();
	if (ContainerNr < NARRAY(TPlayer::OpenContainer)) {
		Object Con = Player->GetOpenContainer(ContainerNr);
		if (Con != NONE) {
			Object Up = Con.get_container();
			ObjectType UpType = Up.get_object_type();
			if (!UpType.is_map_container() && !UpType.is_body_container()) {
				Player->SetOpenContainer(ContainerNr, Up);
				send_container(Connection, ContainerNr);
			} else {
				Player->SetOpenContainer(ContainerNr, NONE);
				send_close_container(Connection, ContainerNr);
			}
		}
	}
}

void CEditText(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CEditText: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	Object Obj = Object(Buffer->readQuad());
	if (!Obj.exists()) {
		send_result(Connection, NOTACCESSIBLE);
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (!ObjType.get_flag(WRITE) && (!ObjType.get_flag(WRITEONCE) || Obj.get_attribute(TEXTSTRING) != 0)) {
		send_result(Connection, NOTACCESSIBLE);
		return;
	}

	char Text[4096];
	Buffer->read_string(Text, sizeof(Text));
	int TextLength = (int)strlen(Text);
	int MaxLength =
		(ObjType.get_flag(WRITE) ? (int)ObjType.get_attribute(MAXLENGTH) : (int)ObjType.get_attribute(MAXLENGTHONCE));
	if (TextLength >= MaxLength) {
		send_result(Connection, NOROOM);
		return;
	}

	try {
		edit_text(Player->ID, Obj, Text);
	} catch (RESULT r) {
		send_result(Connection, r);
	}
}

void CEditList(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CEditList: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	char Text[4096];
	int Type = Buffer->readByte();
	uint32 ID = Buffer->readQuad();
	Buffer->read_string(Text, sizeof(Text));
	if (Type == GUESTLIST) {
		change_guests((uint16)ID, Player, Text);
	} else if (Type == SUBOWNERLIST) {
		change_subowners((uint16)ID, Player, Text);
	} else if (Type == DOORLIST) {
		Object Door = Object(ID);
		if (!Door.exists()) {
			send_result(Connection, NOTACCESSIBLE);
			return;
		}

		ObjectType DoorType = Door.get_object_type();
		if (!DoorType.get_flag(NAMEDOOR) || !DoorType.get_flag(TEXT)) {
			send_result(Connection, NOTACCESSIBLE);
			return;
		}

		change_name_door(Door, Player, Text);
	} else {
		error("CEditList: Unknown type %d.\n", Type);
	}
}

void CLookAtPoint(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CLookAtPoint: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ObjX = Buffer->readWord();
	int ObjY = Buffer->readWord();
	int ObjZ = Buffer->readByte();
	if (!CheckSpecialCoordinates(CL_CMD_LOOK_AT_POINT, ObjX, ObjY, ObjZ, false)) {
		return;
	}

	if (ObjX != 0xFFFF && !CheckVisibility(CL_CMD_LOOK_AT_POINT, Connection, ObjX, ObjY, ObjZ)) {
		return;
	}

	int RNum = (ObjX == 0xFFFF ? ObjZ : -1);
	Object Obj = get_object(Player->ID, ObjX, ObjY, ObjZ, RNum, 0);
	if (Obj.exists()) {
		try {
			look(Player->ID, Obj);
		} catch (RESULT r) {
			send_result(Connection, r);
		}
	}
}

void CTalk(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CTalk: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		print(3, "CTalk: No player is assigned to this connection.\n");
		return;
	}

	int Mode = Buffer->readByte();
	if (Mode != TALK_SAY && Mode != TALK_WHISPER && Mode != TALK_YELL && Mode != TALK_PRIVATE_MESSAGE &&
		Mode != TALK_CHANNEL_CALL && Mode != TALK_GAMEMASTER_REQUEST && Mode != TALK_GAMEMASTER_ANSWER &&
		Mode != TALK_PLAYER_ANSWER && Mode != TALK_GAMEMASTER_BROADCAST && Mode != TALK_GAMEMASTER_CHANNELCALL &&
		Mode != TALK_GAMEMASTER_MESSAGE && Mode != TALK_ANONYMOUS_BROADCAST && Mode != TALK_ANONYMOUS_CHANNELCALL &&
		Mode != TALK_ANONYMOUS_MESSAGE) {
		print(3, "CTalk: Unbekannter Sprechmodus %d.\n", Mode);
		return;
	}

	char Addressee[30] = {};
	if (Mode == TALK_PRIVATE_MESSAGE || Mode == TALK_GAMEMASTER_ANSWER || Mode == TALK_GAMEMASTER_MESSAGE ||
		Mode == TALK_ANONYMOUS_MESSAGE) {
		Buffer->read_string(Addressee, sizeof(Addressee));
		if (Addressee[0] == 0) {
			print(3, "CTalk: Recipient not specified.\n");
			return;
		}
	}

	int Channel = 0;
	if (Mode == TALK_CHANNEL_CALL || Mode == TALK_GAMEMASTER_CHANNELCALL || Mode == TALK_ANONYMOUS_CHANNELCALL) {
		Channel = Buffer->readWord();
		if (Channel >= get_number_of_channels()) {
			print(3, "CTalk: Invalid channel %d.\n", Channel);
			return;
		}

		snprintf(Addressee, sizeof(Addressee), "%d", Channel);
	}

	char Text[256];
	Buffer->read_string(Text, sizeof(Text));
	if (Text[0] == 0) {
		print(3, "CTalk: Kein Text.\n");
		return;
	}

	if (findFirst(Text, '\n') != NULL) {
		error("CTalk: %s verwendet Newlines (%d,%s)\n", Player->Name, Mode, Text);
		return;
	}

	if (Mode == TALK_GAMEMASTER_ANSWER && !check_right(Player->ID, READ_GAMEMASTER_CHANNEL)) {
		Mode = TALK_PRIVATE_MESSAGE;
	} else if (Mode == TALK_GAMEMASTER_BROADCAST && !check_right(Player->ID, GAMEMASTER_BROADCAST)) {
		Mode = TALK_SAY;
	} else if (Mode == TALK_GAMEMASTER_CHANNELCALL && !check_right(Player->ID, GAMEMASTER_BROADCAST)) {
		Mode = TALK_CHANNEL_CALL;
	} else if (Mode == TALK_GAMEMASTER_MESSAGE && !check_right(Player->ID, GAMEMASTER_BROADCAST)) {
		Mode = TALK_PRIVATE_MESSAGE;
	} else if (Mode == TALK_ANONYMOUS_BROADCAST && !check_right(Player->ID, ANONYMOUS_BROADCAST)) {
		Mode = TALK_SAY;
	} else if (Mode == TALK_ANONYMOUS_CHANNELCALL && !check_right(Player->ID, ANONYMOUS_BROADCAST)) {
		Mode = TALK_CHANNEL_CALL;
	} else if (Mode == TALK_ANONYMOUS_MESSAGE && !check_right(Player->ID, ANONYMOUS_BROADCAST)) {
		Mode = TALK_PRIVATE_MESSAGE;
	}

	if (Mode == TALK_YELL && Player->Skills[SKILL_LEVEL]->Get() <= 1) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "You may not yell as long as you are on level 1.");
		return;
	}

	if (Mode == TALK_CHANNEL_CALL && Player->Skills[SKILL_LEVEL]->Get() <= 1) {
		send_message(Connection, TALK_FAILURE_MESSAGE,
					 "You may not speak into channels as long as you are on level 1.");
		return;
	}

	if (Mode == TALK_CHANNEL_CALL || Mode == TALK_GAMEMASTER_CHANNELCALL || Mode == TALK_ANONYMOUS_CHANNELCALL) {
		if (Channel == CHANNEL_RULEVIOLATIONS) {
			print(3, "Channel call for GM request queue.\n");
			return;
		}

		if (!channel_subscribed(Channel, Player->ID)) {
			Addressee[0] = 0;
			Mode = TALK_SAY;
		}
	}

	if (Mode == TALK_CHANNEL_CALL && Channel == CHANNEL_HELP && check_right(Player->ID, HIGHLIGHT_HELP_CHANNEL)) {
		Mode = TALK_HIGHLIGHT_CHANNELCALL;
		Player->TutorActivities += 1;
		print(3, "Activity point for %s.\n", Player->Name);
	}

	if (Mode == TALK_PLAYER_ANSWER) {
		if (Player->Request == 0) {
			return;
		}

		if (Player->RequestProcessingGamemaster == 0) {
			send_message(Connection, TALK_FAILURE_MESSAGE, "Please wait until your request is answered.");
			return;
		}

		TPlayer *Gamemaster = get_player(Player->RequestProcessingGamemaster);
		if (Gamemaster == NULL) {
			send_close_request(Connection);
			Player->Request = 0;
			return;
		}

		strcpy(Addressee, Gamemaster->Name);
	}

	if (Mode == TALK_GAMEMASTER_REQUEST) {
		if (Player->Request != 0) {
			send_message(Connection, TALK_FAILURE_MESSAGE,
						 "You have already submitted a request."
						 " Please wait until it is answered.");
			return;
		}

		Player->Request = AddDynamicString(Text);
		Player->RequestTimestamp = RoundNr;
		Player->RequestProcessingGamemaster = 0;
		create_gamemaster_request(Player->Name, Text);
	} else {
		try {
			Player->ToDoTalk(Mode, Addressee, Text, true);
			Player->ToDoStart();
		} catch (RESULT r) {
			send_result(Connection, r);
			Player->ToDoYield();
		}
	}
}

void CGetChannels(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CGetChannels: Connection is NULL.\n");
		return;
	}

	send_channels(Connection);
}

void CJoinChannel(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CJoinChannel: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int Channel = Buffer->readWord();
	if (Channel < get_number_of_channels()) {
		try {
			bool OwnChannel = join_channel(Channel, Player->ID);
			if (Channel == CHANNEL_RULEVIOLATIONS) {
				send_open_request_queue(Connection);
				send_existing_requests(Connection);
			} else if (OwnChannel) {
				send_open_own_channel(Connection, Channel);
			} else {
				send_open_channel(Connection, Channel);
			}
		} catch (RESULT r) {
			print(3, "Player is not allowed to listen to channel.\n");
		}
	}
}

void CLeaveChannel(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CLeaveChannel: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int Channel = Buffer->readWord();
	if (Channel < get_number_of_channels()) {
		leave_channel(Channel, Player->ID, true);
	}
}

void CPrivateChannel(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CPrivateChannel: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	char Name[30];
	Buffer->read_string(Name, sizeof(Name));
	if (Name[0] == 0) {
		return;
	}

	uint32 CharacterID = get_character_id(Name);
	if (CharacterID == 0) {
		send_result(Connection, PLAYERNOTEXISTING);
		return;
	}

	if (CharacterID == Player->ID) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "You cannot set up a private message channel with yourself.");
		return;
	}

	send_private_channel(Connection, get_character_name(Name));
}

void CProcessRequest(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CProcessRequest: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL || !channel_available(CHANNEL_RULEVIOLATIONS, Player->ID)) {
		return;
	}

	char Name[30];
	Buffer->read_string(Name, sizeof(Name));
	if (Name[0] == 0) {
		return;
	}

	TPlayer *Other;
	if (identify_player(Name, true, true, &Other) != 0) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "Player is not online any more.");
		send_finish_request(Connection, Name);
		return;
	}

	if (Other->Request == 0 || Other->RequestProcessingGamemaster != 0) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "Request has already been processed.");
		send_finish_request(Connection, Name);
		return;
	}

	Other->RequestProcessingGamemaster = Player->ID;
	delete_gamemaster_request(Other->Name);
}

void CRemoveRequest(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CRemoveRequest: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL || !channel_available(CHANNEL_RULEVIOLATIONS, Player->ID)) {
		return;
	}

	char Name[30];
	Buffer->read_string(Name, sizeof(Name));
	if (Name[0] == 0) {
		return;
	}

	TPlayer *Other;
	if (identify_player(Name, true, true, &Other) != 0) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "Player is not online any more.");
		send_finish_request(Connection, Name);
		return;
	}

	if (Other->Request != 0) {
		if (Other->RequestProcessingGamemaster == Player->ID) {
			Other->Request = 0;
			send_close_request(Other->Connection);
		} else if (Other->RequestProcessingGamemaster == 0) {
			delete_gamemaster_request(Other->Name);
		}
	}
}

void CCancelRequest(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CCancelRequest: Connection is NULL.\n");
		return;
	}

	// TODO(fusion): Does this mean regular players can't cancel their requests?
	TPlayer *Player = Connection->get_player();
	if (Player == NULL || !channel_available(CHANNEL_RULEVIOLATIONS, Player->ID)) {
		return;
	}

	if (Player->Request != 0) {
		Player->Request = 0;
		if (Player->RequestProcessingGamemaster != 0) {
			TPlayer *Gamemaster = get_player(Player->RequestProcessingGamemaster);
			if (Gamemaster != NULL) {
				send_finish_request(Gamemaster->Connection, Player->Name);
			}
		} else {
			delete_gamemaster_request(Player->Name);
		}
	}
}

void CSetTactics(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CSetTactics: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int AttackMode = Buffer->readByte();
	int ChaseMode = Buffer->readByte();
	int SecureMode = Buffer->readByte();

	if (AttackMode != ATTACK_MODE_OFFENSIVE && AttackMode != ATTACK_MODE_BALANCED &&
		AttackMode != ATTACK_MODE_DEFENSIVE) {
		print(3, "CSetTactics: Invalid attack mode %d.\n", AttackMode);
		return;
	}

	if (ChaseMode != CHASE_MODE_NONE && ChaseMode != CHASE_MODE_CLOSE) {
		print(3, "CSetTactics: Invalid chase mode %d.\n", ChaseMode);
		return;
	}

	if (SecureMode != SECURE_MODE_DISABLED && SecureMode != SECURE_MODE_ENABLED) {
		print(3, "CSetTactics: Invalid secure mode %d.\n", SecureMode);
		return;
	}

	Player->Combat.SetAttackMode((uint8)AttackMode);
	Player->Combat.SetChaseMode((uint8)ChaseMode);
	Player->Combat.SetSecureMode((uint8)SecureMode);
}

void CAttack(TConnection *Connection, TReadBuffer *Buffer, bool Follow) {
	if (Connection == NULL) {
		error("CAttack: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	uint32 TargetID = Buffer->readQuad();
	try {
		Player->Combat.SetAttackDest(TargetID, Follow);
		Player->ToDoAttack();
		Player->ToDoStart();
	} catch (RESULT r) {
		if (r != NOERROR) {
			send_result(Connection, r);
			Player->ToDoYield();
		}
	}
}

void CInviteToParty(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CInviteToParty: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	uint32 GuestID = Buffer->readQuad();
	TCreature *Guest = get_creature(GuestID);
	if (Guest == NULL) {
		return;
	}

	if (!CheckVisibility(CL_CMD_INVITE_TO_PARTY, Connection, Guest->posx, Guest->posy, Guest->posz)) {
		return;
	}

	invite_to_party(Player->ID, GuestID);
}

void CJoinParty(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CJoinParty: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	uint32 HostID = Buffer->readQuad();
	join_party(Player->ID, HostID);
}

void CRevokeInvitation(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CRevokeInvitation: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	if (Player->GetPartyLeader(false) == 0) {
		print(3, "Player is not a member of a party.\n");
		return;
	}

	uint32 GuestID = Buffer->readQuad();
	revoke_invitation(Player->ID, GuestID);
}

void CPassLeadership(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CPassLeadership: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	if (Player->GetPartyLeader(false) == 0) {
		print(3, "Player is not a member of a party.\n");
		return;
	}

	uint32 NewLeaderID = Buffer->readQuad();
	pass_leadership(Player->ID, NewLeaderID);
}

void CLeaveParty(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CLeaveParty: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	if (Player->GetPartyLeader(false) == 0) {
		print(3, "Player is not a member of a party.\n");
		return;
	}

	leave_party(Player->ID, false);
}

void COpenChannel(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("COpenChannel: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	try {
		open_channel(Player->ID);
	} catch (RESULT r) {
		send_result(Connection, r);
	}
}

void CInviteToChannel(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CInviteToChannel: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	char Name[30];
	Buffer->read_string(Name, sizeof(Name));
	if (Name[0] == 0) {
		print(2, "CInviteToChannel: No name specified.\n");
		return;
	}

	try {
		invite_to_channel(Player->ID, Name);
	} catch (RESULT r) {
		send_result(Connection, r);
	}
}

void CExcludeFromChannel(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CExcludeFromChannel: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	// TODO(fusion): The original function didn check if name was empty but it
	// is probably a good idea.
	char Name[30];
	Buffer->read_string(Name, sizeof(Name));
	if (Name[0] == 0) {
		print(2, "CInviteToChannel: No name specified.\n");
		return;
	}

	try {
		exclude_from_channel(Player->ID, Name);
	} catch (RESULT r) {
		send_result(Connection, r);
	}
}

void CCancel(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CCancel: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	try {
		Player->Combat.StopAttack(0);
		if (Player->ToDoClear()) {
			send_snapback(Connection);
		}
	} catch (RESULT r) {
		// no-op
	}

	Player->ToDoYield();
}

void CRefreshField(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CRefreshField: Connection is NULL.\n");
		return;
	}

	int FieldX = Buffer->readWord();
	int FieldY = Buffer->readWord();
	int FieldZ = Buffer->readByte();

	if (!is_on_map(FieldX, FieldY, FieldZ)) {
		return;
	}

	if (!CheckVisibility(CL_CMD_REFRESH_FIELD, Connection, FieldX, FieldY, FieldZ)) {
		return;
	}

	send_field_data(Connection, FieldX, FieldY, FieldZ);
}

void CRefreshContainer(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CRefreshContainer: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	int ContainerNr = Buffer->readByte();
	if (ContainerNr < NARRAY(TPlayer::OpenContainer)) {
		Object Con = Player->GetOpenContainer(ContainerNr);
		if (Con != NONE) {
			send_container(Connection, ContainerNr);
		}
	}
}

void CGetOutfit(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CGetOutfit: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	if (check_right(Player->ID, GAMEMASTER_OUTFIT)) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "You may not change your outfit.");
		return;
	}

	send_outfit(Connection);
}

void CSetOutfit(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CSetOutfit: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	TOutfit NewOutfit = {};
	NewOutfit.OutfitID = Buffer->readWord();
	Buffer->readBytes(NewOutfit.Colors, sizeof(NewOutfit.Colors));

	int FirstOutfit = (Player->Sex == 1) ? 128 : 136;
	int LastOutfit = (Player->Sex == 1) ? 131 : 139;
	if (check_right(Player->ID, PREMIUM_ACCOUNT)) {
		LastOutfit += 3;
	}

	if (NewOutfit.OutfitID < FirstOutfit || NewOutfit.OutfitID > LastOutfit) {
		if (Player->Sex == 1) {
			print(3, "CSetOutfit: Invalid outfit %d for males.\n", NewOutfit.OutfitID);
		} else {
			print(3, "CSetOutfit: Invalid outfit %d for females.\n", NewOutfit.OutfitID);
		}
		return;
	}

	if (NewOutfit.Colors[0] > 132 || NewOutfit.Colors[1] > 132 || NewOutfit.Colors[2] > 132 ||
		NewOutfit.Colors[3] > 132) {
		print(3, "CSetOutfit: Invalid color for outfit.\n");
		return;
	}

	if (Player->Outfit == Player->OrgOutfit) {
		Player->Outfit = NewOutfit;
		announce_changed_creature(Player->ID, CREATURE_OUTFIT_CHANGED);
	}

	Player->OrgOutfit = NewOutfit;
}

void CAddBuddy(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CAddBuddy: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	char Name[30];
	Buffer->read_string(Name, sizeof(Name));
	if (Name[0] != 0) {
		Player->AddBuddy(Name);
	}
}

void CRemoveBuddy(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CRemoveBuddy: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	uint32 CharacterID = Buffer->readQuad();
	Player->RemoveBuddy(CharacterID);
}

// NOTE(fusion): These logging functions were out of control. It's not necessarily
// better now but it is way simpler and do we even need any special formatting?

void CBugReport(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CBugReport: Connection is NULL.\n");
		return;
	}

	char Text[1024];
	Buffer->read_string(Text, sizeof(Text));

	bool empty = true;
	for (int i = 0; Text[i] != 0; i += 1) {
		if (!isprint(Text[i])) {
			Text[i] = 0;
			break;
		} else if (!isSpace(Text[i])) {
			empty = false;
		}
	}

	if (empty) {
		return;
	}

	// NOTE(fusion): `ctime` will already add a new line character which is why
	// there is none in this first `log_message` call.
	int PlayerX, PlayerY, PlayerZ;
	time_t Time = time(NULL);
	Connection->get_position(&PlayerX, &PlayerY, &PlayerZ);
	log_message("bugreport", "%s - %d/%d - [%d,%d,%d] - %s", Connection->get_name(), Connection->TerminalType,
				Connection->TerminalVersion, PlayerX, PlayerY, PlayerZ, ctime(&Time));
	log_message("bugreport", "%s\n", Text);
	log_message("bugreport", "---------------------------------------------------------------------------\n");

	send_message(Connection, TALK_STATUS_MESSAGE, "Comment sent.");
}

void CRuleViolation(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CRuleViolation: Connection is NULL.\n");
		return;
	}

	TPlayer *Player = Connection->get_player();
	if (Player == NULL) {
		return;
	}

	char Name[30];
	char Comment[200];
	Buffer->read_string(Name, sizeof(Name));
	int Reason = Buffer->readByte();
	int Action = Buffer->readByte();
	Buffer->read_string(Comment, sizeof(Comment));
	uint32 StatementID = Buffer->readQuad();
	bool ip_banishment = Buffer->readByte() != 0;

	if (!check_banishment_right(Player->ID, Reason, Action) ||
		(ip_banishment && !check_right(Player->ID, IP_BANISHMENT))) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "You have no authorization for this action.");
		return;
	}

	if (Comment[0] == 0) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "Please provide a comment for this report.");
		return;
	}

	if (Action == 6 && StatementID == 0) {
		send_message(Connection, TALK_FAILURE_MESSAGE, "Please provide a statement for this rule violation.");
		return;
	}

	TPlayer *Criminal;
	const char *IPAddress = NULL;
	bool IgnoreGamemasters = !check_right(Player->ID, READ_GAMEMASTER_CHANNEL);
	if (identify_player(Name, false, IgnoreGamemasters, &Criminal) == 0) {
		if (check_right(Criminal->ID, NO_BANISHMENT)) {
			if (Action == 1 || Action == 3 || Action == 5) {
				send_message(Connection, TALK_FAILURE_MESSAGE, "This name has already been approved.");
				return;
			}

			if (Action != 6) {
				send_message(Connection, TALK_FAILURE_MESSAGE, "You may not report a god or gamemaster.");
				return;
			}
		}

		strcpy(Name, Criminal->Name);
		IPAddress = Criminal->IPAddress;
	} else {
		if (get_character_id(Name) == 0) {
			send_result(Connection, PLAYERNOTEXISTING);
			return;
		}

		strcpy(Name, get_character_name(Name));
		if (ip_banishment) {
			send_message(Connection, TALK_FAILURE_MESSAGE, "Player is not online. No IP address was banished.");
			ip_banishment = false;
		}
	}

	int NumberOfStatements = 0;
	vector<ReportedStatement> *ReportedStatements = NULL;
	bool HasStatement = ((Reason >= 9 && Reason <= 27) || Reason == 29);
	if (HasStatement && StatementID != 0) {
		uint32 ListenerID = (Action != 6 ? Player->ID : 0);
		int Context = get_communication_context(ListenerID, StatementID, &NumberOfStatements, &ReportedStatements);
		if (Context == 1) {
			send_message(Connection, TALK_FAILURE_MESSAGE, "Statement is unknown. Perhaps it is too old?");
			return;
		} else if (Context == 2) {
			send_message(Connection, TALK_FAILURE_MESSAGE, "Statement has already been reported.");
			return;
		}
	}

	punishment_order(Player, Name, IPAddress, Reason, Action, Comment, NumberOfStatements, ReportedStatements,
					 StatementID, ip_banishment);
}

void CErrorFileEntry(TConnection *Connection, TReadBuffer *Buffer) {
	if (Connection == NULL) {
		error("CErrorFileEntry: Connection is NULL.\n");
		return;
	}

	char Title[64];
	char Date[32];
	char Stack[2048];
	char Comment[512];
	Buffer->read_string(Title, sizeof(Title));
	Buffer->read_string(Date, sizeof(Date));
	Buffer->read_string(Stack, sizeof(Stack));
	Buffer->read_string(Comment, sizeof(Comment));
	if (Stack[0] == 0 && Comment[0] == 0) {
		return;
	}

	// NOTE(fusion): `ctime` will already add a new line character which is why
	// there is none in this first `log_message` call.
	int PlayerX, PlayerY, PlayerZ;
	time_t Time = time(NULL);
	Connection->get_position(&PlayerX, &PlayerY, &PlayerZ);
	log_message("client-error", "%s - %d/%d - [%d,%d,%d] - %s", Connection->get_name(), Connection->TerminalType,
				Connection->TerminalVersion, PlayerX, PlayerY, PlayerZ, ctime(&Time));
	log_message("client-error", "%s\n", Title);
	log_message("client-error", "%s\n", Date);

	if (Stack[0] != 0) {
		log_message("client-error", "\n");
		log_message("client-error", "%s\n", Stack);
	}

	if (Comment[0] != 0) {
		log_message("client-error", "\n");
		log_message("client-error", "%s\n", Comment);
	}

	log_message("client-error", "---------------------------------------------------------------------------\n");
}

void receive_data(TConnection *Connection) {
	if (Connection == NULL) {
		error("ReceiveData: Connection is NULL.\n");
		return;
	}

	if (Connection->InDataSize <= 0) {
		error("ReceiveData: No data available.\n");
		return;
	}

	if ((Connection->InDataSize + 2) > (int)sizeof(Connection->InData)) {
		error("ReceiveData: Packet length %d too large.\n", Connection->InDataSize);
		return;
	}

	// TODO(fusion): I thought the client would actually pack multiple commands
	// in the same packet. Perhaps not until some later version?

	// IMPORTANT(fusion): The actual payload size is in the buffer's first two
	// bytes which is why we start reading from offset two.
	TReadBuffer Buffer(Connection->InData + 2, Connection->InDataSize);

	uint8 Command;
	try {
		Command = Buffer.readByte();
	} catch (const char *str) {
		error("ReceiveData: Error reading command (%s).\n", str);
		return;
	}

	if (Connection->State == CONNECTION_LOGIN) {
		if (Command != CL_CMD_LOGIN) {
			error("ReceiveData: Wrong login command %d.\n", Command);
		} else if (!Connection->join_game(&Buffer)) {
			log_message("game", "Login failed.\n");
			send_result(Connection, LOGINERROR);
			TPlayer *Player = Connection->get_player();
			if (Player != NULL) {
				decrement_is_online_order(Player->ID);
			} else {
				error("ReceiveData: Login failed and no creature known.\n");
			}
			Connection->disconnect();
		}
		return;
	}

	if (!CommandAllowed(Connection, Command)) {
		return;
	}

	Connection->reset_timer(Command);
	try {
		switch (Command) {
		case CL_CMD_LOGOUT:
			CQuitGame(Connection, &Buffer);
			break;
		case CL_CMD_PING:
			CPing(Connection, &Buffer);
			break;
		case CL_CMD_GO_PATH:
			CGoPath(Connection, &Buffer);
			break;
		case CL_CMD_GO_NORTH:
			CGoDirection(Connection, 0, -1);
			break;
		case CL_CMD_GO_EAST:
			CGoDirection(Connection, 1, 0);
			break;
		case CL_CMD_GO_SOUTH:
			CGoDirection(Connection, 0, 1);
			break;
		case CL_CMD_GO_WEST:
			CGoDirection(Connection, -1, 0);
			break;
		case CL_CMD_GO_STOP:
			CGoStop(Connection, &Buffer);
			break;
		case CL_CMD_GO_NORTHEAST:
			CGoDirection(Connection, 1, -1);
			break;
		case CL_CMD_GO_SOUTHEAST:
			CGoDirection(Connection, 1, 1);
			break;
		case CL_CMD_GO_SOUTHWEST:
			CGoDirection(Connection, -1, 1);
			break;
		case CL_CMD_GO_NORTHWEST:
			CGoDirection(Connection, -1, -1);
			break;
		case CL_CMD_ROTATE_NORTH:
			CRotate(Connection, DIRECTION_NORTH);
			break;
		case CL_CMD_ROTATE_EAST:
			CRotate(Connection, DIRECTION_EAST);
			break;
		case CL_CMD_ROTATE_SOUTH:
			CRotate(Connection, DIRECTION_SOUTH);
			break;
		case CL_CMD_ROTATE_WEST:
			CRotate(Connection, DIRECTION_WEST);
			break;
		case CL_CMD_MOVE_OBJECT:
			CMoveObject(Connection, &Buffer);
			break;
		case CL_CMD_TRADE_OBJECT:
			CTradeObject(Connection, &Buffer);
			break;
		case CL_CMD_INSPECT_TRADE:
			CInspectTrade(Connection, &Buffer);
			break;
		case CL_CMD_ACCEPT_TRADE:
			CAcceptTrade(Connection, &Buffer);
			break;
		case CL_CMD_REJECT_TRADE:
			CRejectTrade(Connection, &Buffer);
			break;
		case CL_CMD_USE_OBJECT:
			CUseObject(Connection, &Buffer);
			break;
		case CL_CMD_USE_TWO_OBJECTS:
			CUseTwoObjects(Connection, &Buffer);
			break;
		case CL_CMD_USE_ON_CREATURE:
			CUseOnCreature(Connection, &Buffer);
			break;
		case CL_CMD_TURN_OBJECT:
			CTurnObject(Connection, &Buffer);
			break;
		case CL_CMD_CLOSE_CONTAINER:
			CCloseContainer(Connection, &Buffer);
			break;
		case CL_CMD_UP_CONTAINER:
			CUpContainer(Connection, &Buffer);
			break;
		case CL_CMD_EDIT_TEXT:
			CEditText(Connection, &Buffer);
			break;
		case CL_CMD_EDIT_LIST:
			CEditList(Connection, &Buffer);
			break;
		case CL_CMD_LOOK_AT_POINT:
			CLookAtPoint(Connection, &Buffer);
			break;
		case CL_CMD_TALK:
			CTalk(Connection, &Buffer);
			break;
		case CL_CMD_GET_CHANNELS:
			CGetChannels(Connection, &Buffer);
			break;
		case CL_CMD_JOIN_CHANNEL:
			CJoinChannel(Connection, &Buffer);
			break;
		case CL_CMD_LEAVE_CHANNEL:
			CLeaveChannel(Connection, &Buffer);
			break;
		case CL_CMD_PRIVATE_CHANNEL:
			CPrivateChannel(Connection, &Buffer);
			break;
		case CL_CMD_PROCESS_REQUEST:
			CProcessRequest(Connection, &Buffer);
			break;
		case CL_CMD_REMOVE_REQUEST:
			CRemoveRequest(Connection, &Buffer);
			break;
		case CL_CMD_CANCEL_REQUEST:
			CCancelRequest(Connection, &Buffer);
			break;
		case CL_CMD_SET_TACTICS:
			CSetTactics(Connection, &Buffer);
			break;
		case CL_CMD_ATTACK:
			CAttack(Connection, &Buffer, false);
			break;
		case CL_CMD_FOLLOW:
			CAttack(Connection, &Buffer, true);
			break;
		case CL_CMD_INVITE_TO_PARTY:
			CInviteToParty(Connection, &Buffer);
			break;
		case CL_CMD_JOIN_PARTY:
			CJoinParty(Connection, &Buffer);
			break;
		case CL_CMD_REVOKE_INVITATION:
			CRevokeInvitation(Connection, &Buffer);
			break;
		case CL_CMD_PASS_LEADERSHIP:
			CPassLeadership(Connection, &Buffer);
			break;
		case CL_CMD_LEAVE_PARTY:
			CLeaveParty(Connection, &Buffer);
			break;
		case CL_CMD_OPEN_CHANNEL:
			COpenChannel(Connection, &Buffer);
			break;
		case CL_CMD_INVITE_TO_CHANNEL:
			CInviteToChannel(Connection, &Buffer);
			break;
		case CL_CMD_EXCLUDE_FROM_CHANNEL:
			CExcludeFromChannel(Connection, &Buffer);
			break;
		case CL_CMD_CANCEL:
			CCancel(Connection, &Buffer);
			break;
		case CL_CMD_REFRESH_FIELD:
			CRefreshField(Connection, &Buffer);
			break;
		case CL_CMD_REFRESH_CONTAINER:
			CRefreshContainer(Connection, &Buffer);
			break;
		case CL_CMD_GET_OUTFIT:
			CGetOutfit(Connection, &Buffer);
			break;
		case CL_CMD_SET_OUTFIT:
			CSetOutfit(Connection, &Buffer);
			break;
		case CL_CMD_ADD_BUDDY:
			CAddBuddy(Connection, &Buffer);
			break;
		case CL_CMD_REMOVE_BUDDY:
			CRemoveBuddy(Connection, &Buffer);
			break;
		case CL_CMD_BUG_REPORT:
			CBugReport(Connection, &Buffer);
			break;
		case CL_CMD_RULE_VIOLATION:
			CRuleViolation(Connection, &Buffer);
			break;
		case CL_CMD_ERROR_FILE_ENTRY:
			CErrorFileEntry(Connection, &Buffer);
			break;
		default: {
			print(3, "Unknown command %d.\n", Command);
			break;
		}
		}
	} catch (const char *str) {
		error("Error reading data for command %d (%s).\n", Command, str);
	}
}

void receive_data(void) {
	TConnection *Connection = get_first_connection();
	while (Connection != NULL) {
		if (Connection->live() && Connection->WaitingForACK) {
			receive_data(Connection);
			Connection->WaitingForACK = false;
			// NOTE(fusion): `SIGUSR1` is used to signal the connection thread
			// that we parsed all received data and that it may resume reading.
			// We check if the connection is still live because it may have been
			// disconnected inside `ReceiveData`.
			if (Connection->live()) {
				tgkill(GetGameProcessID(), Connection->get_thread_id(), SIGUSR1);
			}
		}
		Connection = get_next_connection();
	}
}

void init_receiving(void) {
	// no-op
}

void exit_receiving(void) {
	// no-op
}
