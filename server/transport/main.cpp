// main.cpp — wires the real GameRoom + uWebSockets transport together.
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <random>

#include "core/game_room.hpp"
#include "transport/dispatch.hpp"
#include "transport/itransport.hpp"
#include "transport/rooms.hpp"
#include "transport/sessions.hpp"

namespace mg::tx {
std::unique_ptr<ITransport> makeUwsTransport(ITransportCallbacks&);  // uws_transport.cpp
}

int main(int argc, char** argv) {
    using namespace mg;
    using namespace mg::tx;

    const uint16_t port = (argc > 1) ? static_cast<uint16_t>(std::atoi(argv[1])) : 9001;

    std::random_device rd;
    RoomFactory factory = [seedSrc = std::mt19937{rd()}](const GameSettings& s) mutable {
        return std::make_unique<GameRoom>(seedSrc(), s);
    };

    RoomRegistry registry(std::move(factory));
    SessionStore sessions;
    auto clock = [] {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    };

    Dispatcher dispatch(registry, sessions, clock);
    auto transport = makeUwsTransport(dispatch);
    dispatch.setTransport(transport.get());
    transport->run(port);
    return 0;
}
