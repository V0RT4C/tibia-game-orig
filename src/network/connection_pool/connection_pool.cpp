// src/network/connection_pool/connection_pool.cpp
#include "network/connection_pool/connection_pool.h"
#include "connections.h"
#include "threads/semaphore/semaphore.h"

static Semaphore ConnectionMutex(1);
static int ConnectionIterator;
static TConnection Connections[MAX_CONNECTIONS];

TConnection *AssignFreeConnection(void){
	TConnection *Connection = NULL;
	ConnectionMutex.down();
	for(int i = 0; i < MAX_CONNECTIONS; i += 1){
		if(Connections[i].State == CONNECTION_FREE){
			Connection = &Connections[i];
			Connection->Assign();
			break;
		}
	}
	ConnectionMutex.up();
	return Connection;
}

TConnection *GetFirstConnection(void){
	ConnectionIterator = 0;
	return GetNextConnection();
}

TConnection *GetNextConnection(void){
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

void ProcessConnections(void){
	TConnection *Connection = GetFirstConnection();
	while(Connection != NULL){
		Connection->Process();
		Connection = GetNextConnection();
	}
}

void InitConnections(void){
	InitSending();
	InitReceiving();

	ConnectionIterator = 0;
	for(int i = 0; i < MAX_CONNECTIONS; i += 1){
		Connections[i].State = CONNECTION_FREE;
	}
}

void ExitConnections(void){
	ExitSending();
	ExitReceiving();
}
