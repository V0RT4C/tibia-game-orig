#ifndef TIBIA_OPERATE_H_
#define TIBIA_OPERATE_H_ 1

#include "common.h"
#include "containers.h"
#include "cr.h"
#include "map.h"

enum : int {
	CREATURE_HEALTH_CHANGED		= 1,
	CREATURE_LIGHT_CHANGED		= 2,
	CREATURE_OUTFIT_CHANGED		= 3,
	CREATURE_SPEED_CHANGED		= 4,
	CREATURE_SKULL_CHANGED		= 5,
	CREATURE_PARTY_CHANGED		= 6,
};

enum : int {
	OBJECT_DELETED				= 0,
	OBJECT_CREATED				= 1,
	OBJECT_CHANGED				= 2,
	OBJECT_MOVED				= 3,
};

enum : int {
	CHANNEL_GUILD				= 0,
	CHANNEL_GAMEMASTER			= 1,
	CHANNEL_TUTOR				= 2,
	CHANNEL_RULEVIOLATIONS		= 3,
	CHANNEL_GAMECHAT			= 4,
	CHANNEL_TRADE				= 5,
	CHANNEL_RLCHAT				= 6,
	CHANNEL_HELP				= 7,

	PUBLIC_CHANNELS				= 8,
	FIRST_PRIVATE_CHANNEL		= PUBLIC_CHANNELS,
	MAX_CHANNELS				= 0xFFFF,
};

struct Channel {
	Channel(void);

	// TODO(fusion): `Channel` will primarily live inside the `Channel` vector,
	// which needs to resize its internal array as needed. To resize, it needs
	// to copy/swap elements, which would be impossible since `Channel` itself
	// owns a few vectors which were made NON-COPYABLE to prevent memory bugs.
	//	To fix this problem and allow `Channel` to be copyable, we need to
	// implement the copy constructor and assignment manually. They're annoying
	// and expensive but should allow everything to compile again.
	//	This problem arises from the fact that our unconventional `vector` type
	// has a weird interface but still manages its underlying memory. If we are
	// to resolve this completely, we'd need to re-implement `vector` properly
	// and preferably with move semantics (if we want to follow the C++ route,
	// which we may not).
	Channel(const Channel &Other);
	void operator=(const Channel &Other);

	// DATA
	// =================
	uint32 Moderator;
	char ModeratorName[30];
	vector<uint32> Subscriber;
	int Subscribers;
	vector<uint32> InvitedPlayer;
	int InvitedPlayers;
};

struct Party {
	Party(void);

	// TODO(fusion): Same as `Channel`.
	Party(const Party &Other);
	void operator=(const Party &Other);

	// DATA
	// =================
	uint32 Leader;
	vector<uint32> Member;
	int Members;
	vector<uint32> InvitedPlayer;
	int InvitedPlayers;
};

struct Statement {
	uint32 StatementID;
	int TimeStamp;
	uint32 CharacterID;
	int Mode;
	int Channel;
	uint32 Text;
	bool Reported;
};

struct Listener {
	uint32 StatementID;
	uint32 CharacterID;
};

struct ReportedStatement {
	uint32 StatementID;
	int TimeStamp;
	uint32 CharacterID;
	int Mode;
	int Channel;
	char Text[256];
};

void announce_moving_creature(uint32 CreatureID, Object Con);
void announce_changed_creature(uint32 CreatureID, int Type);
void announce_changed_field(Object Obj, int Type);
void announce_changed_container(Object Obj, int Type);
void announce_changed_inventory(Object Obj, int Type);
void announce_changed_object(Object Obj, int Type);
void announce_graphical_effect(int x, int y, int z, int Type);
void announce_textual_effect(int x, int y, int z, int Color, const char *Text);
void announce_missile(int OrigX, int OrigY, int OrigZ,
		int DestX, int DestY, int DestZ, int Type);
void check_top_move_object(uint32 CreatureID, Object Obj, Object Ignore);
void check_top_use_object(uint32 CreatureID, Object Obj);
void check_top_multiuse_object(uint32 CreatureID, Object Obj);
void check_move_object(uint32 CreatureID, Object Obj, bool Take);
void check_map_destination(uint32 CreatureID, Object Obj, Object MapCon);
void check_map_place(uint32 CreatureID, ObjectType Type, Object MapCon);
void check_container_destination(Object Obj, Object Con);
void check_container_place(ObjectType Type, Object Con, Object OldObj);
void check_depot_space(uint32 CreatureID, Object Source, Object Destination, int Count);
void check_inventory_destination(Object Obj, Object Con, bool Split);
void check_inventory_place(ObjectType Type, Object Con, Object OldObj);
void check_weight(uint32 CreatureID, Object Obj, int Count);
void check_weight(uint32 CreatureID, ObjectType Type, uint32 Value, int OldWeight);
void notify_creature(uint32 CreatureID, Object Obj, bool Inventory);
void notify_creature(uint32 CreatureID, ObjectType Type, bool Inventory);
void notify_all_creatures(Object Obj, int Type, Object OldCon);
void notify_trades(Object Obj);
void notify_depot(uint32 CreatureID, Object Obj, int Count);
void close_container(Object Con, bool Force);
Object create(Object Con, ObjectType Type, uint32 Value);
Object copy(Object Con, Object Source);
void move(uint32 CreatureID, Object Obj, Object Con, int Count, bool NoMerge, Object Ignore);
void merge(uint32 CreatureID, Object Obj, Object Dest, int Count, Object Ignore);
void change(Object Obj, ObjectType NewType, uint32 Value);
void change(Object Obj, INSTANCEATTRIBUTE Attribute, uint32 Value);
void delete_op(Object Obj, int Count);
void empty(Object Con, int Remainder);
void graphical_effect(int x, int y, int z, int Type);
void graphical_effect(Object Obj, int Type);
void textual_effect(Object Obj, int Color, const char *Format, ...) ATTR_PRINTF(3, 4);
void missile(Object Start, Object Dest, int Type);
void look(uint32 CreatureID, Object Obj);
void talk(uint32 CreatureID, int Mode, const char *Addressee, const char *Text, bool CheckSpamming);
void use(uint32 CreatureID, Object Obj1, Object Obj2, uint8 Info);
void turn(uint32 CreatureID, Object Obj);
void create_pool(Object Con, ObjectType Type, uint32 Value);
void edit_text(uint32 CreatureID, Object Obj, const char *Text);
Object create_at_creature(uint32 CreatureID, ObjectType Type, uint32 Value);
void delete_at_creature(uint32 CreatureID, ObjectType Type, int Amount, uint32 Value);

void process_cron_system(void);
bool sector_refreshable(int SectorX, int SectorY, int SectorZ);
void refresh_sector(int SectorX, int SectorY, int SectorZ, const uint8 *Data, int Count);
void refresh_map(void);
void refresh_cylinders(void);
void apply_patch(int SectorX, int SectorY, int SectorZ,
		bool FullSector, ReadScriptFile *Script, bool SaveHouses);
void apply_patches(void);

uint32 log_communication(uint32 CreatureID, int Mode, int Channel, const char *Text);
uint32 log_listener(uint32 StatementID, TPlayer *Player);
void process_communication_control(void);
int get_communication_context(uint32 CharacterID, uint32 StatementID,
		int *NumberOfStatements, vector<ReportedStatement> **ReportedStatements);

int get_number_of_channels(void);
bool channel_active(int ChannelID);
bool channel_available(int ChannelID, uint32 CharacterID);
const char *get_channel_name(int ChannelID, uint32 CharacterID);
bool channel_subscribed(int ChannelID, uint32 CharacterID);
uint32 get_first_subscriber(int ChannelID);
uint32 get_next_subscriber(void);
bool may_open_channel(uint32 CharacterID);
void open_channel(uint32 CharacterID);
void close_channel(int ChannelID);
void invite_to_channel(uint32 CharacterID, const char *Name);
void exclude_from_channel(uint32 CharacterID, const char *Name);
bool join_channel(int ChannelID, uint32 CharacterID);
void leave_channel(int ChannelID, uint32 CharacterID, bool Close);
void leave_all_channels(uint32 CharacterID);

Party *get_party(uint32 LeaderID);
bool is_invited_to_party(uint32 GuestID, uint32 HostID);
void disband_party(uint32 LeaderID);
void invite_to_party(uint32 HostID, uint32 GuestID);
void revoke_invitation(uint32 HostID, uint32 GuestID);
void join_party(uint32 GuestID, uint32 HostID);
void pass_leadership(uint32 OldLeaderID, uint32 NewLeaderID);
void leave_party(uint32 MemberID, bool Forced);

#endif //TIBIA_OPERATE_H_
