// src/network/connection_pool/connection_pool.h
#ifndef TIBIA_NETWORK_CONNECTION_POOL_H_
#define TIBIA_NETWORK_CONNECTION_POOL_H_ 1

#include "network/connection/connection.h"

#define MAX_CONNECTIONS 1100

TConnection *assign_free_connection(void);
TConnection *get_first_connection(void);
TConnection *get_next_connection(void);
void process_connections(void);
void init_connections(void);
void exit_connections(void);

#endif // TIBIA_NETWORK_CONNECTION_POOL_H_
