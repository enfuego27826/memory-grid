// scoring.hpp — pure scoring formulas (spec §9, §8). No I/O, no state.
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/config.hpp"

namespace mg {

// How a walk ended, for multiplier selection (spec §9).
enum class WalkOutcome { Success, FailLives, FailEliminated };

struct WalkerScoreInput {
    WalkOutcome outcome = WalkOutcome::Success;
    std::size_t pathLen = 0;
    std::size_t bestProgress = 0;   // furthest correct tiles reached (any attempt)
    int wrongSteps = 0;
    uint64_t walkTimeMs = 0;        // time from walk start to finish
    bool randomlyAssigned = false;
    uint64_t memoriseRemainingMsAtPress = 0;  // 0 if random
    uint64_t memoriseTotalMs = 0;
};

struct WalkerScore {
    int base = 0;
    int speed = 0;
    int volunteer = 0;
    int deductions = 0;
    int total = 0;          // clamped ≥ 0
    double multiplier = 0;  // 1.0 / 0.5 / 0.0
};

// Speed bonus: max × clamp(1 − walkTime/par, 0, 1). Only meaningful on success.
int speedBonus(uint64_t walkTimeMs, const GameSettings& s);

// Volunteer bonus: max × (remaining/total), clamped; zero if randomly assigned.
int volunteerBonus(uint64_t remainingMs, uint64_t totalMs, bool randomlyAssigned,
                   const GameSettings& s);

// Full walker score per spec §9.
WalkerScore scoreWalker(const WalkerScoreInput& in, const GameSettings& s);

// Points a single followed hint is worth, by the zone of the referenced tile.
// `pathIndex` is the tile's 0-based position within the path (spec §8 zones:
// early = tiles 1..⌊L/3⌋, mid = ⌊L/3⌋+1..⌊2L/3⌋, late = remaining).
int hintPointsForZone(std::size_t pathIndex, std::size_t pathLen, const GameSettings& s);

}  // namespace mg
