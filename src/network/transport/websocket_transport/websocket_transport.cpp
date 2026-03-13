#include "network/transport/websocket_transport/websocket_transport.h"
#include "logging/logging.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#include <App.h>

// NOTE: PerSocketData is defined in websocket_acceptor.cpp. We don't need the
// full type here — the ws_ pointer is cast to a generic uWS::WebSocket type
// using a minimal local struct for the template parameter.
struct WsPerSocketData {};

WebSocketTransport::WebSocketTransport(void *ws, void *event_loop, const char *remote_address)
    : ws_(ws), event_loop_(event_loop), event_fd_(-1), connected_(true), thread_id_(0) {
    strncpy(remote_address_, remote_address, sizeof(remote_address_) - 1);
    remote_address_[sizeof(remote_address_) - 1] = '\0';

    event_fd_ = eventfd(0, EFD_NONBLOCK);
    if (event_fd_ == -1) {
        error("WebSocketTransport: Failed to create eventfd: (%d) %s\n", errno, strerror(errno));
        connected_ = false;
        return;
    }
}

WebSocketTransport::~WebSocketTransport() {
    if (event_fd_ != -1) {
        ::close(event_fd_);
        event_fd_ = -1;
    }
}

int WebSocketTransport::write(const uint8 *buffer, int size) {
    if (!connected_.load(std::memory_order_acquire) || event_fd_ == -1) {
        errno = EPIPE;
        return -1;
    }

    if (!out_buffer_.write(buffer, size)) {
        error("WebSocketTransport: Outbound ring buffer full, dropping %d bytes\n", size);
        errno = EAGAIN;
        return -1;
    }

    // Wake the event loop thread to drain outbound data and send it.
    // uWS::Loop::defer() is documented as thread-safe.
    if (event_loop_) {
        ((uWS::Loop *)event_loop_)->defer([this]() {
            if (!connected_.load(std::memory_order_acquire)) return;

            uint8 drain_buf[16384];
            int n = out_buffer_.read(drain_buf, sizeof(drain_buf));
            while (n > 0) {
                auto *ws = (uWS::WebSocket<false, true, WsPerSocketData> *)ws_;
                auto status = ws->send(std::string_view((char *)drain_buf, n), uWS::OpCode::BINARY);
                if (status == uWS::WebSocket<false, true, WsPerSocketData>::DROPPED) {
                    error("WebSocketTransport: Send dropped by uWS (backpressure)\n");
                    break;
                }
                n = out_buffer_.read(drain_buf, sizeof(drain_buf));
            }
        });
    }

    return size;
}

int WebSocketTransport::read(uint8 *buffer, int size) {
    if (!connected_.load(std::memory_order_acquire)) {
        return -1;
    }

    // Drain the eventfd counter to prevent spurious SIGIO wakeups.
    uint64_t val;
    ssize_t ignored __attribute__((unused)) = ::read(event_fd_, &val, sizeof(val));

    int n = in_buffer_.read(buffer, size);
    if (n == 0) {
        errno = EAGAIN;
        return -1;
    }
    return n;
}

void WebSocketTransport::close() {
    connected_.store(false, std::memory_order_release);
}

const char *WebSocketTransport::get_remote_address() const {
    return remote_address_;
}

int WebSocketTransport::get_fd() const {
    return event_fd_;
}

bool WebSocketTransport::is_connected() const {
    return connected_.load(std::memory_order_acquire);
}

bool WebSocketTransport::push_inbound(const uint8 *data, int len) {
    if (!in_buffer_.write(data, len)) {
        return false;
    }

    // Signal the connection thread via eventfd — triggers SIGIO.
    uint64_t val = 1;
    ssize_t ignored __attribute__((unused)) = ::write(event_fd_, &val, sizeof(val));
    return true;
}

int WebSocketTransport::drain_outbound(uint8 *out, int max_len) {
    return out_buffer_.read(out, max_len);
}

void *WebSocketTransport::get_event_loop() const {
    return event_loop_;
}

void *WebSocketTransport::get_ws() const {
    return ws_;
}

void WebSocketTransport::set_thread_id(pid_t tid) {
    thread_id_.store(tid, std::memory_order_release);
}

pid_t WebSocketTransport::get_thread_id() const {
    return thread_id_.load(std::memory_order_acquire);
}
