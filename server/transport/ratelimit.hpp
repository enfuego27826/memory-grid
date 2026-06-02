// ratelimit.hpp — per-player chat throttle + mute (transport-only, spec §12).
//
// Enforced BEFORE a SendChat reaches the core (build brief §6). Min-interval
// model: a chat is dropped if it arrives sooner than chatRateLimitMs after the
// last allowed one. More than kMuteDropThreshold consecutive drops → a kMuteMs
// mute. Dropped chats are silent.
#pragma once

#include <cstdint>
#include <unordered_map>

#include "core/config.hpp"
#include "protocol.hpp"

namespace mg::tx {

struct PlayerRate {
    uint64_t lastAllowedMs = 0;
    int consecutiveDrops = 0;
    uint64_t mutedUntilMs = 0;
};

class RateState {
public:
    // Returns true if this chat is allowed; updates counters/mute as a side effect.
    // Also reports (out) whether a mute *just started*, so the caller may notify once.
    bool allow(mg::PlayerId p, uint64_t now, int chatRateLimitMs, bool* muteStarted = nullptr) {
        if (muteStarted) *muteStarted = false;
        PlayerRate& r = byPlayer_[p];
        if (now < r.mutedUntilMs) { ++r.consecutiveDrops; return false; }
        if (r.lastAllowedMs != 0 &&
            now - r.lastAllowedMs < static_cast<uint64_t>(chatRateLimitMs)) {
            if (++r.consecutiveDrops > limits::kMuteDropThreshold) {
                r.mutedUntilMs = now + limits::kMuteMs;
                if (muteStarted) *muteStarted = true;
            }
            return false;
        }
        r.lastAllowedMs = now;
        r.consecutiveDrops = 0;
        return true;
    }

    void forget(mg::PlayerId p) { byPlayer_.erase(p); }

private:
    std::unordered_map<mg::PlayerId, PlayerRate> byPlayer_;
};

}  // namespace mg::tx
