// game_room.hpp — the functional core's IGameRoom implementation.
//
// Pure game logic: round state machine, volunteer race, walk validation
// (adjacency / lives / reset / elimination), scoring orchestration, hint
// following, leaderboard. Knows NOTHING about sockets, JSON, threads or timers'
// real-world passage — it only emits semantic Events and TimerOps across the
// protocol.hpp seam. Determinism comes from an injected, seeded RNG.
#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "core/hint_parser.hpp"
#include "core/path_gen.hpp"
#include "core/safepath.hpp"
#include "core/scoring.hpp"
#include "protocol.hpp"

namespace mg {

// Stable timer ids (one room only ever has a subset live at once — see brief §5).
enum TimerId : uint32_t {
    TIMER_MEMORISE     = 1,  // memorise countdown / volunteer race deadline
    TIMER_RANDOM_NOTE  = 2,  // 1s "randomly assigning walker…" before the walk
    TIMER_SCORE        = 3,  // 5s score-reveal dwell
    TIMER_ELIM         = 4,  // 3s elimination-display dwell
    TIMER_FREEZE       = 5,  // tab-switch tile-click freeze
};

class GameRoom : public IGameRoom {
public:
    explicit GameRoom(uint32_t seed = 0xC0FFEEu,
                      GameSettings settings = mediumPreset());

    StepResult apply(PlayerId from, const Command& cmd, uint64_t atMs) override;
    StepResult onTimer(uint32_t timerId, uint64_t atMs) override;

    // Test/inspection accessors (never used by the transport).
    Phase phase() const { return phase_; }
    PlayerId walker() const { return walkerId_; }
    const GameSettings& settings() const { return settings_; }

private:
    struct Player {
        PlayerId id = kNoPlayer;
        std::string name;
        std::string colour;
        int scoreTotal = 0;
        int lives = 0;
        bool eliminated = false;
        bool spectator = false;     // mid-game joiner or eliminated→spectate
        bool host = false;
        bool readyPressed = false;
        bool canVolunteer = true;   // cleared for one round if walker disconnects
        uint32_t joinOrder = 0;
        int successfulWalks = 0;
        int hintsFollowed = 0;
    };

    struct PendingHint {
        PlayerId helper = kNoPlayer;
        std::vector<TileIndex> candidates;
        uint64_t sentMs = 0;
        bool followed = false;
    };

    // ---- command handlers (dispatched by phase) ----
    void onJoin(PlayerId from, const JoinRoom& c, StepResult& r);
    void onLeave(PlayerId from, uint64_t atMs, StepResult& r);
    void onConfigure(PlayerId from, const ConfigureRoom& c, StepResult& r);
    void onStart(PlayerId from, uint64_t atMs, StepResult& r);
    void onPressReady(PlayerId from, uint64_t atMs, StepResult& r);
    void onClick(PlayerId from, const ClickTile& c, uint64_t atMs, StepResult& r);
    void onChat(PlayerId from, const SendChat& c, uint64_t atMs, StepResult& r);
    void onTabSwitched(PlayerId from, uint64_t atMs, StepResult& r);
    void onSkipRound(PlayerId from, uint64_t atMs, StepResult& r);
    void onEndEarly(PlayerId from, StepResult& r);
    void onPauseToggle(PlayerId from, StepResult& r);

    // ---- phase transitions ----
    void enterPattern(uint64_t atMs, bool reusePath, StepResult& r);
    void enterWalk(uint64_t atMs, StepResult& r);
    void enterScore(WalkOutcome outcome, uint64_t atMs, StepResult& r);
    void enterEliminationOrNext(uint64_t atMs, StepResult& r);
    void endGame(StepResult& r);

    // ---- walk helpers ----
    void handleWrongStep(uint64_t atMs, StepResult& r);
    void recordFollowedHints(TileIndex clickedTile, std::size_t pathIndex,
                             uint64_t atMs, StepResult& r);

    // ---- utilities ----
    Player* find(PlayerId id);
    PlayerId chooseRandomWalker();
    void assignHostIfNeeded(StepResult& r);
    std::vector<PlayerView> playerViews() const;
    void pushRoomUpdate(StepResult& r, Reach reach = Reach::Broadcast,
                        PlayerId who = kNoPlayer) const;
    int activeCount() const;  // non-eliminated, non-spectator players
    std::vector<LeaderboardEntry> leaderboard() const;

    // ---- state ----
    std::mt19937 rng_;
    GameSettings settings_;
    Phase phase_ = Phase::Lobby;
    std::vector<Player> players_;
    PlayerId hostId_ = kNoPlayer;
    uint32_t nextJoinOrder_ = 1;
    int currentRound_ = 0;
    bool paused_ = false;
    bool awaitingResume_ = false;  // a round transition is held by pause

    // round state
    SafePath path_;
    PlayerId walkerId_ = kNoPlayer;
    bool wasRandom_ = false;
    std::size_t progress_ = 0;     // correctly-occupied path tiles (start = 1)
    std::size_t bestProgress_ = 0;
    int wrongSteps_ = 0;
    uint64_t walkStartMs_ = 0;
    uint64_t memoriseDeadlineMs_ = 0;
    uint64_t memoriseTotalMs_ = 0;
    uint64_t pressRemainingMs_ = 0;
    int tabSwitchCount_ = 0;
    uint64_t frozenUntilMs_ = 0;
    std::vector<PendingHint> hints_;
    std::vector<std::pair<PlayerId, int>> roundHelperPoints_;  // helperId -> pts
    WalkOutcome lastOutcome_ = WalkOutcome::Success;
};

}  // namespace mg
