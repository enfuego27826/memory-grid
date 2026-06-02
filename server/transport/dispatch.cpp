#include "transport/dispatch.hpp"

#include <nlohmann/json.hpp>

using namespace mg;

namespace mg::tx {

Dispatcher::Dispatcher(RoomRegistry& registry, SessionStore& sessions, Clock clock)
    : registry_(registry), sessions_(sessions), now_(std::move(clock)) {}

// ---------------------------------------------------------------------------
// HTTP: POST /create, POST /join
// ---------------------------------------------------------------------------
HttpResponse Dispatcher::onHttp(const HttpRequest& req) {
    if (req.method == "POST" && req.path == "/create") {
        auto params = wire::parseCreate(req.body);
        std::string code = registry_.create(params.settings, params.password, now_());
        return {200, "application/json", nlohmann::json{{"code", code}}.dump()};
    }
    if (req.method == "POST" && req.path == "/join") {
        auto h = wire::parseJoinHttp(req.body);
        RoomCtx* room = registry_.lookup(h.code);
        if (!room) return {404, "application/json", R"({"error":"no_such_room"})"};
        if (!room->password.empty() && room->password != h.password)
            return {403, "application/json", R"({"error":"bad_password"})"};
        return {200, "application/json", nlohmann::json{{"ok", true}, {"code", h.code}}.dump()};
    }
    return {404, "application/json", R"({"error":"not_found"})"};
}

void Dispatcher::onOpen(ConnId) { /* unbound until join_room */ }

// ---------------------------------------------------------------------------
// WS message pipeline
// ---------------------------------------------------------------------------
void Dispatcher::onMessage(ConnId connId, std::string_view bytes) {
    if (bytes.size() > kMaxPayload) return;  // trust boundary: oversized

    auto bound = sessions_.bound(connId);
    if (!bound) {
        // Unbound sockets may only join (or reconnect via token).
        if (auto ji = wire::parseJoin(bytes)) handleJoin(connId, *ji);
        return;
    }
    RoomCtx* room = registry_.lookup(bound->room);
    if (!room) return;
    const PlayerId pid = bound->player;

    auto parsed = wire::parseCommand(bytes);
    if (!parsed) return;  // malformed / unknown / bad fields — never reaches core
    const Command& cmd = *parsed;

    if (std::holds_alternative<JoinRoom>(cmd)) return;  // already joined; ignore re-join

    // Chat rate-limit is enforced BEFORE the core sees a SendChat.
    if (std::holds_alternative<SendChat>(cmd)) {
        bool muteStarted = false;
        if (!room->rates.allow(pid, now_(), room->settings.chatRateLimitMs, &muteStarted)) {
            if (muteStarted)
                transport_->sendOne(connId, wire::serializeNotice("You are muted for spamming."));
            return;  // dropped silently
        }
    }

    applyAndFlush(*room, pid, cmd);
}

// ---------------------------------------------------------------------------
// Join / reconnect
// ---------------------------------------------------------------------------
void Dispatcher::handleJoin(ConnId connId, const wire::JoinInfo& ji) {
    // Reconnect path: a valid token rebinds to the existing player.
    if (!ji.token.empty()) {
        if (auto s = sessions_.resolve(ji.token)) { rebind(connId, *s); return; }
        // unknown token → fall through to a fresh join
    }
    RoomCtx* room = registry_.lookup(ji.code);
    if (!room) { transport_->sendOne(connId, wire::serializeNotice("No such room.")); return; }
    if (!room->password.empty() && room->password != ji.password) {
        transport_->sendOne(connId, wire::serializeNotice("Wrong password."));
        return;
    }
    auto idOpt = registry_.addPlayer(ji.code, ji.name);
    if (!idOpt) { transport_->sendOne(connId, wire::serializeNotice("Room is full.")); return; }
    const PlayerId id = *idOpt;

    std::string token = sessions_.issue(ji.code, id);
    PlayerSlot* slot = room->find(id);
    slot->sessionToken = token;
    slot->conn = connId;
    slot->connected = true;
    ++room->connectedCount;
    sessions_.bind(connId, ji.code, id);
    transport_->subscribe(connId, ji.code);
    transport_->cancelTimer(ji.code, TEARDOWN_TIMER_ID);

    transport_->sendOne(connId, wire::serializeJoined(id, token));
    applyAndFlush(*room, id, JoinRoom{ji.name});
}

void Dispatcher::rebind(ConnId connId, const SessionStore::Session& s) {
    RoomCtx* room = registry_.lookup(s.room);
    if (!room) { transport_->sendOne(connId, wire::serializeNotice("Room gone.")); return; }
    PlayerSlot* slot = room->find(s.player);
    if (!slot) { transport_->sendOne(connId, wire::serializeNotice("Seat gone.")); return; }

    slot->conn = connId;
    if (!slot->connected) { slot->connected = true; ++room->connectedCount; }
    slot->graceDeadlineMs = 0;
    sessions_.bind(connId, s.room, s.player);
    transport_->subscribe(connId, s.room);
    transport_->cancelTimer(s.room, GRACE_TIMER_BASE + s.player);
    transport_->cancelTimer(s.room, TEARDOWN_TIMER_ID);

    transport_->sendOne(connId, wire::serializeJoined(s.player, slot->sessionToken));
    // Resync without involving the core: replay the cached last RoomUpdate.
    if (!room->lastRoomUpdateJson.empty())
        transport_->sendOne(connId, room->lastRoomUpdateJson);
}

// ---------------------------------------------------------------------------
// Apply a command and flush the resulting events + timers
// ---------------------------------------------------------------------------
void Dispatcher::applyAndFlush(RoomCtx& room, PlayerId from, const Command& cmd) {
    StepResult res = room.game->apply(from, cmd, now_());
    deliver(room, res.out);
    applyTimers(room, res.timers);
    room.lastActivityMs = now_();
}

void Dispatcher::deliver(RoomCtx& room, const std::vector<Event>& events) {
    auto nameOf = [&room](PlayerId id) -> std::string_view {
        if (PlayerSlot* s = room.find(id)) return s->name;
        return {};
    };
    for (const Event& e : events) {
        if (const auto* ru = std::get_if<RoomUpdate>(&e.body)) refreshMirror(room, *ru);
        std::string msg = wire::serializeEvent(e.body, nameOf, room.settings.grid.cols);
        switch (e.reach) {
            case Reach::Broadcast:
                transport_->publish(room.code, msg);
                break;
            case Reach::One:
                if (PlayerSlot* s = room.find(e.who); s && s->connected)
                    transport_->sendOne(s->conn, msg);
                break;
            case Reach::AllExcept:
                for (auto& [pid, slot] : room.players)
                    if (pid != e.who && slot.connected) transport_->sendOne(slot.conn, msg);
                break;
            case Reach::Spectators:
                for (PlayerId pid : room.spectators)
                    if (PlayerSlot* s = room.find(pid); s && s->connected)
                        transport_->sendOne(s->conn, msg);
                break;
        }
    }
}

void Dispatcher::refreshMirror(RoomCtx& room, const RoomUpdate& ru) {
    room.settings = ru.settings;
    room.phase = ru.state;
    room.spectators.clear();
    room.walkerId = kNoPlayer;
    room.hostId = kNoPlayer;
    for (const auto& pv : ru.players) {
        if (PlayerSlot* s = room.find(pv.id)) { s->name = pv.name; s->spectator = pv.isSpectator; }
        if (pv.isSpectator) room.spectators.insert(pv.id);
        if (pv.isWalker) room.walkerId = pv.id;
        if (pv.isHost) room.hostId = pv.id;
    }
    // Cache for reconnect resync (build the same wire bytes).
    auto nameOf = [&room](PlayerId id) -> std::string_view {
        if (PlayerSlot* s = room.find(id)) return s->name;
        return {};
    };
    room.lastRoomUpdateJson = wire::serializeEvent(ru, nameOf, room.settings.grid.cols);
}

void Dispatcher::applyTimers(RoomCtx& room, const std::vector<TimerOp>& timers) {
    for (const auto& t : timers) {
        if (const auto* st = std::get_if<StartTimer>(&t))
            transport_->scheduleTimer(room.code, st->id, st->delayMs);
        else if (const auto* ct = std::get_if<CancelTimer>(&t))
            transport_->cancelTimer(room.code, ct->id);
    }
}

// ---------------------------------------------------------------------------
// Disconnect + timers
// ---------------------------------------------------------------------------
void Dispatcher::onClose(ConnId connId) {
    auto bound = sessions_.bound(connId);
    sessions_.unbind(connId);
    if (!bound) return;
    RoomCtx* room = registry_.lookup(bound->room);
    if (!room) return;
    PlayerSlot* slot = room->find(bound->player);
    if (!slot || slot->conn != connId) return;  // already rebound to a newer socket

    slot->connected = false;
    slot->conn = kNoConn;
    if (room->connectedCount > 0) --room->connectedCount;

    if (room->connectedCount == 0) {
        // Last one out: 60s teardown, no per-player PlayerLeft storm.
        transport_->scheduleTimer(room->code, TEARDOWN_TIMER_ID,
                                  static_cast<uint32_t>(mg::limits::kEmptyRoomTeardownMs));
        return;
    }
    // Others remain: grace window before telling the core the player left.
    const bool isWalker = (bound->player == room->walkerId && room->phase == Phase::Walk);
    const uint64_t grace = isWalker ? kGraceWalkerMs : kGraceNonWalkerMs;
    slot->graceDeadlineMs = now_() + grace;
    transport_->scheduleTimer(room->code, GRACE_TIMER_BASE + bound->player,
                              static_cast<uint32_t>(grace));
}

void Dispatcher::onTimer(const RoomKey& roomKey, uint32_t timerId) {
    RoomCtx* room = registry_.lookup(roomKey);
    if (!room) return;

    if (timerId == TEARDOWN_TIMER_ID) {
        if (room->connectedCount == 0) registry_.destroy(roomKey);
        return;
    }
    if (timerId >= GRACE_TIMER_BASE) {
        const PlayerId player = timerId - GRACE_TIMER_BASE;
        PlayerSlot* slot = room->find(player);
        if (slot && !slot->connected) firePlayerLeft(*room, player);
        return;
    }
    // Otherwise a core game timer.
    StepResult res = room->game->onTimer(timerId, now_());
    deliver(*room, res.out);
    applyTimers(*room, res.timers);
    room->lastActivityMs = now_();
}

void Dispatcher::firePlayerLeft(RoomCtx& room, PlayerId player) {
    StepResult res = room.game->apply(player, PlayerLeft{}, now_());
    // Drop the transport-side slot before delivering (so routing skips it).
    room.players.erase(player);
    room.spectators.erase(player);
    room.rates.forget(player);
    deliver(room, res.out);
    applyTimers(room, res.timers);
}

}  // namespace mg::tx
