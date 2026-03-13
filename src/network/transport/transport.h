// src/network/transport/transport.h
#ifndef TIBIA_NETWORK_TRANSPORT_H_
#define TIBIA_NETWORK_TRANSPORT_H_ 1

#include "common/types/types.h"

class ITransport {
  public:
	virtual ~ITransport() = default;
	virtual int write(const uint8 *buffer, int size) = 0;
	virtual int read(uint8 *buffer, int size) = 0;
	virtual void close() = 0;
	virtual const char *get_remote_address() const = 0;
	virtual int get_fd() const = 0;
	virtual bool is_connected() const = 0;
};

#endif // TIBIA_NETWORK_TRANSPORT_H_
