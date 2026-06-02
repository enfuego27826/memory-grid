#include <doctest/doctest.h>

#include <vector>

#include "core/game_room.hpp"
#include "core/path_gen.hpp"
#include "tests/room_driver.hpp"

using namespace mg;
using namespace mgtest;

namespace {

// Drive a fresh room to the start of a walk with player 2 as the volunteer,
// returning the (now known, from the reveal) path. Player 1 is host.
struct Started {
    GameRoom room;
    std::vector<TileIndex> path;
    int cols;
    explicit Started(GameSettings s, uint32_t seed = 123)
        : room(seed, s), cols(s.grid.cols) {
        room.apply(1, JoinRoom{"Host"}, 0);
        room.apply(2, JoinRoom{"Bob"}, 0);
        StepResult started = room.apply(1, StartGame{}, 0);
        const PatternReveal* pr = firstEvent<PatternReveal>(started);
        REQUIRE(pr != nullptr);
        path = pr->path;
        REQUIRE(hasStartTimer(started, TIMER_MEMORISE));
    }
};

// An in-bounds neighbour of `cur` that is NOT `expected` — a wrong tile to click.
ClickTile wrongAdjacent(TileIndex cur, TileIndex expected, int rows, int cols) {
    int r = rowOf(cur, cols), c = colOf(cur, cols);
    const int drc[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (auto& d : drc) {
        int nr = r + d[0], nc = c + d[1];
        if (nr < 0 || nc < 0 || nr >= rows || nc >= cols) continue;
        TileIndex t = tileAt(nr, nc, cols);
        if (t != expected) return clickFor(t, cols);
    }
    FAIL("no wrong adjacent tile found");
    return clickFor(cur, cols);
}

}  // namespace

TEST_CASE("press-ready makes the presser the walker and cancels the timer") {
    Started g(easyPreset());
    StepResult r = g.room.apply(2, PressReady{}, 0);
    CHECK(hasCancelTimer(r, TIMER_MEMORISE));
    const WalkerAssigned* wa = firstEvent<WalkerAssigned>(r);
    REQUIRE(wa != nullptr);
    CHECK(wa->id == 2);
    CHECK(wa->wasRandom == false);
    CHECK(g.room.phase() == Phase::Walk);
    CHECK(g.room.walker() == 2);
}

TEST_CASE("a perfect walk yields full base+speed+volunteer score") {
    Started g(easyPreset());                 // base 500, speed 300, vol 50
    g.room.apply(2, PressReady{}, 0);        // full timer remaining → vol 50
    StepResult last;
    for (size_t i = 1; i < g.path.size(); ++i)
        last = g.room.apply(2, clickFor(g.path[i], g.cols), 0);  // instant → speed 300

    CHECK(g.room.phase() == Phase::Score);
    const ScoreUpdate* su = firstEvent<ScoreUpdate>(last);
    REQUIRE(su != nullptr);
    int walkerTotal = 0;
    for (const auto& line : su->scores) if (line.id == 2) walkerTotal = line.total;
    CHECK(walkerTotal == 850);
    CHECK(hasStartTimer(last, TIMER_SCORE));
}

TEST_CASE("non-adjacent click is silently rejected") {
    Started g(easyPreset());
    g.room.apply(2, PressReady{}, 0);
    // Click a far corner that is (almost certainly) not adjacent to the start.
    TileIndex start = g.path[0];
    int rows = g.room.settings().grid.rows;
    // Pick a tile two rows away if possible.
    int r = (rowOf(start, g.cols) + 2) % rows;
    StepResult res = g.room.apply(2, clickFor(tileAt(r, colOf(start, g.cols), g.cols), g.cols), 0);
    // If it happened to be adjacent we'd see a TileResult; assert rejection only
    // when truly non-adjacent.
    if (std::abs(r - rowOf(start, g.cols)) != 1) {
        CHECK(firstEvent<TileResult>(res) == nullptr);
        CHECK(g.room.phase() == Phase::Walk);
    }
}

TEST_CASE("wrong step in lives mode deducts a life and resets; running out fails") {
    GameSettings s = easyPreset();           // lives 3, life_and_reset
    s.livesPerWalker = 2;
    Started g(s);
    g.room.apply(2, PressReady{}, 0);

    TileIndex start = g.path[0];
    auto bad = wrongAdjacent(start, g.path[1], s.grid.rows, g.cols);

    StepResult r1 = g.room.apply(2, bad, 0);
    const TileResult* t1 = firstEvent<TileResult>(r1);
    REQUIRE(t1 != nullptr);
    CHECK(t1->correct == false);
    CHECK(t1->lives == 1);
    CHECK(firstEvent<WalkerReset>(r1) != nullptr);
    CHECK(g.room.phase() == Phase::Walk);    // reset, still walking

    StepResult r2 = g.room.apply(2, bad, 0); // second wrong → out of lives
    const TileResult* t2 = firstEvent<TileResult>(r2);
    REQUIRE(t2 != nullptr);
    CHECK(t2->lives == 0);
    CHECK(g.room.phase() == Phase::Score);   // walk failed
}

TEST_CASE("wrong step in elimination penalty ends the walk immediately") {
    GameSettings s = easyPreset();
    s.wrongStepPenalty = WrongStepPenalty::Eliminate;
    Started g(s);
    g.room.apply(2, PressReady{}, 0);
    auto bad = wrongAdjacent(g.path[0], g.path[1], s.grid.rows, g.cols);
    StepResult r = g.room.apply(2, bad, 0);
    CHECK(g.room.phase() == Phase::Score);
    const ScoreUpdate* su = firstEvent<ScoreUpdate>(r);
    REQUIRE(su != nullptr);
    for (const auto& line : su->scores) if (line.id == 2) CHECK(line.roundScore == 0);
}

TEST_CASE("timer expiry assigns a random walker with zero volunteer bonus") {
    Started g(easyPreset());
    StepResult t1 = g.room.onTimer(TIMER_MEMORISE, 30000);
    CHECK(firstEvent<SystemNotice>(t1) != nullptr);
    CHECK(hasStartTimer(t1, TIMER_RANDOM_NOTE));

    StepResult t2 = g.room.onTimer(TIMER_RANDOM_NOTE, 31000);
    const WalkerAssigned* wa = firstEvent<WalkerAssigned>(t2);
    REQUIRE(wa != nullptr);
    CHECK(wa->wasRandom == true);
    CHECK(g.room.phase() == Phase::Walk);

    // Complete the walk; volunteer bonus must be 0 for a random walker.
    PlayerId w = g.room.walker();
    StepResult last;
    for (size_t i = 1; i < g.path.size(); ++i)
        last = g.room.apply(w, clickFor(g.path[i], g.cols), 31000);
    const ScoreUpdate* su = firstEvent<ScoreUpdate>(last);
    REQUIRE(su != nullptr);
    for (const auto& line : su->scores)
        if (line.id == w) CHECK(line.total == 800);  // 500 base + 300 speed + 0 vol
}

TEST_CASE("a followed hint scores the helper") {
    Started g(easyPreset());
    g.room.apply(2, PressReady{}, 0);        // player 2 walks; player 1 is a helper
    // Helper names the next tile via row/col; walker clicks it within the window.
    TileIndex next = g.path[1];
    int hr = rowOf(next, g.cols) + 1, hc = colOf(next, g.cols) + 1;  // 1-based
    g.room.apply(1, SendChat{"row " + std::to_string(hr) + " col " + std::to_string(hc)}, 100);
    StepResult clk = g.room.apply(2, clickFor(next, g.cols), 200);   // within 3s
    const HintScored* hs = firstEvent<HintScored>(clk);
    REQUIRE(hs != nullptr);
    CHECK(hs->helper == 1);
    CHECK(hs->points == easyPreset().hintPointsEarly);
}

TEST_CASE("walker disconnect replays the round with the same path") {
    Started g(easyPreset());
    g.room.apply(2, PressReady{}, 0);
    auto pathBefore = g.path;
    StepResult r = g.room.apply(2, PlayerLeft{}, 500);
    CHECK(g.room.phase() == Phase::Pattern);          // back to memorise
    const PatternReveal* pr = firstEvent<PatternReveal>(r);
    REQUIRE(pr != nullptr);
    CHECK(pr->path == pathBefore);                    // SAME path (spec §16)
}

TEST_CASE("start requires >=2 players and host privileges") {
    GameRoom room(1, easyPreset());
    room.apply(1, JoinRoom{"Solo"}, 0);
    StepResult r1 = room.apply(1, StartGame{}, 0);    // only 1 player
    CHECK(room.phase() == Phase::Lobby);
    CHECK(firstEvent<PatternReveal>(r1) == nullptr);

    room.apply(2, JoinRoom{"Two"}, 0);
    StepResult r2 = room.apply(2, StartGame{}, 0);    // non-host tries to start
    CHECK(room.phase() == Phase::Lobby);

    StepResult r3 = room.apply(1, StartGame{}, 0);    // host, 2 players → ok
    CHECK(room.phase() == Phase::Pattern);
    CHECK(firstEvent<PatternReveal>(r3) != nullptr);
}

TEST_CASE("host leaving transfers host to the earliest remaining player") {
    GameRoom room(1, easyPreset());
    room.apply(1, JoinRoom{"A"}, 0);
    room.apply(2, JoinRoom{"B"}, 0);
    room.apply(3, JoinRoom{"C"}, 0);
    StepResult r = room.apply(1, PlayerLeft{}, 0);    // host leaves in lobby
    const RoomUpdate* ru = firstEvent<RoomUpdate>(r);
    REQUIRE(ru != nullptr);
    bool bIsHost = false;
    for (const auto& p : ru->players) if (p.id == 2) bIsHost = p.isHost;
    CHECK(bIsHost);                                   // earliest remaining (join order)
}
