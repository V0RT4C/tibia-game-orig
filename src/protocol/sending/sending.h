// src/protocol/sending/sending.h
#ifndef TIBIA_PROTOCOL_SENDING_H_
#define TIBIA_PROTOCOL_SENDING_H_ 1

#include "map.h" // for Object type used in Send* signatures
#include "network/connection/connection.h"
#include "protocol/protocol_enums.h"

void send_all(void);
bool begin_send_data(TConnection *Connection);
void finish_send_data(TConnection *Connection);
void skip_flush(TConnection *Connection);
void send_map_object(TConnection *Connection, Object Obj);
void send_map_point(TConnection *Connection, int x, int y, int z);
void send_result(TConnection *Connection, RESULT r);
void send_refresh(TConnection *Connection);
void send_init_game(TConnection *Connection, uint32 CreatureID);
void send_rights(TConnection *Connection);
void send_ping(TConnection *Connection);
void send_full_screen(TConnection *Connection);
void send_row(TConnection *Connection, int Direction);
void send_floors(TConnection *Connection, bool Up);
void send_field_data(TConnection *Connection, int x, int y, int z);
void send_add_field(TConnection *Connection, int x, int y, int z, Object Obj);
void send_change_field(TConnection *Connection, int x, int y, int z, Object Obj);
void send_delete_field(TConnection *Connection, int x, int y, int z, Object Obj);
void send_move_creature(TConnection *Connection, uint32 CreatureID, int DestX, int DestY, int DestZ);
void send_container(TConnection *Connection, int ContainerNr);
void send_close_container(TConnection *Connection, int ContainerNr);
void send_create_in_container(TConnection *Connection, int ContainerNr, Object Obj);
void send_change_in_container(TConnection *Connection, int ContainerNr, Object Obj);
void send_delete_in_container(TConnection *Connection, int ContainerNr, Object Obj);
void send_body_inventory(TConnection *Connection, uint32 CreatureID);
void send_set_inventory(TConnection *Connection, int Position, Object Obj);
void send_delete_inventory(TConnection *Connection, int Position);
void send_trade_offer(TConnection *Connection, const char *Name, bool OwnOffer, Object Obj);
void send_close_trade(TConnection *Connection);
void send_ambiente(TConnection *Connection);
void send_graphical_effect(TConnection *Connection, int x, int y, int z, int Type);
void send_textual_effect(TConnection *Connection, int x, int y, int z, int Color, const char *Text);
void send_missile_effect(TConnection *Connection, int OrigX, int OrigY, int OrigZ, int DestX, int DestY, int DestZ,
						 int Type);
void send_mark_creature(TConnection *Connection, uint32 CreatureID, int Color);
void send_creature_health(TConnection *Connection, uint32 CreatureID);
void send_creature_light(TConnection *Connection, uint32 CreatureID);
void send_creature_outfit(TConnection *Connection, uint32 CreatureID);
void send_creature_speed(TConnection *Connection, uint32 CreatureID);
void send_creature_skull(TConnection *Connection, uint32 CreatureID);
void send_creature_party(TConnection *Connection, uint32 CreatureID);
void send_edit_text(TConnection *Connection, Object Obj);
void send_edit_list(TConnection *Connection, uint8 Type, uint32 ID, const char *Text);
void send_player_data(TConnection *Connection);
void send_player_skills(TConnection *Connection);
void send_player_state(TConnection *Connection, uint8 State);
void send_clear_target(TConnection *Connection);
void send_talk(TConnection *Connection, uint32 StatementID, const char *Sender, int Mode, const char *Text, int Data);
void send_talk(TConnection *Connection, uint32 StatementID, const char *Sender, int Mode, int Channel,
			   const char *Text);
void send_talk(TConnection *Connection, uint32 StatementID, const char *Sender, int Mode, int x, int y, int z,
			   const char *Text);
void send_channels(TConnection *Connection);
void send_open_channel(TConnection *Connection, int Channel);
void send_private_channel(TConnection *Connection, const char *Name);
void send_open_request_queue(TConnection *Connection);
void send_delete_request(TConnection *Connection, const char *Name);
void send_finish_request(TConnection *Connection, const char *Name);
void send_close_request(TConnection *Connection);
void send_open_own_channel(TConnection *Connection, int Channel);
void send_close_channel(TConnection *Connection, int Channel);
void send_message(TConnection *Connection, int Mode, const char *Text, ...) ATTR_PRINTF(3, 4);
void send_snapback(TConnection *Connection);
void send_outfit(TConnection *Connection);
void send_buddy_data(TConnection *Connection, uint32 CharacterID, const char *Name, bool Online);
void send_buddy_status(TConnection *Connection, uint32 CharacterID, bool Online);
void send_challenge(TConnection *Connection);
void broadcast_message(int Mode, const char *Text, ...) ATTR_PRINTF(2, 3);
void create_gamemaster_request(const char *Name, const char *Text);
void delete_gamemaster_request(const char *Name);
void init_sending(void);
void exit_sending(void);

#endif // TIBIA_PROTOCOL_SENDING_H_
