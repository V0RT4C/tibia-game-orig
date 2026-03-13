#ifndef TIBIA_CONNECTIONS_H_
#define TIBIA_CONNECTIONS_H_ 1

#include "network/connection/connection.h"
#include "network/connection_pool/connection_pool.h"
#include "protocol/protocol_enums.h"
#include "protocol/sending/sending.h"
#include "map.h"

// receiving.cc
void ReceiveData(TConnection *Connection);
void ReceiveData(void);
void InitReceiving(void);
void ExitReceiving(void);

#endif // TIBIA_CONNECTIONS_H_
