#include <doctest/doctest.h>

#include <random>
#include <set>

#include "core/config.hpp"
#include "core/path_gen.hpp"

using namespace mg;

namespace {

// Validate every structural invariant a generated path must satisfy.
void checkValid(const std::vector<TileIndex>& path, const GameSettings& s) {
    const int rows = s.grid.rows, cols = s.grid.cols;
    REQUIRE(path.size() == static_cast<size_t>(effectivePathLength(s)));

    // Start on the start edge, finish on the opposite edge.
    if (s.startEdge == StartEdge::Left) {
        CHECK(colOf(path.front(), cols) == 0);
        CHECK(colOf(path.back(), cols) == cols - 1);
    } else {
        CHECK(rowOf(path.front(), cols) == 0);
        CHECK(rowOf(path.back(), cols) == rows - 1);
    }

    std::set<TileIndex> seen;
    for (size_t i = 0; i < path.size(); ++i) {
        // In bounds.
        CHECK(rowOf(path[i], cols) < rows);
        CHECK(colOf(path[i], cols) < cols);
        // No revisits.
        CHECK(seen.insert(path[i]).second);
        // 4-adjacency between consecutive tiles.
        if (i > 0) {
            int dr = std::abs(rowOf(path[i], cols) - rowOf(path[i - 1], cols));
            int dc = std::abs(colOf(path[i], cols) - colOf(path[i - 1], cols));
            CHECK(dr + dc == 1);
        }
    }
}

}  // namespace

TEST_CASE("path generation is valid across grids, styles and seeds") {
    for (PathStyle style : {PathStyle::Straight, PathStyle::Zigzag, PathStyle::Random}) {
        for (auto preset : {easyPreset(), mediumPreset(), hardPreset()}) {
            preset.pathStyle = style;
            for (uint32_t seed = 1; seed <= 20; ++seed) {
                std::mt19937 rng(seed);
                SafePath p = generatePath(preset, rng);
                checkValid(p.revealForMemorise(), preset);
            }
        }
    }
}

TEST_CASE("auto path length is ~60% of tiles, clamped to range") {
    GameSettings s;          // 6x6 default
    s.pathLength = -1;       // Auto
    CHECK(autoPathLength(s.grid) == 22);  // round(0.6*36)=22, within [3, 32]

    GameSettings big;
    big.grid = {10, 10};
    big.pathLength = -1;
    std::mt19937 rng(7);
    SafePath p = generatePath(big, rng);
    CHECK(p.size() == 60);   // 60% of 100
}

TEST_CASE("path generation works with top/bottom edges") {
    GameSettings s = mediumPreset();
    s.startEdge = StartEdge::Top;
    s.pathStyle = PathStyle::Random;
    for (uint32_t seed = 1; seed <= 10; ++seed) {
        std::mt19937 rng(seed);
        SafePath p = generatePath(s, rng);
        checkValid(p.revealForMemorise(), s);
    }
}
