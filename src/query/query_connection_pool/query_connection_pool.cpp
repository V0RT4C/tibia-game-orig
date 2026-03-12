#include "query/query_connection_pool/query_connection_pool.h"
#include "common/assert/assert.h"
#include "logging/logging.h"

#include <algorithm>
#include <stddef.h>

// QueryConnectionPool
// =============================================================================
// TODO(fusion): Same as `QueryClient::QueryClient`.
QueryConnectionPool::QueryConnectionPool(int Connections) :
		NumberOfConnections(std::max<int>(Connections, 1)),
		QueryManagerConnection(NULL),
		QueryManagerConnectionFree(NULL),
		FreeQueryManagerConnections(this->NumberOfConnections),
		QueryManagerConnectionMutex(1)
{
	if(Connections <= 0){
		error("QueryConnectionPool::QueryConnectionPool:"
				" Invalid connection count %d.\n", Connections);
	}
}

void QueryConnectionPool::init(void){
	this->QueryManagerConnection = new QueryClient[this->NumberOfConnections];
	this->QueryManagerConnectionFree = new bool[this->NumberOfConnections];
	for(int i = 0; i < this->NumberOfConnections; i += 1){
		if(!this->QueryManagerConnection[i].isConnected()){
			error("QueryConnectionPool::init: Cannot connect to query manager.\n");
			throw "cannot connect to query manager";
		}

		this->QueryManagerConnectionFree[i] = true;
	}
}

void QueryConnectionPool::exit(void){
	for(int i = 0; i < this->NumberOfConnections; i += 1){
		this->FreeQueryManagerConnections.down();
	}

	delete[] this->QueryManagerConnection;
	delete[] this->QueryManagerConnectionFree;
}

QueryClient *QueryConnectionPool::getConnection(void){
	int ConnectionIndex = -1;
	this->FreeQueryManagerConnections.down();
	this->QueryManagerConnectionMutex.down();
	for(int i = 0; i < this->NumberOfConnections; i += 1){
		if(this->QueryManagerConnectionFree[i]){
			this->QueryManagerConnectionFree[i] = false;
			ConnectionIndex = i;
			break;
		}
	}
	this->QueryManagerConnectionMutex.up();

	if(ConnectionIndex == -1){
		error("QueryConnectionPool::getConnection: No free connection found.\n");
		return NULL;
	}

	return &this->QueryManagerConnection[ConnectionIndex];
}

void QueryConnectionPool::releaseConnection(QueryClient *Connection){
	int ConnectionIndex = -1;
	for(int i = 0; i < this->NumberOfConnections; i += 1){
		if(&this->QueryManagerConnection[i] == Connection){
			ConnectionIndex = i;
			break;
		}
	}

	if(ConnectionIndex == -1){
		error("QueryConnectionPool::releaseConnection: Connection not found.\n");
		return;
	}

	this->QueryManagerConnectionFree[ConnectionIndex] = true;
	this->FreeQueryManagerConnections.up();
}

// QueryPoolConnection
// =============================================================================
QueryPoolConnection::QueryPoolConnection(QueryConnectionPool *Pool) :
		QueryManagerConnectionPool(Pool),
		QueryManagerConnection(NULL)
{
	if(this->QueryManagerConnectionPool == NULL){
		error("QueryPoolConnection::QueryPoolConnection: Pool is NULL.\n");
		return;
	}

	this->QueryManagerConnection = this->QueryManagerConnectionPool->getConnection();
}

QueryPoolConnection::~QueryPoolConnection(void){
	if(this->QueryManagerConnection != NULL){
		ASSERT(this->QueryManagerConnectionPool != NULL);
		this->QueryManagerConnectionPool->releaseConnection(this->QueryManagerConnection);
	}
}
