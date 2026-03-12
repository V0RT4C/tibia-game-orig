#ifndef GAMESERVER_QUERY_CONNECTION_POOL_H
#define GAMESERVER_QUERY_CONNECTION_POOL_H

#include "query/query_client/query_client.h"
#include "threads/semaphore/semaphore.h"

struct QueryConnectionPool {
	QueryConnectionPool(int Connections);
	void init(void);
	void exit(void);
	QueryClient *getConnection(void);
	void releaseConnection(QueryClient *Connection);

	// DATA
	// =================
	int NumberOfConnections;
	QueryClient *QueryManagerConnection;
	bool *QueryManagerConnectionFree;
	Semaphore FreeQueryManagerConnections;
	Semaphore QueryManagerConnectionMutex;
};

struct QueryPoolConnection {
	QueryPoolConnection(QueryConnectionPool *Pool);
	~QueryPoolConnection(void);

	// TODO(fusion): I don't know if this was the indended way to access this
	// structure but it is only a small wrapper and accessing members directly
	// would be too verbose, specially when it is also named `QueryManagerConnection`.

	QueryClient *operator->(void){
		return this->QueryManagerConnection;
	}

	operator bool(void) const {
		return this->QueryManagerConnection != NULL;
	}

	// DATA
	// =================
	QueryConnectionPool *QueryManagerConnectionPool;
	QueryClient *QueryManagerConnection;
};

#endif // GAMESERVER_QUERY_CONNECTION_POOL_H
