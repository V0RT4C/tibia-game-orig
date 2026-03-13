// src/network/connection_pool/connection_pool.cpp
#include "network/connection_pool/connection_pool.h"
#include "connections.h"
#include "threads/semaphore/semaphore.h"

static Semaphore ConnectionMutex(1);
static int ConnectionIterator;
static TConnection Connections[MAX_CONNECTIONS];

TConnection *assign_free_connection(void){
	TConnection *Connection = NULL;
	ConnectionMutex.down();
	for(int i = 0; i < MAX_CONNECTIONS; i += 1){
		if(Connections[i].State == CONNECTION_FREE){
			Connection = &Connections[i];
			Connection->assign();
			break;
		}
	}
	ConnectionMutex.up();
	return Connection;
}

TConnection *get_first_connection(void){
	ConnectionIterator = 0;
	return get_next_connection();
}

TConnection *get_next_connection(void){
	TConnection *NextConnection = NULL;
	while(ConnectionIterator < MAX_CONNECTIONS){
		TConnection *Connection = &Connections[ConnectionIterator];
		ConnectionIterator += 1;
		if(Connection->State != CONNECTION_FREE){
			NextConnection = Connection;
			break;
		}
	}
	return NextConnection;
}

void process_connections(void){
	TConnection *Connection = get_first_connection();
	while(Connection != NULL){
		Connection->process();
		Connection = get_next_connection();
	}
}

void init_connections(void){
	init_sending();
	init_receiving();

	ConnectionIterator = 0;
	for(int i = 0; i < MAX_CONNECTIONS; i += 1){
		Connections[i].State = CONNECTION_FREE;
	}
}

void exit_connections(void){
	exit_sending();
	exit_receiving();
}
