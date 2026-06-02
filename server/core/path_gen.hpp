// path_gen.hpp — generation of the secret safe path (pure logic).
//
// Produces a valid path across the grid: 4-adjacent steps (no diagonals), no
// revisited tiles (one-way, spec §7), starting on the start edge and ending on
// the opposite (finish) edge, with an exact target length. Three styles bias the
// shape; randomized backtracking guarantees validity regardless of style.
//
// RNG is injected (seeded std::mt19937&) so generation is deterministic and the
// validity invariants are unit-testable (build brief §8).
#pragma once

#include <random>

#include "core/config.hpp"
#include "core/safepath.hpp"

namespace mg {

// Coordinate helpers for flattened tile indices.
constexpr int rowOf(TileIndex t, int cols) { return t / cols; }
constexpr int colOf(TileIndex t, int cols) { return t % cols; }
constexpr TileIndex tileAt(int r, int c, int cols) {
    return static_cast<TileIndex>(r * cols + c);
}

// Generates a path of exactly `effectivePathLength(settings)` tiles for the
// given grid/style/edge. Returns a valid SafePath; on the (rare) event that the
// randomized search exhausts its budget, falls back to a guaranteed-valid direct
// path so generation never fails at runtime.
SafePath generatePath(const GameSettings& settings, std::mt19937& rng);

}  // namespace mg
