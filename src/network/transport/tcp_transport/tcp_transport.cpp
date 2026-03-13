// src/network/transport/tcp_transport/tcp_transport.cpp
#include "network/transport/tcp_transport/tcp_transport.h"

#include <unistd.h>
#include <cstring>

TcpTransport::TcpTransport(int socket_fd, const char* remote_address)
    : socket_fd_(socket_fd)
    , connected_(true)
{
    strncpy(remote_address_, remote_address, sizeof(remote_address_) - 1);
    remote_address_[sizeof(remote_address_) - 1] = '\0';
}

TcpTransport::~TcpTransport() {
    // NOTE: Does NOT close the socket — ownership is with the caller
    // (CommunicationThread closes the socket after Connection::Free).
}

int TcpTransport::write(const uint8* buffer, int size) {
    if (!connected_ || socket_fd_ == -1) return -1;
    return static_cast<int>(::write(socket_fd_, buffer, size));
}

int TcpTransport::read(uint8* buffer, int size) {
    if (!connected_ || socket_fd_ == -1) return -1;
    return static_cast<int>(::read(socket_fd_, buffer, size));
}

void TcpTransport::close() {
    connected_ = false;
    socket_fd_ = -1;
}

const char* TcpTransport::get_remote_address() const {
    return remote_address_;
}

int TcpTransport::get_fd() const {
    return socket_fd_;
}

bool TcpTransport::is_connected() const {
    return connected_;
}
