// FakeTransport — an ITransport that captures all outbound calls for assertions.
#pragma once

#include <set>
#include <string>
#include <vector>

#include "transport/itransport.hpp"

namespace mgtest {
using namespace mg::tx;

struct FakeTransport : ITransport {
    struct Sent { ConnId conn; std::string msg; };
    struct Sched { RoomKey room; uint32_t id; uint32_t delay; };

    std::vector<Sent> sent;
    std::vector<std::pair<RoomKey, std::string>> published;
    std::set<std::pair<ConnId, RoomKey>> subs;
    std::vector<ConnId> closed;
    std::vector<Sched> scheduled;
    std::vector<std::pair<RoomKey, uint32_t>> cancelled;

    void run(uint16_t) override {}
    void sendOne(ConnId c, std::string_view b) override { sent.push_back({c, std::string(b)}); }
    void closeOne(ConnId c) override { closed.push_back(c); }
    void subscribe(ConnId c, const RoomKey& r) override { subs.insert({c, r}); }
    void unsubscribe(ConnId c, const RoomKey& r) override { subs.erase({c, r}); }
    void publish(const RoomKey& r, std::string_view b) override {
        published.push_back({r, std::string(b)});
    }
    void scheduleTimer(const RoomKey& r, uint32_t id, uint32_t delay) override {
        // mimic "replace existing": drop any prior schedule of the same key
        cancel(r, id);
        scheduled.push_back({r, id, delay});
    }
    void cancelTimer(const RoomKey& r, uint32_t id) override {
        cancelled.push_back({r, id});
        cancel(r, id);
    }

    // ---- test helpers ----
    void cancel(const RoomKey& r, uint32_t id) {
        scheduled.erase(std::remove_if(scheduled.begin(), scheduled.end(),
                                       [&](const Sched& s) { return s.room == r && s.id == id; }),
                        scheduled.end());
    }
    bool hasScheduled(const RoomKey& r, uint32_t id) const {
        for (const auto& s : scheduled) if (s.room == r && s.id == id) return true;
        return false;
    }
    void clear() { sent.clear(); published.clear(); closed.clear(); cancelled.clear(); }
};

}  // namespace mgtest
