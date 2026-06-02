// dispatch.hpp — the application core of the transport. Implements
// ITransportCallbacks: turns inbound HTTP/WS events into core Commands, routes
// the core's Events back out by Reach, and schedules its TimerOps. This file has
// ZERO uWebSockets dependency (it talks only to ITransport), so it compiles into
// the transport tests against a FakeTransport + FakeRoom.
#pragma once

#include <cstdint>
#include <functional>

#include "protocol.hpp"
#include "transport/itransport.hpp"
#include "transport/rooms.hpp"
#include "transport/sessions.hpp"
#include "transport/wire.hpp"

namespace mg::tx {

// Transport-internal timer ids, kept far from the core's small TimerId range.
constexpr uint32_t TEARDOWN_TIMER_ID = 0x10000000u;
constexpr uint32_t GRACE_TIMER_BASE  = 0x20000000u;  // + PlayerId

// Disconnect grace windows (transport-only; not in core config).
constexpr uint64_t kGraceNonWalkerMs = 15000;
constexpr uint64_t kGraceWalkerMs    = 1500;   // a hung walk should free fast → replay

constexpr std::size_t kMaxPayload = 8 * 1024;

class Dispatcher : public ITransportCallbacks {
public:
    using Clock = std::function<uint64_t()>;
    Dispatcher(RoomRegistry& registry, SessionStore& sessions, Clock clock);

    void setTransport(ITransport* t) { transport_ = t; }

    // ITransportCallbacks
    HttpResponse onHttp(const HttpRequest&) override;
    void onOpen(ConnId) override;
    void onMessage(ConnId, std::string_view bytes) override;
    void onClose(ConnId) override;
    void onTimer(const RoomKey&, uint32_t timerId) override;

private:
    void handleJoin(ConnId, const wire::JoinInfo&);
    void rebind(ConnId, const SessionStore::Session&);
    void applyAndFlush(RoomCtx&, mg::PlayerId from, const mg::Command&);
    void deliver(RoomCtx&, const std::vector<mg::Event>&);
    void applyTimers(RoomCtx&, const std::vector<mg::TimerOp>&);
    void refreshMirror(RoomCtx&, const mg::RoomUpdate&);
    void firePlayerLeft(RoomCtx&, mg::PlayerId);

    RoomRegistry& registry_;
    SessionStore& sessions_;
    ITransport* transport_ = nullptr;
    Clock now_;
};

}  // namespace mg::tx
