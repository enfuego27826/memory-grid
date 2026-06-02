// hint_parser.hpp — loose, hand-rolled parsing of chat hints into candidate
// tiles / directions (spec §8). NO std::regex (build brief §7).
//
// Recognised forms (case-insensitive):
//   chess coords  "A3", "b4"      → column letter + 1-based row number
//   explicit      "row 2 col 3"   → 1-based row & col
//   directional   "go right", "go down", "up", "top", "bottom", "left"
//   corners       "top left"      → the union of its direction words
//
// DECISION: chess-style coords follow the chess convention — the LETTER is the
// column (A = leftmost) and the NUMBER is the 1-based row from the top. This
// matches the explicit "row N col M" being row-first while keeping "A3"
// column-first, which is how players read a lettered grid.
#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "core/safepath.hpp"

namespace mg {

enum class Direction { Up, Down, Left, Right };

struct HintMatch {
    std::vector<TileIndex> absolute;     // explicit coordinate candidates (in-bounds)
    std::vector<Direction> directions;   // relative directions (resolved vs walker later)
    bool empty() const { return absolute.empty() && directions.empty(); }
};

// Parse a chat message against a grid of the given dimensions. Absolute
// candidates are bounds-checked; directions are returned raw for the caller to
// resolve against the walker's current position.
HintMatch parseHint(std::string_view text, int rows, int cols);

}  // namespace mg
