// protocol.hpp — THE SEAM (build brief §5).
//
// The functional core and the transport shell compile against THIS and nothing
// else of each other's. The transport parses inbound JSON into Commands, calls
// IGameRoom::apply / onTimer, and serializes the returned Events to wire JSON.
// Events are SEMANTIC (the core emits meaning); the transport owns the wire
// format end to end. Durations are game policy (core); waiting is I/O (transport).
//
// DECISIONS encoded here that extend the brief's starter shape:
//   - A monotonic `atMs` timestamp is threaded into apply()/onTimer(). The core
//     must compute speed/volunteer bonuses while staying pure and time-agnostic;
//     the transport stamps every command/timer with a monotonic millisecond
//     clock. This keeps the core deterministic and unit-testable.
//   - Player membership changes are modelled as Commands (JoinRoom / PlayerLeft)
//     so the seam stays strictly "commands in". Spec §16 has real rules for
//     walker-disconnect, host-transfer and join-as-spectator. Reconnection
//     (token → rebind socket) stays transport-only; the core never sees a swap.
//   - Host mid-game controls and the client anti-cheat signal (tab switch) are
//     Commands too, for the same reason.
#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/config.hpp"
#include "core/safepath.hpp"

namespace mg {

using PlayerId = uint32_t;
constexpr PlayerId kNoPlayer = 0;   // 0 is never a valid player id

// ===========================================================================
// Commands — transport parses inbound JSON into these.
// ===========================================================================
struct JoinRoom      { std::string name; };
struct ConfigureRoom { GameSettings settings; };  // host, lobby only
struct StartGame     {};                          // host only, ≥2 players
struct PressReady    {};                          // "I'm Ready — Walk It!" (volunteer + ready)
struct ClickTile     { uint8_t row, col; };
struct SendChat      { std::string text; };
struct TabSwitched   {};                          // client anti-cheat signal (walker)
struct PlayerLeft    {};                           // disconnect / leave (the `from` player)
struct SkipRound     {};                          // host
struct EndGameEarly  {};                          // host
struct PauseToggle   {};                          // host, between rounds
struct KickPlayer    { PlayerId target; };        // host (lobby control, spec §11)
struct TransferHost  { PlayerId target; };        // host (lobby control, spec §11)

using Command = std::variant<JoinRoom, ConfigureRoom, StartGame, PressReady,
                             ClickTile, SendChat, TabSwitched, PlayerLeft,
                             SkipRound, EndGameEarly, PauseToggle, KickPlayer,
                             TransferHost>;

// ===========================================================================
// Shared value types carried by events.
// ===========================================================================
enum class Phase { Lobby, Pattern, Walk, Score, Elimination, Ended };

// Public, safe-to-broadcast view of a player (no path data, ever).
struct PlayerView {
    PlayerId id = kNoPlayer;
    std::string name;
    std::string colour;
    int score = 0;          // cumulative
    int lives = 0;          // this round
    bool isEliminated = false;
    bool isWalker = false;
    bool isSpectator = false;
    bool isHost = false;
    bool isReady = false;   // pressed ready this round (informational)
    bool operator==(const PlayerView&) const = default;
};

struct ScoreLine {
    PlayerId id = kNoPlayer;
    int roundScore = 0;
    int total = 0;
    // Screen-5 breakdown. Walker fields (base/speed/volunteer/deductions) are set
    // for the walker line; helper fields (hintsFollowed/hintsIgnored) for helpers.
    // Unused fields stay 0.
    int base = 0;
    int speed = 0;
    int volunteer = 0;
    int deductions = 0;
    int hintsFollowed = 0;
    int hintsIgnored = 0;
    bool operator==(const ScoreLine&) const = default;
};

struct LeaderboardEntry {
    PlayerId id = kNoPlayer;
    std::string name;
    int total = 0;
    int successfulWalks = 0;
    int hintsFollowed = 0;
    bool operator==(const LeaderboardEntry&) const = default;
};

// ===========================================================================
// Events — the core returns these; the transport serializes them to wire JSON.
// All are equality-comparable so unit tests can assert exact event vectors.
// ===========================================================================
struct PhaseStart      { Phase phase; bool operator==(const PhaseStart&) const = default; };

// memorise phase ONLY. The only event that carries tile coordinates of the path.
struct PatternReveal   { std::vector<TileIndex> path; FlashMode flashMode;
                         bool operator==(const PatternReveal&) const = default; };

struct WalkerAssigned  { PlayerId id; bool wasRandom;
                         // Endpoints so Screen 4 can mark start + finish. These are
                         // the path's two ends only (already-visible info) — NOT the path.
                         uint8_t startRow, startCol, finishRow, finishCol;
                         bool operator==(const WalkerAssigned&) const = default; };
struct TileResult      { bool correct; uint8_t lives;
                         bool operator==(const TileResult&) const = default; };
struct WalkerPosition  { uint8_t row, col;
                         bool operator==(const WalkerPosition&) const = default; };
struct WalkerReset     { uint8_t lives;          // wrong step in lives mode: back to start
                         bool operator==(const WalkerReset&) const = default; };
struct ChatBroadcast   { PlayerId from; std::string text; bool spectator;
                         bool operator==(const ChatBroadcast&) const = default; };
struct HintScored      { PlayerId helper; int points;
                         bool operator==(const HintScored&) const = default; };
struct ScoreUpdate     { std::vector<ScoreLine> scores;
                         bool operator==(const ScoreUpdate&) const = default; };
struct PlayerEliminated{ PlayerId id;
                         bool operator==(const PlayerEliminated&) const = default; };
struct GameEnded       { std::vector<LeaderboardEntry> leaderboard;
                         bool operator==(const GameEnded&) const = default; };
struct RoomUpdate      { std::vector<PlayerView> players; Phase state; GameSettings settings;
                         int currentRound; int totalRounds; bool paused;
                         bool operator==(const RoomUpdate&) const = default; };
struct TabSwitchNotice { PlayerId id; int offence;   // public "[player] switched tabs"
                         bool operator==(const TabSwitchNotice&) const = default; };
struct SystemNotice    { std::string text;            // generic lobby notice (pause, dev-tools)
                         bool operator==(const SystemNotice&) const = default; };

using EventBody = std::variant<PhaseStart, PatternReveal, WalkerAssigned, TileResult,
                               WalkerPosition, WalkerReset, ChatBroadcast, HintScored,
                               ScoreUpdate, PlayerEliminated, GameEnded, RoomUpdate,
                               TabSwitchNotice, SystemNotice>;

// Who a given event is delivered to.
enum class Reach { Broadcast, One, AllExcept, Spectators };

struct Event {
    Reach reach = Reach::Broadcast;
    PlayerId who = kNoPlayer;   // target (One) or excluded player (AllExcept)
    EventBody body;
    bool operator==(const Event&) const = default;
};

// ===========================================================================
// Timer directives — the core asks the transport to wake it later.
// ===========================================================================
struct StartTimer  { uint32_t id; uint32_t delayMs;
                     bool operator==(const StartTimer&) const = default; };
struct CancelTimer { uint32_t id;
                     bool operator==(const CancelTimer&) const = default; };
using TimerOp = std::variant<StartTimer, CancelTimer>;

struct StepResult {
    std::vector<Event>   out;
    std::vector<TimerOp> timers;
};

// ===========================================================================
// The interface the core implements and the transport consumes.
//
// CONTRACT: apply / onTimer are NEVER called concurrently for the same instance.
// The transport guarantees per-room serial dispatch, so the core needs no locks.
// ===========================================================================
class IGameRoom {
public:
    virtual ~IGameRoom() = default;
    // `atMs` is a monotonic millisecond timestamp supplied by the transport.
    virtual StepResult apply(PlayerId from, const Command& cmd, uint64_t atMs) = 0;
    virtual StepResult onTimer(uint32_t timerId, uint64_t atMs) = 0;
};

}  // namespace mg
