// src/protocol/receiving/receiving.h
#ifndef TIBIA_PROTOCOL_RECEIVING_H_
#define TIBIA_PROTOCOL_RECEIVING_H_ 1

#include "network/connection/connection.h"
#include "protocol/protocol_enums.h"

void receive_data(TConnection *Connection);
void receive_data(void);
void init_receiving(void);
void exit_receiving(void);

#endif // TIBIA_PROTOCOL_RECEIVING_H_
