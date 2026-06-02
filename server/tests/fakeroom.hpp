// FakeRoom — a scriptable IGameRoom for transport tests (no real game logic).
#pragma once

#include <functional>
#include <tuple>
#include <vector>

#include "protocol.hpp"

namespace mgtest {
using namespace mg;

struct FakeRoom : IGameRoom {
    std::vector<std::tuple<PlayerId, Command, uint64_t>> applies;
    std::vector<std::pair<uint32_t, uint64_t>> timerCalls;
    std::function<StepResult(PlayerId, const Command&, uint64_t)> onApply;
    std::function<StepResult(uint32_t, uint64_t)> onTimerFn;

    StepResult apply(PlayerId from, const Command& cmd, uint64_t atMs) override {
        applies.push_back({from, cmd, atMs});
        return onApply ? onApply(from, cmd, atMs) : StepResult{};
    }
    StepResult onTimer(uint32_t id, uint64_t atMs) override {
        timerCalls.push_back({id, atMs});
        return onTimerFn ? onTimerFn(id, atMs) : StepResult{};
    }
};

}  // namespace mgtest
