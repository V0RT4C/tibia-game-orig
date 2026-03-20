#include "protocol/websocket_acceptor/websocket_acceptor.h"
#include "protocol/websocket_acceptor/websocket_types.h"
#include "protocol/communication/communication.h"
#include "network/transport/websocket_transport/websocket_transport.h"
#include "config/config.h"
#include "threads/threads.h"
#include "shm/shm.h"
#include "logging/logging.h"

#include <App.h>
#include <signal.h>
#include <cstring>
#include <atomic>

static us_listen_socket_t *ListenSocket = nullptr;
static ThreadHandle WebSocketThread = INVALID_THREAD_HANDLE;
static std::atomic<uWS::Loop *> EventLoop{nullptr};
static uWS::App *AppInstance = nullptr;

static int websocket_thread_loop(void *Unused) {
    (void)Unused;

    uWS::App app;
    AppInstance = &app;
    EventLoop.store(uWS::Loop::get(), std::memory_order_release);

    // All uWS callbacks (.open, .message, .close) execute on this single event
    // loop thread, so PerSocketData access is safe without synchronization.
    app.ws<PerSocketData>("/*", {
        .compression = uWS::DISABLED,
        .maxPayloadLength = 16 * 1024,
        .idleTimeout = 0,
        .maxBackpressure = 64 * 1024,

        .open = [](auto *ws) {
            if (get_active_connections() >= MAX_COMMUNICATION_THREADS) {
                print(3, "WebSocket: No more connections available.\n");
                ws->close();
                return;
            }

            char RemoteIP[16] = "Unknown";
            auto addr = ws->getRemoteAddressAsText();
            if (addr.length() > 0 && addr.length() < sizeof(RemoteIP)) {
                memcpy(RemoteIP, addr.data(), addr.length());
                RemoteIP[addr.length()] = '\0';
            }

            void *loop = uWS::Loop::get();
            WebSocketTransport *Transport = new WebSocketTransport((void *)ws, loop, RemoteIP);
            if (!Transport->is_connected()) {
                error("WebSocket: Failed to create transport.\n");
                delete Transport;
                ws->close();
                return;
            }

            auto *data = ws->getUserData();
            data->Transport = Transport;

            increment_active_connections();
            ThreadHandle ConnThread = StartThread(handle_ws_connection, (void *)Transport,
                                                  COMMUNICATION_THREAD_STACK_SIZE, true);
            if (ConnThread == INVALID_THREAD_HANDLE) {
                error("WebSocket: Cannot create communication thread.\n");
                decrement_active_connections();
                delete Transport;
                data->Transport = nullptr;
                ws->close();
                return;
            }
        },

        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            if (opCode != uWS::OpCode::BINARY) return;

            auto *data = ws->getUserData();
            if (!data->Transport) return;

            if (!data->Transport->push_inbound((const uint8 *)message.data(), (int)message.length())) {
                error("WebSocket: Inbound buffer full for %s, closing.\n",
                      data->Transport->get_remote_address());
                ws->close();
            }
        },

        .close = [](auto *ws, int code, std::string_view message) {
            auto *data = ws->getUserData();
            if (!data->Transport) return;

            data->Transport->close();
            // Mark the WebSocket as gone so deferred_delete() knows not to
            // access the (soon-to-be-freed) WebSocket object.
            data->Transport->clear_ws();
            pid_t tid = data->Transport->get_thread_id();
            if (tid != 0) {
                tgkill(GetGameProcessID(), tid, SIGHUP);
            }
            data->Transport = nullptr;
        },
    });

    app.listen(WebSocketAddress, WebSocketPort, [](auto *listen_socket) {
        if (listen_socket) {
            ListenSocket = listen_socket;
            print(1, "WebSocket listening on %s:%d\n", WebSocketAddress, WebSocketPort);
        } else {
            error("WebSocket: Failed to listen on %s:%d\n", WebSocketAddress, WebSocketPort);
        }
    });

    app.run();
    AppInstance = nullptr;
    EventLoop.store(nullptr, std::memory_order_release);
    return 0;
}

void start_websocket_thread(void) {
    WebSocketThread = StartThread(websocket_thread_loop, NULL, false);
    if (WebSocketThread == INVALID_THREAD_HANDLE) {
        throw "cannot start WebSocket thread";
    }
}

void stop_websocket_thread(void) {
    uWS::Loop *loop = EventLoop.load(std::memory_order_acquire);
    if (loop) {
        loop->defer([]() {
            if (ListenSocket) {
                us_listen_socket_close(0, ListenSocket);
                ListenSocket = nullptr;
            }
            if (AppInstance) {
                AppInstance->close();
            }
        });
    }

    if (WebSocketThread != INVALID_THREAD_HANDLE) {
        JoinThread(WebSocketThread);
        WebSocketThread = INVALID_THREAD_HANDLE;
    }
}
