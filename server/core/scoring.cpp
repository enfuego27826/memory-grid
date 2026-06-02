#include "core/scoring.hpp"

#include <algorithm>
#include <cmath>

namespace mg {

int speedBonus(uint64_t walkTimeMs, const GameSettings& s) {
    const uint64_t par = parWalkTimeMs(s);
    if (par == 0) return 0;
    // Full bonus at/under par, linear decay to 0 at 2× par (i.e. 1 − t/par).
    double frac = 1.0 - static_cast<double>(walkTimeMs) / static_cast<double>(par);
    frac = std::clamp(frac, 0.0, 1.0);
    return static_cast<int>(std::lround(s.maxSpeedBonus * frac));
}

int volunteerBonus(uint64_t remainingMs, uint64_t totalMs, bool randomlyAssigned,
                   const GameSettings& s) {
    if (randomlyAssigned || totalMs == 0) return 0;  // spec §9: random ⇒ 0
    double frac = static_cast<double>(remainingMs) / static_cast<double>(totalMs);
    frac = std::clamp(frac, 0.0, 1.0);
    return static_cast<int>(std::lround(s.maxVolunteerBonus * frac));
}

WalkerScore scoreWalker(const WalkerScoreInput& in, const GameSettings& s) {
    WalkerScore r;

    // Success multiplier (spec §9).
    const double completedFrac = in.pathLen == 0
        ? 0.0 : static_cast<double>(in.bestProgress) / static_cast<double>(in.pathLen);
    switch (in.outcome) {
        case WalkOutcome::Success:
            r.multiplier = 1.0;
            break;
        case WalkOutcome::FailEliminated:
            r.multiplier = 0.0;  // elimination-mode failure scores zero
            break;
        case WalkOutcome::FailLives:
            r.multiplier =
                completedFrac >= limits::kPartialCreditFraction ? 0.5 : 0.0;
            break;
    }

    r.base = static_cast<int>(std::lround(s.basePoints * r.multiplier));
    // Speed bonus requires a finish.
    r.speed = in.outcome == WalkOutcome::Success ? speedBonus(in.walkTimeMs, s) : 0;
    // DECISION: the volunteer bonus rewards the brave early press, so it applies
    // whenever the walker volunteered — even on a partial (lives-mode) failure.
    // It is suppressed only by the zero multiplier of an elimination-mode failure.
    r.volunteer = in.outcome == WalkOutcome::FailEliminated
        ? 0
        : volunteerBonus(in.memoriseRemainingMsAtPress, in.memoriseTotalMs,
                         in.randomlyAssigned, s);
    r.deductions = in.wrongSteps * s.wrongStepDeduction;

    r.total = r.base + r.speed + r.volunteer - r.deductions;
    if (r.total < 0) r.total = 0;  // DECISION: a round never yields negative score
    return r;
}

int hintPointsForZone(std::size_t pathIndex, std::size_t pathLen,
                      const GameSettings& s) {
    if (pathLen == 0) return 0;
    const std::size_t third = pathLen / 3;          // ⌊L/3⌋
    const std::size_t twoThird = (2 * pathLen) / 3;  // ⌊2L/3⌋
    if (pathIndex < third)      return s.hintPointsEarly;
    if (pathIndex < twoThird)   return s.hintPointsMid;
    return s.hintPointsLate;
}

}  // namespace mg
