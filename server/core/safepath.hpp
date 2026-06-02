// safepath.hpp — the secret safe path.
//
// SECURITY / path-secrecy (build brief §6, spec §12):
//   The safe path must never reach a client during the walk, and must never be
//   serializable as itself. This type is the structural enforcement of that:
//
//   1. It is MOVE-ONLY (copy deleted). You cannot accidentally fan it out.
//   2. It exposes NO accessor that hands back the raw tile vector for general
//      use. The ONLY method that yields the full sequence is
//      revealForMemorise(), named so any misuse is obvious in review, and it is
//      called from exactly one place: building the PatternReveal event while the
//      room is in the memorise phase.
//   3. There is intentionally NO to_json(SafePath) overload anywhere in the
//      transport. Attempting to serialize a SafePath is therefore a COMPILE
//      ERROR — the type system makes "path on the wire" impossible except via
//      the one sanctioned gate above.
//
// Everything else the game needs (click validation, hint zone lookup) is served
// by narrow queries that never expose the whole sequence.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mg {

// A tile is identified by its flattened index = row * cols + col (uint16_t is
// plenty: max grid 12×12 = 144 tiles).
using TileIndex = uint16_t;

class SafePath {
public:
    SafePath() = default;
    explicit SafePath(std::vector<TileIndex> tiles) : tiles_(std::move(tiles)) {}

    // Move-only: see security note above.
    SafePath(const SafePath&) = delete;
    SafePath& operator=(const SafePath&) = delete;
    SafePath(SafePath&&) = default;
    SafePath& operator=(SafePath&&) = default;

    std::size_t size() const { return tiles_.size(); }
    bool empty() const { return tiles_.empty(); }

    // The tile expected at a given step index (0-based). Used for click
    // validation: the walker's next correct tile is at(progress).
    TileIndex at(std::size_t step) const { return tiles_[step]; }

    TileIndex start() const { return tiles_.front(); }
    TileIndex finish() const { return tiles_.back(); }

    // Position of a tile within the path, if present. Used by hint scoring to
    // determine the tile's zone, and to tell on-path tiles from off-path.
    std::optional<std::size_t> indexOf(TileIndex tile) const {
        for (std::size_t i = 0; i < tiles_.size(); ++i)
            if (tiles_[i] == tile) return i;
        return std::nullopt;
    }
    bool contains(TileIndex tile) const { return indexOf(tile).has_value(); }

    // THE ONE SANCTIONED GATE. Returns a copy of the full sequence for rendering
    // during the memorise phase only. Call sites must be in the Pattern phase.
    // SECURITY: do not add other full-sequence accessors, and never write a
    // serializer for SafePath itself.
    std::vector<TileIndex> revealForMemorise() const { return tiles_; }

private:
    std::vector<TileIndex> tiles_;
};

}  // namespace mg
