#ifndef TIBIA_PROTOCOL_WEBSOCKET_TYPES_H_
#define TIBIA_PROTOCOL_WEBSOCKET_TYPES_H_ 1

class WebSocketTransport;

struct PerSocketData {
    WebSocketTransport *Transport;
};

#endif // TIBIA_PROTOCOL_WEBSOCKET_TYPES_H_
