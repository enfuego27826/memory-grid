#include <doctest/doctest.h>

#include "core/config.hpp"
#include "core/scoring.hpp"

using namespace mg;

TEST_CASE("speed bonus clamps and decays linearly") {
    GameSettings s = easyPreset();   // maxSpeedBonus 300, path length 7
    const uint64_t par = parWalkTimeMs(s);
    CHECK(speedBonus(0, s) == 300);             // instant → full
    CHECK(speedBonus(par, s) == 0);             // at par → none
    CHECK(speedBonus(par * 2, s) == 0);         // beyond par → clamped at 0
    CHECK(speedBonus(par / 2, s) == 150);       // half par → half bonus
}

TEST_CASE("volunteer bonus scales with earliness and is zero when random") {
    GameSettings s = easyPreset();   // maxVolunteerBonus 50
    CHECK(volunteerBonus(20000, 20000, false, s) == 50);  // full timer left
    CHECK(volunteerBonus(0, 20000, false, s) == 0);       // pressed at the buzzer
    CHECK(volunteerBonus(10000, 20000, false, s) == 25);  // half
    CHECK(volunteerBonus(20000, 20000, true, s) == 0);    // randomly assigned ⇒ 0
}

TEST_CASE("walker score — success combines base + speed + volunteer − deductions") {
    GameSettings s = easyPreset();   // base 500
    WalkerScoreInput in;
    in.outcome = WalkOutcome::Success;
    in.pathLen = 7;
    in.bestProgress = 7;
    in.walkTimeMs = 0;               // full speed bonus
    in.randomlyAssigned = false;
    in.memoriseRemainingMsAtPress = 30000;
    in.memoriseTotalMs = 30000;      // full volunteer bonus
    in.wrongSteps = 1;               // one deduction (50)
    WalkerScore r = scoreWalker(in, s);
    CHECK(r.multiplier == doctest::Approx(1.0));
    CHECK(r.base == 500);
    CHECK(r.speed == 300);
    CHECK(r.volunteer == 50);
    CHECK(r.deductions == 50);
    CHECK(r.total == 800);
}

TEST_CASE("walker score — elimination-mode failure scores zero") {
    GameSettings s = easyPreset();
    WalkerScoreInput in;
    in.outcome = WalkOutcome::FailEliminated;
    in.pathLen = 8;
    in.bestProgress = 7;             // almost done — still zero
    WalkerScore r = scoreWalker(in, s);
    CHECK(r.multiplier == doctest::Approx(0.0));
    CHECK(r.total == 0);
}

TEST_CASE("walker score — lives-mode failure gives half iff >=50% completed") {
    GameSettings s = easyPreset();   // base 500
    WalkerScoreInput in;
    in.outcome = WalkOutcome::FailLives;
    in.pathLen = 8;

    in.bestProgress = 4;             // exactly 50%
    CHECK(scoreWalker(in, s).multiplier == doctest::Approx(0.5));
    CHECK(scoreWalker(in, s).total == 250);

    in.bestProgress = 3;             // below 50%
    CHECK(scoreWalker(in, s).multiplier == doctest::Approx(0.0));
    CHECK(scoreWalker(in, s).total == 0);
}

TEST_CASE("hint zones partition the path into thirds") {
    GameSettings s = easyPreset();   // early 40, mid 25, late 15
    const size_t L = 9;              // third=3, twoThird=6
    CHECK(hintPointsForZone(0, L, s) == 40);
    CHECK(hintPointsForZone(2, L, s) == 40);
    CHECK(hintPointsForZone(3, L, s) == 25);
    CHECK(hintPointsForZone(5, L, s) == 25);
    CHECK(hintPointsForZone(6, L, s) == 15);
    CHECK(hintPointsForZone(8, L, s) == 15);
}
