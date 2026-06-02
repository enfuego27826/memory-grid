// sessions.hpp — reconnect tokens + the connId↔player binding the core never sees.
//
// A session token (opaque hex) maps to {room, player}. On reconnect the transport
// rebinds the new socket to the existing PlayerId WITHOUT telling the core (build
// brief §6: reconnection identity is transport-only; the core never observes a
// socket swap). The byConn_ map is the live binding used by dispatch.
#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>

#include "protocol.hpp"
#include "transport/itransport.hpp"

namespace mg::tx {

class SessionStore {
public:
    SessionStore() : rng_(std::random_device{}()) {}

    std::string issue(const RoomKey& room, mg::PlayerId player) {
        std::string token = randomHex(16);
        byToken_[token] = Session{room, player};
        return token;
    }
    struct Session { RoomKey room; mg::PlayerId player; };
    std::optional<Session> resolve(std::string_view token) const {
        if (auto it = byToken_.find(std::string(token)); it != byToken_.end()) return it->second;
        return std::nullopt;
    }
    void revoke(std::string_view token) { byToken_.erase(std::string(token)); }

    // live socket binding
    void bind(ConnId c, const RoomKey& room, mg::PlayerId player) { byConn_[c] = {room, player}; }
    void unbind(ConnId c) { byConn_.erase(c); }
    struct Bound { RoomKey room; mg::PlayerId player; };
    std::optional<Bound> bound(ConnId c) const {
        if (auto it = byConn_.find(c); it != byConn_.end()) return it->second;
        return std::nullopt;
    }

private:
    std::string randomHex(int bytes) {
        static const char* hex = "0123456789abcdef";
        std::uniform_int_distribution<int> d(0, 15);
        std::string s;
        s.reserve(bytes * 2);
        for (int i = 0; i < bytes * 2; ++i) s.push_back(hex[d(rng_)]);
        return s;
    }
    std::mt19937 rng_;
    std::unordered_map<std::string, Session> byToken_;
    std::unordered_map<ConnId, Bound> byConn_;
};

}  // namespace mg::tx
