#include "core/game_room.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>

namespace mg {
namespace {

bool isHostCmd(PlayerId from, PlayerId host) { return from == host && host != kNoPlayer; }

int manhattan(int r1, int c1, int r2, int c2) { return std::abs(r1 - r2) + std::abs(c1 - c2); }

}  // namespace

GameRoom::GameRoom(uint32_t seed, GameSettings settings)
    : rng_(seed), settings_(std::move(settings)) {}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------
StepResult GameRoom::apply(PlayerId from, const Command& cmd, uint64_t atMs) {
    StepResult r;
    std::visit([&](auto&& c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, JoinRoom>)        onJoin(from, c, r);
        else if constexpr (std::is_same_v<T, PlayerLeft>)  onLeave(from, atMs, r);
        else if constexpr (std::is_same_v<T, ConfigureRoom>) onConfigure(from, c, r);
        else if constexpr (std::is_same_v<T, StartGame>)   onStart(from, atMs, r);
        else if constexpr (std::is_same_v<T, PressReady>)  onPressReady(from, atMs, r);
        else if constexpr (std::is_same_v<T, ClickTile>)   onClick(from, c, atMs, r);
        else if constexpr (std::is_same_v<T, SendChat>)    onChat(from, c, atMs, r);
        else if constexpr (std::is_same_v<T, TabSwitched>) onTabSwitched(from, atMs, r);
        else if constexpr (std::is_same_v<T, SkipRound>)   onSkipRound(from, atMs, r);
        else if constexpr (std::is_same_v<T, EndGameEarly>) onEndEarly(from, r);
        else if constexpr (std::is_same_v<T, PauseToggle>)  onPauseToggle(from, r);
    }, cmd);
    return r;
}

StepResult GameRoom::onTimer(uint32_t timerId, uint64_t atMs) {
    StepResult r;
    switch (timerId) {
        case TIMER_MEMORISE: {
            if (phase_ != Phase::Pattern || walkerId_ != kNoPlayer) break;
            PlayerId w = chooseRandomWalker();
            if (w == kNoPlayer) { endGame(r); break; }
            walkerId_ = w;
            wasRandom_ = true;
            pressRemainingMs_ = 0;
            r.out.push_back({Reach::Broadcast, kNoPlayer,
                             SystemNotice{"Randomly assigning walker…"}});
            r.timers.push_back(StartTimer{TIMER_RANDOM_NOTE, limits::kRandomAssignNoticeMs});
            break;
        }
        case TIMER_RANDOM_NOTE:
            if (phase_ == Phase::Pattern && walkerId_ != kNoPlayer) enterWalk(atMs, r);
            break;
        case TIMER_SCORE:
            if (phase_ == Phase::Score) enterEliminationOrNext(atMs, r);
            break;
        case TIMER_ELIM:
            if (phase_ == Phase::Elimination) {
                if (currentRound_ >= settings_.numRounds || activeCount() <= 1 ||
                    activeCount() < settings_.minPlayersToEnd)
                    endGame(r);
                else
                    enterPattern(atMs, /*reusePath=*/false, r);
            }
            break;
        case TIMER_FREEZE:
            break;  // freeze simply expires; clicks are gated by frozenUntilMs_
        default:
            break;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Lobby commands
// ---------------------------------------------------------------------------
void GameRoom::onJoin(PlayerId from, const JoinRoom& c, StepResult& r) {
    if (find(from)) return;  // already joined (transport handles reconnect rebind)
    Player p;
    p.id = from;
    p.name = c.name;
    p.joinOrder = nextJoinOrder_++;
    p.colour = std::string(kPlayerColours[(p.joinOrder - 1) % kPlayerColours.size()]);
    p.lives = settings_.livesPerWalker;
    p.spectator = (phase_ != Phase::Lobby);  // mid-game joiner plays from next game
    if (players_.empty()) { p.host = true; hostId_ = from; }
    players_.push_back(std::move(p));
    pushRoomUpdate(r);
}

void GameRoom::onLeave(PlayerId from, uint64_t atMs, StepResult& r) {
    Player* p = find(from);
    if (!p) return;

    const bool wasWalker = (from == walkerId_ && phase_ == Phase::Walk);
    const bool wasHost = (from == hostId_);

    // Remove the player.
    players_.erase(std::remove_if(players_.begin(), players_.end(),
                                  [&](const Player& x) { return x.id == from; }),
                   players_.end());
    if (wasHost) hostId_ = kNoPlayer;
    assignHostIfNeeded(r);

    if (wasWalker) {
        // Spec §16: walk cancelled; round replays with the SAME path; the
        // departed walker is not score-penalised. (They cannot re-press next
        // round, but they have left, so the flag is moot.)
        walkerId_ = kNoPlayer;
        r.out.push_back({Reach::Broadcast, kNoPlayer,
                         SystemNotice{"Walker left — replaying the round."}});
        if (activeCount() >= 1)
            enterPattern(atMs, /*reusePath=*/true, r);
        else
            endGame(r);
        return;
    }

    if (phase_ != Phase::Lobby && phase_ != Phase::Ended && activeCount() <= 1) {
        endGame(r);
        return;
    }
    pushRoomUpdate(r);
}

void GameRoom::onConfigure(PlayerId from, const ConfigureRoom& c, StepResult& r) {
    if (!isHostCmd(from, hostId_) || phase_ != Phase::Lobby) return;
    settings_ = c.settings;
    pushRoomUpdate(r);
}

void GameRoom::onStart(PlayerId from, uint64_t atMs, StepResult& r) {
    if (!isHostCmd(from, hostId_) || phase_ != Phase::Lobby) return;
    if (activeCount() < limits::kMinPlayers) return;  // ≥2 players to start
    currentRound_ = 0;  // enterPattern increments to 1
    enterPattern(atMs, /*reusePath=*/false, r);
}

// ---------------------------------------------------------------------------
// Pattern / volunteer race
// ---------------------------------------------------------------------------
void GameRoom::onPressReady(PlayerId from, uint64_t atMs, StepResult& r) {
    if (phase_ != Phase::Pattern || walkerId_ != kNoPlayer) return;  // first press wins
    if (settings_.walkerSelection != WalkerSelection::FirstPress) return;
    Player* p = find(from);
    if (!p || p->eliminated || p->spectator || !p->canVolunteer) return;

    walkerId_ = from;
    wasRandom_ = false;
    p->readyPressed = true;
    pressRemainingMs_ = memoriseDeadlineMs_ > atMs ? (memoriseDeadlineMs_ - atMs) : 0;
    r.timers.push_back(CancelTimer{TIMER_MEMORISE});
    enterWalk(atMs, r);
}

// ---------------------------------------------------------------------------
// The walk
// ---------------------------------------------------------------------------
void GameRoom::onClick(PlayerId from, const ClickTile& c, uint64_t atMs, StepResult& r) {
    if (phase_ != Phase::Walk || from != walkerId_) return;
    if (atMs < frozenUntilMs_) return;  // tab-switch freeze in effect

    const int cols = settings_.grid.cols;
    const int rows = settings_.grid.rows;
    if (c.row >= rows || c.col >= cols) return;  // out of bounds → silent reject

    const TileIndex cur = path_.at(progress_ - 1);
    const int curR = rowOf(cur, cols), curC = colOf(cur, cols);
    if (manhattan(c.row, c.col, curR, curC) != 1) return;  // non-adjacent → silent reject (§16)

    const TileIndex clicked = tileAt(c.row, c.col, cols);
    const TileIndex expected = path_.at(progress_);

    Player* w = find(walkerId_);
    const uint8_t lives = w ? static_cast<uint8_t>(w->lives) : 0;

    if (clicked == expected) {
        const std::size_t pathIndex = progress_;  // 0-based index of the clicked tile
        ++progress_;
        bestProgress_ = std::max(bestProgress_, progress_);
        r.out.push_back({Reach::One, walkerId_, TileResult{true, lives}});
        r.out.push_back({Reach::Broadcast, kNoPlayer,
                         WalkerPosition{c.row, c.col}});
        recordFollowedHints(clicked, pathIndex, atMs, r);
        if (progress_ == path_.size()) enterScore(WalkOutcome::Success, atMs, r);
    } else {
        handleWrongStep(atMs, r);
    }
}

void GameRoom::handleWrongStep(uint64_t atMs, StepResult& r) {
    ++wrongSteps_;
    if (settings_.wrongStepPenalty == WrongStepPenalty::Eliminate) {
        r.out.push_back({Reach::One, walkerId_, TileResult{false, 0}});
        enterScore(WalkOutcome::FailEliminated, atMs, r);
        return;
    }
    // Lives mode: lose a life, reset to start.
    Player* w = find(walkerId_);
    if (w) --w->lives;
    const uint8_t lives = w ? static_cast<uint8_t>(std::max(0, w->lives)) : 0;
    r.out.push_back({Reach::One, walkerId_, TileResult{false, lives}});
    if (lives == 0) {
        enterScore(WalkOutcome::FailLives, atMs, r);
        return;
    }
    progress_ = 1;  // back to the start tile
    const TileIndex start = path_.start();
    r.out.push_back({Reach::Broadcast, kNoPlayer, WalkerReset{lives}});
    r.out.push_back({Reach::Broadcast, kNoPlayer,
                     WalkerPosition{static_cast<uint8_t>(rowOf(start, settings_.grid.cols)),
                                    static_cast<uint8_t>(colOf(start, settings_.grid.cols))}});
}

void GameRoom::onChat(PlayerId from, const SendChat& c, uint64_t atMs, StepResult& r) {
    Player* p = find(from);
    if (!p) return;
    const bool spectator = p->spectator || p->eliminated;

    if (phase_ == Phase::Walk) {
        const bool isWalker = (from == walkerId_);
        if (isWalker && !settings_.walkerCanChat) return;  // walker is read-only
        r.out.push_back({Reach::Broadcast, kNoPlayer, ChatBroadcast{from, c.text, spectator}});
        if (!isWalker) {  // parse a hint from a helper's message
            HintMatch hm = parseHint(c.text, settings_.grid.rows, settings_.grid.cols);
            std::vector<TileIndex> candidates = hm.absolute;
            if (walkerId_ != kNoPlayer && !hm.directions.empty()) {
                const TileIndex cur = path_.at(progress_ - 1);
                const int cr = rowOf(cur, settings_.grid.cols);
                const int cc = colOf(cur, settings_.grid.cols);
                for (Direction d : hm.directions) {
                    int nr = cr, nc = cc;
                    switch (d) {
                        case Direction::Up: --nr; break;
                        case Direction::Down: ++nr; break;
                        case Direction::Left: --nc; break;
                        case Direction::Right: ++nc; break;
                    }
                    if (nr >= 0 && nc >= 0 && nr < settings_.grid.rows &&
                        nc < settings_.grid.cols)
                        candidates.push_back(tileAt(nr, nc, settings_.grid.cols));
                }
            }
            if (!candidates.empty())
                hints_.push_back(PendingHint{from, std::move(candidates), atMs, false});
        }
        return;
    }
    if (phase_ == Phase::Lobby) {  // pre-game chat (no hint parsing)
        r.out.push_back({Reach::Broadcast, kNoPlayer, ChatBroadcast{from, c.text, spectator}});
    }
    // Other phases: chat is closed (spec Screen 3 / §6).
}

void GameRoom::recordFollowedHints(TileIndex clickedTile, std::size_t pathIndex,
                                   uint64_t atMs, StepResult& r) {
    for (auto& h : hints_) {
        if (h.followed) continue;
        if (atMs - h.sentMs > limits::kHintFollowWindowMs) continue;
        if (std::find(h.candidates.begin(), h.candidates.end(), clickedTile) ==
            h.candidates.end())
            continue;
        h.followed = true;
        const int pts = hintPointsForZone(pathIndex, path_.size(), settings_);
        // accumulate this round's helper points
        auto it = std::find_if(roundHelperPoints_.begin(), roundHelperPoints_.end(),
                               [&](auto& kv) { return kv.first == h.helper; });
        if (it == roundHelperPoints_.end())
            roundHelperPoints_.push_back({h.helper, pts});
        else
            it->second += pts;
        if (Player* hp = find(h.helper)) ++hp->hintsFollowed;
        r.out.push_back({Reach::Broadcast, kNoPlayer, HintScored{h.helper, pts}});
    }
}

void GameRoom::onTabSwitched(PlayerId from, uint64_t atMs, StepResult& r) {
    if (phase_ != Phase::Walk || from != walkerId_) return;
    ++tabSwitchCount_;
    r.out.push_back({Reach::Broadcast, kNoPlayer, TabSwitchNotice{from, tabSwitchCount_}});
    if (tabSwitchCount_ == 1) {
        frozenUntilMs_ = atMs + limits::kTabFreeze1Ms;
        r.timers.push_back(StartTimer{TIMER_FREEZE, static_cast<uint32_t>(limits::kTabFreeze1Ms)});
    } else if (tabSwitchCount_ == 2) {
        frozenUntilMs_ = atMs + limits::kTabFreeze2Ms;
        r.timers.push_back(StartTimer{TIMER_FREEZE, static_cast<uint32_t>(limits::kTabFreeze2Ms)});
    } else {  // 3rd offence: treated as a wrong step (spec §7)
        handleWrongStep(atMs, r);
    }
}

// ---------------------------------------------------------------------------
// Host mid-game controls
// ---------------------------------------------------------------------------
void GameRoom::onSkipRound(PlayerId from, uint64_t atMs, StepResult& r) {
    if (!isHostCmd(from, hostId_)) return;
    if (phase_ == Phase::Lobby || phase_ == Phase::Ended) return;
    r.timers.push_back(CancelTimer{TIMER_MEMORISE});
    r.timers.push_back(CancelTimer{TIMER_SCORE});
    r.out.push_back({Reach::Broadcast, kNoPlayer, SystemNotice{"Round skipped by host."}});
    if (currentRound_ >= settings_.numRounds) endGame(r);
    else enterPattern(atMs, /*reusePath=*/false, r);
}

void GameRoom::onEndEarly(PlayerId from, StepResult& r) {
    if (!isHostCmd(from, hostId_) || phase_ == Phase::Ended) return;
    r.timers.push_back(CancelTimer{TIMER_MEMORISE});
    r.timers.push_back(CancelTimer{TIMER_SCORE});
    r.timers.push_back(CancelTimer{TIMER_ELIM});
    endGame(r);
}

void GameRoom::onPauseToggle(PlayerId from, StepResult& r) {
    if (!isHostCmd(from, hostId_)) return;
    paused_ = !paused_;
    r.out.push_back({Reach::Broadcast, kNoPlayer,
                     SystemNotice{paused_ ? "Game paused." : "Game resumed."}});
    pushRoomUpdate(r);
    if (!paused_ && awaitingResume_) {
        awaitingResume_ = false;
        // Resume the held between-rounds transition.
        enterEliminationOrNext(/*atMs=*/0, r);
    }
}

// ---------------------------------------------------------------------------
// Phase transitions
// ---------------------------------------------------------------------------
void GameRoom::enterPattern(uint64_t atMs, bool reusePath, StepResult& r) {
    if (!reusePath) {
        ++currentRound_;
        // Difficulty escalation between rounds (spec §5).
        if (settings_.difficultyEscalation && currentRound_ > 1) {
            if (settings_.escalationStep == EscalationStep::GridPlusOne ||
                settings_.escalationStep == EscalationStep::Both) {
                settings_.grid.rows = std::min(12, settings_.grid.rows + 1);
                settings_.grid.cols = std::min(12, settings_.grid.cols + 1);
            }
            if (settings_.escalationStep == EscalationStep::MinusFiveMemorise ||
                settings_.escalationStep == EscalationStep::Both) {
                settings_.memoriseTimeSec = std::max(5, settings_.memoriseTimeSec - 5);
            }
        }
        path_ = generatePath(settings_, rng_);
    }

    phase_ = Phase::Pattern;
    walkerId_ = kNoPlayer;
    wasRandom_ = false;
    progress_ = 0;
    bestProgress_ = 0;
    wrongSteps_ = 0;
    tabSwitchCount_ = 0;
    frozenUntilMs_ = 0;
    hints_.clear();
    roundHelperPoints_.clear();

    for (auto& p : players_) {
        p.readyPressed = false;
        p.canVolunteer = true;  // restored each fresh round
        if (settings_.lifeRefillPerRound || currentRound_ == 1)
            p.lives = settings_.livesPerWalker;
    }

    memoriseTotalMs_ = static_cast<uint64_t>(settings_.memoriseTimeSec) * 1000;
    memoriseDeadlineMs_ = atMs + memoriseTotalMs_;

    pushRoomUpdate(r);
    r.out.push_back({Reach::Broadcast, kNoPlayer, PhaseStart{Phase::Pattern}});
    // SECURITY: the path reaches the wire ONLY here, in the Pattern phase.
    assert(phase_ == Phase::Pattern);
    r.out.push_back({Reach::Broadcast, kNoPlayer,
                     PatternReveal{path_.revealForMemorise(), settings_.flashMode}});
    r.timers.push_back(StartTimer{TIMER_MEMORISE, static_cast<uint32_t>(memoriseTotalMs_)});
}

void GameRoom::enterWalk(uint64_t atMs, StepResult& r) {
    phase_ = Phase::Walk;
    progress_ = 1;       // walker occupies the start tile
    bestProgress_ = 1;
    wrongSteps_ = 0;
    tabSwitchCount_ = 0;
    frozenUntilMs_ = 0;
    walkStartMs_ = atMs;
    hints_.clear();
    roundHelperPoints_.clear();

    const TileIndex start = path_.start();
    pushRoomUpdate(r);
    r.out.push_back({Reach::Broadcast, kNoPlayer, PhaseStart{Phase::Walk}});
    r.out.push_back({Reach::Broadcast, kNoPlayer, WalkerAssigned{walkerId_, wasRandom_}});
    r.out.push_back({Reach::Broadcast, kNoPlayer,
                     WalkerPosition{static_cast<uint8_t>(rowOf(start, settings_.grid.cols)),
                                    static_cast<uint8_t>(colOf(start, settings_.grid.cols))}});
}

void GameRoom::enterScore(WalkOutcome outcome, uint64_t atMs, StepResult& r) {
    phase_ = Phase::Score;
    lastOutcome_ = outcome;

    // Walker score.
    WalkerScoreInput in;
    in.outcome = outcome;
    in.pathLen = path_.size();
    in.bestProgress = bestProgress_;
    in.wrongSteps = wrongSteps_;
    in.walkTimeMs = (outcome == WalkOutcome::Success) ? (atMs - walkStartMs_) : 0;
    in.randomlyAssigned = wasRandom_;
    in.memoriseRemainingMsAtPress = pressRemainingMs_;
    in.memoriseTotalMs = memoriseTotalMs_;
    const WalkerScore ws = scoreWalker(in, settings_);

    if (Player* w = find(walkerId_)) {
        w->scoreTotal += ws.total;
        if (outcome == WalkOutcome::Success) ++w->successfulWalks;
    }

    // On failure, helpers who gave correct (on-path) hints the walker ignored
    // keep HALF their hint points (spec §8/§9).
    if (outcome != WalkOutcome::Success) {
        for (const auto& h : hints_) {
            if (h.followed) continue;
            std::size_t bestIdx = path_.size();  // smallest on-path index among candidates
            for (TileIndex t : h.candidates)
                if (auto idx = path_.indexOf(t); idx && *idx < bestIdx) bestIdx = *idx;
            if (bestIdx == path_.size()) continue;  // no on-path candidate → not a correct hint
            const int half = hintPointsForZone(bestIdx, path_.size(), settings_) / 2;
            if (half <= 0) continue;
            auto it = std::find_if(roundHelperPoints_.begin(), roundHelperPoints_.end(),
                                   [&](auto& kv) { return kv.first == h.helper; });
            if (it == roundHelperPoints_.end()) roundHelperPoints_.push_back({h.helper, half});
            else it->second += half;
        }
    }

    // Fold helper round points into cumulative totals.
    for (const auto& [hid, pts] : roundHelperPoints_)
        if (Player* hp = find(hid)) hp->scoreTotal += pts;

    // Build the score lines.
    ScoreUpdate su;
    for (const auto& p : players_) {
        int roundScore = 0;
        if (p.id == walkerId_) roundScore = ws.total;
        else {
            auto it = std::find_if(roundHelperPoints_.begin(), roundHelperPoints_.end(),
                                   [&](auto& kv) { return kv.first == p.id; });
            if (it != roundHelperPoints_.end()) roundScore = it->second;
        }
        su.scores.push_back({p.id, roundScore, p.scoreTotal});
    }

    pushRoomUpdate(r);
    r.out.push_back({Reach::Broadcast, kNoPlayer, PhaseStart{Phase::Score}});
    r.out.push_back({Reach::Broadcast, kNoPlayer, std::move(su)});
    r.timers.push_back(StartTimer{TIMER_SCORE, static_cast<uint32_t>(limits::kScoreRevealMs)});
}

void GameRoom::enterEliminationOrNext(uint64_t atMs, StepResult& r) {
    if (paused_) { awaitingResume_ = true; return; }  // hold the transition

    // Determine eliminations.
    std::vector<PlayerId> eliminatedNow;
    const bool elimThisRound =
        settings_.eliminationMode == EliminationMode::On ||
        (settings_.eliminationMode == EliminationMode::LastRound &&
         currentRound_ >= settings_.numRounds);
    if (elimThisRound) {
        // Rank active players ascending by score; eliminate the bottom N.
        std::vector<Player*> active;
        for (auto& p : players_)
            if (!p.eliminated && !p.spectator) active.push_back(&p);
        std::sort(active.begin(), active.end(),
                  [](Player* a, Player* b) { return a->scoreTotal < b->scoreTotal; });
        int n = settings_.eliminationsPerRound;
        for (Player* p : active) {
            if (n-- <= 0) break;
            if (static_cast<int>(active.size()) <= 1) break;  // never eliminate the last one here
            p->eliminated = true;
            p->spectator = (settings_.eliminatedBehaviour == EliminatedBehaviour::SpectateOnly);
            eliminatedNow.push_back(p->id);
        }
    }

    const bool ending = currentRound_ >= settings_.numRounds || activeCount() <= 1 ||
                        activeCount() < settings_.minPlayersToEnd;

    if (!eliminatedNow.empty()) {
        phase_ = Phase::Elimination;
        for (PlayerId id : eliminatedNow) {
            std::string name = find(id) ? find(id)->name : std::string{};
            r.out.push_back({Reach::Broadcast, kNoPlayer, PlayerEliminated{id}});
        }
        pushRoomUpdate(r);
        r.out.push_back({Reach::Broadcast, kNoPlayer, PhaseStart{Phase::Elimination}});
        r.timers.push_back(StartTimer{TIMER_ELIM, static_cast<uint32_t>(limits::kEliminationDisplayMs)});
        // onTimer(TIMER_ELIM) decides end-vs-next based on the same conditions.
        return;
    }

    if (ending) endGame(r);
    else enterPattern(atMs, /*reusePath=*/false, r);
}

void GameRoom::endGame(StepResult& r) {
    phase_ = Phase::Ended;
    pushRoomUpdate(r);
    r.out.push_back({Reach::Broadcast, kNoPlayer, PhaseStart{Phase::Ended}});
    r.out.push_back({Reach::Broadcast, kNoPlayer, GameEnded{leaderboard()}});
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
GameRoom::Player* GameRoom::find(PlayerId id) {
    for (auto& p : players_) if (p.id == id) return &p;
    return nullptr;
}

PlayerId GameRoom::chooseRandomWalker() {
    std::vector<PlayerId> eligible;
    for (auto& p : players_)
        if (!p.eliminated && !p.spectator) eligible.push_back(p.id);
    if (eligible.empty()) return kNoPlayer;
    std::uniform_int_distribution<size_t> d(0, eligible.size() - 1);
    return eligible[d(rng_)];
}

void GameRoom::assignHostIfNeeded(StepResult&) {
    if (hostId_ != kNoPlayer && find(hostId_)) return;
    Player* best = nullptr;
    for (auto& p : players_)
        if (!best || p.joinOrder < best->joinOrder) best = &p;
    if (best) { best->host = true; hostId_ = best->id; }
    else hostId_ = kNoPlayer;
}

int GameRoom::activeCount() const {
    int n = 0;
    for (const auto& p : players_) if (!p.eliminated && !p.spectator) ++n;
    return n;
}

std::vector<PlayerView> GameRoom::playerViews() const {
    std::vector<PlayerView> out;
    out.reserve(players_.size());
    for (const auto& p : players_) {
        PlayerView v;
        v.id = p.id;
        v.name = p.name;
        v.colour = p.colour;
        v.score = p.scoreTotal;
        v.lives = p.lives;
        v.isEliminated = p.eliminated;
        v.isWalker = (p.id == walkerId_);
        v.isSpectator = p.spectator;
        v.isHost = p.host;
        v.isReady = p.readyPressed;
        out.push_back(std::move(v));
    }
    return out;
}

void GameRoom::pushRoomUpdate(StepResult& r, Reach reach, PlayerId who) const {
    r.out.push_back({reach, who,
                     RoomUpdate{playerViews(), phase_, settings_, currentRound_,
                                settings_.numRounds, paused_}});
}

std::vector<LeaderboardEntry> GameRoom::leaderboard() const {
    std::vector<LeaderboardEntry> entries;
    for (const auto& p : players_)
        entries.push_back({p.id, p.name, p.scoreTotal, p.successfulWalks, p.hintsFollowed});
    // Build a join-order lookup for the final tiebreak.
    auto joinOrderOf = [&](PlayerId id) {
        for (const auto& p : players_) if (p.id == id) return p.joinOrder;
        return UINT32_MAX;
    };
    std::sort(entries.begin(), entries.end(), [&](const auto& a, const auto& b) {
        if (a.total != b.total) return a.total > b.total;
        if (a.successfulWalks != b.successfulWalks) return a.successfulWalks > b.successfulWalks;
        if (a.hintsFollowed != b.hintsFollowed) return a.hintsFollowed > b.hintsFollowed;
        return joinOrderOf(a.id) < joinOrderOf(b.id);  // earliest join wins ties
    });
    return entries;
}

}  // namespace mg
