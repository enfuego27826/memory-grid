// Test helpers for driving the core through the protocol.hpp seam — no sockets.
#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "core/game_room.hpp"
#include "protocol.hpp"

namespace mgtest {
using namespace mg;

// First event in the result whose body holds alternative T (or nullptr).
template <class T>
const T* firstEvent(const StepResult& r) {
    for (const auto& e : r.out)
        if (const T* p = std::get_if<T>(&e.body)) return p;
    return nullptr;
}

template <class T>
std::vector<const T*> allEvents(const StepResult& r) {
    std::vector<const T*> v;
    for (const auto& e : r.out)
        if (const T* p = std::get_if<T>(&e.body)) v.push_back(p);
    return v;
}

inline bool hasStartTimer(const StepResult& r, uint32_t id) {
    for (const auto& t : r.timers)
        if (const auto* p = std::get_if<StartTimer>(&t)) if (p->id == id) return true;
    return false;
}

inline bool hasCancelTimer(const StepResult& r, uint32_t id) {
    for (const auto& t : r.timers)
        if (const auto* p = std::get_if<CancelTimer>(&t)) if (p->id == id) return true;
    return false;
}

inline ClickTile clickFor(TileIndex t, int cols) {
    return ClickTile{static_cast<uint8_t>(t / cols), static_cast<uint8_t>(t % cols)};
}

}  // namespace mgtest
