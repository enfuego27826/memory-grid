#include "core/path_gen.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace mg {
namespace {

struct GridGeom {
    int rows, cols;
    StartEdge edge;

    // Distance (in steps) from a tile to the nearest cell on the finish edge.
    int distToFinishEdge(TileIndex t) const {
        if (edge == StartEdge::Left)   return (cols - 1) - colOf(t, cols);  // finish = right
        return (rows - 1) - rowOf(t, cols);                                 // finish = bottom
    }
    bool onFinishEdge(TileIndex t) const { return distToFinishEdge(t) == 0; }

    // Net progress along the start→finish axis (how "deep" we are).
    int progress(TileIndex t) const {
        if (edge == StartEdge::Left) return colOf(t, cols);
        return rowOf(t, cols);
    }
};

// 4-neighbours of a tile within bounds.
std::vector<TileIndex> neighbours(TileIndex t, const GridGeom& g) {
    int r = rowOf(t, g.cols), c = colOf(t, g.cols);
    std::vector<TileIndex> out;
    out.reserve(4);
    if (r > 0)            out.push_back(tileAt(r - 1, c, g.cols));
    if (r + 1 < g.rows)   out.push_back(tileAt(r + 1, c, g.cols));
    if (c > 0)            out.push_back(tileAt(r, c - 1, g.cols));
    if (c + 1 < g.cols)   out.push_back(tileAt(r, c + 1, g.cols));
    return out;
}

// Order neighbours according to the path style. The order is a heuristic bias
// only — the backtracking search below preserves validity for any ordering.
void orderByStyle(std::vector<TileIndex>& ns, const GridGeom& g,
                  PathStyle style, std::mt19937& rng) {
    std::shuffle(ns.begin(), ns.end(), rng);
    if (style == PathStyle::Random) return;

    if (style == PathStyle::Straight) {
        // Prefer tiles that advance toward the finish edge → direct-looking path.
        std::stable_sort(ns.begin(), ns.end(), [&](TileIndex a, TileIndex b) {
            return g.distToFinishEdge(a) < g.distToFinishEdge(b);
        });
    } else {  // Zigzag: prefer NOT advancing (lateral moves) to create serpentine
              // sweeps; advancing is the fallback when laterals are exhausted.
        std::stable_sort(ns.begin(), ns.end(), [&](TileIndex a, TileIndex b) {
            return g.distToFinishEdge(a) > g.distToFinishEdge(b);
        });
    }
}

// Randomized DFS with Manhattan pruning. Builds `path` to exactly `target`
// tiles, ending on the finish edge. Returns true on success.
bool search(std::vector<TileIndex>& path, std::vector<char>& visited,
            const GridGeom& g, int target, PathStyle style, std::mt19937& rng,
            int& budget) {
    if (--budget < 0) return false;

    const int len = static_cast<int>(path.size());
    if (len == target) return g.onFinishEdge(path.back());

    const TileIndex cur = path.back();
    const int stepsLeft = target - len;  // tiles still to place after `cur`
    // Prune: must be able to still reach the finish edge with the steps left.
    if (g.distToFinishEdge(cur) > stepsLeft) return false;

    auto ns = neighbours(cur, g);
    orderByStyle(ns, g, style, rng);
    for (TileIndex n : ns) {
        if (visited[n]) continue;
        visited[n] = 1;
        path.push_back(n);
        if (search(path, visited, g, target, style, rng, budget)) return true;
        path.pop_back();
        visited[n] = 0;
    }
    return false;
}

// Guaranteed-valid fallback: walk straight to the finish edge, padding length
// with a simple perpendicular detour if a longer path was requested. Always
// produces a valid (adjacent, non-revisiting, edge-to-edge) path.
SafePath directFallback(const GridGeom& g, int target, std::mt19937& rng) {
    std::vector<TileIndex> path;
    std::vector<char> visited(static_cast<size_t>(g.rows) * g.cols, 0);
    auto push = [&](TileIndex t) { path.push_back(t); visited[t] = 1; };

    // Start cell on the start edge.
    std::uniform_int_distribution<int> pick(
        0, (g.edge == StartEdge::Left ? g.rows : g.cols) - 1);
    int lane = pick(rng);
    TileIndex start = g.edge == StartEdge::Left ? tileAt(lane, 0, g.cols)
                                                : tileAt(0, lane, g.cols);
    push(start);

    auto tryStep = [&](int dr, int dc) -> bool {
        int r = rowOf(path.back(), g.cols) + dr;
        int c = colOf(path.back(), g.cols) + dc;
        if (r < 0 || c < 0 || r >= g.rows || c >= g.cols) return false;
        TileIndex t = tileAt(r, c, g.cols);
        if (visited[t]) return false;
        push(t);
        return true;
    };

    // Advance one step toward the finish edge.
    auto advance = [&]() { return g.edge == StartEdge::Left ? tryStep(0, 1)
                                                            : tryStep(1, 0); };
    // A lateral wiggle (perpendicular to progress) to consume length.
    auto wiggle = [&]() {
        return g.edge == StartEdge::Left ? (tryStep(1, 0) || tryStep(-1, 0))
                                         : (tryStep(0, 1) || tryStep(0, -1));
    };

    while (!g.onFinishEdge(path.back()) ||
           static_cast<int>(path.size()) < target) {
        if (static_cast<int>(path.size()) < target && !g.onFinishEdge(path.back())) {
            // Room to spare: occasionally wiggle, else advance.
            if (!(wiggle() || advance())) break;
        } else if (!g.onFinishEdge(path.back())) {
            if (!advance()) { if (!wiggle()) break; }
        } else {
            break;  // on finish edge and length satisfied
        }
        if (static_cast<int>(path.size()) > target) break;
    }
    return SafePath(std::move(path));
}

}  // namespace

SafePath generatePath(const GameSettings& settings, std::mt19937& rng) {
    GridGeom g{settings.grid.rows, settings.grid.cols, settings.startEdge};
    const int target = effectivePathLength(settings);

    const int laneCount = (g.edge == StartEdge::Left ? g.rows : g.cols);
    std::uniform_int_distribution<int> laneDist(0, laneCount - 1);

    constexpr int kAttempts = 64;
    for (int attempt = 0; attempt < kAttempts; ++attempt) {
        int lane = laneDist(rng);
        TileIndex start = g.edge == StartEdge::Left ? tileAt(lane, 0, g.cols)
                                                    : tileAt(0, lane, g.cols);
        std::vector<TileIndex> path{start};
        std::vector<char> visited(static_cast<size_t>(g.rows) * g.cols, 0);
        visited[start] = 1;
        int budget = 200000;  // node-visit cap to bound worst-case search
        if (search(path, visited, g, target, settings.pathStyle, rng, budget))
            return SafePath(std::move(path));
    }
    // Search exhausted (very rare for sane settings) — never fail at runtime.
    return directFallback(g, target, rng);
}

}  // namespace mg
