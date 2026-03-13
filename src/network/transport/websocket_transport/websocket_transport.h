#ifndef TIBIA_NETWORK_WEBSOCKET_TRANSPORT_H_
#define TIBIA_NETWORK_WEBSOCKET_TRANSPORT_H_ 1

#include "network/transport/transport.h"
#include "common/spsc_ring_buffer/spsc_ring_buffer.h"

#include <atomic>

// Forward declarations — avoid pulling uWebSockets headers into game code.
// The actual uWS types are only needed in the .cpp file.

class WebSocketTransport : public ITransport {
public:
    // Called from the uWebSockets event loop thread.
    WebSocketTransport(void *ws, void *event_loop, const char *remote_address);
    ~WebSocketTransport() override;

    // ITransport interface — called from the connection thread.
    int write(const uint8 *buffer, int size) override;
    int read(uint8 *buffer, int size) override;
    void close() override;
    const char *get_remote_address() const override;
    int get_fd() const override;       // returns eventfd
    bool is_connected() const override;

    // Called from the uWebSockets event loop thread when a message arrives.
    bool push_inbound(const uint8 *data, int len);

    // Called from the uWebSockets event loop thread to drain outbound data.
    // Returns number of bytes drained, writes data into `out`, max `max_len`.
    int drain_outbound(uint8 *out, int max_len);

    // Get the uWS::Loop* for deferred calls. Set during construction.
    void *get_event_loop() const;

    // The raw WebSocket pointer for sending. Only valid on the event loop thread.
    void *get_ws() const;

    // Set from the communication thread so the event loop can signal it.
    void set_thread_id(pid_t tid);
    pid_t get_thread_id() const;

private:
    void *ws_;               // uWS::WebSocket<...>* — opaque to avoid header dep
    void *event_loop_;       // uWS::Loop* — for defer() calls
    int event_fd_;
    char remote_address_[16];
    std::atomic<bool> connected_;
    std::atomic<pid_t> thread_id_;  // set by comm thread, read by event loop for SIGHUP

    SPSCRingBuffer<65536> in_buffer_;   // event loop writes, connection thread reads
    SPSCRingBuffer<65536> out_buffer_;  // connection thread writes, event loop reads
};

#endif // TIBIA_NETWORK_WEBSOCKET_TRANSPORT_H_
