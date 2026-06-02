// wire.hpp — JSON ⇆ protocol translation. The transport owns the wire format
// end to end; the core never sees JSON. Field names mirror spec §15.
//
// SECURITY: there is deliberately NO serializer for SafePath anywhere. Only the
// PatternReveal event carries path tiles, and the core emits it only during the
// memorise phase. serializeEvent handles EventBody variants exclusively.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "protocol.hpp"

namespace mg::tx::wire {

// Caps enforced at the trust boundary (also guarded by the transport's payload cap).
constexpr std::size_t kMaxName = 32;
constexpr std::size_t kMaxChat = 280;

const char* phaseStr(mg::Phase);

// Settings: parse overlays provided fields onto presetFor(difficulty); serialize
// emits spec §15 field names. parse↔serialize round-trips.
mg::GameSettings parseSettings(const std::string& jsonText);  // tolerant; bad → medium preset
std::string settingsToJsonString(const mg::GameSettings&);    // for tests/inspection

// Join is handled specially (carries transport-level code/token/password).
struct JoinInfo {
    std::string code;
    std::string name;
    std::string password;
    std::string token;     // empty ⇒ fresh join; non-empty ⇒ reconnect attempt
};
std::optional<JoinInfo> parseJoin(std::string_view bytes);

// HTTP bodies (tolerant; missing fields use defaults).
struct CreateParams { mg::GameSettings settings; std::string password; };
CreateParams parseCreate(std::string_view body);          // POST /create
struct JoinHttp { std::string code, name, password; };
JoinHttp parseJoinHttp(std::string_view body);            // POST /join

// All other inbound commands. Returns nullopt on malformed/unknown/oversized/bad
// fields (the trust boundary rejects them before they reach the core).
std::optional<mg::Command> parseCommand(std::string_view bytes);

// Resolves a PlayerId to a display name for events whose EventBody carries only id.
using NameLookup = std::function<std::string_view(mg::PlayerId)>;

// Serialize one event to a wire JSON string. `cols` expands PatternReveal tile
// indices into [row,col] pairs.
std::string serializeEvent(const mg::EventBody&, const NameLookup&, int cols);

// Transport-synthesized messages (no core involvement).
std::string serializeJoined(mg::PlayerId, std::string_view token);
std::string serializeNotice(std::string_view text);  // {type:"system_notice",text}

}  // namespace mg::tx::wire
