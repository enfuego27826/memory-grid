// itransport.hpp — the thin transport seam that hides the WebSocket/HTTP library.
//
// Everything above this interface (dispatch, rooms, wire, sessions, ratelimit)
// touches ONLY ConnId / RoomKey / string_view — never a uWS type. uWebSockets
// implements ITransport in uws_transport.cpp (the one swappable file); the tests
// implement it as FakeTransport. Swapping WS libraries touches one file.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mg::tx {

using ConnId = uint64_t;          // issued by the transport on WS open; 0 == invalid
using RoomKey = std::string;      // the 6-char room code; also the pub/sub topic
constexpr ConnId kNoConn = 0;

struct HttpRequest  { std::string method; std::string path; std::string body; };
struct HttpResponse { int status = 200; std::string contentType = "application/json"; std::string body; };

// The transport calls these back into the application (Dispatcher implements it).
class ITransportCallbacks {
public:
    virtual ~ITransportCallbacks() = default;
    virtual HttpResponse onHttp(const HttpRequest&) = 0;                  // POST /create, /join
    virtual void onOpen(ConnId) = 0;                                     // ws connected, unbound
    virtual void onMessage(ConnId, std::string_view bytes) = 0;
    virtual void onClose(ConnId) = 0;
    virtual void onTimer(const RoomKey&, uint32_t timerId) = 0;          // a scheduled timer fired
};

// The application calls these into the transport.
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual void run(uint16_t port) = 0;                                 // blocks: single event loop

    virtual void sendOne(ConnId, std::string_view bytes) = 0;
    virtual void closeOne(ConnId) = 0;

    virtual void subscribe(ConnId, const RoomKey&) = 0;
    virtual void unsubscribe(ConnId, const RoomKey&) = 0;
    virtual void publish(const RoomKey&, std::string_view bytes) = 0;

    // One-shot timer keyed by (room, timerId); scheduling the same key replaces it.
    virtual void scheduleTimer(const RoomKey&, uint32_t timerId, uint32_t delayMs) = 0;
    virtual void cancelTimer(const RoomKey&, uint32_t timerId) = 0;
};

}  // namespace mg::tx
