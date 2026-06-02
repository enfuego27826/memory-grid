#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "core/config.hpp"
#include "transport/wire.hpp"

using namespace mg;
using nlohmann::json;

namespace {
std::string ev(const EventBody& b, int cols = 6) {
    return tx::wire::serializeEvent(b, [](PlayerId) -> std::string_view { return "Zoe"; }, cols);
}
}  // namespace

TEST_CASE("parseCommand handles known message types") {
    auto c1 = tx::wire::parseCommand(R"({"type":"click_tile","row":2,"col":3})");
    REQUIRE(c1.has_value());
    REQUIRE(std::holds_alternative<ClickTile>(*c1));
    CHECK(std::get<ClickTile>(*c1).row == 2);
    CHECK(std::get<ClickTile>(*c1).col == 3);

    auto c2 = tx::wire::parseCommand(R"({"type":"send_chat","message":"go right"})");
    REQUIRE(std::holds_alternative<SendChat>(*c2));
    CHECK(std::get<SendChat>(*c2).text == "go right");

    auto c3 = tx::wire::parseCommand(R"({"type":"kick_player","target":4})");
    REQUIRE(std::holds_alternative<KickPlayer>(*c3));
    CHECK(std::get<KickPlayer>(*c3).target == 4);
}

TEST_CASE("parseCommand rejects malformed / unknown / bad fields") {
    CHECK(!tx::wire::parseCommand("not json").has_value());
    CHECK(!tx::wire::parseCommand(R"({"type":"bogus"})").has_value());
    CHECK(!tx::wire::parseCommand(R"({"no":"type"})").has_value());
    CHECK(!tx::wire::parseCommand(R"({"type":"click_tile","row":"x"})").has_value());
    CHECK(!tx::wire::parseCommand(R"({"type":"send_chat","message":""})").has_value());
}

TEST_CASE("parseJoin extracts code/name/token") {
    auto j = tx::wire::parseJoin(R"({"type":"join_room","code":"ABC123","name":"Bob"})");
    REQUIRE(j.has_value());
    CHECK(j->code == "ABC123");
    CHECK(j->name == "Bob");
    CHECK(j->token.empty());
    CHECK(!tx::wire::parseJoin(R"({"type":"click_tile"})").has_value());
}

TEST_CASE("serializeEvent emits spec type tags and fields") {
    auto u = json::parse(ev(RoomUpdate{{}, Phase::Walk, mediumPreset(), 2, 5, false}));
    CHECK(u["type"] == "room_update");
    CHECK(u["state"] == "walk");
    CHECK(u["currentRound"] == 2);

    auto t = json::parse(ev(TileResult{true, 2}));
    CHECK(t["type"] == "tile_result");
    CHECK(t["correct"] == true);
    CHECK(t["lives"] == 2);

    // pattern_reveal expands tile indices to [row,col] pairs.
    auto p = json::parse(ev(PatternReveal{{0, 1, 7}, FlashMode::Solid}, /*cols=*/6));
    CHECK(p["type"] == "pattern_reveal");
    CHECK(p["path"][0] == json::array({0, 0}));
    CHECK(p["path"][1] == json::array({0, 1}));
    CHECK(p["path"][2] == json::array({1, 1}));  // index 7 on a 6-wide grid

    // walker_assigned resolves the name via the lookup.
    auto w = json::parse(ev(WalkerAssigned{3, false, 0, 0, 5, 5}));
    CHECK(w["type"] == "walker_assigned");
    CHECK(w["name"] == "Zoe");

    // score_update carries the breakdown fields.
    ScoreUpdate su;
    su.scores.push_back({1, 850, 850, 500, 300, 50, 0, 0, 0});
    auto s = json::parse(ev(su));
    CHECK(s["scores"][0]["base"] == 500);
    CHECK(s["scores"][0]["speed"] == 300);
}

TEST_CASE("settings round-trip through parse/serialize") {
    GameSettings in = hardPreset();
    GameSettings out = tx::wire::parseSettings(tx::wire::settingsToJsonString(in));
    CHECK(out.grid.rows == in.grid.rows);
    CHECK(out.memoriseTimeSec == in.memoriseTimeSec);
    CHECK(out.livesPerWalker == in.livesPerWalker);
    CHECK(out.eliminationMode == in.eliminationMode);
    CHECK(out.chatRateLimitMs == in.chatRateLimitMs);
}
