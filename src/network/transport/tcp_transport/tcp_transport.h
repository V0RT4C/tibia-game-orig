// src/network/transport/tcp_transport/tcp_transport.h
#ifndef TIBIA_NETWORK_TCP_TRANSPORT_H_
#define TIBIA_NETWORK_TCP_TRANSPORT_H_ 1

#include "network/transport/transport.h"

class TcpTransport : public ITransport {
public:
    TcpTransport(int socket_fd, const char* remote_address);
    ~TcpTransport() override;

    int write(const uint8* buffer, int size) override;
    int read(uint8* buffer, int size) override;
    void close() override;
    const char* get_remote_address() const override;
    int get_fd() const override;
    bool is_connected() const override;

private:
    int socket_fd_;
    char remote_address_[16];
    bool connected_;
};

#endif // TIBIA_NETWORK_TCP_TRANSPORT_H_
