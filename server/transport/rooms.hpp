// rooms.hpp — room registry + per-room transport metadata.
//
// The registry owns the IGameRoom instance via an injected factory (so tests use
// FakeRoom and main uses the real GameRoom — this is the one core-swap point).
// All game rules live in the core; the structs here are transport mirrors,
// refreshed from each RoomUpdate the core emits.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/config.hpp"
#include "protocol.hpp"
#include "transport/itransport.hpp"
#include "transport/ratelimit.hpp"

namespace mg::tx {

using RoomFactory = std::function<std::unique_ptr<mg::IGameRoom>(const mg::GameSettings&)>;

struct PlayerSlot {
    mg::PlayerId id = mg::kNoPlayer;
    ConnId conn = kNoConn;
    std::string name;
    std::string sessionToken;
    bool connected = false;
    bool spectator = false;            // refreshed from RoomUpdate
    uint64_t graceDeadlineMs = 0;      // disconnect grace deadline (0 = none)
};

struct RoomCtx {
    RoomKey code;
    std::unique_ptr<mg::IGameRoom> game;
    mg::GameSettings settings;         // mirror, seeded at create, refreshed from RoomUpdate
    std::string password;

    std::unordered_map<mg::PlayerId, PlayerSlot> players;
    std::unordered_set<mg::PlayerId> spectators;
    mg::PlayerId nextPlayerId = 1;     // 0 reserved (kNoPlayer)

    // mirrors refreshed from RoomUpdate (no rule-bearing use)
    mg::Phase phase = mg::Phase::Lobby;
    mg::PlayerId walkerId = mg::kNoPlayer;
    mg::PlayerId hostId = mg::kNoPlayer;

    int connectedCount = 0;
    uint64_t lastActivityMs = 0;
    RateState rates;
    std::string lastRoomUpdateJson;    // cached for reconnect resync

    PlayerSlot* find(mg::PlayerId id) {
        auto it = players.find(id);
        return it == players.end() ? nullptr : &it->second;
    }

    // Law of Demeter: callers ask the room directly instead of reaching through
    // its internal player map / nested settings (e.g. room.find(id)->name or
    // room.settings.grid.cols).
    std::string_view nameOf(mg::PlayerId id) const {
        auto it = players.find(id);
        return it == players.end() ? std::string_view{} : std::string_view{it->second.name};
    }
    int cols() const { return settings.grid.cols; }
};

class RoomRegistry {
public:
    explicit RoomRegistry(RoomFactory factory)
        : factory_(std::move(factory)), codeRng_(std::random_device{}()) {}

    std::string create(const mg::GameSettings& initial, std::string password, uint64_t nowMs);
    RoomCtx* lookup(const RoomKey& code);
    bool exists(const RoomKey& code) const { return rooms_.count(code) != 0; }
    void destroy(const RoomKey& code) { rooms_.erase(code); }

    // Allocates a PlayerId + a PlayerSlot (not yet bound to a socket). Returns the
    // new id, or nullopt if the room is full / missing. spectator is set from the
    // current mirror phase (mid-game join ⇒ spectator, spec §16).
    std::optional<mg::PlayerId> addPlayer(const RoomKey& code, const std::string& name);

private:
    std::string generateCode();
    RoomFactory factory_;
    std::mt19937 codeRng_;
    std::unordered_map<RoomKey, std::unique_ptr<RoomCtx>> rooms_;
};

}  // namespace mg::tx
