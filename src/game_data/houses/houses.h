#ifndef TIBIA_HOUSES_H_
#define TIBIA_HOUSES_H_ 1

#include "common.h"
#include "containers.h"
#include "map.h"

#define MAX_HOUSE_GUEST_NAME 60

struct TPlayer;
struct TPlayerData;

struct HelpDepot {
	uint32 CharacterID;
	Object Box;
	int DepotNr;
};

struct HouseArea {
	uint16 ID;
	int SQMPrice;
	int DepotNr;
};

struct HouseGuest {
	char Name[MAX_HOUSE_GUEST_NAME];
};

struct House {
	House(void);

	// TODO(fusion): Same as `Channel` in `operate.hh`.
	House(const House &Other);
	void operator=(const House &Other);

	// DATA
	// =================
	uint16 ID;
	char Name[50];
	char Description[500];
	int Size;
	int Rent;
	int DepotNr;
	bool NoAuction;
	bool GuildHouse;
	int ExitX;
	int ExitY;
	int ExitZ;
	int CenterX;
	int CenterY;
	int CenterZ;
	uint32 OwnerID;
	char OwnerName[30];
	int LastTransition;
	int PaidUntil;
	vector<HouseGuest> Subowner;
	int Subowners;
	vector<HouseGuest> Guest;
	int Guests;
	int Help;
};

HouseArea *get_house_area(uint16 ID);
int check_access_right(const char *Rule, TPlayer *Player);
House *get_house(uint16 ID);
bool is_owner(uint16 HouseID, TPlayer *Player);
bool is_subowner(uint16 HouseID, TPlayer *Player, int TimeStamp);
bool is_guest(uint16 HouseID, TPlayer *Player, int TimeStamp);
bool is_invited(uint16 HouseID, TPlayer *Player, int TimeStamp);
const char *get_house_name(uint16 HouseID);
const char *get_house_owner(uint16 HouseID);
void show_subowner_list(uint16 HouseID, TPlayer *Player, char *Buffer);
void show_guest_list(uint16 HouseID, TPlayer *Player, char *Buffer);
void change_subowners(uint16 HouseID, TPlayer *Player, const char *Buffer);
void change_guests(uint16 HouseID, TPlayer *Player, const char *Buffer);
void get_exit_position(uint16 HouseID, int *x, int *y, int *z);
void kick_guest(uint16 HouseID, TPlayer *Guest);
void kick_guest(uint16 HouseID, TPlayer *Host, TPlayer *Guest);
void kick_guests(uint16 HouseID);
bool may_open_door(Object Door, TPlayer *Player);
void show_name_door(Object Door, TPlayer *Player, char *Buffer);
void change_name_door(Object Door, TPlayer *Player, const char *Buffer);
void clean_field(int x, int y, int z, Object Depot);
void clean_house(House *House, TPlayerData *PlayerData);
void clear_house(House *House);
bool finish_auctions(void);
bool transfer_houses(void);
bool evict_free_accounts(void);
bool evict_deleted_characters(void);
bool evict_ex_guild_leaders(void);
void collect_rent(void);
void process_rent(void);
bool start_auctions(void);
bool update_house_owners(void);
void prepare_house_cleanup(void);
void finish_house_cleanup(void);
void clean_house_field(int x, int y, int z);
void load_house_areas(void);
void load_houses(void);
void load_owners(void);
void save_owners(void);
void process_houses(void);
void init_houses(void);
void exit_houses(void);

#endif //TIBIA_HOUSES_H_
