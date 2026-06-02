#include <doctest/doctest.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "core/game_room.hpp"  // TimerId
#include "tests/faketransport.hpp"
#include "tests/fakeroom.hpp"
#include "transport/dispatch.hpp"
#include "transport/rooms.hpp"
#include "transport/sessions.hpp"

using namespace mg;
using namespace mg::tx;
using nlohmann::json;

namespace {

struct Harness {
    uint64_t nowMs = 1000;
    mgtest::FakeRoom* room = nullptr;
    RoomRegistry registry;
    SessionStore sessions;
    mgtest::FakeTransport transport;
    Dispatcher dispatch;

    Harness()
        : registry([this](const GameSettings&) {
              auto p = std::make_unique<mgtest::FakeRoom>();
              room = p.get();
              return p;
          }),
          dispatch(registry, sessions, [this] { return nowMs; }) {
        dispatch.setTransport(&transport);
    }

    std::string create() {
        HttpResponse r = dispatch.onHttp({"POST", "/create", "{}"});
        return json::parse(r.body)["code"].get<std::string>();
    }
    static std::string joinMsg(const std::string& code, const std::string& name) {
        return json{{"type", "join_room"}, {"code", code}, {"name", name}}.dump();
    }
    std::string tokenSentTo(ConnId c) {
        for (const auto& s : transport.sent) {
            auto j = json::parse(s.msg, nullptr, false);
            if (!j.is_discarded() && j.value("type", "") == "joined" && s.conn == c)
                return j["token"].get<std::string>();
        }
        return {};
    }
    int sentToCount(ConnId c) {
        int n = 0;
        for (const auto& s : transport.sent) if (s.conn == c) ++n;
        return n;
    }
};

}  // namespace

TEST_CASE("create + join binds the socket and applies JoinRoom") {
    Harness h;
    auto code = h.create();
    h.dispatch.onOpen(1);
    h.dispatch.onMessage(1, Harness::joinMsg(code, "Bob"));

    REQUIRE(h.room->applies.size() == 1);
    CHECK(std::get<0>(h.room->applies[0]) == 1);
    CHECK(std::holds_alternative<JoinRoom>(std::get<1>(h.room->applies[0])));
    CHECK(h.transport.subs.count({1, code}) == 1);
    CHECK(!h.tokenSentTo(1).empty());
}

TEST_CASE("PlayerIds are assigned per join in order") {
    Harness h;
    auto code = h.create();
    h.dispatch.onMessage(1, Harness::joinMsg(code, "A"));
    h.dispatch.onMessage(2, Harness::joinMsg(code, "B"));
    REQUIRE(h.room->applies.size() == 2);
    CHECK(std::get<0>(h.room->applies[0]) == 1);
    CHECK(std::get<0>(h.room->applies[1]) == 2);
}

TEST_CASE("Reach routing: Broadcast / One / AllExcept / Spectators") {
    Harness h;
    auto code = h.create();
    h.dispatch.onMessage(1, Harness::joinMsg(code, "A"));   // id 1, conn 1
    h.dispatch.onMessage(2, Harness::joinMsg(code, "B"));   // id 2, conn 2

    h.room->onApply = [](PlayerId, const Command&, uint64_t) {
        StepResult r;
        PlayerView p1; p1.id = 1;
        PlayerView p2; p2.id = 2; p2.isSpectator = true;
        r.out.push_back({Reach::Broadcast, kNoPlayer,
                         RoomUpdate{{p1, p2}, Phase::Walk, mediumPreset(), 1, 5, false}});
        r.out.push_back({Reach::Broadcast, kNoPlayer, SystemNotice{"hi"}});
        r.out.push_back({Reach::One, 1, SystemNotice{"to-one"}});
        r.out.push_back({Reach::AllExcept, 1, SystemNotice{"except-1"}});
        r.out.push_back({Reach::Spectators, kNoPlayer, SystemNotice{"to-spectators"}});
        return r;
    };
    h.transport.clear();
    h.dispatch.onMessage(1, R"({"type":"press_ready_walk"})");

    // 2 broadcasts (RoomUpdate + "hi") published to the room topic.
    int broadcasts = 0;
    for (auto& p : h.transport.published) if (p.first == code) ++broadcasts;
    CHECK(broadcasts == 2);
    // One→conn1; AllExcept(1)→conn2; Spectators(id2)→conn2.
    CHECK(h.sentToCount(1) == 1);   // just the "to-one"
    CHECK(h.sentToCount(2) == 2);   // "except-1" + "to-spectators"
}

TEST_CASE("chat rate-limit drops a too-soon second message before the core") {
    Harness h;
    auto code = h.create();
    h.dispatch.onMessage(1, Harness::joinMsg(code, "A"));
    h.room->onApply = [](PlayerId, const Command&, uint64_t) { return StepResult{}; };
    size_t before = h.room->applies.size();

    h.dispatch.onMessage(1, R"({"type":"send_chat","message":"first"})");   // allowed
    h.dispatch.onMessage(1, R"({"type":"send_chat","message":"second"})");  // same ms → dropped

    int chatApplies = 0;
    for (auto& a : h.room->applies)
        if (std::holds_alternative<SendChat>(std::get<1>(a))) ++chatApplies;
    CHECK(chatApplies == 1);
    CHECK(h.room->applies.size() == before + 1);
}

TEST_CASE("rate-limit mutes after >5 consecutive drops") {
    Harness h;
    auto code = h.create();
    h.dispatch.onMessage(1, Harness::joinMsg(code, "A"));
    auto chat = R"({"type":"send_chat","message":"x"})";
    h.dispatch.onMessage(1, chat);                 // allowed (now=1000)
    for (int i = 0; i < 6; ++i) h.dispatch.onMessage(1, chat);  // 6 drops → mute triggered
    h.nowMs += 5000;                               // past rate interval, within 10s mute
    int before = 0;
    for (auto& a : h.room->applies) if (std::holds_alternative<SendChat>(std::get<1>(a))) ++before;
    h.dispatch.onMessage(1, chat);                 // still muted → dropped
    int after = 0;
    for (auto& a : h.room->applies) if (std::holds_alternative<SendChat>(std::get<1>(a))) ++after;
    CHECK(after == before);
}

TEST_CASE("malformed / unknown / unbound / oversized messages never reach the core") {
    Harness h;
    auto code = h.create();
    h.dispatch.onMessage(99, R"({"type":"press_ready_walk"})");  // unbound, not a join
    CHECK(h.room->applies.empty());  // unbound command never reached the core

    h.dispatch.onMessage(1, Harness::joinMsg(code, "A"));
    size_t after_join = h.room->applies.size();
    h.dispatch.onMessage(1, "not json");
    h.dispatch.onMessage(1, R"({"type":"bogus"})");
    h.dispatch.onMessage(1, std::string(9000, 'x'));  // oversized
    CHECK(h.room->applies.size() == after_join);
}

TEST_CASE("timers: StartTimer scheduled; onTimer routes to the core") {
    Harness h;
    auto code = h.create();
    h.dispatch.onMessage(1, Harness::joinMsg(code, "A"));
    h.room->onApply = [](PlayerId, const Command&, uint64_t) {
        StepResult r;
        r.timers.push_back(StartTimer{TIMER_MEMORISE, 20000});
        return r;
    };
    h.dispatch.onMessage(1, R"({"type":"start_game"})");
    CHECK(h.transport.hasScheduled(code, TIMER_MEMORISE));

    h.room->onTimerFn = [](uint32_t, uint64_t) {
        StepResult r;
        r.out.push_back({Reach::Broadcast, kNoPlayer, PhaseStart{Phase::Walk}});
        return r;
    };
    h.transport.clear();
    h.dispatch.onTimer(code, TIMER_MEMORISE);
    REQUIRE(h.room->timerCalls.size() == 1);
    CHECK(h.room->timerCalls[0].first == TIMER_MEMORISE);
    CHECK(h.transport.published.size() == 1);
}

TEST_CASE("reconnect rebinds without the core seeing PlayerLeft") {
    Harness h;
    auto code = h.create();
    h.dispatch.onMessage(1, Harness::joinMsg(code, "A"));  // id 1
    h.dispatch.onMessage(2, Harness::joinMsg(code, "B"));  // id 2 (so closing 1 isn't last)
    std::string token = h.tokenSentTo(1);
    REQUIRE(!token.empty());

    h.dispatch.onClose(1);  // non-last → grace scheduled, no PlayerLeft
    CHECK(h.transport.hasScheduled(code, GRACE_TIMER_BASE + 1));

    h.transport.clear();
    auto rec = json{{"type", "join_room"}, {"code", code}, {"token", token}}.dump();
    h.dispatch.onMessage(3, rec);  // reconnect on a new socket

    CHECK(h.transport.subs.count({3, code}) == 1);
    CHECK(h.transport.cancelled.size() >= 1);  // grace cancelled
    int leftApplies = 0;
    for (auto& a : h.room->applies)
        if (std::holds_alternative<PlayerLeft>(std::get<1>(a))) ++leftApplies;
    CHECK(leftApplies == 0);
}

TEST_CASE("disconnect grace fires PlayerLeft; last-out schedules teardown") {
    Harness h;
    auto code = h.create();
    h.dispatch.onMessage(1, Harness::joinMsg(code, "A"));
    h.dispatch.onMessage(2, Harness::joinMsg(code, "B"));

    h.dispatch.onClose(1);  // non-last → grace
    h.dispatch.onTimer(code, GRACE_TIMER_BASE + 1);
    int leftApplies = 0;
    for (auto& a : h.room->applies)
        if (std::holds_alternative<PlayerLeft>(std::get<1>(a))) ++leftApplies;
    CHECK(leftApplies == 1);

    h.dispatch.onClose(2);  // now last connected leaves
    CHECK(h.transport.hasScheduled(code, TEARDOWN_TIMER_ID));
    h.dispatch.onTimer(code, TEARDOWN_TIMER_ID);
    CHECK(h.registry.lookup(code) == nullptr);  // room destroyed
}
