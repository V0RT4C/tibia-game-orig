// src/protocol/receiving/receiving.h
#ifndef TIBIA_PROTOCOL_RECEIVING_H_
#define TIBIA_PROTOCOL_RECEIVING_H_ 1

#include "network/connection/connection.h"
#include "protocol/protocol_enums.h"

void ReceiveData(TConnection *Connection);
void ReceiveData(void);
void InitReceiving(void);
void ExitReceiving(void);

#endif // TIBIA_PROTOCOL_RECEIVING_H_
