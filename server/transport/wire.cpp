#include "transport/wire.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "core/path_gen.hpp"  // rowOf/colOf for PatternReveal expansion

using nlohmann::json;
using namespace mg;

namespace mg::tx::wire {
namespace {

// ---- enum <-> string maps -------------------------------------------------
const char* difficultyStr(Difficulty d) {
    switch (d) { case Difficulty::Easy: return "easy"; case Difficulty::Medium: return "medium";
                 case Difficulty::Hard: return "hard"; case Difficulty::Custom: return "custom"; }
    return "medium";
}
Difficulty difficultyOf(const std::string& s) {
    if (s == "easy") return Difficulty::Easy;
    if (s == "hard") return Difficulty::Hard;
    if (s == "custom") return Difficulty::Custom;
    return Difficulty::Medium;
}
const char* pathStyleStr(PathStyle s) {
    switch (s) { case PathStyle::Straight: return "straight"; case PathStyle::Zigzag: return "zigzag";
                 case PathStyle::Random: return "random"; }
    return "random";
}
const char* flashStr(FlashMode m) {
    switch (m) { case FlashMode::Solid: return "solid"; case FlashMode::Flashing: return "flashing";
                 case FlashMode::Fading: return "fading"; }
    return "solid";
}
const char* walkerSelStr(WalkerSelection w) {
    return w == WalkerSelection::Host ? "host" : "first_press";
}
const char* wrongPenaltyStr(WrongStepPenalty p) {
    return p == WrongStepPenalty::Eliminate ? "eliminate" : "life_and_reset";
}
const char* elimModeStr(EliminationMode m) {
    switch (m) { case EliminationMode::Off: return "off"; case EliminationMode::On: return "on";
                 case EliminationMode::LastRound: return "last_round"; }
    return "off";
}
const char* elimBehaviourStr(EliminatedBehaviour b) {
    return b == EliminatedBehaviour::StayNoWalk ? "stay" : "spectate";
}
const char* escalationStr(EscalationStep e) {
    switch (e) { case EscalationStep::GridPlusOne: return "grid";
                 case EscalationStep::MinusFiveMemorise: return "memorise";
                 case EscalationStep::Both: return "both"; }
    return "grid";
}
const char* startEdgeStr(StartEdge e) { return e == StartEdge::Top ? "top" : "left"; }

template <class T>
T getOr(const json& j, const char* key, T fallback) {
    if (auto it = j.find(key); it != j.end() && !it->is_null()) {
        try { return it->get<T>(); } catch (...) { return fallback; }
    }
    return fallback;
}

GameSettings validateAndNormalizeSettings(GameSettings s) {
    s.grid.rows = std::clamp(s.grid.rows, 4, 12);
    s.grid.cols = std::clamp(s.grid.cols, 4, 12);
    const int maxPath = std::max(3, s.grid.tiles() - 4);
    if (s.pathLength >= 0) s.pathLength = std::clamp(s.pathLength, 3, maxPath);

    s.memoriseTimeSec = std::clamp(s.memoriseTimeSec, 5, 120);
    s.livesPerWalker = std::clamp(s.livesPerWalker, 1, 10);
    s.wrongStepDeduction = std::clamp(s.wrongStepDeduction, 0, 100000);
    s.eliminationsPerRound = std::clamp(s.eliminationsPerRound, 0, limits::kMaxPlayers - 1);
    s.minPlayersToEnd = std::clamp(s.minPlayersToEnd, 1, limits::kMaxPlayers);
    s.basePoints = std::clamp(s.basePoints, 0, 100000);
    s.maxSpeedBonus = std::clamp(s.maxSpeedBonus, 0, 100000);
    s.maxVolunteerBonus = std::clamp(s.maxVolunteerBonus, 0, 100000);
    s.hintPointsEarly = std::clamp(s.hintPointsEarly, 0, 100000);
    s.hintPointsMid = std::clamp(s.hintPointsMid, 0, 100000);
    s.hintPointsLate = std::clamp(s.hintPointsLate, 0, 100000);
    s.numRounds = std::clamp(s.numRounds, 1, 50);
    s.chatRateLimitMs = std::clamp(s.chatRateLimitMs, 250, 10000);
    return s;
}

GameSettings parseSettingsJson(const json& j) {
    GameSettings s = presetFor(difficultyOf(getOr<std::string>(j, "difficulty", "medium")));
    if (auto g = j.find("gridSize"); g != j.end() && g->is_object()) {
        s.grid.rows = getOr<int>(*g, "rows", s.grid.rows);
        s.grid.cols = getOr<int>(*g, "cols", s.grid.cols);
    }
    // pathLength may be a number or "auto".
    if (auto p = j.find("pathLength"); p != j.end()) {
        if (p->is_number_integer()) s.pathLength = p->get<int>();
        else if (p->is_string() && p->get<std::string>() == "auto") s.pathLength = -1;
    }
    if (auto v = j.find("pathStyle"); v != j.end() && v->is_string()) {
        const std::string ps = *v;
        s.pathStyle = ps == "straight" ? PathStyle::Straight
                    : ps == "zigzag"   ? PathStyle::Zigzag : PathStyle::Random;
    }
    if (auto v = j.find("startEdge"); v != j.end() && v->is_string())
        s.startEdge = (v->get<std::string>() == "top") ? StartEdge::Top : StartEdge::Left;
    s.memoriseTimeSec = getOr<int>(j, "memoriseTime", s.memoriseTimeSec);
    if (auto v = j.find("patternFlashMode"); v != j.end() && v->is_string()) {
        const std::string f = *v;
        s.flashMode = f == "flashing" ? FlashMode::Flashing
                    : f == "fading"   ? FlashMode::Fading : FlashMode::Solid;
    }
    if (auto v = j.find("walkerSelection"); v != j.end() && v->is_string())
        s.walkerSelection = (v->get<std::string>() == "host") ? WalkerSelection::Host
                                                              : WalkerSelection::FirstPress;
    s.walkerCanChat = getOr<bool>(j, "walkerCanChat", s.walkerCanChat);
    s.livesPerWalker = getOr<int>(j, "livesPerWalker", s.livesPerWalker);
    if (auto v = j.find("wrongStepPenalty"); v != j.end() && v->is_string())
        s.wrongStepPenalty = (v->get<std::string>() == "eliminate") ? WrongStepPenalty::Eliminate
                                                                    : WrongStepPenalty::LifeAndReset;
    s.wrongStepDeduction = getOr<int>(j, "wrongStepDeduction", s.wrongStepDeduction);
    s.lifeRefillPerRound = getOr<bool>(j, "lifeRefill", s.lifeRefillPerRound);
    if (auto v = j.find("eliminationMode"); v != j.end() && v->is_string()) {
        const std::string e = *v;
        s.eliminationMode = e == "on" ? EliminationMode::On
                          : e == "last_round" ? EliminationMode::LastRound : EliminationMode::Off;
    }
    s.eliminationsPerRound = getOr<int>(j, "eliminationsPerRound", s.eliminationsPerRound);
    if (auto v = j.find("eliminatedPlayers"); v != j.end() && v->is_string())
        s.eliminatedBehaviour = (v->get<std::string>() == "stay") ? EliminatedBehaviour::StayNoWalk
                                                                  : EliminatedBehaviour::SpectateOnly;
    s.minPlayersToEnd = getOr<int>(j, "minPlayers", s.minPlayersToEnd);
    s.basePoints = getOr<int>(j, "basePoints", s.basePoints);
    s.maxSpeedBonus = getOr<int>(j, "maxSpeedBonus", s.maxSpeedBonus);
    s.maxVolunteerBonus = getOr<int>(j, "maxVolunteerBonus", s.maxVolunteerBonus);
    s.hintPointsEarly = getOr<int>(j, "hintPointsEarly", s.hintPointsEarly);
    s.hintPointsMid = getOr<int>(j, "hintPointsMid", s.hintPointsMid);
    s.hintPointsLate = getOr<int>(j, "hintPointsLate", s.hintPointsLate);
    s.numRounds = getOr<int>(j, "numRounds", s.numRounds);
    s.difficultyEscalation = getOr<bool>(j, "difficultyEscalation", s.difficultyEscalation);
    if (auto v = j.find("escalationStep"); v != j.end() && v->is_string()) {
        const std::string e = *v;
        s.escalationStep = e == "memorise" ? EscalationStep::MinusFiveMemorise
                         : e == "both" ? EscalationStep::Both : EscalationStep::GridPlusOne;
    }
    s.chatRateLimitMs = getOr<int>(j, "chatRateLimit", s.chatRateLimitMs);
    return validateAndNormalizeSettings(s);
}

json settingsToJson(const GameSettings& s) {
    return json{
        {"difficulty", difficultyStr(s.difficulty)},
        {"gridSize", {{"rows", s.grid.rows}, {"cols", s.grid.cols}}},
        {"pathLength", effectivePathLength(s)},
        {"pathLengthAuto", s.pathLength < 0},
        {"pathStyle", pathStyleStr(s.pathStyle)},
        {"startEdge", startEdgeStr(s.startEdge)},
        {"memoriseTime", s.memoriseTimeSec},
        {"patternFlashMode", flashStr(s.flashMode)},
        {"walkerSelection", walkerSelStr(s.walkerSelection)},
        {"walkerCanChat", s.walkerCanChat},
        {"livesPerWalker", s.livesPerWalker},
        {"wrongStepPenalty", wrongPenaltyStr(s.wrongStepPenalty)},
        {"wrongStepDeduction", s.wrongStepDeduction},
        {"lifeRefill", s.lifeRefillPerRound},
        {"eliminationMode", elimModeStr(s.eliminationMode)},
        {"eliminationsPerRound", s.eliminationsPerRound},
        {"eliminatedPlayers", elimBehaviourStr(s.eliminatedBehaviour)},
        {"minPlayers", s.minPlayersToEnd},
        {"basePoints", s.basePoints},
        {"maxSpeedBonus", s.maxSpeedBonus},
        {"maxVolunteerBonus", s.maxVolunteerBonus},
        {"hintPointsEarly", s.hintPointsEarly},
        {"hintPointsMid", s.hintPointsMid},
        {"hintPointsLate", s.hintPointsLate},
        {"numRounds", s.numRounds},
        {"difficultyEscalation", s.difficultyEscalation},
        {"escalationStep", escalationStr(s.escalationStep)},
        {"chatRateLimit", s.chatRateLimitMs},
    };
}

json playerViewJson(const PlayerView& p) {
    return json{{"id", p.id}, {"name", p.name}, {"colour", p.colour}, {"score", p.score},
                {"lives", p.lives}, {"isEliminated", p.isEliminated}, {"isWalker", p.isWalker},
                {"isSpectator", p.isSpectator}, {"isHost", p.isHost}, {"isReady", p.isReady}};
}

}  // namespace

const char* phaseStr(Phase p) {
    switch (p) {
        case Phase::Lobby: return "lobby";       case Phase::Pattern: return "pattern";
        case Phase::Walk: return "walk";         case Phase::Score: return "score";
        case Phase::Elimination: return "elimination"; case Phase::Ended: return "ended";
    }
    return "lobby";
}

GameSettings parseSettings(const std::string& text) {
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return mediumPreset();
    return parseSettingsJson(j);
}

std::string settingsToJsonString(const GameSettings& s) { return settingsToJson(s).dump(); }

std::optional<JoinInfo> parseJoin(std::string_view bytes) {
    json j = json::parse(bytes, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    if (getOr<std::string>(j, "type", "") != "join_room") return std::nullopt;
    JoinInfo info;
    info.code = getOr<std::string>(j, "code", "");
    info.name = getOr<std::string>(j, "name", "");
    info.password = getOr<std::string>(j, "password", "");
    info.token = getOr<std::string>(j, "token", "");
    if (info.name.size() > kMaxName) info.name.resize(kMaxName);
    if (info.code.empty()) return std::nullopt;
    if (info.token.empty() && info.name.empty()) return std::nullopt;
    return info;
}

CreateParams parseCreate(std::string_view body) {
    CreateParams p;
    p.settings = mediumPreset();
    json j = json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return p;
    // settings may be nested under "settings" or provided at top level.
    if (auto s = j.find("settings"); s != j.end() && s->is_object()) p.settings = parseSettingsJson(*s);
    else p.settings = parseSettingsJson(j);
    p.password = getOr<std::string>(j, "password", "");
    return p;
}

JoinHttp parseJoinHttp(std::string_view body) {
    JoinHttp h;
    json j = json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return h;
    h.code = getOr<std::string>(j, "code", "");
    h.name = getOr<std::string>(j, "name", "");
    h.password = getOr<std::string>(j, "password", "");
    return h;
}

std::optional<Command> parseCommand(std::string_view bytes) {
    json j = json::parse(bytes, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    const std::string type = getOr<std::string>(j, "type", "");
    if (type.empty()) return std::nullopt;

    if (type == "press_ready_walk") return PressReady{};
    if (type == "start_game")       return StartGame{};
    if (type == "tab_switched")     return TabSwitched{};
    if (type == "skip_round")       return SkipRound{};
    if (type == "end_game")         return EndGameEarly{};
    if (type == "pause_toggle")     return PauseToggle{};
    if (type == "click_tile") {
        if (!j.contains("row") || !j.contains("col")) return std::nullopt;
        if (!j["row"].is_number_integer() || !j["col"].is_number_integer()) return std::nullopt;
        int row = j["row"], col = j["col"];
        if (row < 0 || row > 255 || col < 0 || col > 255) return std::nullopt;
        return ClickTile{static_cast<uint8_t>(row), static_cast<uint8_t>(col)};
    }
    if (type == "send_chat") {
        if (!j.contains("message") || !j["message"].is_string()) return std::nullopt;
        std::string m = j["message"];
        if (m.empty() || m.size() > kMaxChat) return std::nullopt;
        return SendChat{std::move(m)};
    }
    if (type == "configure_room") {
        if (!j.contains("settings") || !j["settings"].is_object()) return std::nullopt;
        return ConfigureRoom{parseSettingsJson(j["settings"])};
    }
    if (type == "kick_player") {
        if (!j.contains("target") || !j["target"].is_number_integer()) return std::nullopt;
        return KickPlayer{static_cast<PlayerId>(j["target"].get<long long>())};
    }
    if (type == "transfer_host") {
        if (!j.contains("target") || !j["target"].is_number_integer()) return std::nullopt;
        return TransferHost{static_cast<PlayerId>(j["target"].get<long long>())};
    }
    if (type == "join_room") {
        std::string name = getOr<std::string>(j, "name", "");
        if (name.size() > kMaxName) name.resize(kMaxName);
        return JoinRoom{std::move(name)};
    }
    return std::nullopt;  // unknown type
}

std::string serializeEvent(const EventBody& body, const NameLookup& nameOf, int cols) {
    json j;
    std::visit([&](auto&& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, PhaseStart>) {
            j = {{"type", "phase_start"}, {"phase", phaseStr(e.phase)}};
        } else if constexpr (std::is_same_v<T, PatternReveal>) {
            json path = json::array();
            for (TileIndex t : e.path) path.push_back({rowOf(t, cols), colOf(t, cols)});
            j = {{"type", "pattern_reveal"}, {"path", path}, {"flashMode", flashStr(e.flashMode)}};
        } else if constexpr (std::is_same_v<T, WalkerAssigned>) {
            j = {{"type", "walker_assigned"}, {"playerId", e.id},
                 {"name", std::string(nameOf(e.id))}, {"wasRandom", e.wasRandom},
                 {"startRow", e.startRow}, {"startCol", e.startCol},
                 {"finishRow", e.finishRow}, {"finishCol", e.finishCol}};
        } else if constexpr (std::is_same_v<T, TileResult>) {
            j = {{"type", "tile_result"}, {"correct", e.correct}, {"lives", e.lives}};
        } else if constexpr (std::is_same_v<T, WalkerPosition>) {
            j = {{"type", "walker_position"}, {"row", e.row}, {"col", e.col}};
        } else if constexpr (std::is_same_v<T, WalkerReset>) {
            j = {{"type", "walker_reset"}, {"lives", e.lives}};
        } else if constexpr (std::is_same_v<T, ChatBroadcast>) {
            j = {{"type", "chat_message"}, {"from", e.from}, {"name", std::string(nameOf(e.from))},
                 {"message", e.text}, {"isSpectator", e.spectator}};
        } else if constexpr (std::is_same_v<T, HintScored>) {
            j = {{"type", "hint_scored"}, {"helperId", e.helper}, {"points", e.points}};
        } else if constexpr (std::is_same_v<T, ScoreUpdate>) {
            json arr = json::array();
            for (const auto& s : e.scores)
                arr.push_back({{"playerId", s.id}, {"roundScore", s.roundScore}, {"total", s.total},
                               {"base", s.base}, {"speed", s.speed}, {"volunteer", s.volunteer},
                               {"deductions", s.deductions}, {"hintsFollowed", s.hintsFollowed},
                               {"hintsIgnored", s.hintsIgnored}});
            j = {{"type", "score_update"}, {"scores", arr}};
        } else if constexpr (std::is_same_v<T, PlayerEliminated>) {
            j = {{"type", "player_eliminated"}, {"playerId", e.id}, {"name", std::string(nameOf(e.id))}};
        } else if constexpr (std::is_same_v<T, GameEnded>) {
            json arr = json::array();
            for (const auto& l : e.leaderboard)
                arr.push_back({{"playerId", l.id}, {"name", l.name}, {"total", l.total},
                               {"successfulWalks", l.successfulWalks}, {"hintsFollowed", l.hintsFollowed}});
            j = {{"type", "game_ended"}, {"leaderboard", arr}};
        } else if constexpr (std::is_same_v<T, RoomUpdate>) {
            json players = json::array();
            for (const auto& p : e.players) players.push_back(playerViewJson(p));
            j = {{"type", "room_update"}, {"players", players}, {"state", phaseStr(e.state)},
                 {"settings", settingsToJson(e.settings)}, {"currentRound", e.currentRound},
                 {"totalRounds", e.totalRounds}, {"paused", e.paused}};
        } else if constexpr (std::is_same_v<T, TabSwitchNotice>) {
            j = {{"type", "tab_switch_notice"}, {"playerId", e.id}, {"name", std::string(nameOf(e.id))},
                 {"offence", e.offence}};
        } else if constexpr (std::is_same_v<T, SystemNotice>) {
            j = {{"type", "system_notice"}, {"text", e.text}};
        }
    }, body);
    return j.dump();
}

std::string serializeJoined(PlayerId id, std::string_view token) {
    return json{{"type", "joined"}, {"playerId", id}, {"token", std::string(token)}}.dump();
}

std::string serializeNotice(std::string_view text) {
    return json{{"type", "system_notice"}, {"text", std::string(text)}}.dump();
}

}  // namespace mg::tx::wire
