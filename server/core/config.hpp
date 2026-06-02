// config.hpp — ALL tunable game numbers live here.
//
// Per the build brief: every grid size, timer, point value, bonus, rate limit,
// life count and deduction is a named constant or a field of GameSettings. There
// are NO magic numbers in the game logic — the difficulty presets below are just
// named bundles of these values. The spec stresses that all numbers are estimates
// subject to playtesting, so this is the single place to retune them.
//
// This header is pure data + constants (no logic, no I/O). It is shared by the
// core and, via protocol.hpp, by the transport — GameSettings appears on the wire
// in room_update, so it deliberately lives in this shared, dependency-free header.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mg {

// ---------------------------------------------------------------------------
// Enumerations mirroring the spec's GameSettings data model (spec §5, §15).
// ---------------------------------------------------------------------------
enum class Difficulty { Easy, Medium, Hard, Custom };
enum class PathStyle { Straight, Zigzag, Random };
enum class FlashMode { Solid, Flashing, Fading };
enum class WalkerSelection { FirstPress, Host };          // random fallback always applies
enum class WrongStepPenalty { LifeAndReset, Eliminate };
enum class EliminationMode { Off, On, LastRound };
enum class EliminatedBehaviour { SpectateOnly, StayNoWalk };
enum class EscalationStep { GridPlusOne, MinusFiveMemorise, Both };
// Start tile sits on this edge; finish sits on the opposite edge.
// DECISION: spec says start=left/finish=right with host able to switch to
// top/bottom; we model the axis with one enum (the finish is always opposite).
enum class StartEdge { Left, Top };

struct GridSize {
    int rows = 6;
    int cols = 6;
    constexpr int tiles() const { return rows * cols; }
    constexpr bool operator==(const GridSize&) const = default;
};

// ---------------------------------------------------------------------------
// The full tunable settings struct (spec §15 GameSettings, faithfully mapped).
// ---------------------------------------------------------------------------
struct GameSettings {
    Difficulty difficulty = Difficulty::Medium;

    // Grid
    GridSize grid{6, 6};
    int pathLength = -1;                 // -1 => Auto (≈60% of tiles); else explicit
    PathStyle pathStyle = PathStyle::Random;
    StartEdge startEdge = StartEdge::Left;

    // Memorise phase
    int memoriseTimeSec = 20;
    FlashMode flashMode = FlashMode::Solid;

    // Walker
    WalkerSelection walkerSelection = WalkerSelection::FirstPress;
    bool walkerCanChat = false;

    // Lives & penalties
    int livesPerWalker = 2;
    WrongStepPenalty wrongStepPenalty = WrongStepPenalty::LifeAndReset;
    int wrongStepDeduction = 50;
    bool lifeRefillPerRound = true;

    // Elimination
    EliminationMode eliminationMode = EliminationMode::Off;
    int eliminationsPerRound = 1;
    EliminatedBehaviour eliminatedBehaviour = EliminatedBehaviour::SpectateOnly;
    int minPlayersToEnd = 2;

    // Scoring
    int basePoints = 500;
    int maxSpeedBonus = 300;
    int maxVolunteerBonus = 50;
    int hintPointsEarly = 40;
    int hintPointsMid = 25;
    int hintPointsLate = 15;

    // Rounds
    int numRounds = 5;
    bool difficultyEscalation = false;
    EscalationStep escalationStep = EscalationStep::GridPlusOne;

    // Chat (enforced in the transport; lives here as a shared tunable)
    int chatRateLimitMs = 1500;

    bool operator==(const GameSettings&) const = default;
};

// ---------------------------------------------------------------------------
// Global constants that are not per-room settings.
// ---------------------------------------------------------------------------
namespace limits {
constexpr int kRoomCodeLen = 6;
constexpr int kMinPlayers = 2;            // host can Start with ≥2 players
constexpr int kMaxPlayers = 20;

// A hint counts as "followed" if the walker clicks the referenced tile within
// this window of the message being sent (spec §8).
constexpr uint64_t kHintFollowWindowMs = 3000;

// Tab-switch escalation freezes (spec §7).
constexpr uint64_t kTabFreeze1Ms = 2000;  // 1st offence
constexpr uint64_t kTabFreeze2Ms = 4000;  // 2nd offence
// 3rd offence is treated as a wrong step (no freeze timer).

// Phase display dwells (spec §6).
constexpr uint64_t kScoreRevealMs = 5000;
constexpr uint64_t kEliminationDisplayMs = 3000;
constexpr uint64_t kRandomAssignNoticeMs = 1000;

// Transport-only (kept here so all tunables are co-located).
constexpr int kMuteDropThreshold = 5;     // >5 dropped in a row → mute
constexpr uint64_t kMuteMs = 10000;
constexpr uint64_t kEmptyRoomTeardownMs = 60000;

// Auto path length ≈ 60% of total tiles (spec §5, §16).
constexpr double kAutoPathFraction = 0.60;

// Path is fully completed at ≥ this fraction for partial credit in lives mode.
constexpr double kPartialCreditFraction = 0.50;

// DECISION: the spec's speed-bonus formula divides time_taken by a "time_limit"
// it never defines for the walk. We derive a par time from path length: a walk
// finishing at or under par earns full speed bonus, decaying linearly to zero at
// 2× par. This per-tile reference is a tunable.
constexpr uint64_t kSpeedBonusMsPerTile = 2000;
}  // namespace limits

// Avatar colours assigned on join, by join order (spec: colour assigned on join).
inline constexpr std::array<std::string_view, 20> kPlayerColours{
    "#ef4444", "#3b82f6", "#22c55e", "#eab308", "#a855f7", "#ec4899",
    "#14b8a6", "#f97316", "#8b5cf6", "#06b6d4", "#84cc16", "#f43f5e",
    "#0ea5e9", "#10b981", "#d946ef", "#f59e0b", "#6366f1", "#65a30d",
    "#e11d48", "#0891b2"};

// Auto path length: round(60% of tiles), clamped to the valid range
// [3, tiles-4] required by the spec's custom-mode path length range.
constexpr int autoPathLength(GridSize g) {
    int n = static_cast<int>(g.tiles() * limits::kAutoPathFraction + 0.5);
    int lo = 3;
    int hi = g.tiles() - 4;
    if (hi < lo) hi = lo;
    if (n < lo) n = lo;
    if (n > hi) n = hi;
    return n;
}

// Effective path length given settings (resolves Auto).
constexpr int effectivePathLength(const GameSettings& s) {
    return s.pathLength < 0 ? autoPathLength(s.grid) : s.pathLength;
}

// Reference ("par") walk time used as the speed-bonus denominator.
constexpr uint64_t parWalkTimeMs(const GameSettings& s) {
    return static_cast<uint64_t>(effectivePathLength(s)) * limits::kSpeedBonusMsPerTile;
}

// ---------------------------------------------------------------------------
// Difficulty presets (spec §4). A preset is just a bundle of the values above.
// Custom mode starts from medium-ish defaults (the struct's defaults) and is
// fully host-editable.
// ---------------------------------------------------------------------------
inline GameSettings easyPreset() {
    GameSettings s;
    s.difficulty = Difficulty::Easy;
    s.grid = {4, 4};
    s.memoriseTimeSec = 30;
    s.pathLength = 7;                 // spec: 6–8 tiles; mid value (overridable feel)
    s.livesPerWalker = 3;
    s.eliminationMode = EliminationMode::Off;
    s.chatRateLimitMs = 1000;         // 1 msg / 1s
    return s;
}

inline GameSettings mediumPreset() {
    GameSettings s;
    s.difficulty = Difficulty::Medium;
    s.grid = {6, 6};
    s.memoriseTimeSec = 20;
    s.pathLength = 12;                // spec: 10–14 tiles
    s.livesPerWalker = 2;
    s.eliminationMode = EliminationMode::On;  // Medium = combined mode (spec §10)
    s.chatRateLimitMs = 1500;         // 1 msg / 1.5s
    return s;
}

inline GameSettings hardPreset() {
    GameSettings s;
    s.difficulty = Difficulty::Hard;
    s.grid = {8, 8};
    s.memoriseTimeSec = 15;
    s.pathLength = 19;                // spec: 16–22 tiles
    s.livesPerWalker = 1;
    s.eliminationMode = EliminationMode::On;  // mandatory
    s.chatRateLimitMs = 2000;         // 1 msg / 2s
    return s;
}

// Custom defaults = the GameSettings struct defaults (spec §5 default column),
// with Auto path length.
inline GameSettings customPreset() {
    GameSettings s;
    s.difficulty = Difficulty::Custom;
    s.pathLength = -1;                // Auto
    return s;
}

inline GameSettings presetFor(Difficulty d) {
    switch (d) {
        case Difficulty::Easy: return easyPreset();
        case Difficulty::Medium: return mediumPreset();
        case Difficulty::Hard: return hardPreset();
        case Difficulty::Custom: return customPreset();
    }
    return mediumPreset();
}

}  // namespace mg
