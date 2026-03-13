// src/network/connection_pool/connection_pool.h
#ifndef TIBIA_NETWORK_CONNECTION_POOL_H_
#define TIBIA_NETWORK_CONNECTION_POOL_H_ 1

#include "network/connection/connection.h"

#define MAX_CONNECTIONS 1100

TConnection *AssignFreeConnection(void);
TConnection *GetFirstConnection(void);
TConnection *GetNextConnection(void);
void ProcessConnections(void);
void InitConnections(void);
void ExitConnections(void);

#endif // TIBIA_NETWORK_CONNECTION_POOL_H_
