#include "transport/rooms.hpp"

namespace mg::tx {

std::string RoomRegistry::generateCode() {
    // Legible alphabet: no 0/O/1/I.
    static const char* alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static const int n = 32;
    std::uniform_int_distribution<int> d(0, n - 1);
    for (int attempt = 0; attempt < 16; ++attempt) {
        std::string code;
        code.reserve(mg::limits::kRoomCodeLen);
        for (int i = 0; i < mg::limits::kRoomCodeLen; ++i) code.push_back(alphabet[d(codeRng_)]);
        if (!rooms_.count(code)) return code;
    }
    // Extremely unlikely fallthrough: append a counter to guarantee uniqueness.
    std::string code = "ROOM" + std::to_string(rooms_.size());
    return code;
}

std::string RoomRegistry::create(const mg::GameSettings& initial, std::string password,
                                 uint64_t nowMs) {
    std::string code = generateCode();
    auto ctx = std::make_unique<RoomCtx>();
    ctx->code = code;
    ctx->settings = initial;
    ctx->password = std::move(password);
    ctx->game = factory_(initial);
    ctx->lastActivityMs = nowMs;
    rooms_[code] = std::move(ctx);
    return code;
}

RoomCtx* RoomRegistry::lookup(const RoomKey& code) {
    auto it = rooms_.find(code);
    return it == rooms_.end() ? nullptr : it->second.get();
}

std::optional<mg::PlayerId> RoomRegistry::addPlayer(const RoomKey& code, const std::string& name) {
    RoomCtx* room = lookup(code);
    if (!room) return std::nullopt;
    if (room->players.size() >= static_cast<size_t>(mg::limits::kMaxPlayers)) return std::nullopt;
    const mg::PlayerId id = room->nextPlayerId++;
    PlayerSlot slot;
    slot.id = id;
    slot.name = name;
    slot.spectator = (room->phase != mg::Phase::Lobby);  // mid-game join ⇒ spectator
    room->players[id] = std::move(slot);
    return id;
}

}  // namespace mg::tx
