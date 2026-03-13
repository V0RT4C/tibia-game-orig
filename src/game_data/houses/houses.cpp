#include "game_data/houses/houses.h"
#include "config.h"
#include "cr.h"
#include "info.h"
#include "moveuse.h"
#include "game_data/operate/operate.h"
#include "query.h"
#include "writer.h"

static vector<HelpDepot> HelpDepotArray(0, 9, 10);
static int HelpDepots;

static vector<HouseArea> HouseAreaArray(0, 99, 50);
static int HouseAreas;

static vector<House> HouseArray(0, 99, 100);
static int Houses;
static int MaxHouseX;
static int MaxHouseY;

static TQueryManagerConnection *QueryManagerConnection;
static int PaymentExtension;

House::House(void) : Subowner(0, 4, 5), Guest(0, 9, 10) {
	this->ID = 0;
	this->Name[0] = 0;
	this->Description[0] = 0;
	this->Size = 0;
	this->Rent = 0;
	this->DepotNr = 0;
	this->NoAuction = 0;
	this->GuildHouse = 0;
	this->ExitX = 0;
	this->ExitY = 0;
	this->ExitZ = 0;
	this->CenterX = 0;
	this->CenterY = 0;
	this->CenterZ = 0;
	this->OwnerID = 0;
	this->OwnerName[0] = 0;
	this->LastTransition = 0;
	this->PaidUntil = 0;
	this->Subowners = 0;
	this->Guests = 0;
	this->Help = 0;
}

House::House(const House &Other) : House() {
	this->operator=(Other);
}

void House::operator=(const House &Other){
	this->ID = Other.ID;
	memcpy(this->Name, Other.Name, sizeof(this->Name));
	memcpy(this->Description, Other.Description, sizeof(this->Description));
	this->Size = Other.Size;
	this->Rent = Other.Rent;
	this->DepotNr = Other.DepotNr;
	this->NoAuction = Other.NoAuction;
	this->GuildHouse = Other.GuildHouse;
	this->ExitX = Other.ExitX;
	this->ExitY = Other.ExitY;
	this->ExitZ = Other.ExitZ;
	this->CenterX = Other.CenterX;
	this->CenterY = Other.CenterY;
	this->CenterZ = Other.CenterZ;
	this->OwnerID = Other.OwnerID;
	memcpy(this->OwnerName, Other.OwnerName, sizeof(this->OwnerName));
	this->LastTransition = Other.LastTransition;
	this->PaidUntil = Other.PaidUntil;
	this->Help = Other.Help;

	this->Subowners = Other.Subowners;
	for(int i = 0; i < Other.Subowners; i += 1){
		*this->Subowner.at(i) = Other.Subowner.copyAt(i);
	}

	this->Guests = Other.Guests;
	for(int i = 0; i < Other.Guests; i += 1){
		*this->Guest.at(i) = Other.Guest.copyAt(i);
	}
}

HouseArea *get_house_area(uint16 ID){
	HouseArea *Result = NULL;
	for(int i = 0; i < HouseAreas; i += 1){
		if(HouseAreaArray.at(i)->ID == ID){
			Result = HouseAreaArray.at(i);
			break;
		}
	}

	if(Result == NULL){
		error("get_house_area: Area with ID %d not found.\n", ID);
	}

	return Result;
}

int check_access_right(const char *Rule, TPlayer *Player){
	if(Rule == NULL){
		error("check_access_right: Rule is NULL.\n");
		return 0; // NOT_APPLICABLE ?
	}

	if(Player == NULL){
		error("check_access_right: pl is NULL.\n");
		return 0; // NOT_APPLICABLE ?
	}

	bool Negate = Rule[0] == '!';
	if(Negate){
		Rule += 1;
	}

	bool Match;
	if(strchr(Rule, '@') == NULL){
		Match = MatchString(Rule, Player->Name);
	}else{
		if(Player->Guild[0] == 0){
			return 0; // NOT_APPLICABLE ?
		}

		char Help[200];
		if(Player->Rank[0] == 0){
			snprintf(Help, sizeof(Help), "@%s", Player->Guild);
		}else{
			snprintf(Help, sizeof(Help), "%s@%s", Player->Rank, Player->Guild);
		}

		Match = MatchString(Rule, Help);
	}

	if(!Match){
		return 0; // NOT_APPLICABLE ?
	}else if(Negate){
		return -1; // DENY_ACCESS ?
	}else{
		return 1; // ALLOW_ACCESS ?
	}
}

House *get_house(uint16 ID){
	// NOTE(fusion): A little binary search action.
	int Left = 0;
	int Right = Houses - 1;
	House *Result = NULL;
	while(Left <= Right){
		int Current = (Left + Right) / 2;
		uint16 CurrentID = HouseArray.at(Current)->ID;
		if(CurrentID < ID){
			Left = Current + 1;
		}else if(CurrentID > ID){
			Right = Current - 1;
		}else{
			Result = HouseArray.at(Current);
			break;
		}
	}

	if(Result == NULL){
		error("get_house: House with ID %d not found.\n", ID);
	}

	return Result;
}

bool is_owner(uint16 HouseID, TPlayer *Player){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("is_owner: House with ID %d does not exist.\n", HouseID);
		return false;
	}

	if(Player == NULL){
		error("is_owner: pl is NULL.\n");
		return false;
	}

	return House->OwnerID == Player->ID;
}

bool is_subowner(uint16 HouseID, TPlayer *Player, int TimeStamp){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("is_subowner: House with ID %d does not exist.\n", HouseID);
		return false;
	}

	if(Player == NULL){
		error("is_subowner: pl is NULL.\n");
		return false;
	}

	if(House->LastTransition > TimeStamp){
		return false;
	}

	// TODO(fusion): Should allow on a positive rule but keep looking for a
	// negative rule to make it order independent?
	for(int i = 0; i < House->Subowners; i += 1){
		int Access = check_access_right(House->Subowner.at(i)->Name, Player);
		if(Access == -1){
			return false;
		}else if(Access == 1){
			return true;
		}
	}

	return false;
}

bool is_guest(uint16 HouseID, TPlayer *Player, int TimeStamp){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("is_guest: House with ID %d does not exist.\n", HouseID);
		return false;
	}

	if(Player == NULL){
		error("is_guest: pl is NULL.\n");
		return false;
	}

	if(House->LastTransition > TimeStamp){
		return false;
	}

	// TODO(fusion): Should allow on a positive rule but keep looking for a
	// negative rule to make it order independent?
	for(int i = 0; i < House->Guests; i += 1){
		int Access = check_access_right(House->Guest.at(i)->Name, Player);
		if(Access == -1){
			return false;
		}else if(Access == 1){
			return true;
		}
	}

	return false;
}

bool is_invited(uint16 HouseID, TPlayer *Player, int TimeStamp){
	if(Player == NULL){
		error("is_invited: pl is NULL.\n");
		return false;
	}

	return is_owner(HouseID, Player)
		|| is_subowner(HouseID, Player, TimeStamp)
		|| is_guest(HouseID, Player, TimeStamp);
}

const char *get_house_name(uint16 HouseID){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("get_house_name: House with ID %d does not exist.\n", HouseID);
		return NULL;
	}

	return House->Name;
}

const char *get_house_owner(uint16 HouseID){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("get_house_owner: House with ID %d does not exist.\n", HouseID);
		return NULL;
	}

	return House->OwnerName;
}

// TODO(fusion): This function is unsafe like `strcpy`.
void show_subowner_list(uint16 HouseID, TPlayer *Player, char *Buffer){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("show_subowner_list: House with ID %d does not exist.\n", HouseID);
		throw ERROR;
	}

	if(Player == NULL){
		error("show_subowner_list: pl is NULL.\n");
		throw ERROR;
	}

	if(Buffer == NULL){
		error("show_subowner_list: Buffer is NULL.\n");
		throw ERROR;
	}

	if(!is_owner(HouseID, Player)){
		throw NOTACCESSIBLE;
	}

	print(3, "Editing subtenant list of house %d.\n", HouseID);
	sprintf(Buffer, "# Subowners of %s\n", House->Name);
	for(int i = 0; i < House->Subowners; i += 1){
		strcat(Buffer, House->Subowner.at(i)->Name);
		strcat(Buffer, "\n");
	}
}

// TODO(fusion): This function is unsafe like `strcpy`.
void show_guest_list(uint16 HouseID, TPlayer *Player, char *Buffer){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("show_guest_list: House with ID %d does not exist.\n", HouseID);
		throw ERROR;
	}

	if(Player == NULL){
		error("show_guest_list: pl is NULL.\n");
		throw ERROR;
	}

	if(Buffer == NULL){
		error("show_guest_list: Buffer is NULL.\n");
		throw ERROR;
	}

	if(!is_owner(HouseID, Player) && (!is_subowner(HouseID, Player, INT_MAX)
									|| !check_right(Player->ID, PREMIUM_ACCOUNT))){
		throw NOTACCESSIBLE;
	}

	print(3, "Editing guest list of house %d.\n", HouseID);
	sprintf(Buffer, "# Guests of %s\n", House->Name);
	for(int i = 0; i < House->Guests; i += 1){
		strcat(Buffer, House->Guest.at(i)->Name);
		strcat(Buffer, "\n");
	}
}

void change_subowners(uint16 HouseID, TPlayer *Player, const char *Buffer){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("change_subowners: House with ID %d does not exist.\n", HouseID);
		throw ERROR;
	}

	if(Player == NULL){
		error("change_subowners: pl is NULL.\n");
		throw ERROR;
	}

	if(Buffer == NULL){
		error("change_subowners: Buffer is NULL.\n");
		throw ERROR;
	}

	if(!is_owner(HouseID, Player)){
		throw NOTACCESSIBLE;
	}

	log_message("houses", "%s changes subtenant list of house %d.\n", Player->Name, HouseID);

	House->Subowners = 0;
	for(int ReadPos = 0; Buffer[ReadPos] != 0;){
		const char *LineStart = &Buffer[ReadPos];
		const char *LineEnd = strchr(LineStart, '\n');

		int LineLength;
		if(LineEnd != NULL){
			LineLength = (int)(LineEnd - LineStart);
			ReadPos += LineLength + 1;
		}else{
			LineLength = (int)strlen(LineStart);
			ReadPos += LineLength;
		}

		// TODO(fusion): Ignore lines with whitespace only?
		if(LineStart[0] != '#' && LineLength > 0){
			if(LineLength >= MAX_HOUSE_GUEST_NAME){
				LineLength = MAX_HOUSE_GUEST_NAME - 1;
			}

			HouseGuest *Subowner = House->Subowner.at(House->Subowners);
			memcpy(Subowner->Name, LineStart, LineLength);
			Subowner->Name[LineLength] = 0;
			House->Subowners += 1;

			if(House->Subowners >= 10){ // MAX_SUBOWNERS ?
				break;
			}
		}
	}

	kick_guests(HouseID);
}

void change_guests(uint16 HouseID, TPlayer *Player, const char *Buffer){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("change_guests: House with ID %d does not exist.\n", HouseID);
		throw ERROR;
	}

	if(Player == NULL){
		error("change_guests: pl is NULL.\n");
		throw ERROR;
	}

	if(Buffer == NULL){
		error("change_guests: Buffer is NULL.\n");
		throw ERROR;
	}

	if(!is_owner(HouseID, Player) && (!is_subowner(HouseID, Player, INT_MAX)
									|| !check_right(Player->ID, PREMIUM_ACCOUNT))){
		throw NOTACCESSIBLE;
	}

	log_message("houses", "%s changes guest list of house %d.\n", Player->Name, HouseID);

	House->Guests = 0;
	for(int ReadPos = 0; Buffer[ReadPos] != 0;){
		const char *LineStart = &Buffer[ReadPos];
		const char *LineEnd = strchr(LineStart, '\n');

		int LineLength;
		if(LineEnd != NULL){
			LineLength = (int)(LineEnd - LineStart);
			ReadPos += LineLength + 1;
		}else{
			LineLength = (int)strlen(LineStart);
			ReadPos += LineLength;
		}

		// TODO(fusion): Ignore lines with whitespace only?
		if(LineStart[0] != '#' && LineLength > 0){
			if(LineLength >= MAX_HOUSE_GUEST_NAME){
				LineLength = MAX_HOUSE_GUEST_NAME - 1;
			}

			HouseGuest *Guest = House->Guest.at(House->Guests);
			memcpy(Guest->Name, LineStart, LineLength);
			Guest->Name[LineLength] = 0;
			House->Guests += 1;

			if(House->Guests >= 100){ // MAX_GUESTS ?
				break;
			}
		}
	}

	kick_guests(HouseID);
}

void get_exit_position(uint16 HouseID, int *x, int *y, int *z){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("get_exit_position: House with ID %d does not exist.\n", HouseID);
		return;
	}

	*x = House->ExitX;
	*y = House->ExitY;
	*z = House->ExitZ;
}

void kick_guest(uint16 HouseID, TPlayer *Guest){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("kick_guest(houses 1): House with ID %d does not exist.\n", HouseID);
		throw ERROR;
	}

	if(Guest == NULL){
		error("kick_guest(houses 1): Guest is NULL.\n");
		throw ERROR;
	}

	graphical_effect(Guest->CrObject, EFFECT_POFF);

	// TODO(fusion): Not sure what's going on here. Maybe the player is still
	// online but was getting into a bed?
	Object Bed = get_first_object(Guest->posx, Guest->posy, Guest->posz);
	while(Bed != NONE){
		if(Bed.get_object_type().get_flag(BED)){
			break;
		}
		Bed = Bed.get_next_object();
	}

	if(Bed != NONE && Bed.get_attribute(TEXTSTRING) != 0){
		print(3, "Player is standing on a bed while being kicked from the house.\n");
		try{
			use_objects(0, Bed, Bed);
		}catch(RESULT r){
			error("kick_guest: Exception %d while cleaning up bed.\n", r);
		}
	}

	Object Exit = get_map_container(House->ExitX, House->ExitY, House->ExitZ);
	move(0, Guest->CrObject, Exit, -1, false, NONE);
	graphical_effect(Guest->CrObject, EFFECT_ENERGY);
}

void kick_guest(uint16 HouseID, TPlayer *Host, TPlayer *Guest){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("kick_guest(houses 2): House with ID %d does not exist.\n", HouseID);
		throw ERROR;
	}

	if(Host == NULL){
		error("kick_guest(houses 2): Host is NULL.\n");
		throw ERROR;
	}

	if(Guest == NULL){
		error("kick_guest(houses 2): Guest is NULL.\n");
		throw ERROR;
	}

	if(get_house_id(Guest->posx, Guest->posy, Guest->posz) != HouseID){
		throw NOTACCESSIBLE;
	}

	if(Guest != Host){
		if(check_right(Guest->ID, ENTER_HOUSES)){
			throw NOTACCESSIBLE;
		}

		if(!is_owner(HouseID, Host) && !is_subowner(HouseID, Host, INT_MAX)){
			throw NOTACCESSIBLE;
		}

		if(is_owner(HouseID, Guest)){
			throw NOTACCESSIBLE;
		}
	}

	kick_guest(HouseID, Guest);
}

void kick_guests(uint16 HouseID){
	House *House = get_house(HouseID);
	if(House == NULL){
		error("kick_guests: House with ID %d does not exist.\n", HouseID);
		return;
	}

	// TODO(fusion): I think `MaxHouseX` and `MaxHouseY` are the maximum house
	// radii but it is a mistery as to why they're not stored INSIDE `House`.
	TFindCreatures Search(MaxHouseX, MaxHouseY, House->CenterX, House->CenterY, FIND_PLAYERS);
	while(true){
		uint32 CharacterID = Search.getNext();
		if(CharacterID == 0){
			break;
		}

		TPlayer *Player = GetPlayer(CharacterID);
		if(get_house_id(Player->posx, Player->posy, Player->posz) == HouseID
				&& !is_invited(HouseID, Player, INT_MAX)
				&& !check_right(CharacterID, ENTER_HOUSES)){
			kick_guest(HouseID, Player);
		}
	}
}

bool may_open_door(Object Door, TPlayer *Player){
	if(!Door.exists()){
		error("may_open_door: Door does not exist.\n");
		return false;
	}

	if(Player == NULL){
		error("may_open_door: pl is NULL.\n");
		return false;
	}

	ObjectType DoorType = Door.get_object_type();
	if(!DoorType.get_flag(NAMEDOOR) || !DoorType.get_flag(TEXT)){
		error("may_open_door: Door is not a NameDoor.\n");
		return false;
	}

	const char *Text = GetDynamicString(Door.get_attribute(TEXTSTRING));
	if(Text == NULL){
		return false;
	}

	// TODO(fusion): Should allow on a positive rule but keep looking for a
	// negative rule to make it order independent?
	for(int ReadPos = 0; Text[ReadPos] != 0;){
		const char *LineStart = &Text[ReadPos];
		const char *LineEnd = strchr(LineStart, '\n');

		int LineLength;
		if(LineEnd != NULL){
			LineLength = (int)(LineEnd - LineStart);
			ReadPos += LineLength + 1;
		}else{
			LineLength = (int)strlen(LineStart);
			ReadPos += LineLength;
		}

		// TODO(fusion): Ignore lines with whitespace only?
		if(LineStart[0] != '#' && LineLength > 0){
			if(LineLength >= MAX_HOUSE_GUEST_NAME){
				LineLength = MAX_HOUSE_GUEST_NAME - 1;
			}

			char Rule[MAX_HOUSE_GUEST_NAME];
			memcpy(Rule, LineStart, LineLength);
			Rule[LineLength] = 0;

			int Access = check_access_right(Rule, Player);
			if(Access == -1){
				return false;
			}else if(Access == 1){
				return true;
			}
		}
	}

	return false;
}

// TODO(fusion): This function is unsafe like `strcpy`.
void show_name_door(Object Door, TPlayer *Player, char *Buffer){
	if(!Door.exists()){
		error("show_name_door: Door does not exist.\n");
		throw ERROR;
	}

	if(Player == NULL){
		error("show_name_door: pl is NULL.\n");
		throw ERROR;
	}

	if(Buffer == NULL){
		error("show_name_door: Buffer is NULL.\n");
		throw ERROR;
	}

	int DoorX, DoorY, DoorZ;
	get_object_coordinates(Door, &DoorX, &DoorY, &DoorZ);
	uint16 HouseID = get_house_id(DoorX, DoorY, DoorZ);
	if(HouseID == 0){
		error("show_name_door: Door at coordinate [%d,%d,%d] does not belong to any house.\n",
				DoorX, DoorY, DoorZ);
		throw ERROR;
	}

	if(!is_owner(HouseID, Player)){
		print(3, "Player %s is not the tenant of house %d.\n",
				Player->Name, HouseID);
		throw NOTACCESSIBLE;
	}

	print(3, "Editing NameDoor.\n");

	// TODO(fusion): Check for `NAMEDOOR` flag as well?
	if(!Door.get_object_type().get_flag(TEXT)){
		error("show_name_door: Door at coordinate [%d,%d,%d] contains no text.\n",
				DoorX, DoorY, DoorZ);
		throw ERROR;
	}

	const char *Text = GetDynamicString(Door.get_attribute(TEXTSTRING));
	if(Text != NULL){
		strcpy(Buffer, Text);
	}else{
		strcpy(Buffer, "# Players allowed to open this door\n");
	}
}

void change_name_door(Object Door, TPlayer *Player, const char *Buffer){
	if(!Door.exists()){
		error("change_name_door: Door does not exist.\n");
		throw ERROR;
	}

	if(Player == NULL){
		error("change_name_door: pl is NULL.\n");
		throw ERROR;
	}

	if(Buffer == NULL){
		error("change_name_door: Buffer is NULL.\n");
		throw ERROR;
	}

	int DoorX, DoorY, DoorZ;
	get_object_coordinates(Door, &DoorX, &DoorY, &DoorZ);
	uint16 HouseID = get_house_id(DoorX, DoorY, DoorZ);
	if(HouseID == 0){
		error("change_name_door: Door at coordinate [%d,%d,%d] does not belong to any house.\n",
				DoorX, DoorY, DoorZ);
		throw ERROR;
	}

	if(!is_owner(HouseID, Player)){
		throw NOTACCESSIBLE;
	}

	// TODO(fusion): Check for `NAMEDOOR` flag as well?
	if(!Door.get_object_type().get_flag(TEXT)){
		error("change_name_door: Door at coordinate [%d,%d,%d] contains no text.\n",
				DoorX, DoorY, DoorZ);
		throw ERROR;
	}

	log_message("houses", "%s changes NameDoor of house %d at coordinate [%d,%d,%d].\n",
			Player->Name, HouseID, DoorX, DoorY, DoorZ);

	DeleteDynamicString(Door.get_attribute(TEXTSTRING));
	change(Door, TEXTSTRING, AddDynamicString(Buffer));
}

static void CreateCoins(Object Con, ObjectType Type, int Amount){
	// TODO(fusion): Can we guarantee `Amount` is in the [0, 100] range?
	Object Obj = set_object(Con, Type, 0);
	change_object(Obj, AMOUNT, Amount);
}

static int DeleteCoins(Object Con, ObjectType Type, int Amount){
	Object Obj = get_first_container_object(Con);
	while(Amount > 0 && Obj != NONE){
		Object Next = Obj.get_next_object();
		ObjectType ObjType = Obj.get_object_type();
		if(ObjType == Type){
			// TODO(fusion): `change` will notify spectators while `delete_object`
			// will not, so there is an asymmetry here. We also don't check if
			// `Type` is `CUMULATE` but I suppose coins are implicitly `CUMULATE`.
			int ObjAmount = Obj.get_attribute(AMOUNT);
			if(ObjAmount > Amount){
				change(Obj, AMOUNT, (ObjAmount - Amount));
				Amount = 0;
			}else{
				delete_object(Obj);
				Amount -= ObjAmount;
			}
		}else if(ObjType.get_flag(CONTAINER)){
			Amount = DeleteCoins(Obj, Type, Amount);
		}
		Obj = Next;
	}
	return Amount;
}

static void create_money(Object Con, int Amount){
	int Crystal  = (Amount / 10000);
	int Platinum = (Amount % 10000) / 100;
	int Gold     = (Amount % 10000) % 100;

	while(Crystal > 0){
		int StackAmount = std::min<int>(Crystal, 100);
		CreateCoins(Con, get_special_object(MONEY_TENTHOUSAND), StackAmount);
		Crystal -= StackAmount;
	}

	if(Platinum > 0){
		CreateCoins(Con, get_special_object(MONEY_HUNDRED), Platinum);
	}

	if(Gold > 0){
		CreateCoins(Con, get_special_object(MONEY_ONE), Gold);
	}
}

static void DeleteMoney(Object Con, int Amount){
	int Crystal  = count_objects(get_first_container_object(Con), get_special_object(MONEY_TENTHOUSAND), 0);
	int Platinum = count_objects(get_first_container_object(Con), get_special_object(MONEY_HUNDRED), 0);
	int Gold     = count_objects(get_first_container_object(Con), get_special_object(MONEY_ONE), 0);
	calculate_change(Amount, &Gold, &Platinum, &Crystal);

	if(Gold > 0){
		DeleteCoins(Con, get_special_object(MONEY_ONE), Gold);
	}else if(Gold < 0){
		CreateCoins(Con, get_special_object(MONEY_ONE), -Gold);
	}

	if(Platinum > 0){
		DeleteCoins(Con, get_special_object(MONEY_HUNDRED), Platinum);
	}else if(Platinum < 0){
		CreateCoins(Con, get_special_object(MONEY_HUNDRED), -Platinum);
	}

	ASSERT(Crystal >= 0);
	if(Crystal > 0){
		DeleteCoins(Con, get_special_object(MONEY_TENTHOUSAND), Crystal);
	}
}

static Object CreateTempDepot(void){
	// TODO(fusion): All house processing functions use this temporary box placed
	// at the starting position in Rookgaard when loading player depots. It could
	// be problematic in the wrong circumstances (mostly exceptions) but doing
	// it differently at this point would just increase complexity. We just need
	// to make sure this box is destroyed at the end of any house processing.
	int TempX, TempY, TempZ;
	get_start_position(&TempX, &TempY, &TempZ, true);
	Object TempDepot = set_object(get_map_container(TempX, TempY, TempZ),
							get_special_object(DEPOT_LOCKER), 0);
	return TempDepot;
}

// NOTE(fusion): This is used inside house processing functions to empty the depot
// container after processing a player's depot.
static void DeleteContainerObjects(Object Con){
	Object Obj = get_first_container_object(Con);
	while(Obj != NONE){
		Object Next = Obj.get_next_object();
		delete_object(Obj);
		Obj = Next;
	}
}

void clean_field(int x, int y, int z, Object Depot){
	if(Depot == NONE){
		error("clean_field: Depot box does not exist.\n");
		return;
	}

	Object Obj = get_first_object(x, y, z);
	while(Obj != NONE){
		Object Next = Obj.get_next_object();
		ObjectType ObjType = Obj.get_object_type();

		if(ObjType.get_flag(NAMEDOOR) && ObjType.get_flag(TEXT)){
			// TODO(fusion): I don't think we even set `EDITOR` for name doors
			// at all. We're also using `change` which notifies clients, whereas
			// below we're using non notifying functions like `move_object` and
			// `delete_object`.
			DeleteDynamicString(Obj.get_attribute(TEXTSTRING));
			DeleteDynamicString(Obj.get_attribute(EDITOR));
			change(Obj, TEXTSTRING, 0);
			change(Obj, EDITOR, 0);
		}

		if(!ObjType.get_flag(UNMOVE) && ObjType.get_flag(TAKE)){
			move_object(Obj, Depot);
		}else{
			if(ObjType.get_flag(CONTAINER)){
				Object Help = get_first_container_object(Obj);
				while(Help != NONE){
					Object HelpNext = Help.get_next_object();
					move_object(Help, Depot);
					Help = HelpNext;
				}
			}

			if(!ObjType.get_flag(UNMOVE)){
				delete_object(Obj);
			}
		}

		Obj = Next;
	}
}

void clean_house(House *House, TPlayerData *PlayerData){
	if(House == NULL){
		error("clean_house: house is NULL.\n");
		return;
	}

	if(House->OwnerID == 0){
		error("clean_house: House %d has no owner.\n", House->ID);
		return;
	}

	bool PlayerDataAssigned = false;
	if(PlayerData == NULL){
		PlayerData = AssignPlayerPoolSlot(House->OwnerID, false);
		if(PlayerData == NULL){
			error("clean_house: Cannot allocate slot for player data.\n");
			return;
		}

		PlayerDataAssigned = true;
	}

	Object TempDepot = CreateTempDepot();
	LoadDepot(PlayerData, House->DepotNr, TempDepot);

	int MinX = House->CenterX - MaxHouseX;
	int MaxX = House->CenterX + MaxHouseX;
	int MinY = House->CenterY - MaxHouseY;
	int MaxY = House->CenterY + MaxHouseY;
	int MinZ = SectorZMin;
	int MaxZ = SectorZMax;
	for(int FieldZ = MinZ; FieldZ <= MaxZ; FieldZ += 1)
	for(int FieldY = MinY; FieldY <= MaxY; FieldY += 1)
	for(int FieldX = MinX; FieldX <= MaxX; FieldX += 1){
		bool HouseField = (get_house_id(FieldX, FieldY, FieldZ) == House->ID);

		if(!HouseField && coordinate_flag(FieldX, FieldY, FieldZ, HOOKSOUTH)){
			HouseField = (get_house_id(FieldX - 1, FieldY + 1, FieldZ) == House->ID
					||    get_house_id(FieldX,     FieldY + 1, FieldZ) == House->ID
					||    get_house_id(FieldX + 1, FieldY + 1, FieldZ) == House->ID);
		}

		if(!HouseField && coordinate_flag(FieldX, FieldY, FieldZ, HOOKEAST)){
			HouseField = (get_house_id(FieldX + 1, FieldY - 1, FieldZ) == House->ID
					||    get_house_id(FieldX + 1, FieldY,     FieldZ) == House->ID
					||    get_house_id(FieldX + 1, FieldY + 1, FieldZ) == House->ID);
		}

		if(HouseField){
			clean_field(FieldX, FieldY, FieldZ, TempDepot);
		}
	}

	SaveDepot(PlayerData, House->DepotNr, TempDepot);
	delete_object(TempDepot);

	if(PlayerDataAssigned){
		ReleasePlayerPoolSlot(PlayerData);
	}
}

void clear_house(House *House){
	House->OwnerID = 0;
	House->OwnerName[0] = 0;
	House->Subowners = 0;
	House->Guests = 0;
}

bool finish_auctions(void){
	log_message("houses","Processing finished auctions...\n");

	// TODO(fusion): It could be worse.
	int NumberOfAuctions       = Houses;
	uint16 *HouseIDs           = (uint16*)alloca(NumberOfAuctions * sizeof(uint16));
	uint32 *CharacterIDs       = (uint32*)alloca(NumberOfAuctions * sizeof(uint32));
	char (*CharacterNames)[30] = (char(*)[30])alloca(NumberOfAuctions * 30);
	int *Bids                  = (int*)alloca(NumberOfAuctions * sizeof(int));
	int Ret = QueryManagerConnection->finishAuctions(&NumberOfAuctions,
							HouseIDs, CharacterIDs, CharacterNames, Bids);
	if(Ret != 0){
		error("finish_auctions: Cannot determine auctions.\n");
		return false;
	}

	// TODO(fusion): `AddSlashes` will add escape slashes so it should probably
	// be called `EscapeString` or something similar.
	char HelpWorld[60];
	AddSlashes(HelpWorld, WorldName);

	int TimeStamp = (int)time(NULL);
	Object TempDepot = CreateTempDepot();
	for(int AuctionNr = 0; AuctionNr < NumberOfAuctions; AuctionNr += 1){
		uint16 HouseID = HouseIDs[AuctionNr];
		uint32 CharacterID = CharacterIDs[AuctionNr];
		const char *CharacterName = CharacterNames[AuctionNr];
		int Bid = Bids[AuctionNr];

		log_message("houses", "Auction for house %d finished: Won by %s for %d gold.\n",
				HouseID, CharacterName, Bid);
		log_message("houses", "on reset: INSERT INTO HouseAssignments VALUES ('%s',%d,%d,%d);\n",
				HelpWorld, HouseID, CharacterID, Bid);

		House *House = get_house(HouseID);
		if(House == NULL){
			error("finish_auctions: House with number %d does not exist.\n",
					HouseID);
			continue;
		}

		if(House->OwnerID != 0){
			error("finish_auctions: House with number %d already belongs to character %u.\n",
					HouseID, House->OwnerID);
			continue;
		}

		TPlayerData *PlayerData = AssignPlayerPoolSlot(CharacterID, false);
		if(PlayerData == NULL){
			error("finish_auctions: Cannot allocate slot for player data.\n");
			continue;
		}

		LoadDepot(PlayerData, House->DepotNr, TempDepot);
		int DepotMoney = count_money(get_first_container_object(TempDepot));
		if(DepotMoney < (House->Rent + Bid)){
			log_message("houses", "Auction winner does not have enough money.\n");
			House->OwnerID = CharacterID;
			strcpy(House->OwnerName, CharacterName);
			House->PaidUntil = 0;
		}else{
			log_message("houses", "Auction winner has enough money.\n");
			DeleteMoney(TempDepot, (House->Rent + Bid));

			char WelcomeMessage[500];
			snprintf(WelcomeMessage, sizeof(WelcomeMessage),
					"Welcome!\n"
					"\n"
					"Congratulations on your choice\n"
					"for house \"%s\".\n"
					"The rent for the first month\n"
					"has already been debited to your\n"
					"depot. The next rent will be\n"
					"payable in thirty days.\n"
					"Have a good time in your new home!",
					House->Name);
			Object WelcomeLetter = set_object(TempDepot, get_special_object(LETTER_STAMPED), 0);
			change_object(WelcomeLetter, TEXTSTRING, AddDynamicString(WelcomeMessage));
			SaveDepot(PlayerData, House->DepotNr, TempDepot);

			House->OwnerID = CharacterID;
			strcpy(House->OwnerName, CharacterName);
			House->LastTransition = TimeStamp;
			House->PaidUntil = TimeStamp + (30 * 24 * 60 * 60); // one month
		}

		DeleteContainerObjects(TempDepot);
		ReleasePlayerPoolSlot(PlayerData);
	}

	delete_object(TempDepot);

	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		House *H = HouseArray.at(HouseNr);
		if(H->PaidUntil == 0 && H->OwnerID != 0){
			if(QueryManagerConnection->excludeFromAuctions(H->OwnerID, true) != 0){
				error("finish_auctions: Cannot exclude auction winner.\n");
			}

			clear_house(H);
		}
	}

	return true;
}

bool transfer_houses(void){
	log_message("houses", "Processing voluntary cancellations...\n");
	int NumberOfTransfers     = Houses;
	uint16 *HouseIDs          = (uint16*)alloca(NumberOfTransfers * sizeof(uint16));
	uint32 *NewOwnerIDs       = (uint32*)alloca(NumberOfTransfers * sizeof(uint32));
	char (*NewOwnerNames)[30] = (char(*)[30])alloca(NumberOfTransfers * 30);
	int *Prices               = (int*)alloca(NumberOfTransfers * sizeof(int));
	int Ret = QueryManagerConnection->transferHouses(&NumberOfTransfers,
							HouseIDs, NewOwnerIDs, NewOwnerNames, Prices);
	if(Ret != 0){
		error("CollectRents: Cannot determine cancellations.\n");
		return false;
	}

	char HelpWorld[60];
	AddSlashes(HelpWorld, WorldName);

	int TimeStamp = (int)time(NULL);
	Object TempDepot = CreateTempDepot();
	for(int TransferNr = 0; TransferNr < NumberOfTransfers; TransferNr += 1){
		uint16 HouseID = HouseIDs[TransferNr];
		uint32 NewOwnerID = NewOwnerIDs[TransferNr];
		const char *NewOwnerName = NewOwnerNames[TransferNr];
		int TransferPrice = Prices[TransferNr];

		House *House = get_house(HouseID);
		if(House == NULL){
			error("CollectRents: House with number %d does not exist.\n", HouseID);
			continue;
		}

		if(NewOwnerID != 0 && TransferPrice > 0){
			log_message("houses", "Selling house %d to %u for %d gold.\n",
					HouseID, NewOwnerID, TransferPrice);
			TPlayerData *PlayerData = AssignPlayerPoolSlot(NewOwnerID, false);
			if(PlayerData == NULL){
				error("CollectRents: Cannot allocate slot for player data (1a).\n");
				continue;
			}

			LoadDepot(PlayerData, House->DepotNr, TempDepot);
			int DepotMoney = count_money(get_first_container_object(TempDepot));
			if(DepotMoney < TransferPrice){
				log_message("houses", "Buyer does not have enough money.\n");

				char WarningMessage[500];
				snprintf(WarningMessage, sizeof(WarningMessage),
						"Warning!\n"
						"\n"
						"You have not enough gold to pay\n"
						"the price for house \"%s\".\n"
						"Therefore the transfer is cancelled.",
						House->Name);
				Object WarningLetter = set_object(TempDepot, get_special_object(LETTER_STAMPED), 0);
				change_object(WarningLetter, TEXTSTRING, AddDynamicString(WarningMessage));
				SaveDepot(PlayerData, House->DepotNr, TempDepot);
				DeleteContainerObjects(TempDepot);
				ReleasePlayerPoolSlot(PlayerData);

				log_message("houses", "on reset: UPDATE HouseOwners SET Termination=null,NewOwner=null,Accepted=0,Price=null WHERE World=\'%s\' AND HouseID=%d;\n",
						HelpWorld, HouseID);
				QueryManagerConnection->cancelHouseTransfer(HouseID);
				continue;
			}

			DeleteMoney(TempDepot, TransferPrice);
			SaveDepot(PlayerData, House->DepotNr, TempDepot);
			DeleteContainerObjects(TempDepot);
			ReleasePlayerPoolSlot(PlayerData);

			PlayerData = AssignPlayerPoolSlot(House->OwnerID, false);
			if(PlayerData == NULL){
				error("CollectRents: Cannot allocate slot for player data (1b).\n");
				continue;
			}

			LoadDepot(PlayerData, House->DepotNr, TempDepot);
			create_money(TempDepot, TransferPrice);
			SaveDepot(PlayerData, House->DepotNr, TempDepot);
			DeleteContainerObjects(TempDepot);
			ReleasePlayerPoolSlot(PlayerData);
		}

		// TODO(fusion): The old owner needs to manually move out when transfering
		// to a new owner?
		log_message("houses", "Cleaning house %d from %u.\n", HouseID, House->OwnerID);
		if(NewOwnerID == 0){
			clean_house(House, NULL);
		}

		clear_house(House);
		House->Help = 1;

		if(NewOwnerID != 0){
			log_message("houses", "Transferring house %d to %u.\n", HouseID, NewOwnerID);
			House->OwnerID = NewOwnerID;
			strcpy(House->OwnerName, NewOwnerName);
			House->LastTransition = TimeStamp;
			log_message("houses", "on reset: UPDATE HouseOwners SET Termination=%d,NewOwner=%u,Accepted=1,Price=%d WHERE World='%s' AND HouseID=%d;\n",
					TimeStamp, NewOwnerID, TransferPrice, HelpWorld, HouseID);
		}else{
			log_message("houses", "on reset: UPDATE HouseOwners SET Termination=%d,NewOwner=null,Accepted=0,Price=0 WHERE World=\'%s\' AND HouseID=%d;\n",
					TimeStamp, HelpWorld, HouseID);
		}
	}

	delete_object(TempDepot);
	return true;
}

bool evict_free_accounts(void){
	int NumberOfEvictions = Houses;
	uint16 *HouseIDs      = (uint16*)alloca(NumberOfEvictions * sizeof(uint16));
	uint32 *OwnerIDs      = (uint32*)alloca(NumberOfEvictions * sizeof(uint32));
	int Ret = QueryManagerConnection->evictFreeAccounts(&NumberOfEvictions, HouseIDs, OwnerIDs);
	if(Ret != 0){
		error("CollectRents: Cannot determine payment data.\n");
		return false;
	}

	for(int EvictionNr = 0; EvictionNr < NumberOfEvictions; EvictionNr += 1){
		uint16 HouseID = HouseIDs[EvictionNr];
		uint32 OwnerID = OwnerIDs[EvictionNr];

		House *House = get_house(HouseID);
		if(House == NULL){
			error("CollectRents: House with number %d does not exist.\n", HouseID);
			continue;
		}

		log_message("houses", "Account of house %d is no longer paid.\n", HouseID);
		if(OwnerID != House->OwnerID){
			log_message("houses", "... but house was just transferred.\n");
			continue;
		}

		clean_house(House, NULL);
		clear_house(House);
		House->Help = 1;
	}

	return true;
}

bool evict_deleted_characters(void){
	int NumberOfEvictions = Houses;
	uint16 *HouseIDs      = (uint16*)alloca(NumberOfEvictions * sizeof(uint16));
	int Ret = QueryManagerConnection->evictDeletedCharacters(&NumberOfEvictions, HouseIDs);
	if(Ret != 0){
		error("CollectRents: Cannot determine deleted characters.\n");
		return false;
	}

	for(int EvictionNr = 0; EvictionNr < NumberOfEvictions; EvictionNr += 1){
		uint16 HouseID = HouseIDs[EvictionNr];
		House *House = get_house(HouseID);
		if(House == NULL){
			error("CollectRents: House with number %d does not exist.\n", HouseID);
			continue;
		}

		log_message("houses", "Owner of house %d is deleted.\n", HouseID);
		clean_house(House, NULL);
		clear_house(House);
		House->Help = 1;
	}

	return true;
}

bool evict_ex_guild_leaders(void){
	int NumberOfGuildHouses = 0;
	uint16 *HouseIDs        = (uint16*)alloca(Houses * sizeof(uint16));
	uint32 *GuildLeaders    = (uint32*)alloca(Houses * sizeof(uint32));
	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		House *H = HouseArray.at(HouseNr);
		if(H->GuildHouse && H->OwnerID != 0){
			HouseIDs[NumberOfGuildHouses] = H->ID;
			GuildLeaders[NumberOfGuildHouses] = H->OwnerID;
			NumberOfGuildHouses += 1;
		}
	}

	if(NumberOfGuildHouses > 0){
		int NumberOfEvictions;
		int Ret = QueryManagerConnection->evictExGuildleaders(
				NumberOfGuildHouses, &NumberOfEvictions, HouseIDs, GuildLeaders);
		if(Ret != 0){
			error("CollectRents: Cannot determine guild ranks.\n");
			return false;
		}

		for(int EvictionNr = 0; EvictionNr < NumberOfEvictions; EvictionNr += 1){
			uint16 HouseID = HouseIDs[EvictionNr];
			House *House = get_house(HouseID);
			if(House == NULL){
				error("CollectRents: House with number %d does not exist.\n", HouseID);
				continue;
			}

			log_message("houses", "Tenant of guild house %d is no longer guild leader.\n", HouseID);
			clean_house(House, NULL);
			clear_house(House);
			House->Help = 1;
		}
	}

	return true;
}

void collect_rent(void){
	log_message("houses", "Collecting rents...\n");
	int TimeStamp = (int)time(NULL);
	Object TempDepot = CreateTempDepot();
	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		House *H = HouseArray.at(HouseNr);
		if(H->OwnerID == 0 || H->PaidUntil > TimeStamp){
			continue;
		}

		log_message("houses", "Collecting rent for house %d from %u.\n", H->ID, H->OwnerID);

		int Deadline = H->PaidUntil + (7 * 24 * 60 * 60); // 1 week notice
		// TODO(fusion): How is `PaymentExtension` set? This doesn't make a lot of sense.
		if(Deadline < PaymentExtension){
			Deadline = PaymentExtension;
		}

		TPlayerData *PlayerData = AssignPlayerPoolSlot(H->OwnerID, false);
		if(PlayerData == NULL){
			error("CollectRents: Cannot allocate slot for player data (2).\n");
			continue;
		}

		LoadDepot(PlayerData, H->DepotNr, TempDepot);
		int DepotMoney = count_money(get_first_container_object(TempDepot));
		if(DepotMoney < H->Rent){
			if(TimeStamp < Deadline){
				log_message("houses", "Tenant receives warning.\n");

				char WarningMessage[500];
				int DaysLeft = 1 + ((Deadline - TimeStamp - 3600) / 86400);
				snprintf(WarningMessage, sizeof(WarningMessage),
						"Warning!\n"
						"\n"
						"The monthly rent of %d gold\n"
						"for your house \"%s\"\n"
						"is payable. Have it available\n"
						"within %d day%s, or you will\n"
						"lose this house.",
						H->Rent, H->Name, DaysLeft, (DaysLeft != 1 ? "s" : ""));
				Object WarningLetter = set_object(TempDepot, get_special_object(LETTER_STAMPED), 0);
				change_object(WarningLetter, TEXTSTRING, AddDynamicString(WarningMessage));
				SaveDepot(PlayerData, H->DepotNr, TempDepot);
			}else{
				log_message("houses", "Tenant is being evicted.\n");

				if(QueryManagerConnection->excludeFromAuctions(H->OwnerID, false) != 0){
					error("CollectRents: Cannot exclude tenant.\n");
				}

				clean_house(H, PlayerData);
				clear_house(H);
			}
		}else{
			log_message("houses", "Tenant has enough money.\n");
			DeleteMoney(TempDepot, H->Rent);
			SaveDepot(PlayerData, H->DepotNr, TempDepot);
			H->PaidUntil += 30 * 24 * 60 * 60; // one month
		}

		DeleteContainerObjects(TempDepot);
		ReleasePlayerPoolSlot(PlayerData);
	}
	delete_object(TempDepot);
}

void process_rent(void){
	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		HouseArray.at(HouseNr)->Help = 0;
	}

	// TODO(fusion): These functions were all inlined here but each one did some
	// stack allocation and it was a complete mess. I'd assume they were using
	// variable length arrays instead of `alloca` which makes sense but it's not
	// widely supported in C++.
	if(!transfer_houses()
			|| !evict_free_accounts()
			|| !evict_deleted_characters()
			|| !evict_ex_guild_leaders()){
		return;
	}

	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		House *H = HouseArray.at(HouseNr);
		if(H->Help == 1){
			if(QueryManagerConnection->deleteHouseOwner(H->ID) != 0){
				error("CollectRents: Cannot remove house %d from HouseOwners.\n", H->ID);
			}
		}
	}

	collect_rent();
}

bool start_auctions(void){
	log_message("houses", "Starting new auctions...\n");

	int NumberOfAuctions = Houses;
	uint16 *HouseIDs     = (uint16*)alloca(NumberOfAuctions * sizeof(uint16));
	int Ret = QueryManagerConnection->getAuctions(&NumberOfAuctions, HouseIDs);
	if(Ret != 0){
		error("start_auctions: Cannot determine ongoing auctions.\n");
		return false;
	}

	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		HouseArray.at(HouseNr)->Help = 0;
	}

	for(int AuctionNr = 0; AuctionNr < NumberOfAuctions; AuctionNr += 1){
		House *House = get_house(HouseIDs[AuctionNr]);
		if(House == NULL){
			error("start_auctions: House with number %d does not exist (1).\n", HouseIDs[AuctionNr]);
			continue;
		}

		House->Help = 1;
	}

	char HelpWorld[60];
	AddSlashes(HelpWorld, WorldName);
	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		House *H = HouseArray.at(HouseNr);
		if(H->OwnerID == 0 && H->Help == 0 && !H->NoAuction){
			log_message("houses", "Registering house %d for auction.\n", H->ID);
			log_message("houses", "on reset: DELETE FROM Auctions WHERE World=\'%s\' AND HouseID=%d;\n",
					HelpWorld, H->ID);
			if(QueryManagerConnection->startAuction(H->ID) != 0){
				error("start_auctions: Cannot register house %d for auction.\n", H->ID);
			}
		}
	}

	return true;
}

bool update_house_owners(void){
	int NumberOfOwners     = Houses;
	uint16 *HouseIDs       = (uint16*)alloca(NumberOfOwners * sizeof(uint16));
	uint32 *OwnerIDs       = (uint32*)alloca(NumberOfOwners * sizeof(uint32));
	char (*OwnerNames)[30] = (char(*)[30])alloca(NumberOfOwners * 30);
	int *PaidUntils        = (int*)alloca(NumberOfOwners * sizeof(int));
	int Ret = QueryManagerConnection->getHouseOwners(&NumberOfOwners,
						HouseIDs, OwnerIDs, OwnerNames, PaidUntils);
	if(Ret != 0){
		error("start_auctions: Cannot determine registered rented houses.\n");
		return false;
	}

	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		HouseArray.at(HouseNr)->Help = 0;
	}

	for(int OwnerNr = 0; OwnerNr < NumberOfOwners; OwnerNr += 1){
		House *House = get_house(HouseIDs[OwnerNr]);
		if(House == NULL){
			error("start_auctions: House with number %d does not exist (2).\n", HouseIDs[OwnerNr]);
			continue;
		}

		if(House->OwnerID == OwnerIDs[OwnerNr] && House->PaidUntil == PaidUntils[OwnerNr]){
			House->Help = 1;
		}else{
			House->Help = 2;
		}
	}

	log_message("houses", "Updating tenant list...\n");
	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		House *H = HouseArray.at(HouseNr);

		if(H->OwnerID == 0 && H->Help != 0){
			log_message("houses", "Removing no longer rented house %d.\n", H->ID);
			if(QueryManagerConnection->deleteHouseOwner(H->ID) != 0){
				error("start_auctions: Cannot remove no longer rented house %d.\n", H->ID);
			}
		}

		if(H->OwnerID != 0 && H->Help == 0){
			log_message("houses", "Registering rented house %d.\n", H->ID);
			if(QueryManagerConnection->insertHouseOwner(H->ID, H->OwnerID, H->PaidUntil) != 0){
				error("start_auctions: Cannot register rented house %d.\n", H->ID);
			}
		}

		if(H->Help == 2){
			log_message("houses", "Updating rented house %d.\n", H->ID);
			if(QueryManagerConnection->updateHouseOwner(H->ID, H->OwnerID, H->PaidUntil) != 0){
				error("start_auctions: Cannot update data for rented house %d.\n", H->ID);
			}
		}
	}

	return true;
}

void prepare_house_cleanup(void){
	HelpDepots = 0;
}

void finish_house_cleanup(void){
	for(int HelpDepotNr = 0; HelpDepotNr < HelpDepots; HelpDepotNr += 1){
		HelpDepot *Help = HelpDepotArray.at(HelpDepotNr);
		TPlayerData *PlayerData = AssignPlayerPoolSlot(Help->CharacterID, false);
		if(PlayerData == NULL){
			error("finish_house_cleanup: Cannot allocate slot for player data.\n");
			continue;
		}

		SaveDepot(PlayerData, Help->DepotNr, Help->Box);
		delete_object(Help->Box);
		ReleasePlayerPoolSlot(PlayerData);
	}

	HelpDepots = 0;
}

void clean_house_field(int x, int y, int z){
	uint16 HouseID = get_house_id(x, y, z);

	if(HouseID == 0 && coordinate_flag(x, y, z, HOOKSOUTH)){
		HouseID = get_house_id(x, y + 1, z);

		if(HouseID == 0){
			HouseID = get_house_id(x - 1, y + 1, z);
		}

		if(HouseID == 0){
			HouseID = get_house_id(x + 1, y + 1, z);
		}
	}

	if(HouseID == 0 && coordinate_flag(x, y, z, HOOKEAST)){
		HouseID = get_house_id(x + 1, y, z);

		if(HouseID == 0){
			HouseID = get_house_id(x + 1, y - 1, z);
		}

		if(HouseID == 0){
			HouseID = get_house_id(x + 1, y + 1, z);
		}
	}

	if(HouseID == 0){
		error("clean_house_field: No house found for field [%d,%d,%d].\n", x, y, z);
		return;
	}

	House *House = get_house(HouseID);
	if(House == NULL || House->OwnerID == 0){
		return;
	}

	int HelpDepotNr = 0;
	while(HelpDepotNr < HelpDepots){
		HelpDepot *Help = HelpDepotArray.at(HelpDepotNr);
		if(Help->CharacterID == House->OwnerID && Help->DepotNr == House->DepotNr){
			break;
		}
		HelpDepotNr += 1;
	}

	if(HelpDepotNr >= HelpDepots){
		TPlayerData *PlayerData = AssignPlayerPoolSlot(House->OwnerID, false);
		if(PlayerData == NULL){
			error("clean_house_field: Cannot allocate slot for player data.\n");
			return;
		}

		HelpDepotNr = HelpDepots;
		HelpDepots += 1;

		HelpDepot *Help = HelpDepotArray.at(HelpDepotNr);
		Help->CharacterID = House->OwnerID;
		Help->DepotNr = House->DepotNr;
		Help->Box = CreateTempDepot();
		LoadDepot(PlayerData, House->DepotNr, Help->Box);
		ReleasePlayerPoolSlot(PlayerData);
	}

	clean_field(x, y, z, HelpDepotArray.at(HelpDepotNr)->Box);
}

void load_house_areas(void){
	log_message("houses", "Loading house area data...\n");

	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/houseareas.dat", DATAPATH);
	if(!FileExists(FileName)){
		log_message("houses", "No house area data found.\n");
		return;
	}

	ReadScriptFile Script;
	Script.open(FileName);
	while(true){
		Script.next_token();
		if(Script.Token == ENDOFFILE){
			Script.close();
			break;
		}

		// TODO(fusion): Any identifier? Seems that `Area` is used so we should
		// probably check that?.
		//if(strcmp(Script.get_identifier(), "area") != 0){
		//	Script.error("area expected");
		//}
		Script.get_identifier();

		HouseArea *Area = HouseAreaArray.at(HouseAreas);
		Script.read_symbol('=');
		Script.read_symbol('(');
		Area->ID = (uint16)Script.read_number();
		Script.read_symbol(',');
		Script.read_string(); // area name
		Script.read_symbol(',');
		Area->SQMPrice = Script.read_number();
		Script.read_symbol(',');
		Area->DepotNr = Script.read_number();
		Script.read_symbol(')');
		HouseAreas += 1;
	}
}

void load_houses(void){
	log_message("houses", "Loading house data...\n");

	Houses = 0;
	MaxHouseX = 0;
	MaxHouseY = 0;

	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/houses.dat", DATAPATH);
	if(!FileExists(FileName)){
		log_message("houses", "No house data found.\n");
		return;
	}

	ReadScriptFile Script;
	Script.open(FileName);
	while(true){
		Script.next_token();
		if(Script.Token == ENDOFFILE){
			Script.close();
			break;
		}

		// TODO(fusion): We expect house fields to be in an exact order and the
		// identifiers don't matter at all. For whatever reason these feel precarious
		// when compared to other loaders.

		Script.get_identifier(); // "id"
		Script.read_symbol('=');
		uint16 HouseID = (uint16)Script.read_number();
		if(HouseID == 0){
			error("load_houses: Invalid ID %d.\n", HouseID);
			throw "cannot load houses";
		}

		if(Houses > 0 && HouseID <= HouseArray.at(Houses - 1)->ID){
			error("load_houses: IDs not sorted in ascending order (ID=%d).\n", HouseID);
			throw "cannot load houses";
		}

		House *H = HouseArray.at(Houses);
		H->ID = HouseID;

		Script.read_identifier(); // "name"
		Script.read_symbol('=');
		strcpy(H->Name, Script.read_string());

		Script.read_identifier(); // "description"
		Script.read_symbol('=');
		strcpy(H->Description, Script.read_string());

		Script.read_identifier(); // "rentoffset"
		Script.read_symbol('=');
		int RentOffset = Script.read_number();

		Script.read_identifier(); // "area"
		Script.read_symbol('=');
		uint16 AreaID = (uint16)Script.read_number();
		HouseArea *Area = get_house_area(AreaID);
		if(Area == NULL){
			error("load_houses: Area for house %d does not exist.\n", HouseID);
			throw "cannot load houses";
		}
		H->DepotNr = Area->DepotNr;

		Script.read_identifier(); // "guildhouse"
		Script.read_symbol('=');
		H->GuildHouse = (strcmp(Script.read_identifier(), "true") == 0);

		H->NoAuction = false;

		Script.read_identifier(); // "exit"
		Script.read_symbol('=');
		Script.read_coordinate(&H->ExitX, &H->ExitY, &H->ExitZ);
		if(H->ExitX == 0){
			error("load_houses: Exit for house %d not set.\n", HouseID);
		}

		Script.read_identifier(); // "center"
		Script.read_symbol('=');
		Script.read_coordinate(&H->CenterX, &H->CenterY, &H->CenterZ);
		if(H->CenterX == 0){
			// TODO(fusion): Same error message as above?
			error("load_houses: Exit for house %d not set.\n", HouseID);
		}

		Script.read_identifier(); // "fields"
		Script.read_symbol('=');
		Script.read_symbol('{');
		while(true){
			Script.next_token();
			if(Script.Token == SPECIAL){
				if(Script.get_special() == '}'){
					break;
				}else if(Script.get_special() == ','){
					continue;
				}
			}

			int FieldX, FieldY, FieldZ;
			Script.get_coordinate(&FieldX, &FieldY, &FieldZ);
			set_house_id(FieldX, FieldY, FieldZ, HouseID);
			H->Size += 1;

			int RadiusX = std::abs(FieldX - H->CenterX);
			int RadiusY = std::abs(FieldY - H->CenterY);

			if(RadiusX > MaxHouseX){
				MaxHouseX = RadiusX;
			}

			if(RadiusY > MaxHouseY){
				MaxHouseY = RadiusY;
			}
		}

		H->Rent = Area->SQMPrice * H->Size + RentOffset;
		H->OwnerID = 0;
		H->OwnerName[0] = 0;
		H->LastTransition = 0;
		H->PaidUntil = 0;
		H->Subowners = 0;
		H->Guests = 0;

		Houses += 1;
	}

	uint16 *HouseIDs          = (uint16*)alloca(Houses * sizeof(uint16));
	const char **Names        = (const char**)alloca(Houses * sizeof(const char*));
	int *Rents                = (int*)alloca(Houses * sizeof(int));
	const char **Descriptions = (const char**)alloca(Houses * sizeof(const char*));
	int *Sizes                = (int*)alloca(Houses * sizeof(int));
	int *PositionsX           = (int*)alloca(Houses * sizeof(int));
	int *PositionsY           = (int*)alloca(Houses * sizeof(int));
	int *PositionsZ           = (int*)alloca(Houses * sizeof(int));
	char (*Towns)[30]         = (char(*)[30])alloca(Houses * 30);
	bool *Guildhouses         = (bool*)alloca(Houses * sizeof(bool));
	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		House *H = HouseArray.at(HouseNr);
		HouseIDs[HouseNr] = H->ID;
		Names[HouseNr] = H->Name;
		Rents[HouseNr] = H->Rent;
		Descriptions[HouseNr] = H->Description;
		Sizes[HouseNr] = H->Size;
		PositionsX[HouseNr] = H->CenterX;
		PositionsY[HouseNr] = H->CenterY;
		PositionsZ[HouseNr] = H->CenterZ;
		strcpy(Towns[HouseNr], get_depot_name(H->DepotNr));
		Guildhouses[HouseNr] = H->GuildHouse;
	}

	int QueryBufferSize = Houses * 600;
	if(QueryBufferSize < (int)kb(16)){
		QueryBufferSize = (int)kb(16);
	}

	QueryManagerConnection = new TQueryManagerConnection(QueryBufferSize);
	int Ret = QueryManagerConnection->insertHouses(Houses, HouseIDs, Names, Rents,
			Descriptions, Sizes, PositionsX, PositionsY, PositionsZ, Towns, Guildhouses);
	if(Ret != 0){
		error("load_houses: Cannot register house master data.\n");
	}
}

void load_owners(void){
	log_message("houses", "Loading tenant data...\n");

	PaymentExtension = 0;
	bool ClearGuests = false;
	bool ClearBeds = false;

	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/owners.dat", DATAPATH);
	if(!FileExists(FileName)){
		log_message("houses", "No tenant data found.\n");
		return;
	}

	ReadScriptFile Script;
	Script.open(FileName);
	while(true){
		Script.next_token();
		if(Script.Token == ENDOFFILE){
			Script.close();
			break;
		}

		const char *Identifier = Script.get_identifier();
		if(strcmp(Identifier, "extension") == 0){
			Script.read_symbol('=');
			PaymentExtension = Script.read_number();
		}else if(strcmp(Identifier, "clearguests") == 0){
			ClearGuests = true;
		}else if(strcmp(Identifier, "clearbeds") == 0){
			ClearBeds = true;
		}else if(strcmp(Identifier, "id") == 0){
			Script.read_symbol('=');
			uint16 HouseID = (uint16)Script.read_number();
			House *House = get_house(HouseID);
			if(House == NULL){
				error("load_owners: House with ID %d does not exist.", HouseID);
				throw "Cannot load owners";
			}

			Script.read_identifier(); // "owner"
			Script.read_symbol('=');
			House->OwnerID = (uint32)Script.read_number();

			Script.read_identifier(); // "lasttransition"
			Script.read_symbol('=');
			House->LastTransition = Script.read_number();

			Script.read_identifier(); // "paiduntil"
			Script.read_symbol('=');
			House->PaidUntil = Script.read_number();

			Script.read_identifier(); // "guests"
			Script.read_symbol('=');
			Script.read_symbol('{');
			while(true){
				Script.next_token();
				if(Script.Token == SPECIAL){
					if(Script.get_special() == '}'){
						break;
					}else if(Script.get_special() == ','){
						continue;
					}
				}

				strcpy(House->Guest.at(House->Guests)->Name, Script.get_string());
				House->Guests += 1;
			}

			Script.read_identifier(); // "subowners"
			Script.read_symbol('=');
			Script.read_symbol('{');
			while(true){
				Script.next_token();
				if(Script.Token == SPECIAL){
					if(Script.get_special() == '}'){
						break;
					}else if(Script.get_special() == ','){
						continue;
					}
				}

				strcpy(House->Subowner.at(House->Subowners)->Name, Script.get_string());
				House->Subowners += 1;
			}

			if(ClearGuests){
				House->Guests = 0;
			}
		}else{
			Script.error("Unknown identifier");
		}
	}

	int NumberOfOwners     = Houses;
	uint16 *HouseIDs       = (uint16*)alloca(NumberOfOwners * sizeof(uint16));
	uint32 *OwnerIDs       = (uint32*)alloca(NumberOfOwners * sizeof(uint32));
	char (*OwnerNames)[30] = (char(*)[30])alloca(NumberOfOwners * 30);
	int *PaidUntils        = (int*)alloca(NumberOfOwners * sizeof(int));
	int Ret = QueryManagerConnection->getHouseOwners(&NumberOfOwners,
						HouseIDs, OwnerIDs, OwnerNames, PaidUntils);
	if(Ret != 0){
		error("load_owners: Cannot determine tenant names.\n");
		throw "Cannot load owners";
	}

	for(int OwnerNr = 0; OwnerNr < NumberOfOwners; OwnerNr += 1){
		House *House = get_house(HouseIDs[OwnerNr]);
		if(House == NULL){
			error("load_owners: House %d does not exist.\n", HouseIDs[OwnerNr]);
			continue;
		}

		if(House->OwnerID != 0){
			strcpy(House->OwnerName, OwnerNames[OwnerNr]);
		}
	}

	if(ClearBeds){
		print(1, "Cleaning all beds...\n");

		for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
			House *H = HouseArray.at(HouseNr);
			int MinX = H->CenterX - MaxHouseX;
			int MaxX = H->CenterX + MaxHouseX;
			int MinY = H->CenterY - MaxHouseY;
			int MaxY = H->CenterY + MaxHouseY;
			int MinZ = SectorZMin;
			int MaxZ = SectorZMax;
			for(int FieldZ = MinZ; FieldZ <= MaxZ; FieldZ += 1)
			for(int FieldY = MinY; FieldY <= MaxY; FieldY += 1)
			for(int FieldX = MinX; FieldX <= MaxX; FieldX += 1){
				Object Bed = get_first_object(FieldX, FieldY, FieldZ);
				while(Bed != NONE){
					if(Bed.get_object_type().get_flag(BED)){
						break;
					}
					Bed = Bed.get_next_object();
				}

				if(Bed != NONE && Bed.get_attribute(TEXTSTRING) != 0){
					try{
						use_objects(0, Bed, Bed);
					}catch(RESULT r){
						error("load_owners: Exception %d while cleaning up a bed.\n", r);
					}
				}
			}
		}
	}
}

void save_owners(void){
	char FileName[4096];
	snprintf(FileName, sizeof(FileName), "%s/owners.dat", DATAPATH);

	WriteScriptFile Script;
	Script.open(FileName);

	if(PaymentExtension > (int)time(NULL)){
		Script.write_text("Extension = ");
		Script.write_number(PaymentExtension);
		Script.write_ln();
		Script.write_ln();
	}

	for(int HouseNr = 0; HouseNr < Houses; HouseNr += 1){
		House *H = HouseArray.at(HouseNr);
		if(H->OwnerID == 0){
			continue;
		}

		Script.write_text("ID = ");
		Script.write_number((int)H->ID);
		Script.write_ln();

		Script.write_text("Owner = ");
		Script.write_number((int)H->OwnerID);
		Script.write_ln();

		Script.write_text("LastTransition = ");
		Script.write_number((int)H->LastTransition);
		Script.write_ln();

		Script.write_text("PaidUntil = ");
		Script.write_number((int)H->PaidUntil);
		Script.write_ln();

		Script.write_text("Guests = {");
		for(int i = 0; i < H->Guests; i += 1){
			if(i > 0){
				Script.write_text(",");
			}
			Script.write_string(H->Guest.at(i)->Name);
		}
		Script.write_text("}");
		Script.write_ln();

		Script.write_text("Subowners = {");
		for(int i = 0; i < H->Subowners; i += 1){
			if(i > 0){
				Script.write_text(",");
			}
			Script.write_string(H->Subowner.at(i)->Name);
		}
		Script.write_text("}");
		Script.write_ln();

		Script.write_ln();
	}

	Script.close();
}

void process_houses(void){
	finish_auctions();
	process_rent();
	start_auctions();
	update_house_owners();
}

void init_houses(void){
	// TODO(fusion): Connect to query manager HERE then pass the connection around
	// house initialization and processing functions. It is currently connecting in
	// `load_houses`, used by other functions, then deleted at the end of this function.
	//TQueryManagerConnection QueryManagerConnection(QueryBufferSize);

	init_log("houses");
	load_house_areas();
	load_houses();
	load_owners();
	process_houses();
	delete QueryManagerConnection;
}

void exit_houses(void){
	save_owners();
}
