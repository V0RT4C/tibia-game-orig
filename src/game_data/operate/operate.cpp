#include "game_data/operate/operate.h"
#include "config.h"
#include "houses.h"
#include "info.h"
#include "magic.h"
#include "moveuse.h"
#include "reader.h"

#include <dirent.h>

static fifo<Statement> Statements(1024);
static fifo<Listener> Listeners(1024);

static vector<Channel> ChannelArray(0, PUBLIC_CHANNELS + 10, 10);
static int Channels = PUBLIC_CHANNELS;
static int CurrentChannelID;
static int CurrentSubscriberNumber;

static vector<Party> PartyArray(0, 100, 50);
static int Parties;

// World Operations
// =============================================================================

// TODO(fusion): The radii parameters for `FindCreatures` are commonly around
// 16 and 14 for x and y respectively so there is probably some constant involved.
//	Also, since we're talking about radii, these values are quite large when you
// consider the regular client's viewport dimensions of 15x11 visible fields or
// 18x14 total fields.

void announce_moving_creature(uint32 CreatureID, Object Con) {
	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("announce_moving_creature: Creature %d does not exist.\n", CreatureID);
		return;
	}

	int ConX, ConY, ConZ;
	get_object_coordinates(Con, &ConX, &ConY, &ConZ);

	int SearchRadiusX = 16 + (std::abs(Creature->posx - ConX) / 2) + 1;
	int SearchRadiusY = 14 + (std::abs(Creature->posy - ConY) / 2) + 1;
	int SearchCenterX = (Creature->posx + ConX) / 2;
	int SearchCenterY = (Creature->posy + ConY) / 2;
	FindCreatures Search(SearchRadiusX, SearchRadiusY, SearchCenterX, SearchCenterY, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL || Player->Connection == NULL) {
			continue;
		}

		SendMoveCreature(Player->Connection, CreatureID, ConX, ConY, ConZ);
	}
}

void announce_changed_creature(uint32 CreatureID, int Type) {
	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("announce_changed_creature: Creature %d does not exist (Type=%d).\n", CreatureID, Type);
		return;
	}

	for (TKnownCreature *KnownCreature = Creature->FirstKnowingConnection; KnownCreature != NULL;
		 KnownCreature = KnownCreature->Next) {
		TConnection *KnowingConnection = KnownCreature->Connection;
		if (KnowingConnection->State != CONNECTION_GAME || KnownCreature->State == KNOWNCREATURE_OUTDATED) {
			continue;
		}

		if (KnowingConnection->IsVisible(Creature->posx, Creature->posy, Creature->posz)) {
			switch (Type) {
			case CREATURE_HEALTH_CHANGED:
				SendCreatureHealth(KnowingConnection, CreatureID);
				break;
			case CREATURE_LIGHT_CHANGED:
				SendCreatureLight(KnowingConnection, CreatureID);
				break;
			case CREATURE_OUTFIT_CHANGED:
				SendCreatureOutfit(KnowingConnection, CreatureID);
				break;
			case CREATURE_SPEED_CHANGED:
				SendCreatureSpeed(KnowingConnection, CreatureID);
				break;
			case CREATURE_SKULL_CHANGED:
				SendCreatureSkull(KnowingConnection, CreatureID);
				break;
			case CREATURE_PARTY_CHANGED:
				SendCreatureParty(KnowingConnection, CreatureID);
				break;
			}
		} else {
			KnownCreature->State = KNOWNCREATURE_OUTDATED;
		}
	}
}

void announce_changed_field(Object Obj, int Type) {
	if (!Obj.exists()) {
		error("announce_changed_field: Passed object does not exist.\n");
		return;
	}

	int ObjX, ObjY, ObjZ;
	get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
	FindCreatures Search(16, 14, ObjX, ObjY, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL || Player->Connection == NULL) {
			continue;
		}

		if (!Player->Connection->IsVisible(ObjX, ObjY, ObjZ)) {
			continue;
		}

		switch (Type) {
		case OBJECT_DELETED:
			SendDeleteField(Player->Connection, ObjX, ObjY, ObjZ, Obj);
			break;
		case OBJECT_CREATED:
			SendAddField(Player->Connection, ObjX, ObjY, ObjZ, Obj);
			break;
		case OBJECT_CHANGED:
			SendChangeField(Player->Connection, ObjX, ObjY, ObjZ, Obj);
			break;
		default: {
			error("announce_changed_field: Invalid type %d.\n", Type);
			return;
		}
		}
	}
}

void announce_changed_container(Object Obj, int Type) {
	if (!Obj.exists()) {
		error("announce_changed_container: Passed object does not exist.\n");
		return;
	}

	Object Con = Obj.get_container();
	FindCreatures Search(1, 1, Obj, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL || Player->Connection == NULL) {
			continue;
		}

		for (int ContainerNr = 0; ContainerNr < NARRAY(Player->OpenContainer); ContainerNr += 1) {
			if (Player->GetOpenContainer(ContainerNr) != Con) {
				continue;
			}

			switch (Type) {
			case OBJECT_DELETED:
				SendDeleteInContainer(Player->Connection, ContainerNr, Obj);
				break;
			case OBJECT_CREATED:
				SendCreateInContainer(Player->Connection, ContainerNr, Obj);
				break;
			case OBJECT_CHANGED:
				SendChangeInContainer(Player->Connection, ContainerNr, Obj);
				break;
			default: {
				error("announce_changed_container: Invalid type %d.\n", Type);
				return;
			}
			}
		}
	}
}

void announce_changed_inventory(Object Obj, int Type) {
	if (!Obj.exists()) {
		error("announce_changed_inventory: Passed object does not exist.\n");
		return;
	}

	uint32 OwnerID = get_object_creature_id(Obj);
	TCreature *Creature = get_creature(OwnerID);
	if (Creature == NULL) {
		error("announce_changed_inventory: Object is not in any inventory.\n");
		return;
	}

	if (Creature->Type != PLAYER) {
		return;
	}

	int Position = get_object_body_position(Obj);
	switch (Type) {
	case OBJECT_DELETED:
		SendDeleteInventory(Creature->Connection, Position);
		break;
	case OBJECT_CREATED:
		SendSetInventory(Creature->Connection, Position, Obj);
		break;
	case OBJECT_CHANGED:
		SendSetInventory(Creature->Connection, Position, Obj);
		break;
	default: {
		error("announce_changed_inventory: Invalid type %d.\n", Type);
		return;
	}
	}
}

void announce_changed_object(Object Obj, int Type) {
	if (!Obj.exists()) {
		error("announce_changed_object: Passed object does not exist.\n");
		return;
	}

	Object Con = Obj.get_container();
	ObjectType ConType = Con.get_object_type();
	if (ConType.is_map_container()) {
		announce_changed_field(Obj, Type);
	} else if (ConType.is_body_container()) {
		announce_changed_inventory(Obj, Type);
	} else {
		announce_changed_container(Obj, Type);
	}
}

void announce_graphical_effect(int x, int y, int z, int Type) {
	if (!is_on_map(x, y, z)) {
		return;
	}

	FindCreatures Search(16, 14, x, y, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL || Player->Connection == NULL) {
			continue;
		}

		if (!Player->Connection->IsVisible(x, y, z)) {
			continue;
		}

		SendGraphicalEffect(Player->Connection, x, y, z, Type);
	}
}

void announce_textual_effect(int x, int y, int z, int Color, const char *Text) {
	if (!is_on_map(x, y, z)) {
		return;
	}

	FindCreatures Search(16, 14, x, y, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL || Player->Connection == NULL) {
			continue;
		}

		if (!Player->Connection->IsVisible(x, y, z)) {
			continue;
		}

		SendTextualEffect(Player->Connection, x, y, z, Color, Text);
	}
}

void announce_missile(int OrigX, int OrigY, int OrigZ, int DestX, int DestY, int DestZ, int Type) {
	if (!is_on_map(OrigX, OrigY, OrigZ) || !is_on_map(DestX, DestY, DestZ)) {
		return;
	}

	int SearchRadiusX = 16 + (std::abs(OrigX - DestX) / 2) + 1;
	int SearchRadiusY = 14 + (std::abs(OrigY - DestY) / 2) + 1;
	int SearchCenterX = (OrigX + DestX) / 2;
	int SearchCenterY = (OrigY + DestY) / 2;
	FindCreatures Search(SearchRadiusX, SearchRadiusY, SearchCenterX, SearchCenterY, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL || Player->Connection == NULL) {
			continue;
		}

		if (!Player->Connection->IsVisible(OrigX, OrigY, OrigZ) &&
			!Player->Connection->IsVisible(DestX, DestY, DestZ)) {
			continue;
		}

		SendMissileEffect(Player->Connection, OrigX, OrigY, OrigZ, DestX, DestY, DestZ, Type);
	}
}

void check_top_move_object(uint32 CreatureID, Object Obj, Object Ignore) {
	if (!Obj.exists()) {
		error("check_top_move_object: Object does not exist.\n");
		throw ERROR;
	}

	if (CreatureID == 0 || !is_creature_player(CreatureID)) {
		return;
	}

	Object Con = Obj.get_container();
	ObjectType ConType = Con.get_object_type();
	if (!ConType.is_map_container()) {
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (ObjType.is_creature_container() && Obj.get_creature_id() == CreatureID) {
		return;
	}

	Object Best = NONE;
	bool BestIsCreature = false;
	Object Help = get_first_container_object(Con);
	while (Help != NONE) {
		if (Help != Ignore) {
			ObjectType HelpType = Help.get_object_type();

			// NOTE(fusion): We're looking for the top move object. The creature
			// check here makes sure only the first creature found is kept as the
			// best candidate, since creatures on a map container are in a sequence.
			if (Best == NONE ||
				(!HelpType.get_flag(UNMOVE) && (!BestIsCreature || !HelpType.is_creature_container()))) {
				Best = Help;
				BestIsCreature = HelpType.is_creature_container();
			}

			if (get_object_priority(Help) == PRIORITY_LOW) {
				break;
			}
		}
		Help = Help.get_next_object();
	}

	if (Obj != Best) {
		throw NOTACCESSIBLE;
	}
}

void check_top_use_object(uint32 CreatureID, Object Obj) {
	if (!Obj.exists()) {
		error("check_top_use_object: Object does not exist.\n");
		throw ERROR;
	}

	if (CreatureID == 0 || !is_creature_player(CreatureID)) {
		return;
	}

	Object Con = Obj.get_container();
	ObjectType ConType = Con.get_object_type();
	if (!ConType.is_map_container()) {
		return;
	}

	Object Best = NONE;
	Object Help = get_first_container_object(Con);
	while (Help != NONE) {
		ObjectType HelpType = Help.get_object_type();
		if (Best == NONE || (!HelpType.is_creature_container() && !HelpType.get_flag(LIQUIDPOOL))) {
			Best = Help;
		}

		if (HelpType.get_flag(FORCEUSE) || get_object_priority(Help) == PRIORITY_LOW) {
			break;
		}

		Help = Help.get_next_object();
	}

	if (Obj != Best) {
		throw NOTACCESSIBLE;
	}
}

void check_top_multiuse_object(uint32 CreatureID, Object Obj) {
	if (!Obj.exists()) {
		error("check_top_multiuse_object: Object does not exist.\n");
		throw ERROR;
	}

	if (CreatureID == 0 || !is_creature_player(CreatureID)) {
		return;
	}

	Object Con = Obj.get_container();
	ObjectType ConType = Con.get_object_type();
	if (!ConType.is_map_container()) {
		return;
	}

	Object Best = NONE;
	Object Help = get_first_container_object(Con);
	while (Help != NONE) {
		ObjectType HelpType = Help.get_object_type();
		if (Best == NONE || !HelpType.get_flag(LIQUIDPOOL)) {
			Best = Help;
		}

		if (HelpType.get_flag(FORCEUSE) || get_object_priority(Help) == PRIORITY_CREATURE ||
			get_object_priority(Help) == PRIORITY_LOW) {
			break;
		}

		Help = Help.get_next_object();
	}

	if (Obj != Best) {
		throw NOTACCESSIBLE;
	}
}

void check_move_object(uint32 CreatureID, Object Obj, bool Take) {
	if (CreatureID == 0) {
		return;
	}

	if (!object_accessible(CreatureID, Obj, 1)) {
		throw NOTACCESSIBLE;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (ObjType.get_flag(UNMOVE)) {
		throw NOTMOVABLE;
	}

	if (ObjType.is_creature_container() && Obj.get_creature_id() != CreatureID) {
		TCreature *MovingCreature = get_creature(Obj);
		if (MovingCreature == NULL) {
			error("check_move_object: Creature does not exist.\n");
			throw ERROR;
		}

		if (get_race_unpushable(MovingCreature->Race) && (WorldType != NON_PVP || !MovingCreature->IsPeaceful())) {
			throw NOTMOVABLE;
		}
	}

	if (Take && !ObjType.get_flag(TAKE)) {
		throw NOTTAKABLE;
	}
}

// NOTE(fusion): This is a helper function for `check_map_destination` and `check_map_place`
// and improves the readability of otherwise two convoluted functions.
static bool IsMapBlocked(int DestX, int DestY, int DestZ, ObjectType Type) {
	bool HasBank = coordinate_flag(DestX, DestY, DestZ, BANK);
	if (HasBank && !coordinate_flag(DestX, DestY, DestZ, UNPASS)) {
		return false;
	}

	if (!Type.get_flag(UNPASS)) {
		if (HasBank && !coordinate_flag(DestX, DestY, DestZ, UNLAY)) {
			return false;
		}

		if (Type.get_flag(HANG)) {
			bool HasHook =
				coordinate_flag(DestX, DestY, DestZ, HOOKSOUTH) || coordinate_flag(DestX, DestY, DestZ, HOOKEAST);
			if (HasHook && !coordinate_flag(DestX, DestY, DestZ, HANG)) {
				return false;
			}
		}
	}

	return true;
}

void check_map_destination(uint32 CreatureID, Object Obj, Object MapCon) {
	if (CreatureID == 0) {
		return;
	}

	int OrigX, OrigY, OrigZ;
	int DestX, DestY, DestZ;
	ObjectType ObjType = Obj.get_object_type();
	get_object_coordinates(Obj, &OrigX, &OrigY, &OrigZ);
	get_object_coordinates(MapCon, &DestX, &DestY, &DestZ);
	if (!ObjType.is_creature_container()) {
		if (IsMapBlocked(DestX, DestY, DestZ, ObjType)) {
			throw NOROOM;
		}

		if (!ObjType.get_flag(TAKE) && !object_in_range(CreatureID, MapCon, 2)) {
			throw OUTOFRANGE;
		}
	} else {
		if (std::abs(OrigX - DestX) > 1 || std::abs(OrigY - DestY) > 1 || std::abs(OrigZ - DestZ) > 1) {
			throw OUTOFRANGE;
		}

		if (DestZ == (OrigZ - 1)) {
			if (get_height(OrigX, OrigY, OrigZ) < 24) { // JUMP_HEIGHT ?
				throw NOROOM;
			}
		} else if (DestZ == (OrigZ + 1)) {
			if (get_height(DestX, DestY, DestZ) < 24) { // JUMP_HEIGHT ?
				throw NOWAY;
			}
		}

		TCreature *MovingCreature = get_creature(Obj);
		if (MovingCreature == NULL) {
			error("check_map_destination: Moving creature is NULL.");
			throw ERROR;
		}

		if (OrigZ == DestZ || MovingCreature->Type != MONSTER) {
			if (!MovingCreature->MovePossible(DestX, DestY, DestZ, true, OrigZ != DestZ)) {
				if (CreatureID == MovingCreature->ID) {
					throw MOVENOTPOSSIBLE;
				} else {
					throw NOROOM;
				}
			}

			if (CreatureID != MovingCreature->ID) {
				if (coordinate_flag(DestX, DestY, DestZ, AVOID)) {
					throw NOROOM;
				}

				if (is_protection_zone(OrigX, OrigY, OrigZ) && !is_protection_zone(DestX, DestY, DestZ)) {
					throw PROTECTIONZONE;
				}
			}
		}
	}

	// TODO(fusion): This looks awfully similar to `object_accessible` outside
	// from the exceptions and floor check.
	if (ObjType.get_flag(HANG)) {
		bool HookSouth = coordinate_flag(DestX, DestY, DestZ, HOOKSOUTH);
		bool HookEast = coordinate_flag(DestX, DestY, DestZ, HOOKEAST);
		if (HookSouth || HookEast) {
			TCreature *Creature = get_creature(CreatureID);
			if (Creature == NULL) {
				error("check_map_destination: Executing creature does not exist.\n");
				throw ERROR;
			}

			if (Creature->posz > DestZ) {
				throw UPSTAIRS;
			} else if (Creature->posz < DestZ) {
				throw DOWNSTAIRS;
			}

			if (HookSouth) {
				if (Creature->posy < DestY || Creature->posy > (DestY + 1) || Creature->posx < (DestX - 1) ||
					Creature->posx > (DestX + 1)) {
					throw OUTOFRANGE;
				}
			}

			if (HookEast) {
				if (Creature->posx < DestX || Creature->posx > (DestX + 1) || Creature->posy < (DestY - 1) ||
					Creature->posy > (DestY + 1)) {
					throw OUTOFRANGE;
				}
			}

			return;
		}
	}

	if (!throw_possible(OrigX, OrigY, OrigZ, DestX, DestY, DestZ, 1)) {
		throw CANNOTTHROW;
	}
}

void check_map_place(uint32 CreatureID, ObjectType Type, Object MapCon) {
	int DestX, DestY, DestZ;
	get_object_coordinates(MapCon, &DestX, &DestY, &DestZ);

	if (!Type.get_flag(UNMOVE) && !coordinate_flag(DestX, DestY, DestZ, BANK)) {
		if (!Type.get_flag(HANG)) {
			throw NOROOM;
		}

		if (!coordinate_flag(DestX, DestY, DestZ, HOOKSOUTH) && !coordinate_flag(DestX, DestY, DestZ, HOOKEAST)) {
			throw NOROOM;
		}
	}

	// TODO(fusion): The original function used a slightly modified version of
	// `IsMapBlocked` which wouldn't check if there was already a HANG item at
	// the destination but since this function seems to only be called from
	// `create` which sets `CreatureID` to zero, I assume this was probably an
	// oversight?
	if (CreatureID != 0 && IsMapBlocked(DestX, DestY, DestZ, Type)) {
		throw NOROOM;
	}
}

void check_container_destination(Object Obj, Object Con) {
	if (is_held_by_container(Con, Obj)) {
		throw CROSSREFERENCE;
	}

	ObjectType ConType = Con.get_object_type();
	if (!ConType.get_flag(CHEST)) {
		int ConObjects = count_objects_in_container(Con);
		int ConCapacity = (int)ConType.get_attribute(CAPACITY);
		if (ConObjects >= ConCapacity) {
			throw CONTAINERFULL;
		}
	}
}

void check_container_place(ObjectType Type, Object Con, Object OldObj) {
	if (Type == get_special_object(DEPOT_CHEST)) {
		return;
	}

	ObjectType ConType = Con.get_object_type();
	if (!ConType.get_flag(CHEST)) {
		if (Type.get_flag(UNMOVE)) {
			throw NOTMOVABLE;
		}

		if (!Type.get_flag(TAKE)) {
			throw NOTTAKABLE;
		}

		if (OldObj == NONE) {
			int ConObjects = count_objects_in_container(Con);
			int ConCapacity = (int)ConType.get_attribute(CAPACITY);
			if (ConObjects >= ConCapacity) {
				throw CONTAINERFULL;
			}
		}
	}
}

void check_depot_space(uint32 CreatureID, Object Source, Object Destination, int Count) {
	if (CreatureID == 0) {
		return;
	}

	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("check_depot_space: Creature %u does not exist.\n", CreatureID);
		throw ERROR;
	}

	if (Destination == NONE) {
		error("check_depot_space: Destination does not exist.\n");
		throw ERROR;
	}

	if (Creature->Type == PLAYER) {
		TPlayer *Player = (TPlayer *)Creature;
		if (Player->Depot == NONE || Count <= Player->DepotSpace) {
			return;
		}

		if (!is_held_by_container(Source, Player->Depot) && is_held_by_container(Destination, Player->Depot)) {
			throw CONTAINERFULL;
		}
	}
}

void check_inventory_destination(Object Obj, Object Con, bool Split) {
	ObjectType ObjType = Obj.get_object_type();
	int ConPosition = get_object_body_position(Con);
	bool HandContainer = ConPosition == INVENTORY_RIGHTHAND || ConPosition == INVENTORY_LEFTHAND;

	if (!HandContainer && ConPosition != INVENTORY_AMMO) {
		if (!ObjType.get_flag(CLOTHES)) {
			throw WRONGPOSITION;
		}

		int ObjPosition = (int)ObjType.get_attribute(BODYPOSITION);
		if (ObjPosition == 0) {
			throw WRONGPOSITION2;
		} else if (ObjPosition != ConPosition) {
			throw WRONGCLOTHES;
		}
	}

	uint32 CreatureID = get_object_creature_id(Con);
	if (HandContainer && ObjType.is_two_handed()) {
		Object RightHand = get_body_object(CreatureID, INVENTORY_RIGHTHAND);
		if (RightHand != NONE && RightHand != Obj) {
			throw HANDSNOTFREE;
		}

		Object LeftHand = get_body_object(CreatureID, INVENTORY_LEFTHAND);
		if (LeftHand != NONE && LeftHand != Obj) {
			throw HANDSNOTFREE;
		}

		return;
	}

	if (get_body_object(CreatureID, ConPosition) != NONE) {
		throw NOROOM;
	}

	if (HandContainer) {
		for (int Position = INVENTORY_HAND_FIRST; Position <= INVENTORY_HAND_LAST; Position += 1) {
			Object Other = get_body_object(CreatureID, Position);
			if (Other != NONE) {
				ObjectType OtherType = Other.get_object_type();
				if (OtherType.is_two_handed()) {
					throw HANDBLOCKED;
				}

				if (Split || Other != Obj) {
					if (OtherType.is_weapon() && ObjType.is_weapon()) {
						throw ONEWEAPONONLY;
					}
				}
			}
		}
	}
}

void check_inventory_place(ObjectType Type, Object Con, Object OldObj) {
	// TODO(fusion): This is awfully similar to `check_inventory_destination` with
	// a few subtle differences. Perhaps they use the same underlying function?

	int ConPosition = get_object_body_position(Con);
	bool HandContainer = ConPosition == INVENTORY_RIGHTHAND || ConPosition == INVENTORY_LEFTHAND;

	if (!HandContainer && ConPosition != INVENTORY_AMMO) {
		if (!Type.get_flag(CLOTHES)) {
			throw WRONGPOSITION;
		}

		int TypePosition = (int)Type.get_attribute(BODYPOSITION);
		if (TypePosition == 0) {
			throw WRONGPOSITION2;
		} else if (TypePosition != ConPosition) {
			throw WRONGCLOTHES;
		}
	}

	uint32 CreatureID = get_object_creature_id(Con);
	if (HandContainer && Type.is_two_handed()) {
		Object RightHand = get_body_object(CreatureID, INVENTORY_RIGHTHAND);
		if (RightHand != NONE && RightHand != OldObj) {
			throw HANDSNOTFREE;
		}

		Object LeftHand = get_body_object(CreatureID, INVENTORY_LEFTHAND);
		if (LeftHand != NONE && LeftHand != OldObj) {
			throw HANDSNOTFREE;
		}

		return;
	}

	if (get_body_object(CreatureID, ConPosition) != NONE && get_body_object(CreatureID, ConPosition) != OldObj) {
		throw NOROOM;
	}

	if (HandContainer) {
		for (int Position = INVENTORY_HAND_FIRST; Position <= INVENTORY_HAND_LAST; Position += 1) {
			Object Other = get_body_object(CreatureID, Position);
			if (Other != NONE && Other != OldObj) {
				ObjectType OtherType = Other.get_object_type();
				if (OtherType.is_two_handed()) {
					throw HANDBLOCKED;
				}

				if (OtherType.is_weapon() && Type.is_weapon()) {
					throw ONEWEAPONONLY;
				}
			}
		}
	}
}

void check_weight(uint32 CreatureID, Object Obj, int Count) {
	if (get_object_creature_id(Obj) == CreatureID) {
		return;
	}

	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("check_weight: Creature %d does not exist.\n", CreatureID);
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (!ObjType.get_flag(TAKE)) {
		throw TOOHEAVY;
	}

	if (Creature->Type == PLAYER) {
		if (check_right(CreatureID, UNLIMITED_CAPACITY)) {
			return;
		}

		if (check_right(CreatureID, ZERO_CAPACITY)) {
			throw TOOHEAVY;
		}
	}

	TSkill *CarryStrength = Creature->Skills[SKILL_CARRY_STRENGTH];
	if (CarryStrength == NULL) {
		error("check_weight: Skill CARRYSTRENGTH does not exist.\n");
		throw ERROR;
	}

	int ObjWeight;
	if (ObjType.get_flag(CUMULATIVE)) {
		ObjWeight = get_weight(Obj, Count);
	} else {
		ObjWeight = get_complete_weight(Obj);
	}

	int InventoryWeight = get_inventory_weight(CreatureID);
	int MaxWeight = CarryStrength->Get() * 100;
	if ((InventoryWeight + ObjWeight) > MaxWeight) {
		throw TOOHEAVY;
	}
}

void check_weight(uint32 CreatureID, ObjectType Type, uint32 Value, int OldWeight) {
	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("check_weight: Creature %d does not exist.\n", CreatureID);
		throw ERROR;
	}

	if (!Type.get_flag(TAKE)) {
		throw TOOHEAVY;
	}

	if (Creature->Type == PLAYER) {
		if (check_right(CreatureID, UNLIMITED_CAPACITY)) {
			return;
		}

		if (check_right(CreatureID, ZERO_CAPACITY)) {
			throw TOOHEAVY;
		}
	}

	TSkill *CarryStrength = Creature->Skills[SKILL_CARRY_STRENGTH];
	if (CarryStrength == NULL) {
		error("check_weight: Skill CARRYSTRENGTH does not exist.\n");
		throw ERROR;
	}

	int TypeWeight = (int)Type.get_attribute(WEIGHT);
	if (Type.get_flag(CUMULATIVE) && (int)Value > 1) {
		TypeWeight *= (int)Value;
	}

	if (TypeWeight > OldWeight) {
		int InventoryWeight = get_inventory_weight(CreatureID);
		int MaxWeight = CarryStrength->Get() * 100;
		if ((InventoryWeight + TypeWeight) > MaxWeight) {
			throw TOOHEAVY;
		}
	}
}

void notify_creature(uint32 CreatureID, Object Obj, bool Inventory) {
	if (CreatureID == 0) {
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (!ObjType.is_creature_container()) {
		TCreature *Creature = get_creature(CreatureID);
		if (Creature == NULL) {
			error("notify_creature: Creature does not exist.\n");
			return;
		}

		if (Inventory && ObjType.get_flag(LIGHT)) {
			announce_changed_creature(CreatureID, CREATURE_LIGHT_CHANGED);
		}

		Creature->NotifyChangeInventory();
	}
}

void notify_creature(uint32 CreatureID, ObjectType Type, bool Inventory) {
	if (CreatureID == 0) {
		return;
	}

	// TODO(fusion): Shouldn't we check if `Type` is creature container too?

	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("notify_creature: Creature does not exist.\n");
		return;
	}

	if (Inventory && Type.get_flag(LIGHT)) {
		announce_changed_creature(CreatureID, CREATURE_LIGHT_CHANGED);
	}

	Creature->NotifyChangeInventory();
}

void notify_all_creatures(Object Obj, int Type, Object OldCon) {
	if (!Obj.exists()) {
		error("notify_all_creatures: Passed object does not exist.\n");
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (!ObjType.is_creature_container()) {
		return;
	}

	int ObjX, ObjY, ObjZ;
	uint32 CreatureID = Obj.get_creature_id();
	get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);

	constexpr int StimulusRadius = 10;
	int SearchRadiusX = StimulusRadius;
	int SearchRadiusY = StimulusRadius;
	int SearchCenterX = ObjX;
	int SearchCenterY = ObjY;
	if (Type == OBJECT_MOVED) {
		if (!OldCon.exists()) {
			error("notify_all_creatures: Passed old container does not exist.\n");
			return;
		}

		// NOTE(fusion): Report stimulus as `OBJECT_CHANGED`.
		Type = OBJECT_CHANGED;

		// NOTE(fusion): Perform a secondary search if the distances are beyond
		// the stimulus diameter or, extend the primary search otherwise.
		int OldX, OldY, OldZ;
		get_object_coordinates(OldCon, &OldX, &OldY, &OldZ);
		int DistanceX = std::abs(ObjX - OldX);
		int DistanceY = std::abs(ObjY - OldY);
		if (DistanceX > (StimulusRadius * 2) || DistanceY > (StimulusRadius * 2)) {
			FindCreatures Search(StimulusRadius, StimulusRadius, OldX, OldY, FIND_ALL);
			while (true) {
				uint32 SpectatorID = Search.getNext();
				if (SpectatorID == 0) {
					break;
				}

				// TODO(fusion): Check if the spectator is NULL?
				TCreature *Spectator = get_creature(SpectatorID);
				Spectator->CreatureMoveStimulus(CreatureID, Type);
			}
		} else {
			SearchRadiusX = StimulusRadius + (DistanceX + 1) / 2;
			SearchRadiusY = StimulusRadius + (DistanceY + 1) / 2;
			SearchCenterX = (ObjX + OldX) / 2;
			SearchCenterY = (ObjY + OldY) / 2;
		}
	}

	FindCreatures Search(SearchRadiusX, SearchRadiusY, SearchCenterX, SearchCenterY, FIND_ALL);
	while (true) {
		uint32 SpectatorID = Search.getNext();
		if (SpectatorID == 0) {
			break;
		}

		// TODO(fusion): Check if the spectator is NULL?
		TCreature *Spectator = get_creature(SpectatorID);
		Spectator->CreatureMoveStimulus(CreatureID, Type);
	}
}

void notify_trades(Object Obj) {
	if (!Obj.exists()) {
		error("notify_trades: Passed object does not exist.\n");
		return;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (ObjType.is_creature_container()) {
		return;
	}

	// TODO(fusion): These radii seem to be correct but I'm skeptical because
	// you're required to be close to the trade object to even start the trade.
	// Maybe you can walk around after a trade is started?
	FindCreatures Search(12, 10, Obj, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL) {
			error("NotifyTrade: Player does not exist.\n");
			continue;
		}

		Object TradeObject = Player->InspectTrade(true, 0);
		if (TradeObject != NONE) {
			if (is_held_by_container(Obj, TradeObject) || is_held_by_container(TradeObject, Obj)) {
				SendCloseTrade(Player->Connection);
				SendMessage(Player->Connection, TALK_FAILURE_MESSAGE, "Trade cancelled.");
				Player->RejectTrade();
			}
		}
	}
}

void notify_depot(uint32 CreatureID, Object Obj, int Count) {
	if (CreatureID == 0) {
		return;
	}

	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("notify_depot: Creature %u does not exist.\n", CreatureID);
		// TODO(fusion): We should probably also throw here.
		// throw ERROR;
		return;
	}

	if (!Obj.exists()) {
		error("notify_depot: Object does not exist.\n");
		throw ERROR;
	}

	if (Creature->Type == PLAYER) {
		TPlayer *Player = (TPlayer *)Creature;
		if (Player->Depot == NONE) {
			return;
		}

		if (is_held_by_container(Obj, Player->Depot)) {
			Player->DepotSpace += Count;
			print(3, "New depot free space for %s: %d\n", Player->Name, Player->DepotSpace);
		}
	}
}

void close_container(Object Con, bool Force) {
	if (!Con.exists()) {
		error("close_container: Passed container does not exist.\n");
		return;
	}

	ObjectType ConType = Con.get_object_type();
	if (ConType.is_creature_container()) {
		return;
	}

	// TODO(fusion): Similar to `notify_trades`.
	FindCreatures Search(12, 10, Con, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL) {
			error("close_container: Player does not exist.\n");
			continue;
		}

		for (int ContainerNr = 0; ContainerNr < NARRAY(Player->OpenContainer); ContainerNr += 1) {
			Object OpenCon = Player->GetOpenContainer(ContainerNr);
			if (is_held_by_container(OpenCon, Con)) {
				if (Force || !object_accessible(Player->ID, Con, 1)) {
					Player->SetOpenContainer(ContainerNr, NONE);
					SendCloseContainer(Player->Connection, ContainerNr);
				} else {
					// TODO(fusion): Why are we doing this? Is the container being refreshed?
					SendContainer(Player->Connection, ContainerNr);
				}
			}
		}
	}
}

Object create(Object Con, ObjectType Type, uint32 Value) {
	if (!Con.exists()) {
		error("create: Destination object does not exist.\n");
		throw ERROR;
	}

	ObjectType ConType = Con.get_object_type();
	if (!ConType.get_flag(CONTAINER) && !ConType.get_flag(CHEST)) {
		error("create: Destination object is not a container.\n");
		throw ERROR;
	}

	// TODO(fusion): The map container does indeed use type id zero but it seems
	// it is also considered a "no type" id so we might also want to add an alias
	// to it as `isValid()`, `isVoid()`, `isNone()`, or `isNull()` perhaps.
	if (Type.is_map_container()) {
		error("create: Object type does not exist.\n");
		throw ERROR;
	}

	if (ConType.is_map_container()) {
		check_map_place(0, Type, Con);
	} else if (ConType.is_body_container()) {
		check_inventory_place(Type, Con, NONE);
	} else {
		check_container_place(Type, Con, NONE);
	}

	uint32 ConOwnerID = get_object_creature_id(Con);
	if (ConOwnerID != 0) {
		check_weight(ConOwnerID, Type, Value, 0);
	}

	// NOTE(fusion): Attempt to merge with top object.
	if (ConType.is_map_container() && Type.get_flag(CUMULATIVE)) {
		int ConX, ConY, ConZ;
		get_object_coordinates(Con, &ConX, &ConY, &ConZ);
		Object Top = get_top_object(ConX, ConY, ConZ, true);
		if (Top != NONE && Top.get_object_type() == Type) {
			uint32 Count = Top.get_attribute(AMOUNT);
			if (Value != 0) {
				Count += Value;
			} else {
				Count += 1;
			}

			if (Count <= 100) {
				change(Top, AMOUNT, Count);
				return Top;
			}
		}
	}

	// TODO(fusion): I feel these usages of `Value` should be exclusive?

	Object Obj = set_object(Con, Type, (Type.is_creature_container() ? Value : 0));

	if (Type.get_flag(CUMULATIVE)) {
		change_object(Obj, AMOUNT, Value);
	}

	if (Type.get_flag(MAGICFIELD)) {
		change_object(Obj, RESPONSIBLE, Value);
	}

	if (Type.get_flag(LIQUIDPOOL)) {
		change_object(Obj, POOLLIQUIDTYPE, Value);
	}

	if (Type.get_flag(LIQUIDCONTAINER)) {
		change_object(Obj, CONTAINERLIQUIDTYPE, Value);
	}

	if (Type.get_flag(KEY)) {
		change_object(Obj, KEYNUMBER, Value);
	}

	if (Type.get_flag(RUNE)) {
		change_object(Obj, CHARGES, Value);
	}

	if (Type.is_creature_container()) {
		// BUG(fusion): We should just check this before creating the object.
		// Also, using `delete_op` instead of `delete_object` here is problematic
		// because the object's placement wasn't yet broadcasted.
		TCreature *Creature = get_creature(Value);
		if (Creature == NULL) {
			error("create: Invalid creature ID %u\n", Value);
			delete_op(Obj, -1);
			throw ERROR;
		}

		// IMPORTANT(fusion): Creature body containers are created here and their
		// type ids match inventory slots exactly. We're looping in reverse because
		// `set_object` will insert objects at the front of the object chain.
		for (int Position = INVENTORY_LAST; Position >= INVENTORY_FIRST; Position -= 1) {
			set_object(Obj, ObjectType(Position), 0);
		}

		Creature->CrObject = Obj;
		Creature->NotifyCreate();
	}

	announce_changed_object(Obj, OBJECT_CREATED);
	notify_trades(Obj);
	notify_creature(ConOwnerID, Obj, ConType.is_body_container());
	movement_event(Obj, Con, Con); // TODO(fusion): This one doesn't make a lot of sense.
	collision_event(Obj, Con);
	notify_all_creatures(Obj, OBJECT_CREATED, NONE);
	return Obj;
}

Object copy(Object Con, Object Source) {
	if (!Con.exists()) {
		error("copy: Destination does not exist.\n");
		throw ERROR;
	}

	ObjectType ConType = Con.get_object_type();
	if (!ConType.get_flag(CONTAINER) && !ConType.get_flag(CHEST)) {
		error("copy: Destination is not a container.\n");
		throw ERROR;
	}

	if (!Source.exists()) {
		error("copy: Source does not exist.\n");
		throw ERROR;
	}

	ObjectType SourceType = Source.get_object_type();
	if (SourceType.is_creature_container()) {
		error("copy: Source is a creature.\n");
		throw ERROR;
	}

	if (ConType.is_map_container()) {
		check_map_destination(0, Source, Con);
	} else if (ConType.is_body_container()) {
		// TODO(fusion): Not sure about this third param.
		check_inventory_destination(Source, Con, true);
	} else {
		check_container_destination(Source, Con);
	}

	uint32 ConOwnerID = get_object_creature_id(Con);
	if (ConOwnerID != 0) {
		check_weight(ConOwnerID, Source, -1);
	}

	// TODO(fusion): Using `copy` recursively here should work because the object
	// is placed when `copy_object` is called. The thing is, announce and notify
	// functions will do meaningless work there (I think, check what happens when
	// we're actually running).
	Object Obj = copy_object(Con, Source);
	if (SourceType.get_flag(CONTAINER)) {
		Object Help = get_first_container_object(Source);
		while (Help != NONE) {
			copy(Obj, Help);
			Help = Help.get_next_object();
		}
	}

	announce_changed_object(Obj, OBJECT_CREATED);
	notify_trades(Obj);
	notify_creature(ConOwnerID, Obj, ConType.is_body_container());
	movement_event(Obj, Con, Con); // TODO(fusion): Same as `create`.
	collision_event(Obj, Con);
	notify_all_creatures(Obj, OBJECT_CREATED, NONE);
	return Obj;
}

void move(uint32 CreatureID, Object Obj, Object Con, int Count, bool NoMerge, Object Ignore) {
	if (!Obj.exists()) {
		error("move: Passed object does not exist.\n");
		throw ERROR;
	}

	if (!Con.exists()) {
		int ObjX, ObjY, ObjZ;
		ObjectType ObjType = Obj.get_object_type();
		get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
		error("move: Destination object does not exist.\n");
		error("# Object %d at [%d,%d,%d]\n", ObjType.TypeID, ObjX, ObjY, ObjZ);
		throw ERROR;
	}

	ObjectType ConType = Con.get_object_type();
	if (!ConType.get_flag(CONTAINER)) {
		error("move: Destination object is not a container.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (ObjType.get_flag(CUMULATIVE) && (Count == 0 || Count > (int)Obj.get_attribute(AMOUNT))) {
		error("move: Invalid count %d of %d parts for cumulative object.\n", Count, (int)Obj.get_attribute(AMOUNT));
		throw ERROR;
	}

	Object OldCon = Obj.get_container();
	if (!NoMerge && ConType.is_map_container() && ObjType.get_flag(CUMULATIVE) && OldCon != Con) {
		int ConX, ConY, ConZ;
		get_object_coordinates(Con, &ConX, &ConY, &ConZ);
		Object Top = get_top_object(ConX, ConY, ConZ, true);
		if (Top != NONE) {
			try {
				merge(CreatureID, Obj, Top, Count, Ignore);
				return; // <-- EARLY RETURN IF `merge` SUCCEEDS.
			} catch (RESULT r) {
				if (r == DESTROYED) {
					throw;
				}
			}
		}
	}

	// TODO(fusion): The only event inside `merge` that could modify the object
	// and still throw some non `DESTROYED` error is the separation event.
	//	From what I've seen, it is used exclusively with non CUMULATIVE objects
	// like depot switches to perform "step out" events but using them with
	// CUMULATIVE objects could be problematic as the event is triggered on the
	// whole object which includes both the remainder and moved parts, meaning
	// that transforming the object then would already make the merge "invalid".

	if (!Obj.exists()) {
		error("move: Passed object no longer exists.\n");
	}

	// NOTE(fusion): Refresh object type just in case a merge event modified it.
	ObjType = Obj.get_object_type();

	int ObjCount = 1;
	if (ObjType.get_flag(CUMULATIVE)) {
		ObjCount = (int)Obj.get_attribute(AMOUNT);
	}

	if (Count == -1) {
		Count = ObjCount;
	}

	bool Split = Count < ObjCount;
	uint32 ObjOwnerID = get_object_creature_id(Obj);
	uint32 ConOwnerID = get_object_creature_id(Con);
	check_top_move_object(CreatureID, Obj, Ignore);
	if (ConType.is_map_container()) {
		check_move_object(CreatureID, Obj, false);
		check_map_destination(CreatureID, Obj, Con);
	} else {
		check_move_object(CreatureID, Obj, true);
		if (ConType.is_body_container()) {
			check_inventory_destination(Obj, Con, Split);
		} else {
			check_container_destination(Obj, Con);
		}

		if (Split) {
			check_depot_space(CreatureID, NONE, Con, 1);
		} else {
			check_depot_space(CreatureID, Obj, Con, count_objects(Obj));
		}

		if (ConOwnerID != 0) {
			check_weight(ConOwnerID, Obj, Count);
		}
	}

	// NOTE(fusion): `Obj` becomes the split part while `Remainder`, the remaining part.
	Object Remainder = NONE;
	if (Split) {
		Remainder = Obj;
		Obj = split_object(Obj, Count);
	}

	if (OldCon != Con) {
		separation_event(Obj, OldCon);
	}

	// TODO(fusion): There is surely a way to reduce these creature checks?
	// perhaps split them into two linear paths, with a few duplications but
	// easier to read.

	if (!ObjType.is_creature_container() && Remainder == NONE) {
		announce_changed_object(Obj, OBJECT_DELETED);
		notify_trades(Obj);
		notify_depot(CreatureID, Obj, count_objects(Obj));
	}

	// TODO(fusion): This could probably be moved to the end of the function
	// along with the other `close_container`.
	//	Probably the other way around, since events could modify the object
	// and I don't think it makes a difference if we close it before or after
	// performing the move.
	//	Except it does because the object's position will be different lol.
	if (ObjType.get_flag(CONTAINER) && CreatureID == 0) {
		close_container(Obj, true);
	}

	if (ObjType.is_creature_container()) {
		uint32 MovingCreatureID = Obj.get_creature_id();
		TCreature *Creature = get_creature(MovingCreatureID);
		if (Creature != NULL) {
			Creature->NotifyTurn(Con);
		} else {
			error("move: Creature with invalid ID %d.\n", MovingCreatureID);
		}
		announce_moving_creature(MovingCreatureID, Con);
	}

	move_object(Obj, Con);

	if (!ObjType.is_creature_container()) {
		announce_changed_object(Obj, OBJECT_CREATED);
		notify_trades(Obj);
		notify_depot(CreatureID, Obj, -count_objects(Obj));
	}

	if (Remainder != NONE) {
		announce_changed_object(Remainder, OBJECT_CHANGED);
		notify_trades(Remainder);
	}

	if (ObjType.is_creature_container()) {
		uint32 MovingCreatureID = Obj.get_creature_id();
		TCreature *Creature = get_creature(MovingCreatureID);
		if (Creature != NULL) {
			Creature->NotifyGo();
		} else {
			error("move: Creature with invalid ID %d.\n", MovingCreatureID);
		}
	}

	notify_creature(ObjOwnerID, Obj, OldCon.get_object_type().is_body_container());
	notify_creature(ConOwnerID, Obj, Con.get_object_type().is_body_container());

	if (ObjType.get_flag(CONTAINER)) {
		close_container(Obj, false);
	}

	movement_event(Obj, OldCon, Con);
	collision_event(Obj, Con);
	notify_all_creatures(Obj, OBJECT_MOVED, OldCon);
}

void merge(uint32 CreatureID, Object Obj, Object Dest, int Count, Object Ignore) {
	if (!Obj.exists()) {
		error("merge: Passed object does not exist.\n");
		throw ERROR;
	}

	if (!Dest.exists()) {
		error("merge: Destination object does not exist.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (ObjType.get_flag(CUMULATIVE) && (Count == 0 || Count > (int)Obj.get_attribute(AMOUNT))) {
		error("merge: Invalid count %d of parts for cumulative object.\n", Count);
		throw ERROR;
	}

	if (Obj == Dest) {
		return;
	}

	if (ObjType != Dest.get_object_type()) {
		throw NOMATCH;
	}

	if (!ObjType.get_flag(CUMULATIVE)) {
		throw NOTCUMULABLE;
	}

	int ObjCount = Obj.get_attribute(AMOUNT);
	int DestCount = Dest.get_attribute(AMOUNT);
	if (Count == -1) {
		Count = ObjCount;
	}

	if ((Count + DestCount) > 100) {
		throw TOOMANYPARTS;
	}

	Object DestCon = Dest.get_container();
	uint32 DestOwnerID = get_object_creature_id(Dest);
	check_top_move_object(CreatureID, Obj, Ignore);
	if (DestCon.get_object_type().is_map_container()) {
		check_move_object(CreatureID, Obj, false);
		check_map_destination(CreatureID, Obj, DestCon);
	} else {
		check_move_object(CreatureID, Obj, true);
		check_depot_space(CreatureID, Obj, DestCon, 0);
		if (DestOwnerID != 0) {
			check_weight(DestOwnerID, Obj, Count);
		}
	}

	Object ObjCon = Obj.get_container();
	uint32 ObjOwnerID = get_object_creature_id(Obj);

	// TODO(fusion): What is even going on here?
	if (ObjType.get_flag(MOVEMENTEVENT)) {
		error("merge: Movement event for cumulative object %d is not triggered.\n", ObjType.TypeID);
	}

	if (ObjCon != DestCon) {
		separation_event(Obj, ObjCon);
	}

	if (Count < ObjCount) {
		change_object(Obj, AMOUNT, ObjCount - Count);
		announce_changed_object(Obj, OBJECT_CHANGED);
		notify_trades(Obj);
		change_object(Dest, AMOUNT, DestCount + Count);
	} else {
		announce_changed_object(Obj, OBJECT_DELETED);
		notify_trades(Obj);
		notify_depot(CreatureID, Obj, 1);
		merge_objects(Obj, Dest);
	}

	announce_changed_object(Dest, OBJECT_CHANGED);
	notify_trades(Dest);
	notify_creature(ObjOwnerID, Dest, ObjCon.get_object_type().is_body_container());
	notify_creature(DestOwnerID, Dest, DestCon.get_object_type().is_body_container());
	collision_event(Dest, DestCon);
	notify_all_creatures(Dest, OBJECT_CHANGED, NONE);
}

void change(Object Obj, ObjectType NewType, uint32 Value) {
	if (!Obj.exists()) {
		error("change: Passed object does not exist.\n");
		throw ERROR;
	}

	ObjectType OldType = Obj.get_object_type();
	if (OldType.is_creature_container() || NewType.is_creature_container()) {
		error("change: Cannot change creatures.\n");
		throw ERROR;
	}

	// TODO(fusion): Same thing as `create`. The zero type id is also used as
	// a "no type" id.
	if (NewType.is_map_container()) {
		delete_op(Obj, -1);
		return;
	}

	if (OldType.get_flag(CONTAINER)) {
		if (!NewType.get_flag(CONTAINER) && get_first_container_object(Obj) != NONE) {
			error("change: Target object %d for %d is no longer a container.\n", NewType.TypeID, OldType.TypeID);
			throw EMPTYCONTAINER;
		}

		if (NewType.get_flag(CONTAINER)) {
			int ObjectCount = count_objects_in_container(Obj);
			int NewCapacity = (int)NewType.get_attribute(CAPACITY);
			if (ObjectCount > NewCapacity) {
				error("change: Target object %d for %d is a smaller container.\n", NewType.TypeID, OldType.TypeID);
				throw EMPTYCONTAINER;
			}
		}
	}

	if (OldType.get_flag(CHEST) && !NewType.get_flag(CHEST)) {
		error("change: Treasure chest must remain a treasure chest.\n");
		throw ERROR;
	}

	if (OldType.get_flag(CUMULATIVE) && OldType != NewType && Obj.get_attribute(AMOUNT) > 1) {
		throw SPLITOBJECT;
	}

	Object Con = Obj.get_container();
	ObjectType ConType = Con.get_object_type();
	uint32 ObjOwnerID = get_object_creature_id(Obj);
	if (ObjOwnerID != 0 && OldType != NewType) {
		if (ConType.is_body_container()) {
			check_inventory_place(NewType, Con, Obj);
		} else {
			check_container_place(NewType, Con, Obj);
		}

		uint32 Count = 1;
		if (OldType.get_flag(CUMULATIVE)) {
			Count = Obj.get_attribute(AMOUNT);
		}

		int OldWeight = get_weight(Obj, -1);
		check_weight(ObjOwnerID, NewType, Count, OldWeight);
	}

	if (OldType.get_flag(CONTAINER) && !NewType.get_flag(CONTAINER)) {
		close_container(Obj, true);
	}

	change_object(Obj, NewType);

	// TODO(fusion): Same thing as `create`. I feel these usages of `Value`
	// should be exclusive.

	if (NewType.get_flag(CUMULATIVE) && !OldType.get_flag(CUMULATIVE)) {
		change_object(Obj, AMOUNT, Value);
	}

	if (NewType.get_flag(MAGICFIELD) && !OldType.get_flag(MAGICFIELD)) {
		change_object(Obj, RESPONSIBLE, Value);
	}

	if (NewType.get_flag(LIQUIDPOOL) && !OldType.get_flag(LIQUIDPOOL)) {
		change_object(Obj, POOLLIQUIDTYPE, Value);
	}

	if (NewType.get_flag(LIQUIDCONTAINER) && !OldType.get_flag(LIQUIDCONTAINER)) {
		change_object(Obj, CONTAINERLIQUIDTYPE, Value);
	}

	if (NewType.get_flag(KEY) && !OldType.get_flag(KEY)) {
		change_object(Obj, KEYNUMBER, Value);
	}

	if (NewType.get_flag(RUNE) && !OldType.get_flag(RUNE)) {
		change_object(Obj, CHARGES, Value);
	}

	announce_changed_object(Obj, OBJECT_CHANGED);
	notify_trades(Obj);
	notify_creature(ObjOwnerID, Obj, ConType.is_body_container());
	notify_all_creatures(Obj, OBJECT_CHANGED, NONE);
}

void change(Object Obj, INSTANCEATTRIBUTE Attribute, uint32 Value) {
	if (!Obj.exists()) {
		error("change: Passed object does not exist.\n");
		throw ERROR;
	}

	if (Attribute == CONTENT || Attribute == CHESTQUESTNUMBER || Attribute == DOORLEVEL ||
		Attribute == DOORQUESTNUMBER || Attribute == DOORQUESTVALUE || Attribute == ABSTELEPORTDESTINATION ||
		Attribute == REMAININGEXPIRETIME) {
		error("change: Cannot change attribute %d.\n", Attribute);
		throw ERROR;
	}

	uint32 ObjOwnerID = get_object_creature_id(Obj);
	if (ObjOwnerID != 0 && Attribute == AMOUNT && Value > Obj.get_attribute(AMOUNT)) {
		int OldWeight = get_weight(Obj, -1);
		check_weight(ObjOwnerID, Obj.get_object_type(), Value, OldWeight);
	}

	change_object(Obj, Attribute, Value);
	if (Attribute == AMOUNT || Attribute == CONTAINERLIQUIDTYPE || Attribute == POOLLIQUIDTYPE) {
		announce_changed_object(Obj, OBJECT_CHANGED);
	}

	notify_trades(Obj);
	notify_creature(ObjOwnerID, Obj, Obj.get_container().get_object_type().is_body_container());
}

void delete_op(Object Obj, int Count) {
	if (!Obj.exists()) {
		error("delete_op: Passed object does not exist.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (ObjType.get_flag(CUMULATIVE) && Count > (int)Obj.get_attribute(AMOUNT)) {
		error("delete_op: Invalid count %d of parts for cumulative object.\n", Count);
		throw ERROR;
	}

	Object Remainder = NONE;
	if (ObjType.get_flag(CUMULATIVE) && Count != -1 && Count < (int)Obj.get_attribute(AMOUNT)) {
		Remainder = Obj;
		Obj = split_object(Obj, Count);
	} else {
		notify_trades(Obj);
	}

	if (ObjType.get_flag(CONTAINER) || ObjType.get_flag(CHEST)) {
		if (ObjType.get_flag(CONTAINER)) {
			close_container(Obj, true);
		}

		Object Help = get_first_container_object(Obj);
		while (Help != NONE) {
			Object Next = Help.get_next_object();
			delete_object(Help);
			Help = Next;
		}
	}

	Object Con = Obj.get_container();
	uint32 ObjOwnerID = get_object_creature_id(Obj);
	separation_event(Obj, Con);

	// BUG(fusion): Object may have been destroyed by the separation event? This
	// looks suspicious because even if the object is properly destroyed, its
	// remainder could be left unannounced.
	//	If it's a container, its objects don't get a separation event trigger,
	// and the separation event trigger on the container itself is only triggered
	// after it is empty.
	if (Obj.exists()) {
		if (Remainder == NONE) {
			announce_changed_object(Obj, OBJECT_DELETED);
		}

		notify_all_creatures(Obj, OBJECT_DELETED, NONE);
		if (ObjType.is_creature_container()) {
			TCreature *Creature = get_creature(ObjOwnerID);
			if (Creature != NULL) {
				Creature->NotifyDelete();
			} else {
				error("delete_op: Creature with invalid ID %d.\n", ObjOwnerID);
			}
		}

		delete_object(Obj);

		if (Remainder != NONE) {
			announce_changed_object(Remainder, OBJECT_CHANGED);
			notify_trades(Remainder);
		}

		notify_creature(ObjOwnerID, ObjType, Con.get_object_type().is_body_container());
	}
}

void empty(Object Con, int Remainder) {
	if (!Con.exists()) {
		error("empty: Passed container does not exist.\n");
		throw ERROR;
	}

	if (Remainder < 0) {
		error("empty: Invalid remainder %d.\n", Remainder);
		throw ERROR;
	}

	ObjectType ConType = Con.get_object_type();
	if (!ConType.get_flag(CONTAINER)) {
		error("empty: Passed object is not a container.\n");
		throw ERROR;
	}

	close_container(Con, true);

	bool IsCorpse = ConType.get_flag(CORPSE);
	int ObjectCount = count_objects_in_container(Con);
	Object Help = get_first_container_object(Con);
	while (Help != NONE && ObjectCount > Remainder) {
		Object Next = Help.get_next_object();
		if (IsCorpse) {
			delete_op(Help, -1);
		} else {
			move(0, Help, Con.get_container(), -1, false, NONE);
		}
		Help = Next;
		ObjectCount -= 1;
	}
}

void graphical_effect(int x, int y, int z, int Type) {
	announce_graphical_effect(x, y, z, Type);
}

void graphical_effect(Object Obj, int Type) {
	if (Obj.exists()) {
		int ObjX, ObjY, ObjZ;
		get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
		announce_graphical_effect(ObjX, ObjY, ObjZ, Type);
	}
}

void textual_effect(Object Obj, int Color, const char *Format, ...) {
	if (!Obj.exists()) {
		error("textual_effect: Passed map container does not exist.\n");
		return;
	}

	char Buffer[10];
	va_list ap;
	va_start(ap, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, ap);
	va_end(ap);

	int ObjX, ObjY, ObjZ;
	get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);
	announce_textual_effect(ObjX, ObjY, ObjZ, Color, Buffer);
}

void missile(Object Start, Object Dest, int Type) {
	if (!Start.exists()) {
		error("missile: Passed start point does not exist.\n");
		return;
	}

	if (!Dest.exists()) {
		error("missile: Passed destination point does not exist.\n");
		return;
	}

	int StartX, StartY, StartZ;
	int DestX, DestY, DestZ;
	get_object_coordinates(Start, &StartX, &StartY, &StartZ);
	get_object_coordinates(Dest, &DestX, &DestY, &DestZ);
	announce_missile(StartX, StartY, StartZ, DestX, DestY, DestZ, Type);
}

void look(uint32 CreatureID, Object Obj) {
	// TODO(fusion): Honestly one of the first things we should focus on, after
	// the server is running, is to implement a small stack buffer writer/formatter
	// to retire `strcpy`, `strcat`, and `sprintf`.

	TPlayer *Player = get_player(CreatureID);
	if (Player == NULL) {
		error("look: Creature does not exist.\n");
		throw ERROR;
	}

	if (!Obj.exists()) {
		error("look: Object does not exist.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (ObjType.is_creature_container()) {
		TCreature *Target = get_creature(Obj);
		if (Target == NULL) {
			error("look: Object %d has no creature!\n", ObjType.TypeID);
			return;
		}

		if (Target->Type == PLAYER) {
			TPlayer *TargetPlayer = (TPlayer *)Target;
			bool Yourself = (CreatureID == TargetPlayer->ID);
			char Pronoun[4] = {};
			char Name[50] = {};
			char Vocation[50] = {};

			if (TargetPlayer->Sex == 1) {
				strcpy(Pronoun, "He");
			} else {
				strcpy(Pronoun, "She");
			}

			uint8 Profession = TargetPlayer->GetActiveProfession();
			if (Yourself) {
				strcpy(Name, "yourself");

				if (Profession != PROFESSION_NONE) {
					char Help[30];
					get_profession_name(Help, Profession, true, false);
					snprintf(Vocation, sizeof(Vocation), "You are %s", Help);
				} else {
					strcpy(Vocation, "You have no vocation");
				}
			} else {
				int Level = 1;
				if (TargetPlayer->Skills[SKILL_LEVEL] != NULL) {
					Level = TargetPlayer->Skills[SKILL_LEVEL]->Get();
				}
				snprintf(Name, sizeof(Name), "%s (Level %d)", TargetPlayer->Name, Level);

				if (Profession != PROFESSION_NONE) {
					char Help[30];
					get_profession_name(Help, Profession, true, false);
					snprintf(Vocation, sizeof(Vocation), "%s is %s", Pronoun, Help);
				} else {
					snprintf(Vocation, sizeof(Vocation), "%s has no vocation", Pronoun);
				}
			}

			if (TargetPlayer->Guild[0] != 0) {
				char Membership[200] = {};
				if (Yourself) {
					strcpy(Membership, "You are ");
				} else {
					snprintf(Membership, sizeof(Membership), "%s is ", Pronoun);
				}

				if (TargetPlayer->Rank[0] != 0) {
					strcat(Membership, TargetPlayer->Rank);
				} else {
					strcat(Membership, "a member");
				}

				strcat(Membership, " of the ");
				strcat(Membership, TargetPlayer->Guild);
				if (TargetPlayer->Title[0] != 0) {
					strcat(Membership, " (");
					strcat(Membership, TargetPlayer->Title);
					strcat(Membership, ")");
				}

				SendMessage(Player->Connection, TALK_INFO_MESSAGE, "You see %s. %s. %s.", Name, Vocation, Membership);
			} else {
				SendMessage(Player->Connection, TALK_INFO_MESSAGE, "You see %s. %s.", Name, Vocation);
			}
		} else {
			SendMessage(Player->Connection, TALK_INFO_MESSAGE, "You see %s.", Target->Name);
		}
	} else {
		char Description[500] = {};
		char Help[500] = {};

		snprintf(Description, sizeof(Description), "You see %s", get_name(Obj));

		if (ObjType.get_flag(LEVELDOOR)) {
			snprintf(Help, sizeof(Help), " for level %u", Obj.get_attribute(DOORLEVEL));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(CONTAINER)) {
			snprintf(Help, sizeof(Help), " (Vol:%u)", ObjType.get_attribute(CAPACITY));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(WEAPON)) {
			snprintf(Help, sizeof(Help), " (Atk:%u Def:%u)", ObjType.get_attribute(WEAPONATTACKVALUE),
					 ObjType.get_attribute(WEAPONDEFENDVALUE));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(THROW)) {
			snprintf(Help, sizeof(Help), " (Atk:%u Def:%u)", ObjType.get_attribute(THROWATTACKVALUE),
					 ObjType.get_attribute(THROWDEFENDVALUE));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(SHIELD)) {
			snprintf(Help, sizeof(Help), " (Def:%u)", ObjType.get_attribute(SHIELDDEFENDVALUE));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(ARMOR)) {
			snprintf(Help, sizeof(Help), " (Arm:%u)", ObjType.get_attribute(ARMORVALUE));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(KEY)) {
			snprintf(Help, sizeof(Help), " (Key:%04u)", Obj.get_attribute(KEYNUMBER));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(KEYDOOR) && check_right(Player->ID, SHOW_KEYHOLE_NUMBERS)) {
			snprintf(Help, sizeof(Help), " (Door:%04u)", Obj.get_attribute(KEYHOLENUMBER));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(SHOWDETAIL)) {
			if (ObjType.get_flag(WEAROUT) && ObjType.get_attribute(TOTALUSES) > 1) {
				uint32 RemainingUses = Obj.get_attribute(REMAININGUSES);
				snprintf(Help, sizeof(Help), " that has %u charge%s left", RemainingUses,
						 (RemainingUses != 1 ? "s" : ""));
				strcat(Description, Help);
			} else if (ObjType.get_flag(EXPIRE) || ObjType.get_flag(EXPIRESTOP)) {
				uint32 SecondsLeft;
				if (ObjType.get_flag(EXPIRE)) {
					SecondsLeft = cron_info(Obj, false);
				} else {
					SecondsLeft = Obj.get_attribute(SAVEDEXPIRETIME);
				}

				uint32 MinutesLeft = (SecondsLeft + 59) / 60;
				if (MinutesLeft == 0) {
					snprintf(Help, sizeof(Help), " that is brand-new");
				} else {
					snprintf(Help, sizeof(Help), " that has energy for %u minute%s left", MinutesLeft,
							 (MinutesLeft != 1 ? "s" : ""));
				}
				strcat(Description, Help);
			} else {
				error("look: Object %d has flag SHOWDETAIL, but neither WEAROUT nor EXPIRE.\n", ObjType.TypeID);
			}
		}

		if (ObjType.get_flag(RESTRICTLEVEL) || ObjType.get_flag(RESTRICTPROFESSION)) {
			strcat(Description, ".\nIt can only be wielded by ");
			if (ObjType.get_flag(RESTRICTPROFESSION)) {
				int ProfessionCount = 0;
				uint32 ProfessionMask = ObjType.get_attribute(PROFESSIONS);

				if (ProfessionMask & 1) {
					ProfessionCount += 1;
					strcpy(Help, "players without vocation");
				} else {
					strcpy(Help, "");
				}

				// NOTE(fusion): We're iterating professions in reverse because
				// the string is built in reverse to avoid extra processing for
				// adding commas and the final "and ...".
				for (int Profession = 4; Profession >= 1; Profession -= 1) {
					if ((ProfessionMask & (1 << Profession)) == 0) {
						continue;
					}

					char Temp[200] = {};
					if (ProfessionCount == 1) {
						snprintf(Temp, sizeof(Temp), " and %s", Help);
					} else if (ProfessionCount > 1) {
						snprintf(Temp, sizeof(Temp), ", %s", Help);
					}

					switch (Profession) {
					case PROFESSION_KNIGHT:
						strcpy(Help, "knights");
						break;
					case PROFESSION_PALADIN:
						strcpy(Help, "paladins");
						break;
					case PROFESSION_SORCERER:
						strcpy(Help, "sorcerers");
						break;
					case PROFESSION_DRUID:
						strcpy(Help, "druids");
						break;
					}

					strcat(Help, Temp);
				}
			} else {
				strcpy(Help, "players");
			}

			if (ObjType.get_flag(RESTRICTLEVEL)) {
				char Temp[200] = {};
				snprintf(Temp, sizeof(Temp), " of level %u or higher", ObjType.get_attribute(MINIMUMLEVEL));
				strcat(Help, Temp);
			}

			strcat(Description, Help);
		}

		if (ObjType.get_flag(RUNE)) {
			int MagicLevel;
			get_magic_item_description(Obj, Help, &MagicLevel);
			if (MagicLevel != 0) {
				char Temp[200] = {};
				snprintf(Temp, sizeof(Temp), " for magic level %d", MagicLevel);
				strcat(Description, Temp);
			}

			strcat(Description, ". It's an \"");
			strcat(Description, Help);
			strcat(Description, "\"-spell");

			snprintf(Help, sizeof(Help), " (%ux)", Obj.get_attribute(CHARGES));
			strcat(Description, Help);
		}

		if (ObjType.get_flag(BED)) {
			uint32 Sleeper = Obj.get_attribute(TEXTSTRING);
			if (Sleeper != 0) {
				snprintf(Help, sizeof(Help), ". %s is sleeping there", GetDynamicString(Sleeper));
				strcat(Description, Help);
			}
		}

		if (ObjType.get_flag(NAMEDOOR)) {
			int ObjX, ObjY, ObjZ;
			get_object_coordinates(Obj, &ObjX, &ObjY, &ObjZ);

			uint16 HouseID = get_house_id(ObjX, ObjY, ObjZ);
			if (HouseID != 0) {
				const char *HouseOwner = get_house_owner(HouseID);
				if (HouseOwner == NULL || HouseOwner[0] == 0) {
					HouseOwner = "Nobody";
				}

				snprintf(Help, sizeof(Help), ". It belongs to house '%s'. %s owns this house", get_house_name(HouseID),
						 HouseOwner);
				strcat(Description, Help);
			} else {
				error("look: NameDoor at [%d,%d,%d] does not belong to any house.\n", ObjX, ObjY, ObjZ);
			}
		}

		strcat(Description, ".");

		if ((ObjType.get_flag(LIQUIDCONTAINER) && Obj.get_attribute(CONTAINERLIQUIDTYPE) == LIQUID_NONE) ||
			(ObjType.get_flag(LIQUIDPOOL) && Obj.get_attribute(POOLLIQUIDTYPE) == LIQUID_NONE)) {
			strcat(Description, " It is empty.");
		}

		// TODO(fusion): This looks like an inlined function because we'd have
		// already returned at the beginning if `CreatureID` was zero.
		if (CreatureID == 0 || object_in_range(CreatureID, Obj, 1)) {
			if (ObjType.get_flag(TAKE) && get_weight(Obj, -1) > 0) {
				int ObjWeight = get_complete_weight(Obj);
				bool Multiple =
					ObjType.get_flag(CUMULATIVE) && Obj.get_attribute(AMOUNT) > 1 && IsCountable(ObjType.get_name(1));
				snprintf(Help, sizeof(Help), "\n%s %d.%02d oz.", (Multiple ? "They weigh" : "It weighs"),
						 (ObjWeight / 100), (ObjWeight % 100));
				strcat(Description, Help);
			}

			const char *ObjInfo = get_info(Obj);
			if (ObjInfo != NULL) {
				snprintf(Help, sizeof(Help), "\n%s.", ObjInfo);
				strcat(Description, Help);
			}
		}

		if (ObjType.get_flag(TEXT)) {
			int FontSize = (int)ObjType.get_attribute(FONTSIZE);
			if (FontSize == 0) {
				uint32 Text = Obj.get_attribute(TEXTSTRING);
				if (Text != 0) {
					snprintf(Help, sizeof(Help), "\n%s.", GetDynamicString(Text));
					strcat(Description, Help);
				}
			} else if (FontSize >= 2) {
				uint32 Text = Obj.get_attribute(TEXTSTRING);
				if (Text != 0) {
					// TODO(fusion): Again, `CreatureID` can't be zero here.
					if (CreatureID == 0 || object_in_range(CreatureID, Obj, FontSize)) {
						uint32 Editor = Obj.get_attribute(EDITOR);
						if (Editor != 0) {
							snprintf(Help, sizeof(Help), "\n%s has written: %s", GetDynamicString(Editor),
									 GetDynamicString(Text));
						} else {
							snprintf(Help, sizeof(Help), "\nYou read: %s", GetDynamicString(Text));
						}
						strcat(Description, Help);
					} else {
						strcat(Description, "\nYou are too far away to read it.");
					}
				} else {
					strcat(Description, "\nNothing is written on it.");
				}
			}
		}

		SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s", Description);
	}
}

void talk(uint32 CreatureID, int Mode, const char *Addressee, const char *Text, bool CheckSpamming) {
	// TODO(fusion): `Text` was originally `char*`, but I wanted to improve
	// string handling overall and modifying string parameters is a bad idea,
	// specially when you don't know their capacity.
	//	It is only modified when a player uses `TALK_YELL` to make it upper
	// case, which shouldn't be a problem, but if we plan to do any cleanup
	// afterwards, we need to get this right from the beginning.
	char YellBuffer[256];

	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("talk: Passed creature %d does not exist.\n", CreatureID);
		throw ERROR;
	}

	if (Text == NULL) {
		error("talk: Text is NULL (speaker: %s, mode: %d).\n", Creature->Name, Mode);
		throw ERROR;
	}

	int TextLen = (int)strlen(Text);
	if (TextLen >= 256) {
		error("talk: Text is too long (speaker: %s, length: %d).\n", Creature->Name, TextLen);
		throw ERROR;
	}

	int Channel = 0;
	if (Mode == TALK_CHANNEL_CALL || Mode == TALK_GAMEMASTER_CHANNELCALL || Mode == TALK_HIGHLIGHT_CHANNELCALL ||
		Mode == TALK_ANONYMOUS_CHANNELCALL) {
		// TODO(fusion): Check if `Addressee` is NULL?
		Channel = atoi(Addressee);
		if (Channel < 0 || Channel >= Channels) {
			error("talk: Invalid channel %d.\n", Channel);
			return;
		}
	}

	print(2, "%s: \"%s\"\n", Creature->Name, Text);

	if (Creature->Type == PLAYER) {
		TPlayer *Player = (TPlayer *)Creature;

		// TODO(fusion): Some `IsTalkMuteable` inlined function?
		bool TalkMuteable = Mode == TALK_SAY || Mode == TALK_WHISPER || Mode == TALK_YELL ||
							Mode == TALK_PRIVATE_MESSAGE ||
							((Mode == TALK_CHANNEL_CALL || Mode == TALK_HIGHLIGHT_CHANNELCALL) &&
							 Channel != CHANNEL_GUILD && Channel < FIRST_PRIVATE_CHANNEL);

		int Muting = 0;
		if (CheckSpamming) {
			Muting = Player->CheckForMuting();
			if (Muting > 0 && TalkMuteable) {
				SendMessage(Player->Connection, TALK_FAILURE_MESSAGE, "You are still muted for %d second%s.", Muting,
							(Muting != 1 ? "s" : ""));
				return;
			}
		}

		int SpellType = 0;
		if (Muting == 0) {
			SpellType = check_for_spell(CreatureID, Text);
			if (SpellType < 0 || SpellType == 1 || SpellType == 5) {
				return;
			}

			if (SpellType != 0) {
				Mode = TALK_SAY;
			}
		}

		if (Mode == TALK_YELL) {
			if (Player->EarliestYellRound > RoundNr) {
				throw EXHAUSTED;
			}

			Player->EarliestYellRound = RoundNr + 30;
			strcpy(YellBuffer, Text);
			strUpper(YellBuffer);
			Text = YellBuffer;
		}

		if (Mode == TALK_CHANNEL_CALL && Channel == 5) {
			if (Player->EarliestTradeChannelRound > RoundNr) {
				SendMessage(Player->Connection, TALK_FAILURE_MESSAGE, "You may only place one offer in two minutes.");
				return;
			}

			Player->EarliestTradeChannelRound = RoundNr + 120;
		}

		if (CheckSpamming && SpellType == 0 && TalkMuteable && !check_right(Player->ID, NO_BANISHMENT)) {
			Muting = Player->RecordTalk();
			if (Muting > 0) {
				SendMessage(Player->Connection, TALK_FAILURE_MESSAGE, "You are muted for %d second%s.", Muting,
							(Muting != 1 ? "s" : ""));
				return;
			}
		}
	}

	if (Mode == TALK_PRIVATE_MESSAGE || Mode == TALK_GAMEMASTER_ANSWER || Mode == TALK_PLAYER_ANSWER ||
		Mode == TALK_GAMEMASTER_MESSAGE || Mode == TALK_GAMEMASTER_MESSAGE || Mode == TALK_ANONYMOUS_MESSAGE) {
		// TODO(fusion): These assumed the creature was a player but it wasn't checked.
		if (Creature->Type != PLAYER) {
			error("talk: Expected player creature for message talk modes.");
			throw ERROR;
		}

		if (Addressee == NULL) {
			error("talk: Addressee for message not specified.\n");
			throw ERROR;
		}

		TPlayer *Player = (TPlayer *)Creature;
		TPlayer *Receiver;
		bool IgnoreGamemasters = !check_right(Player->ID, READ_GAMEMASTER_CHANNEL);
		switch (identify_player(Addressee, false, IgnoreGamemasters, &Receiver)) {
		case 0:
			break; // PLAYERFOUND ?
		case -1:
			throw PLAYERNOTONLINE;
		case -2:
			throw NAMEAMBIGUOUS;
		default: {
			error("talk: Invalid return value from identify_player.\n");
			throw ERROR;
		}
		}

		if (Mode == TALK_PRIVATE_MESSAGE) {
			int Muting = Player->RecordMessage(Receiver->ID);
			if (Muting > 0) {
				SendMessage(Player->Connection, TALK_FAILURE_MESSAGE,
							"You have addressed too many players. You are muted for %d second%s.", Muting,
							(Muting != 1 ? "s" : ""));
				return;
			}
		}

		if (Mode != TALK_ANONYMOUS_MESSAGE) {
			uint32 StatementID = log_communication(CreatureID, Mode, 0, Text);
			log_listener(StatementID, Player);
			SendTalk(Receiver->Connection, log_listener(StatementID, Receiver),
					 (Mode == TALK_GAMEMASTER_ANSWER ? "Gamemaster" : Player->Name), Mode, Text, 0);
		} else {
			SendMessage(Receiver->Connection, TALK_ADMIN_MESSAGE, "%s", Text);
		}

		SendMessage(Player->Connection, TALK_FAILURE_MESSAGE, "Message sent to %s.",
					(Mode == TALK_PLAYER_ANSWER ? "Gamemaster" : Receiver->Name));
		return;
	} else if (Mode == TALK_SAY || Mode == TALK_WHISPER || Mode == TALK_YELL || Mode == TALK_ANIMAL_LOW ||
			   Mode == TALK_ANIMAL_LOUD) {
		uint32 StatementID;
		if (Creature->Type == PLAYER) {
			StatementID = log_communication(CreatureID, Mode, 0, Text);
		} else {
			StatementID = 0;
		}

		int Radius;
		if (Mode == TALK_YELL || Mode == TALK_ANIMAL_LOUD) {
			Radius = 30; // RANGE_YELL ?
		} else {
			Radius = 7; // RANGE_SAY ?
		}

		FindCreatures Search(Radius, Radius, Creature->posx, Creature->posy, FIND_PLAYERS);
		while (true) {
			uint32 SpectatorID = Search.getNext();
			if (SpectatorID == 0) {
				break;
			}

			TPlayer *Spectator = get_player(SpectatorID);
			if (Spectator == NULL || Spectator->Connection == NULL) {
				continue;
			}

			int DistanceX = std::abs(Spectator->posx - Creature->posx);
			int DistanceY = std::abs(Spectator->posy - Creature->posy);
			int DistanceZ = std::abs(Spectator->posz - Creature->posz);
			if (Mode == TALK_SAY || Mode == TALK_ANIMAL_LOW) {
				if (DistanceX > 7 || DistanceY > 5 || DistanceZ > 0) {
					continue;
				}
			} else if (Mode == TALK_WHISPER) {
				if (DistanceX > 7 || DistanceY > 5 || DistanceZ > 0) {
					continue;
				}

				if (DistanceX > 1 && DistanceY > 1) {
					SendTalk(Spectator->Connection, 0, Creature->Name, Mode, Creature->posx, Creature->posy,
							 Creature->posz, "pspsps");
					continue;
				}
			} else if (Mode == TALK_YELL || Mode == TALK_ANIMAL_LOUD) {
				if (DistanceX > 30 || DistanceY > 30) {
					continue;
				}

				// TODO(fusion): This seems to be correct. Underground yells
				// aren't multi floor.
				if (DistanceZ > 0 && (Spectator->posz > 7 || Creature->posz > 7)) {
					continue;
				}
			}

			SendTalk(Spectator->Connection, log_listener(StatementID, Spectator), Creature->Name, Mode, Creature->posx,
					 Creature->posy, Creature->posz, Text);
		}
	} else if (Mode == TALK_GAMEMASTER_BROADCAST) {
		uint32 StatementID = log_communication(CreatureID, Mode, 0, Text);
		TConnection *Connection = GetFirstConnection();
		while (Connection != NULL) {
			if (Connection->Live()) {
				SendTalk(Connection, log_listener(StatementID, Connection->get_player()), Creature->Name, Mode, Text, 0);
			}

			Connection = GetNextConnection();
		}
	} else if (Mode == TALK_ANONYMOUS_BROADCAST) {
		TConnection *Connection = GetFirstConnection();
		while (Connection != NULL) {
			if (Connection->Live()) {
				SendMessage(Connection, TALK_ADMIN_MESSAGE, Text);
			}

			Connection = GetNextConnection();
		}
	} else if (Mode == TALK_CHANNEL_CALL || Mode == TALK_GAMEMASTER_CHANNELCALL || Mode == TALK_HIGHLIGHT_CHANNELCALL ||
			   Mode == TALK_ANONYMOUS_CHANNELCALL) {
		uint32 StatementID = 0;
		if (Mode != TALK_ANONYMOUS_CHANNELCALL) {
			StatementID = log_communication(CreatureID, Mode, Channel, Text);
		}

		// TODO(fusion): We should probably cleanup these global iterators.
		for (uint32 SubscriberID = get_first_subscriber(Channel); SubscriberID != 0;
			 SubscriberID = get_next_subscriber()) {
			TPlayer *Subscriber = get_player(SubscriberID);
			if (Subscriber == NULL) {
				continue;
			}

			// TODO(fusion): We should probably review this. You'd assume creature
			// should be a player when talking to any channel.
			if (Channel == CHANNEL_GUILD &&
				(Creature->Type != PLAYER || strcmp(((TPlayer *)Creature)->Guild, Subscriber->Guild) != 0)) {
				continue;
			}

			// TODO(fusion): There was some weird logic here to select the statement
			// id and it didn't seem to make a difference since `log_listener` was
			// always called, regardless.
			SendTalk(Subscriber->Connection, log_listener(StatementID, Subscriber), Creature->Name, Mode, Channel,
					 Text);
		}
	} else {
		// TODO(fusion): Put private messages into this if-else chain.
		error("talk: Invalid talk mode %d.\n", Mode);
	}

	if (Mode == TALK_SAY && Creature->Type == PLAYER) {
		FindCreatures Search(3, 3, CreatureID, FIND_NPCS);
		while (true) {
			uint32 SpectatorID = Search.getNext();
			if (SpectatorID == 0) {
				break;
			}

			TCreature *Spectator = get_creature(SpectatorID);
			if (Spectator == NULL) {
				error("talk: Creature does not exist.\n");
				continue;
			}

			if (Spectator->posz == Creature->posz) {
				Spectator->TalkStimulus(CreatureID, Text);
			}
		}
	}
}

void use(uint32 CreatureID, Object Obj1, Object Obj2, uint8 Info) {
	if (!Obj1.exists()) {
		error("use: Passed object Obj 1 does not exist.\n");
		throw ERROR;
	}

	if (Obj2 != NONE && !Obj2.exists()) {
		error("use: Passed object Obj 2 does not exist.\n");
		throw ERROR;
	}

	check_top_use_object(CreatureID, Obj1);
	if (!object_accessible(CreatureID, Obj1, 1)) {
		throw NOTACCESSIBLE;
	}

	ObjectType ObjType1 = Obj1.get_object_type();
	if (Obj2 != NONE) {
		// TODO(fusion): Is this correct?
		if (!ObjType1.get_flag(DISTUSE)) {
			check_top_multiuse_object(CreatureID, Obj2);
		}

		if (!object_accessible(CreatureID, Obj2, 1)) {
			if (!ObjType1.get_flag(DISTUSE)) {
				throw NOTACCESSIBLE;
			}

			TCreature *Creature = get_creature(CreatureID);
			if (Creature == NULL) {
				error("use: Causing creature does not exist.\n");
				throw ERROR;
			}

			int ObjX2, ObjY2, ObjZ2;
			get_object_coordinates(Obj2, &ObjX2, &ObjY2, &ObjZ2);
			if (Creature->posz > ObjZ2) {
				throw UPSTAIRS;
			} else if (Creature->posz < ObjZ2) {
				throw DOWNSTAIRS;
			}

			if (!throw_possible(Creature->posx, Creature->posy, Creature->posz, ObjX2, ObjY2, ObjZ2, 0)) {
				throw CANNOTTHROW;
			}
		}
	}

	if (ObjType1.get_flag(CONTAINER)) {
		use_container(CreatureID, Obj1, Info);
	} else if (ObjType1.get_flag(CHEST)) {
		use_chest(CreatureID, Obj1);
	} else if (ObjType1.get_flag(LIQUIDCONTAINER)) {
		use_liquid_container(CreatureID, Obj1, Obj2);
	} else if (ObjType1.get_flag(FOOD)) {
		use_food(CreatureID, Obj1);
	} else if (ObjType1.get_flag(WRITE) || ObjType1.get_flag(WRITEONCE)) {
		use_text_object(CreatureID, Obj1);
	} else if (ObjType1.get_flag(INFORMATION)) {
		use_announcer(CreatureID, Obj1);
	} else if (ObjType1.get_flag(RUNE)) {
		use_magic_item(CreatureID, Obj1, Obj2);
	} else if (ObjType1.get_flag(KEY)) {
		use_key_door(CreatureID, Obj1, Obj2);
	} else if (ObjType1.get_flag(NAMEDOOR)) {
		use_name_door(CreatureID, Obj1);
	} else if (ObjType1.get_flag(LEVELDOOR)) {
		use_level_door(CreatureID, Obj1);
	} else if (ObjType1.get_flag(QUESTDOOR)) {
		use_quest_door(CreatureID, Obj1);
	} else if (ObjType1.is_close_weapon() && Obj2 != NONE && Obj2.get_object_type().get_flag(DESTROY)) {
		use_weapon(CreatureID, Obj1, Obj2);
	} else if (ObjType1.get_flag(CHANGEUSE)) {
		use_change_object(CreatureID, Obj1);
	} else if (ObjType1.get_flag(USEEVENT)) {
		if (Obj2 != NONE) {
			use_objects(CreatureID, Obj1, Obj2);
		} else {
			use_object(CreatureID, Obj1);
		}
	} else if (ObjType1.get_flag(TEXT) && ObjType1.get_attribute(FONTSIZE) == 1) {
		use_text_object(CreatureID, Obj1);
	} else {
		throw NOTUSABLE;
	}
}

void turn(uint32 CreatureID, Object Obj) {
	if (!Obj.exists()) {
		error("turn: Passed object does not exist.\n");
		throw ERROR;
	}

	if (!object_accessible(CreatureID, Obj, 1)) {
		throw NOTACCESSIBLE;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (!ObjType.get_flag(ROTATE)) {
		throw NOTTURNABLE;
	}

	ObjectType RotateTarget = (int)ObjType.get_attribute(ROTATETARGET);
	if (RotateTarget == ObjType) {
		error("turn: Object %d is destroyed by rotation.\n", ObjType.TypeID);
	}

	change(Obj, RotateTarget, 0);
}

void create_pool(Object Con, ObjectType Type, uint32 Value) {
	if (!Con.exists()) {
		error("create_pool: Passed map container does not exist.\n");
		throw ERROR;
	}

	ObjectType ConType = Con.get_object_type();
	if (!ConType.is_map_container()) {
		error("create_pool: Passed object is not a map container.\n");
		throw ERROR;
	}

	// NOTE(fusion): There can be no liquid pools on fields with other BOTTOM
	// objects, with the exception of other liquid pools, in which case the new
	// liquid pool would replace the old one.
	Object Help = get_first_container_object(Con);
	while (Help != NONE) {
		Object Next = Help.get_next_object();
		ObjectType HelpType = Help.get_object_type();
		if (HelpType.get_flag(BOTTOM)) {
			if (!HelpType.get_flag(LIQUIDPOOL)) {
				throw NOROOM;
			}

			try {
				delete_op(Help, -1);
			} catch (RESULT r) {
				error("create_pool: Exception %d while deleting the old pool.\n", r);
			}
		}
		Help = Next;
	}

	create(Con, Type, Value);
}

void edit_text(uint32 CreatureID, Object Obj, const char *Text) {
	if (!Obj.exists()) {
		error("edit_text: Passed object does not exist.\n");
		throw ERROR;
	}

	ObjectType ObjType = Obj.get_object_type();
	if (!ObjType.get_flag(WRITE) && (!ObjType.get_flag(WRITEONCE) || Obj.get_attribute(TEXTSTRING) != 0)) {
		error("edit_text: Object is not writable.\n");
		throw ERROR;
	}

	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		error("edit_text: Creature does not exist.\n");
		throw ERROR;
	}

	int TextLength = (int)strlen(Text);
	int MaxLength =
		(ObjType.get_flag(WRITE) ? (int)ObjType.get_attribute(MAXLENGTH) : (int)ObjType.get_attribute(MAXLENGTHONCE));
	if (TextLength >= MaxLength) {
		throw TOOLONG;
	}

	// TODO(fusion): Similar to maybe inlined function in `look`.
	// TODO(fusion): Same as in `look`. `CreatureID` can't be zero here or we'd
	// have already returned so its probably some inlined function.
	if (CreatureID != 0 && !object_accessible(CreatureID, Obj, 1)) {
		throw NOTACCESSIBLE;
	}

	uint32 ObjText = Obj.get_attribute(TEXTSTRING);
	if ((ObjText != 0 && strcmp(GetDynamicString(ObjText), Text) == 0) || (ObjText == 0 && Text[0] == 0)) {
		print(3, "Text has not changed.\n");
		return;
	}

	DeleteDynamicString(ObjText);
	change(Obj, TEXTSTRING, AddDynamicString(Text));

	DeleteDynamicString(Obj.get_attribute(EDITOR));
	change(Obj, EDITOR, AddDynamicString(Creature->Name));
}

Object create_at_creature(uint32 CreatureID, ObjectType Type, uint32 Value) {
	// TODO(fusion): Bruhh...

	TCreature *Creature = get_creature(CreatureID);
	if (Creature == NULL) {
		// TODO(fusion): Different function name?
		error("GiveObjectToCreature: Cannot find creature.\n");
		throw ERROR;
	}

	try {
		check_weight(CreatureID, Type, Value, 0);
	} catch (RESULT r) {
		return create(get_map_container(Creature->CrObject), Type, Value);
	}

	Object Obj = NONE;
	bool CheckContainers = Type.get_flag(MOVEMENTEVENT);
	for (int i = 0; i < 2; i += 1) {
		bool TooHeavy = false;
		for (int Position = INVENTORY_FIRST; Position <= INVENTORY_LAST; Position += 1) {
			try {
				Object BodyObj = get_body_object(CreatureID, Position);
				if (CheckContainers) {
					if (BodyObj != NONE && BodyObj.get_object_type().get_flag(CONTAINER)) {
						Obj = create(BodyObj, Type, Value);
						break;
					}
				} else {
					if (BodyObj == NONE) {
						Obj = create(get_body_container(CreatureID, Position), Type, Value);
						break;
					}
				}
			} catch (RESULT r) {
				// TODO(fusion): Is this even possible if we're checking the
				// weight before hand?
				if (r == TOOHEAVY) {
					TooHeavy = true;
					break;
				}
			}
		}

		if (Obj != NONE || TooHeavy) {
			break;
		}

		CheckContainers = !CheckContainers;
	}

	if (Obj == NONE) {
		Obj = create(get_map_container(Creature->CrObject), Type, Value);
	}

	return Obj;
}

void delete_at_creature(uint32 CreatureID, ObjectType Type, int Amount, uint32 Value) {
	while (Amount > 0) {
		Object Obj = get_inventory_object(CreatureID, Type, Value);
		if (Obj == NONE) {
			error("delete_at_creature: No object found.\n");
			throw ERROR;
		}

		if (Type.get_flag(CUMULATIVE)) {
			int ObjAmount = (int)Obj.get_attribute(AMOUNT);
			if (ObjAmount <= Amount) {
				delete_op(Obj, -1);
				Amount -= ObjAmount;
			} else {
				change(Obj, AMOUNT, (ObjAmount - Amount));
				Amount = 0;
			}
		} else {
			delete_op(Obj, -1);
			Amount -= 1;
		}
	}
}

void process_cron_system(void) {
	while (true) {
		Object Obj = cron_check();
		if (Obj == NONE) {
			break;
		}

		try {
			ObjectType ObjType = Obj.get_object_type();
			ObjectType ExpireTarget = (int)ObjType.get_attribute(EXPIRETARGET);
			if (ObjType.get_flag(CONTAINER)) {
				int Remainder = 0;
				if (!ExpireTarget.is_map_container() && ExpireTarget.get_flag(CONTAINER)) {
					Remainder = (int)ExpireTarget.get_attribute(CAPACITY);
				}

				empty(Obj, Remainder);
			}

			change(Obj, ExpireTarget, 0);
		} catch (RESULT r) {
			error("process_cron_system: Exception %d while decaying %d.\n", r, Obj.get_object_type().TypeID);

			try {
				delete_op(Obj, -1);
			} catch (RESULT r) {
				error("process_cron_system: Cannot delete object - exception %d.\n", r);
			}
		}
	}
}

bool sector_refreshable(int SectorX, int SectorY, int SectorZ) {
	// TODO(fusion): Have the sector size defined as a constant in `map.hh`?
	// constexpr int SectorSize = 32;
	int SearchRadiusX = 32 - 1;
	int SearchRadiusY = 32 - 1;
	int SearchCenterX = SectorX * 32 + (32 / 2);
	int SearchCenterY = SectorY * 32 + (32 / 2);
	FindCreatures Search(SearchRadiusX, SearchRadiusY, SearchCenterX, SearchCenterY, FIND_PLAYERS);
	while (true) {
		uint32 CharacterID = Search.getNext();
		if (CharacterID == 0) {
			break;
		}

		TPlayer *Player = get_player(CharacterID);
		if (Player == NULL) {
			error("sector_refreshable: Creature does not exist.\n");
			continue;
		}

		if (Player->CanSeeFloor(SectorZ)) {
			return false;
		}
	}

	return true;
}

void refresh_sector(int SectorX, int SectorY, int SectorZ, const uint8 *Data, int Count) {
	if (!sector_refreshable(SectorX, SectorY, SectorZ)) {
		return;
	}

	TReadBuffer Buffer(Data, Count);
	refresh_sector(SectorX, SectorY, SectorZ, &Buffer);

	// TODO(fusion): This function is very similar to `apply_patch`.
	int SearchRadiusX = 32 / 2;
	int SearchRadiusY = 32 / 2;
	int SearchCenterX = SectorX * 32 + (32 / 2);
	int SearchCenterY = SectorY * 32 + (32 / 2);
	FindCreatures Search(SearchRadiusX, SearchRadiusY, SearchCenterX, SearchCenterY, FIND_ALL);
	while (true) {
		uint32 CreatureID = Search.getNext();
		if (CreatureID == 0) {
			break;
		}

		TCreature *Creature = get_creature(CreatureID);
		if (Creature == NULL) {
			error("refresh_sector: Creature does not exist.\n");
			continue;
		}

		if (Creature->posz != SectorZ) {
			continue;
		}

		bool FieldBlocked = false;
		int FieldX = Creature->posx;
		int FieldY = Creature->posy;
		int FieldZ = Creature->posz;
		Object Obj = get_first_object(FieldX, FieldY, FieldZ);
		while (Obj != NONE) {
			ObjectType ObjType = Obj.get_object_type();
			if (!ObjType.is_creature_container() && ObjType.get_flag(UNPASS)) {
				FieldBlocked = true;
				break;
			}
			Obj = Obj.get_next_object();
		}

		if (FieldBlocked) {
			try {
				if (!search_free_field(&FieldX, &FieldY, &FieldZ, 1, 0, false)) {
					if (Creature->Type == NPC) {
						print(2, "NPC at [%d,%d,%d] must be reset to its start.\n", FieldX, FieldY, FieldZ);
						FieldX = Creature->startx;
						FieldY = Creature->starty;
						FieldZ = Creature->startz;
					} else if (Creature->Type == MONSTER) {
						print(2, "Monster at [%d,%d,%d] must be deleted.\n", FieldX, FieldY, FieldZ);
						delete Creature;
						continue;
					} else {
						error("Player at [%d,%d,%d] is affected by refresh.\n", FieldX, FieldY, FieldZ);
						continue;
					}
				}

				Object MapCon = get_map_container(FieldX, FieldY, FieldZ);
				move(0, Creature->CrObject, MapCon, -1, false, NONE);
			} catch (RESULT r) {
				error("refresh_sector: Exception %d while repositioning the monster.\n", r);
			}
		}
	}
}

void refresh_map(void) {
	TDynamicWriteBuffer HelpBuffer(kb(64));
	for (int SectorZ = SectorZMin; SectorZ <= SectorZMax; SectorZ += 1)
		for (int SectorY = SectorYMin; SectorY <= SectorYMax; SectorY += 1)
			for (int SectorX = SectorXMin; SectorX <= SectorXMax; SectorX += 1) {
				if (!sector_refreshable(SectorX, SectorY, SectorZ)) {
					continue;
				}

				char FileName[4096];
				snprintf(FileName, sizeof(FileName), "%s/%04d-%04d-%02d.sec", ORIGMAPPATH, SectorX, SectorY, SectorZ);
				if (!FileExists(FileName)) {
					continue;
				}

				int OffsetX = -1;
				int OffsetY = -1;
				bool Refreshable = false;
				HelpBuffer.Position = 0;
				try {
					ReadScriptFile Script;
					Script.open(FileName);
					while (true) {
						Script.next_token();
						if (Script.Token == ENDOFFILE) {
							Script.close();
							break;
						}

						if (Script.Token == SPECIAL && Script.get_special() == ',') {
							continue;
						}

						if (Script.Token == BYTES) {
							uint8 *SectorOffset = Script.get_bytesequence();
							OffsetX = (int)SectorOffset[0];
							OffsetY = (int)SectorOffset[1];
							Refreshable = false;
							Script.read_symbol(':');
						} else if (Script.Token == IDENTIFIER) {
							if (OffsetX == -1 || OffsetY == -1) {
								Script.error("coordinate expected");
							}

							const char *Identifier = Script.get_identifier();
							if (strcmp(Identifier, "refresh") == 0) {
								Refreshable = true;
							} else if (strcmp(Identifier, "content") == 0) {
								Script.read_symbol('=');
								if (Refreshable) {
									HelpBuffer.writeByte((uint8)OffsetX);
									HelpBuffer.writeByte((uint8)OffsetY);
								}
								load_objects(&Script, &HelpBuffer, !Refreshable);
							}
						}
					}

					if (HelpBuffer.Position > 0) {
						TReadBuffer ReadBuffer(HelpBuffer.Data, HelpBuffer.Position);
						refresh_sector(SectorX, SectorY, SectorZ, &ReadBuffer);
					}
				} catch (const char *str) {
					error("refresh_map: Error processing file (%s).\n", str);
				}
			}
}

void refresh_cylinders(void) {
	// TODO(fusion): `RefreshedCylinders` is the number of cylinders we attempt to
	// refresh in this function, which is called every minute or so by `AdvanceGame`.
	// We should probably rename it to something clearer.
	static int RefreshX = INT_MAX;
	static int RefreshY = INT_MAX;
	for (int i = 0; i < RefreshedCylinders; i += 1) {
		if (RefreshX < SectorXMax) {
			RefreshX += 1;
		} else {
			RefreshX = SectorXMin;
			if (RefreshY < SectorYMax) {
				RefreshY += 1;
			} else {
				RefreshY = SectorYMin;
			}
		}

		for (int RefreshZ = SectorZMin; RefreshZ <= SectorZMax; RefreshZ += 1) {
			if (sector_refreshable(RefreshX, RefreshY, RefreshZ)) {
				load_sector_order(RefreshX, RefreshY, RefreshZ);
			}
		}
	}
}

void apply_patch(int SectorX, int SectorY, int SectorZ, bool FullSector, ReadScriptFile *Script, bool SaveHouses) {
	if (SectorX < SectorXMin || SectorX > SectorXMax || SectorY < SectorYMin || SectorY > SectorYMax ||
		SectorZ < SectorZMin || SectorZ > SectorZMax) {
		return;
	}

	prepare_house_cleanup();
	patch_sector(SectorX, SectorY, SectorZ, FullSector, Script, SaveHouses);
	finish_house_cleanup();

	// TODO(fusion): Similar to `sector_refreshable` but with a reduced radius.
	int SearchRadiusX = 32 / 2;
	int SearchRadiusY = 32 / 2;
	int SearchCenterX = SectorX * 32 + (32 / 2);
	int SearchCenterY = SectorY * 32 + (32 / 2);
	FindCreatures Search(SearchRadiusX, SearchRadiusY, SearchCenterX, SearchCenterY, FIND_ALL);
	while (true) {
		uint32 CreatureID = Search.getNext();
		if (CreatureID == 0) {
			break;
		}

		TCreature *Creature = get_creature(CreatureID);
		if (Creature == NULL) {
			error("apply_patch: Creature does not exist.\n");
			continue;
		}

		if (Creature->posz != SectorZ) {
			continue;
		}

		bool FieldBlocked = false;
		int FieldX = Creature->posx;
		int FieldY = Creature->posy;
		int FieldZ = Creature->posz;
		Object Obj = get_first_object(FieldX, FieldY, FieldZ);
		while (Obj != NONE) {
			ObjectType ObjType = Obj.get_object_type();
			if (!ObjType.is_creature_container() && ObjType.get_flag(UNPASS)) {
				FieldBlocked = true;
				break;
			}
			Obj = Obj.get_next_object();
		}

		if (!FieldBlocked) {
			// TODO(fusion): Not sure why we're moving the creature to the same
			// field, specially with `move_object`. Does `patch_sector` leave
			// creatures on invalid indices?
			Object MapCon = get_map_container(FieldX, FieldY, FieldZ);
			move_object(Creature->CrObject, MapCon);
		} else {
			try {
				if (!search_free_field(&FieldX, &FieldY, &FieldZ, 1, 0, false)) {
					if (Creature->Type == NPC) {
						print(2, "NPC at [%d,%d,%d] must be reset to its start.\n", FieldX, FieldY, FieldZ);
						FieldX = Creature->startx;
						FieldY = Creature->starty;
						FieldZ = Creature->startz;
					} else if (Creature->Type == MONSTER) {
						print(2, "Monster at [%d,%d,%d] must be deleted.\n", FieldX, FieldY, FieldZ);
						delete_op(Creature->CrObject, -1);
						Creature->StartLogout(false, false);
						continue;
					} else {
						error("Player at [%d,%d,%d] is affected by patch.\n", FieldX, FieldY, FieldZ);
						continue;
					}
				}

				Object MapCon = get_map_container(FieldX, FieldY, FieldZ);
				move(0, Creature->CrObject, MapCon, -1, false, NONE);
			} catch (RESULT r) {
				error("apply_patch: Exception %d while repositioning the monster.\n", r);
			}
		}
	}
}

void apply_patches(void) {
	// TODO(fusion): We don't handle any script exceptions in here which could
	// lead to `PatchDir` being leaked. It may not be a problem overall because
	// this function is only called at startup by `InitAll`.

	char FileName[4096];
	bool SaveHouses = false;
	snprintf(FileName, sizeof(FileName), "%s/save-houses", SAVEPATH);
	if (FileExists(FileName)) {
		SaveHouses = true;
		print(2, "Houses will not be patched.\n");
		unlink(FileName);
	}

	DIR *PatchDir = opendir(SAVEPATH);
	if (PatchDir == NULL) {
		error("apply_patches: Subdirectory %s not found.\n", SAVEPATH);
		return;
	}

	int MaxPatch = -1;
	while (dirent *DirEntry = readdir(PatchDir)) {
		if (DirEntry->d_type != DT_REG) {
			continue;
		}

		const char *FileExt = findLast(DirEntry->d_name, '.');
		if (FileExt == NULL) {
			continue;
		}

		if (strcmp(FileExt, ".pat") == 0) {
			int Patch;
			if (sscanf(DirEntry->d_name, "%d.pat", &Patch) == 1) {
				if (MaxPatch < Patch) {
					MaxPatch = Patch;
				}
			}
		} else if (strcmp(FileExt, ".sec") == 0) {
			int SectorX, SectorY, SectorZ;
			if (sscanf(DirEntry->d_name, "%d-%d-%d.sec", &SectorX, &SectorY, &SectorZ) == 3) {
				print(2, "Patching complete sector %d/%d/%d ...\n", SectorX, SectorY, SectorZ);
				snprintf(FileName, sizeof(FileName), "%s/%s", SAVEPATH, DirEntry->d_name);

				ReadScriptFile Script;
				Script.open(FileName);
				apply_patch(SectorX, SectorY, SectorZ, true, &Script, SaveHouses);
				Script.close();
				unlink(FileName);
			}
		}
	}

	closedir(PatchDir);

	for (int Patch = 0; Patch <= MaxPatch; Patch += 1) {
		snprintf(FileName, sizeof(FileName), "%s/%03d.pat", SAVEPATH, Patch);
		if (FileExists(FileName)) {
			ReadScriptFile Script;
			Script.open(FileName);
			if (strcmp(Script.read_identifier(), "sector") != 0) {
				Script.error("Sector expected");
			}

			int SectorX = Script.read_number();
			Script.read_symbol(',');
			int SectorY = Script.read_number();
			Script.read_symbol(',');
			int SectorZ = Script.read_number();

			print(2, "Patching parts of sector %d/%d/%d (patch %d) ...\n", SectorX, SectorY, SectorZ, Patch);
			apply_patch(SectorX, SectorY, SectorZ, false, &Script, false);
			Script.close();
			unlink(FileName);
		}
	}
}

// Communication Logging
// =============================================================================
uint32 log_communication(uint32 CreatureID, int Mode, int Channel, const char *Text) {
	static uint32 StatementID = 0;

	if (Text == NULL) {
		error("log_communication: Text is NULL.\n");
		return 0;
	}

	if (CreatureID == 0) {
		error("log_communication: CharacterID is zero.\n");
		return 0;
	}

	StatementID += 1;

	Statement *Stmt = Statements.append();
	Stmt->StatementID = StatementID;
	Stmt->TimeStamp = (uint32)time(NULL);
	Stmt->CharacterID = CreatureID;
	Stmt->Mode = Mode;
	Stmt->Channel = Channel;
	Stmt->Text = AddDynamicString(Text);
	Stmt->Reported = false;
	return StatementID;
}

uint32 log_listener(uint32 StatementID, TPlayer *Player) {
	if (StatementID == 0 || Player == NULL || !check_right(Player->ID, LOG_COMMUNICATION)) {
		return 0;
	}

	Listener *ListenerEntry = Listeners.append();
	ListenerEntry->StatementID = StatementID;
	ListenerEntry->CharacterID = Player->ID;
	return StatementID;
}

void process_communication_control(void) {
	// NOTE(fusion): Remove statements older than 30 minutes.
	int Now = (int)time(NULL);
	uint32 Limit = 0;
	while (true) {
		Statement *Stmt = Statements.next();
		if (Stmt == NULL || Now <= (Stmt->TimeStamp + 1800)) {
			if (Stmt != NULL) {
				Limit = Stmt->StatementID;
			}
			break;
		}

		DeleteDynamicString(Stmt->Text);
		Statements.remove();
	}

	// TODO(fusion): If there are no statements left we'll end up with `Limit`
	// equal to zero which will prevent any listeners entry from being removed.
	while (true) {
		Listener *ListenerEntry = Listeners.next();
		if (ListenerEntry == NULL || ListenerEntry->StatementID >= Limit) {
			break;
		}

		Listeners.remove();
	}
}

int get_communication_context(uint32 CharacterID, uint32 StatementID, int *NumberOfStatements,
							  vector<ReportedStatement> **ReportedStatements) {
	Statement *Stmt = NULL;
	int StatementsIter = Statements.iterFirst();
	while (true) {
		Stmt = Statements.iterNext(&StatementsIter);
		if (Stmt == NULL || Stmt->StatementID == StatementID) {
			break;
		}
	}

	if (Stmt == NULL) {
		return 1; // STATEMENT_UNKNOWN ?
	}

	if (CharacterID == 0) {
		bool ChannelMode = Stmt->Mode == TALK_CHANNEL_CALL || Stmt->Mode == TALK_GAMEMASTER_CHANNELCALL ||
						   Stmt->Mode == TALK_HIGHLIGHT_CHANNELCALL;

		bool ReportableChannel = Stmt->Channel == CHANNEL_TUTOR || Stmt->Channel == CHANNEL_GAMECHAT ||
								 Stmt->Channel == CHANNEL_TRADE || Stmt->Channel == CHANNEL_RLCHAT ||
								 Stmt->Channel == CHANNEL_HELP;

		if (!ChannelMode || !ReportableChannel) {
			error("get_communication_context: Statement should not be reported.\n");
			return 1; // STATEMENT_UNKNOWN ?
		}
	}

	if (Stmt->Reported) {
		return 2; // STATEMENT_ALREADY_REPORTED ?
	}

	// TODO(fusion): `FreeSpace` limits the amount of data that is gathered. It
	// seems to grow/shrink with the size of the text plus an extra 46 bytes for
	// each statement entry. This flat memory overhead of 46 bytes could be some
	// compilation/decompilation artifact so I've left it out for now.
	//	The end point of this data is `process_punishment_order` which may record
	// statements into the database so it may be related to that.

	bool StatementContained = false;
	int FreeSpace = kb(16);
	*NumberOfStatements = 0;
	*ReportedStatements = new vector<ReportedStatement>(0, 100, 100);
	StatementsIter = Statements.iterLast();
	while (true) {
		Statement *Current = Statements.iterPrev(&StatementsIter);
		if (Current == NULL) {
			break;
		}

		if (Current->TimeStamp < (Stmt->TimeStamp - 180) || Current->TimeStamp > (Stmt->TimeStamp + 60)) {
			continue;
		}

		if (FreeSpace <= 0 && Current->TimeStamp > Stmt->TimeStamp) {
			error("get_communication_context: Context is getting too large. Trimming end.\n");
			break;
		}

		if (CharacterID == 0) {
			bool ChannelMode = Current->Mode == TALK_CHANNEL_CALL || Current->Mode == TALK_GAMEMASTER_CHANNELCALL ||
							   Current->Mode == TALK_HIGHLIGHT_CHANNELCALL;
			if (!ChannelMode || Current->Channel != Stmt->Channel) {
				continue;
			}
		} else {
			// NOTE(fusion): Check if the character listened to the current statement.
			Listener *ListenerEntry = NULL;
			int ListenersIter = Listeners.iterLast();
			while (true) {
				ListenerEntry = Listeners.iterPrev(&ListenersIter);
				if (ListenerEntry == NULL || ListenerEntry->StatementID > Current->StatementID ||
					(ListenerEntry->StatementID == Current->StatementID && ListenerEntry->CharacterID == CharacterID)) {
					break;
				}
			}

			if (ListenerEntry == NULL || ListenerEntry->StatementID != Current->StatementID) {
				continue;
			}
		}

		ReportedStatement *Entry = (*ReportedStatements)->at(*NumberOfStatements);
		Entry->StatementID = Current->StatementID;
		Entry->TimeStamp = Current->TimeStamp;
		Entry->CharacterID = Current->CharacterID;
		Entry->Mode = Current->Mode;
		Entry->Channel = Current->Channel;
		if (Current->Text != 0) {
			// TODO(fusion): OOF SIZE: Large.
			strcpy(Entry->Text, GetDynamicString(Current->Text));
		} else {
			Entry->Text[0] = 0;
		}
		*NumberOfStatements += 1;
		FreeSpace -= (int)strlen(Entry->Text);

		if (Current->StatementID == StatementID) {
			StatementContained = true;
		}
	}

	if (FreeSpace < 0) {
		error("get_communication_context: Context is %d bytes too large. Deleting beginning.\n", -FreeSpace);

		for (int i = 0; i < *NumberOfStatements && FreeSpace < 0; i += 1) {
			ReportedStatement *Entry = (*ReportedStatements)->at(i);
			if (Entry->StatementID != StatementID) {
				Entry->StatementID = 0;
				FreeSpace += (int)strlen(Entry->Text);
			}
		}

		if (FreeSpace < 0) {
			error("get_communication_context: FreeSpace is still negative (%d).\n", FreeSpace);
		}
	}

	if (!StatementContained) {
		error("get_communication_context: Statement is not contained in context.\n");
		delete *ReportedStatements;
		*NumberOfStatements = 0;
		*ReportedStatements = NULL;
		return 1; // STATEMENT_UNKNOWN ?
	}

	Stmt->Reported = true;
	return 0; // STATEMENT_REPORTED ?
}

// Channel
// =============================================================================
Channel::Channel(void) : Subscriber(0, 10, 10), InvitedPlayer(0, 10, 10) {
	this->Moderator = 0;
	this->ModeratorName[0] = 0;
	this->Subscribers = 0;
	this->InvitedPlayers = 0;
}

Channel::Channel(const Channel &Other) : Channel() {
	this->operator=(Other);
}

void Channel::operator=(const Channel &Other) {
	this->Moderator = Other.Moderator;
	memcpy(this->ModeratorName, Other.ModeratorName, sizeof(this->ModeratorName));

	this->Subscribers = Other.Subscribers;
	for (int i = 0; i < Other.Subscribers; i += 1) {
		*this->Subscriber.at(i) = Other.Subscriber.copyAt(i);
	}

	this->InvitedPlayers = Other.InvitedPlayers;
	for (int i = 0; i < Other.InvitedPlayers; i += 1) {
		*this->InvitedPlayer.at(i) = Other.InvitedPlayer.copyAt(i);
	}
}

int get_number_of_channels(void) {
	return Channels;
}

bool channel_active(int ChannelID) {
	if (ChannelID < 0 || ChannelID >= Channels) {
		error("channel_active: Invalid channel number %d.\n", ChannelID);
		return false;
	}

	return ChannelID < FIRST_PRIVATE_CHANNEL || ChannelArray.at(ChannelID)->Moderator != 0;
}

bool channel_available(int ChannelID, uint32 CharacterID) {
	if (!channel_active(ChannelID)) {
		return false;
	}

	TPlayer *Player = get_player(CharacterID);
	if (Player == NULL) {
		error("channel_available: Player does not exist.\n");
		return false;
	}

	switch (ChannelID) {
	case CHANNEL_GUILD:
		return Player->Guild[0] != 0;
	case CHANNEL_GAMEMASTER:
		return check_right(Player->ID, READ_GAMEMASTER_CHANNEL);
	case CHANNEL_TUTOR:
		return check_right(Player->ID, READ_TUTOR_CHANNEL);
	case CHANNEL_RULEVIOLATIONS:
		return check_right(Player->ID, READ_GAMEMASTER_CHANNEL);
	case CHANNEL_GAMECHAT:
		return true;
	case CHANNEL_TRADE:
		return true;
	case CHANNEL_RLCHAT:
		return true;
	case CHANNEL_HELP:
		return true;
	default: {
		if (ChannelID >= FIRST_PRIVATE_CHANNEL) {
			Channel *Chan = ChannelArray.at(ChannelID);
			if (Chan->Moderator == CharacterID) {
				return true;
			}

			for (int i = 0; i < Chan->InvitedPlayers; i += 1) {
				if (*Chan->InvitedPlayer.at(i) == CharacterID) {
					return true;
				}
			}
		}
		return false;
	}
	}
}

const char *get_channel_name(int ChannelID, uint32 CharacterID) {
	static char ChannelName[40];

	if (!channel_active(ChannelID)) {
		return "Unknown";
	}

	TPlayer *Player = get_player(CharacterID);
	if (Player == NULL) {
		error("get_channel_name: Player does not exist.\n");
		return "Unknown";
	}

	switch (ChannelID) {
	case CHANNEL_GUILD:
		return Player->Guild;
	case CHANNEL_GAMEMASTER:
		return "Gamemaster";
	case CHANNEL_TUTOR:
		return "Tutor";
	case CHANNEL_RULEVIOLATIONS:
		return "Rule Violations";
	case CHANNEL_GAMECHAT:
		return "Game-Chat";
	case CHANNEL_TRADE:
		return "Trade";
	case CHANNEL_RLCHAT:
		return "RL-Chat";
	case CHANNEL_HELP:
		return "Help";
	default: {
		if (ChannelID >= FIRST_PRIVATE_CHANNEL) {
			snprintf(ChannelName, sizeof(ChannelName), "%s's Channel", ChannelArray.at(ChannelID)->ModeratorName);
			return ChannelName;
		} else {
			error("get_channel_name: Unused channel %d.\n", ChannelID);
			return "Unknown";
		}
	}
	}
}

bool channel_subscribed(int ChannelID, uint32 CharacterID) {
	if (!channel_active(ChannelID)) {
		return false;
	}

	Channel *Chan = ChannelArray.at(ChannelID);
	for (int i = 0; i < Chan->Subscribers; i += 1) {
		if (*Chan->Subscriber.at(i) == CharacterID) {
			return true;
		}
	}

	return false;
}

// TODO(fusion): This could have been a simple for loop?
uint32 get_first_subscriber(int ChannelID) {
	CurrentChannelID = ChannelID;
	CurrentSubscriberNumber = 0;
	return get_next_subscriber();
}

uint32 get_next_subscriber(void) {
	if (!channel_active(CurrentChannelID)) {
		return 0;
	}

	uint32 Subscriber = 0;
	Channel *Chan = ChannelArray.at(CurrentChannelID);
	if (CurrentSubscriberNumber < Chan->Subscribers) {
		Subscriber = *Chan->Subscriber.at(CurrentSubscriberNumber);
		CurrentSubscriberNumber += 1;
	}

	return Subscriber;
}

bool may_open_channel(uint32 CharacterID) {
	if (Channels >= MAX_CHANNELS) {
		return false;
	}

	if (!check_right(CharacterID, PREMIUM_ACCOUNT)) {
		return false;
	}

	// NOTE(fusion): Check if character is already moderator of any non-public channel.
	for (int ChannelID = FIRST_PRIVATE_CHANNEL; ChannelID < Channels; ChannelID += 1) {
		if (ChannelArray.at(ChannelID)->Moderator == CharacterID) {
			return false;
		}
	}

	return true;
}

void open_channel(uint32 CharacterID) {
	TPlayer *Player = get_player(CharacterID);
	if (Player == NULL) {
		error("open_channel: Creature %u does not exist.\n", CharacterID);
		throw ERROR;
	}

	// TODO(fusion): Shouldn't we scan for a free channel before returning here?
	if (Channels >= MAX_CHANNELS) {
		error("open_channel: Too many channels.\n");
		throw ERROR;
	}

	if (!check_right(CharacterID, PREMIUM_ACCOUNT)) {
		throw NOPREMIUMACCOUNT;
	}

	// NOTE(fusion): Check if character already has an open channel.
	for (int ChannelID = FIRST_PRIVATE_CHANNEL; ChannelID < Channels; ChannelID += 1) {
		if (ChannelArray.at(ChannelID)->Moderator == CharacterID) {
			SendOpenOwnChannel(Player->Connection, ChannelID);
			return;
		}
	}

	// NOTE(fusion): Assign free channel to character.
	int ChannelID = FIRST_PRIVATE_CHANNEL;
	while (ChannelID < Channels) {
		if (ChannelArray.at(ChannelID)->Moderator == 0) {
			break;
		}
		ChannelID += 1;
	}

	if (ChannelID == Channels) {
		Channels += 1;
	}

	Channel *Chan = ChannelArray.at(ChannelID);
	Chan->Moderator = CharacterID;
	strcpy(Chan->ModeratorName, Player->Name);
	*Chan->Subscriber.at(0) = CharacterID;
	Chan->Subscribers = 1;
	Chan->InvitedPlayers = 0;
	SendOpenOwnChannel(Player->Connection, ChannelID);
}

void close_channel(int ChannelID) {
	if (ChannelID < FIRST_PRIVATE_CHANNEL || ChannelID >= Channels) {
		error("close_channel: Invalid ChannelID %d.\n", ChannelID);
		return;
	}

	Channel *Chan = ChannelArray.at(ChannelID);
	if (Chan->Moderator == 0) {
		error("close_channel: Channel is already closed.\n");
		return;
	}

	Chan->Moderator = 0;
}

void invite_to_channel(uint32 CharacterID, const char *Name) {
	TPlayer *Player = get_player(CharacterID);
	if (Player == NULL) {
		error("invite_to_channel: Creature %u does not exist.\n", CharacterID);
		return;
	}

	int ChannelID = FIRST_PRIVATE_CHANNEL;
	while (ChannelID < Channels) {
		if (ChannelArray.at(ChannelID)->Moderator == CharacterID) {
			break;
		}
		ChannelID += 1;
	}

	if (ChannelID >= Channels) {
		return;
	}

	TPlayer *Other = NULL;
	uint32 OtherID = 0;
	bool IgnoreGamemasters = !check_right(Player->ID, READ_GAMEMASTER_CHANNEL);
	switch (identify_player(Name, false, IgnoreGamemasters, &Other)) {
	case 0: { // PLAYERFOUND ?
		OtherID = Other->ID;
		Name = Other->Name;
		break;
	}

	case -1: { // PLAYERNOTONLINE ?
		OtherID = get_character_id(Name);
		if (OtherID == 0) {
			throw PLAYERNOTEXISTING;
		}

		Name = get_character_name(Name);
		break;
	}

	case -2: { // NAMEAMBIGUOUS ?
		OtherID = get_character_id(Name);
		if (OtherID == 0) {
			throw NAMEAMBIGUOUS;
		}

		Name = get_character_name(Name);
		break;
	}

	default: {
		// TODO(fusion): This wasn't here but I don't think `identify_player`
		// can return anything else than 0, -1, and -2 anyways.
		error("invite_to_channel: Invalid return value from identify_player.\n");
		throw ERROR;
	}
	}

	print(3, "Inviting player %s (%u) to private channel.\n", Name, OtherID);
	if (CharacterID != OtherID) {
		Channel *Chan = ChannelArray.at(ChannelID);
		for (int i = 0; i < Chan->InvitedPlayers; i += 1) {
			if (*Chan->InvitedPlayer.at(i) == OtherID) {
				SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s has already been invited.", Name);
				return;
			}
		}

		*Chan->InvitedPlayer.at(Chan->InvitedPlayers) = OtherID;
		Chan->InvitedPlayers += 1;

		SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s has been invited.", Name);

		if (Other != NULL) {
			SendMessage(Other->Connection, TALK_INFO_MESSAGE, "%s invites you to %s private chat channel.",
						Player->Name, (Player->Sex == 1 ? "his" : "her"));
		}
	}
}

void exclude_from_channel(uint32 CharacterID, const char *Name) {
	TPlayer *Player = get_player(CharacterID);
	if (Player == NULL) {
		error("exclude_from_channel: Creature %u does not exist.\n", CharacterID);
		return;
	}

	int ChannelID = FIRST_PRIVATE_CHANNEL;
	while (ChannelID < Channels) {
		if (ChannelArray.at(ChannelID)->Moderator == CharacterID) {
			break;
		}
		ChannelID += 1;
	}

	if (ChannelID >= Channels) {
		return;
	}

	// TODO(fusion): Why don't we call `get_character_name` as we do in
	// `invite_to_channel`, when the player is not found?
	TPlayer *Other = NULL;
	uint32 OtherID = 0;
	bool IgnoreGamemasters = !check_right(Player->ID, READ_GAMEMASTER_CHANNEL);
	switch (identify_player(Name, false, IgnoreGamemasters, &Other)) {
	case 0: { // PLAYERFOUND ?
		OtherID = Other->ID;
		Name = Other->Name;
		break;
	}

	case -1: { // PLAYERNOTONLINE ?
		OtherID = get_character_id(Name);
		if (OtherID == 0) {
			throw PLAYERNOTEXISTING;
		}
		break;
	}

	case -2: { // NAMEAMBIGUOUS ?
		OtherID = get_character_id(Name);
		if (OtherID == 0) {
			throw NAMEAMBIGUOUS;
		}
		break;
	}

	default: {
		// TODO(fusion): Same as `invite_to_channel`.
		error("exclude_from_channel: Invalid return value from identify_player.\n");
		throw ERROR;
	}
	}

	print(3, "Excluding player %s (%u) from private channel.\n", Name, OtherID);
	if (CharacterID != OtherID) {
		bool Removed = false;
		Channel *Chan = ChannelArray.at(ChannelID);
		for (int i = 0; i < Chan->InvitedPlayers; i += 1) {
			if (*Chan->InvitedPlayer.at(i) == OtherID) {
				// NOTE(fusion): A little swap and pop action.
				Chan->InvitedPlayers -= 1;
				*Chan->InvitedPlayer.at(i) = *Chan->InvitedPlayer.at(Chan->InvitedPlayers);
				Removed = true;
				break;
			}
		}

		if (!Removed) {
			SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s has not been invited.", Name);
		} else {
			SendMessage(Player->Connection, TALK_INFO_MESSAGE, "%s has been excluded.", Name);
			if (channel_subscribed(ChannelID, OtherID)) {
				leave_channel(ChannelID, OtherID, false);

				// TODO(fusion): We didn't check if `Other` was NULL here. It
				// seems that the player destructor will call `leave_all_channels`
				// to make sure a player is only subscribed if it is still valid.
				//	Nevertheless, this check should become invisible to the branch
				// predictor and it's always best to be safe.
				if (Other != NULL) {
					SendCloseChannel(Other->Connection, ChannelID);
				}
			}
		}
	}
}

bool join_channel(int ChannelID, uint32 CharacterID) {
	if (!channel_active(ChannelID)) {
		throw ERROR;
	}

	if (!channel_available(ChannelID, CharacterID)) {
		throw NOTACCESSIBLE;
	}

	Channel *Chan = ChannelArray.at(ChannelID);
	if (!channel_subscribed(ChannelID, CharacterID)) {
		*Chan->Subscriber.at(Chan->Subscribers) = CharacterID;
		Chan->Subscribers += 1;
	}

	return CharacterID == Chan->Moderator;
}

void leave_channel(int ChannelID, uint32 CharacterID, bool Close) {
	if (!channel_active(ChannelID)) {
		return;
	}

	Channel *Chan = ChannelArray.at(ChannelID);
	for (int i = 0; i < Chan->Subscribers; i += 1) {
		if (*Chan->Subscriber.at(i) == CharacterID) {
			// NOTE(fusion): A little swap and pop action.
			Chan->Subscribers -= 1;
			*Chan->Subscriber.at(i) = *Chan->Subscriber.at(Chan->Subscribers);
			break;
		}
	}

	if (Close && CharacterID == Chan->Moderator) {
		for (int i = 0; i < Chan->Subscribers; i += 1) {
			uint32 SubscriberID = *Chan->Subscriber.at(i);
			TPlayer *Subscriber = get_player(SubscriberID);
			if (Subscriber != NULL) {
				SendCloseChannel(Subscriber->Connection, ChannelID);
			}
		}
		Chan->Subscribers = 0;
	}

	if (ChannelID >= FIRST_PRIVATE_CHANNEL && Chan->Subscribers == 0) {
		close_channel(ChannelID);
	}
}

void leave_all_channels(uint32 CharacterID) {
	for (int ChannelID = 0; ChannelID < Channels; ChannelID += 1) {
		if (channel_active(ChannelID)) {
			leave_channel(ChannelID, CharacterID, false);
		}
	}
}

// Party
// =============================================================================
Party::Party(void) : Member(0, 10, 10), InvitedPlayer(0, 10, 10) {
	this->Leader = 0;
	this->Members = 0;
	this->InvitedPlayers = 0;
}

Party::Party(const Party &Other) : Party() {
	this->operator=(Other);
}

void Party::operator=(const Party &Other) {
	this->Leader = Other.Leader;

	this->Members = Other.Members;
	for (int i = 0; i < Other.Members; i += 1) {
		*this->Member.at(i) = Other.Member.copyAt(i);
	}

	this->InvitedPlayers = Other.InvitedPlayers;
	for (int i = 0; i < Other.InvitedPlayers; i += 1) {
		*this->InvitedPlayer.at(i) = Other.InvitedPlayer.copyAt(i);
	}
}

Party *get_party(uint32 LeaderID) {
	Party *Result = NULL;
	for (int i = 0; i < Parties; i += 1) {
		if (PartyArray.at(i)->Leader == LeaderID) {
			Result = PartyArray.at(i);
			break;
		}
	}
	return Result;
}

bool is_invited_to_party(uint32 GuestID, uint32 HostID) {
	bool Result = false;
	Party *P = get_party(HostID);
	if (P != NULL) {
		for (int i = 0; i < P->InvitedPlayers; i += 1) {
			if (*P->InvitedPlayer.at(i) == GuestID) {
				Result = true;
				break;
			}
		}
	}
	return Result;
}

void disband_party(uint32 LeaderID) {
	Party *P = get_party(LeaderID);
	if (P == NULL) {
		error("disband_party: Party of leader %u not found.\n", LeaderID);
		return;
	}

	// NOTE(fusion): We need to iterate over members twice because `SendCreatureParty`
	// and `SendCreatureSkull` use party information, which is annoying.

	for (int i = 0; i < P->Members; i += 1) {
		uint32 MemberID = *P->Member.at(i);
		TPlayer *Member = get_player(MemberID);
		if (Member != NULL) {
			Member->LeaveParty();
		}
	}

	P->Leader = 0;

	for (int i = 0; i < P->Members; i += 1) {
		uint32 MemberID = *P->Member.at(i);
		TPlayer *Member = get_player(MemberID);
		if (Member != NULL && Member->Connection != NULL) {
			SendMessage(Member->Connection, TALK_INFO_MESSAGE, "Your party has been disbanded.");
			for (int j = 0; j < P->Members; j += 1) {
				uint32 OtherID = *P->Member.at(j);
				SendCreatureParty(Member->Connection, OtherID);
				SendCreatureSkull(Member->Connection, OtherID);
			}
		}
	}

	TPlayer *Leader = get_player(LeaderID);
	if (Leader != NULL) {
		for (int i = 0; i < P->InvitedPlayers; i += 1) {
			uint32 GuestID = *P->InvitedPlayer.at(i);
			TPlayer *Guest = get_player(GuestID);
			if (Guest != NULL) {
				SendCreatureParty(Guest->Connection, LeaderID);
				SendCreatureParty(Leader->Connection, GuestID);
			}
		}
	}
}

void invite_to_party(uint32 HostID, uint32 GuestID) {
	if (HostID == GuestID) {
		return;
	}

	TPlayer *Host = get_player(HostID);
	if (Host == NULL) {
		error("invite_to_party: Inviting player %u does not exist.\n", HostID);
		return;
	}

	TPlayer *Guest = get_player(GuestID);
	if (Guest == NULL) {
		SendResult(Host->Connection, PLAYERNOTONLINE);
		return;
	}

	if (Host->GetPartyLeader(true) == 0) {
		if (Guest->GetPartyLeader(true) != 0) {
			SendMessage(Host->Connection, TALK_INFO_MESSAGE, "%s is already member of a party.", Guest->Name);
			return;
		}

		Party *P = get_party(0);
		if (P == NULL) {
			P = PartyArray.at(Parties);
			Parties += 1;
		}

		P->Leader = HostID;
		*P->Member.at(0) = HostID;
		P->Members = 1;
		*P->InvitedPlayer.at(0) = GuestID;
		P->InvitedPlayers = 1;

		Host->JoinParty(HostID);
		SendMessage(Host->Connection, TALK_INFO_MESSAGE, "%s has been invited.", Guest->Name);
		SendMessage(Guest->Connection, TALK_INFO_MESSAGE, "%s invites you to %s party.", Host->Name,
					(Host->Sex == 1 ? "his" : "her"));
		SendCreatureParty(Host->Connection, HostID);
		SendCreatureSkull(Host->Connection, HostID);
		SendCreatureParty(Host->Connection, GuestID);
		SendCreatureParty(Guest->Connection, HostID);
	} else if (Host->GetPartyLeader(false) == HostID) {
		if (Guest->GetPartyLeader(true) != 0) {
			SendMessage(Host->Connection, TALK_INFO_MESSAGE, "%s is already member of %s party.", Guest->Name,
						(Guest->GetPartyLeader(true) == HostID ? "your" : "a"));
			return;
		}

		Party *P = get_party(HostID);
		if (P == NULL) {
			error("invite_to_party: Party of leader %u not found.\n", HostID);
			return;
		}

		for (int i = 0; i < P->InvitedPlayers; i += 1) {
			if (*P->InvitedPlayer.at(i) == GuestID) {
				SendMessage(Host->Connection, TALK_INFO_MESSAGE, "%s has already been invited.", Guest->Name);
				return;
			}
		}

		*P->InvitedPlayer.at(P->InvitedPlayers) = GuestID;
		P->InvitedPlayers += 1;

		SendMessage(Host->Connection, TALK_INFO_MESSAGE, "%s has been invited.", Guest->Name);
		SendMessage(Guest->Connection, TALK_INFO_MESSAGE, "%s invites you to %s party.", Host->Name,
					(Host->Sex == 1 ? "his" : "her"));
		SendCreatureParty(Host->Connection, GuestID);
		SendCreatureParty(Guest->Connection, HostID);
	} else {
		SendMessage(Host->Connection, TALK_INFO_MESSAGE, "You may not invite players.");
	}
}

void revoke_invitation(uint32 HostID, uint32 GuestID) {
	TPlayer *Host = get_player(HostID);
	if (Host == NULL) {
		error("revoke_invitation: Inviting player %u does not exist.\n", HostID);
		return;
	}

	if (Host->GetPartyLeader(false) != HostID) {
		SendMessage(Host->Connection, TALK_INFO_MESSAGE, "You may not invite players.");
		return;
	}

	Party *P = get_party(HostID);
	if (P == NULL) {
		error("revoke_invitation: Party of leader %u not found.\n", HostID);
		return;
	}

	int InviteIndex = 0;
	while (InviteIndex < P->InvitedPlayers) {
		if (*P->InvitedPlayer.at(InviteIndex) == GuestID) {
			break;
		}
		InviteIndex += 1;
	}

	TPlayer *Guest = get_player(GuestID);
	if (InviteIndex >= P->InvitedPlayers) {
		if (Guest != NULL) {
			SendMessage(Host->Connection, TALK_INFO_MESSAGE, "%s has not been invited.", Guest->Name);
		} else {
			SendMessage(Host->Connection, TALK_INFO_MESSAGE, "This player has not been invited.");
		}
		return;
	}

	// TODO(fusion): This ordered removal isn't relevant at all. It could be a
	// swap and pop just like in `join_party`, unlike when removing members.
	P->InvitedPlayers -= 1;
	for (int i = InviteIndex; i < P->InvitedPlayers; i += 1) {
		*P->InvitedPlayer.at(i) = *P->InvitedPlayer.at(i + 1);
	}

	if (Guest != NULL) {
		SendMessage(Host->Connection, TALK_INFO_MESSAGE, "Invitation for %s has been revoked.", Guest->Name);
		SendMessage(Guest->Connection, TALK_INFO_MESSAGE, "%s has revoked %s invitation.", Host->Name,
					(Host->Sex == 1 ? "his" : "her"));
		SendCreatureParty(Host->Connection, GuestID);
		SendCreatureParty(Guest->Connection, HostID);
	} else {
		SendMessage(Host->Connection, TALK_INFO_MESSAGE, "Invitation has been revoked.");
	}

	if (P->Members == 1 && P->InvitedPlayers == 0) {
		disband_party(HostID);
	}
}

void join_party(uint32 GuestID, uint32 HostID) {
	if (GuestID == HostID) {
		return;
	}

	TPlayer *Guest = get_player(GuestID);
	if (Guest == NULL) {
		error("join_party: Invited player %u does not exist.\n", GuestID);
		return;
	}

	TPlayer *Host = get_player(HostID);
	if (Host == NULL) {
		SendResult(Guest->Connection, PLAYERNOTONLINE);
		return;
	}

	if (Guest->GetPartyLeader(true) != 0) {
		SendMessage(Guest->Connection, TALK_INFO_MESSAGE, "You are already member of %s party.",
					(Guest->InPartyWith(Host, true) ? "this" : "a"));
		return;
	}

	Party *P = get_party(HostID);
	if (P == NULL) {
		SendMessage(Guest->Connection, TALK_INFO_MESSAGE, "%s has not invited you.", Host->Name);
		return;
	}

	int InviteIndex = 0;
	while (InviteIndex < P->InvitedPlayers) {
		if (*P->InvitedPlayer.at(InviteIndex) == GuestID) {
			break;
		}
		InviteIndex += 1;
	}

	if (InviteIndex >= P->InvitedPlayers) {
		SendMessage(Guest->Connection, TALK_INFO_MESSAGE, "%s has not invited you.", Host->Name);
		return;
	}

	// NOTE(fusion): A little swap and pop action.
	P->InvitedPlayers -= 1;
	*P->InvitedPlayer.at(InviteIndex) = *P->InvitedPlayer.at(P->InvitedPlayers);

	*P->Member.at(P->Members) = GuestID;
	P->Members += 1;

	Guest->JoinParty(HostID);
	SendMessage(Guest->Connection, TALK_INFO_MESSAGE, "You have joined %s's party.", Host->Name);
	for (int i = 0; i < P->Members; i += 1) {
		uint32 MemberID = *P->Member.at(i);
		SendCreatureParty(Guest->Connection, MemberID);
		SendCreatureSkull(Guest->Connection, MemberID);
		if (MemberID != GuestID) {
			TPlayer *Member = get_player(MemberID);
			if (Member != NULL) {
				SendMessage(Member->Connection, TALK_INFO_MESSAGE, "%s has joined the party.", Guest->Name);
				SendCreatureParty(Member->Connection, GuestID);
				SendCreatureSkull(Member->Connection, GuestID);
			}
		}
	}
}

void pass_leadership(uint32 OldLeaderID, uint32 NewLeaderID) {
	if (OldLeaderID == NewLeaderID) {
		return;
	}

	TPlayer *OldLeader = get_player(OldLeaderID);
	if (OldLeader == NULL) {
		error("pass_leadership: Old leader %u does not exist.\n", OldLeaderID);
		return;
	}

	TPlayer *NewLeader = get_player(NewLeaderID);
	if (NewLeader == NULL) {
		SendResult(OldLeader->Connection, PLAYERNOTONLINE);
		return;
	}

	if (OldLeader->GetPartyLeader(false) != OldLeaderID) {
		SendMessage(OldLeader->Connection, TALK_INFO_MESSAGE, "You are not leader of a party.");
		return;
	}

	if (NewLeader->GetPartyLeader(false) != OldLeaderID) {
		SendMessage(OldLeader->Connection, TALK_INFO_MESSAGE, "%s is not member of your party.", NewLeader->Name);
		return;
	}

	Party *P = get_party(OldLeaderID);
	if (P == NULL) {
		error("pass_leadership: Party of leader %u not found.\n", OldLeaderID);
		return;
	}

	P->Leader = NewLeaderID;

	// NOTE(fusion): Same as `disband_party`.
	for (int i = 0; i < P->Members; i += 1) {
		uint32 MemberID = *P->Member.at(i);
		TPlayer *Member = get_player(MemberID);
		if (Member != NULL) {
			Member->JoinParty(NewLeaderID);
		}
	}

	for (int i = 0; i < P->Members; i += 1) {
		uint32 MemberID = *P->Member.at(i);
		TPlayer *Member = get_player(MemberID);
		if (Member != NULL) {
			if (MemberID == NewLeaderID) {
				SendMessage(Member->Connection, TALK_INFO_MESSAGE, "You are now leader of your party.");
			} else {
				SendMessage(Member->Connection, TALK_INFO_MESSAGE, "%s is now leader of your party.", NewLeader->Name);
			}
			SendCreatureParty(Member->Connection, OldLeaderID);
			SendCreatureParty(Member->Connection, NewLeaderID);
		}
	}

	// NOTE(fusion): The list of invited players is cleared here.
	int InvitedPlayers = P->InvitedPlayers;
	P->InvitedPlayers = 0;

	for (int i = 0; i < InvitedPlayers; i += 1) {
		uint32 GuestID = *P->InvitedPlayer.at(i);
		TPlayer *Guest = get_player(GuestID);
		if (Guest != NULL) {
			SendCreatureParty(Guest->Connection, OldLeaderID);
			SendCreatureParty(OldLeader->Connection, GuestID);
		}
	}
}

void leave_party(uint32 MemberID, bool Forced) {
	TPlayer *Member = get_player(MemberID);
	if (Member == NULL) {
		error("leave_party: Member does not exist.\n");
		return;
	}

	uint32 LeaderID = Member->GetPartyLeader(false);
	if (LeaderID == 0) {
		error("leave_party: Player is not a member of a party.\n");
		return;
	}

	if (!Forced && Member->EarliestLogoutRound > RoundNr) {
		SendMessage(Member->Connection, TALK_INFO_MESSAGE,
					"You may not leave your party during or immediately after a fight!");
		return;
	}

	Party *P = get_party(LeaderID);
	if (P == NULL) {
		error("leave_party: Party of leader %u not found.\n", LeaderID);
		return;
	}

	if (P->Members == 1 || (P->Members == 2 && P->InvitedPlayers == 0)) {
		disband_party(LeaderID);
		return;
	}

	if (LeaderID == MemberID) {
		LeaderID = *P->Member.at(0);
		if (LeaderID == MemberID) {
			LeaderID = *P->Member.at(1);
		}
		pass_leadership(MemberID, LeaderID);
	}

	int MemberIndex = 0;
	while (MemberIndex < P->Members) {
		if (*P->Member.at(MemberIndex) == MemberID) {
			break;
		}
		MemberIndex += 1;
	}

	if (MemberIndex >= P->Members) {
		error("leave_party: Member not found.\n");
		return;
	}

	// NOTE(fusion): This ordered removal is important because we want the oldest
	// members to get leadership when the leader leaves.
	P->Members -= 1;
	for (int i = MemberIndex; i < P->Members; i += 1) {
		*P->Member.at(i) = *P->Member.at(i + 1);
	}

	Member->LeaveParty();
	if (!Forced) {
		SendMessage(Member->Connection, TALK_INFO_MESSAGE, "You have left the party.");
	}

	SendCreatureParty(Member->Connection, MemberID);
	SendCreatureSkull(Member->Connection, MemberID);

	for (int i = 0; i < P->Members; i += 1) {
		uint32 OtherID = *P->Member.at(i);
		SendCreatureParty(Member->Connection, OtherID);
		SendCreatureSkull(Member->Connection, OtherID);

		TPlayer *Other = get_player(OtherID);
		if (Other != NULL) {
			SendMessage(Other->Connection, TALK_INFO_MESSAGE, "%s has left the party.", Member->Name);
			SendCreatureParty(Other->Connection, MemberID);
			SendCreatureSkull(Other->Connection, MemberID);
		}
	}
}
