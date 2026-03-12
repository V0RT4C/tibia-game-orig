#ifndef TIBIA_QUERY_H_
#define TIBIA_QUERY_H_ 1

#include "query/query_types/query_types.h"
#include "query/query_client/query_client.h"
#include "query/query_connection_pool/query_connection_pool.h"

using TQueryManagerConnection = QueryClient;
using TQueryManagerConnectionPool = QueryConnectionPool;
using TQueryManagerPoolConnection = QueryPoolConnection;

#endif //TIBIA_QUERY_H_
