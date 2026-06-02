#include <doctest/doctest.h>

#include <algorithm>

#include "core/hint_parser.hpp"
#include "core/path_gen.hpp"

using namespace mg;

namespace {
bool hasDir(const HintMatch& m, Direction d) {
    return std::find(m.directions.begin(), m.directions.end(), d) != m.directions.end();
}
}  // namespace

TEST_CASE("chess coordinate: letter is column, number is 1-based row") {
    auto m = parseHint("A3", 6, 6);
    REQUIRE(m.absolute.size() == 1);
    CHECK(m.absolute[0] == tileAt(2, 0, 6));   // row 3 → r=2, col A → c=0

    auto b = parseHint("b4", 6, 6);
    REQUIRE(b.absolute.size() == 1);
    CHECK(b.absolute[0] == tileAt(3, 1, 6));
}

TEST_CASE("explicit row/col phrase") {
    auto m = parseHint("try row 2 col 3", 6, 6);
    REQUIRE(m.absolute.size() == 1);
    CHECK(m.absolute[0] == tileAt(1, 2, 6));   // row 2 → r=1, col 3 → c=2
}

TEST_CASE("directional words") {
    CHECK(hasDir(parseHint("go right!", 6, 6), Direction::Right));
    CHECK(hasDir(parseHint("now go down", 6, 6), Direction::Down));
    auto tl = parseHint("top left", 6, 6);
    CHECK(hasDir(tl, Direction::Up));
    CHECK(hasDir(tl, Direction::Left));
}

TEST_CASE("out-of-bounds coordinates are ignored") {
    auto m = parseHint("Z9", 4, 4);            // col 25 / row 9 both out of range
    CHECK(m.absolute.empty());
    auto r = parseHint("row 9 col 9", 4, 4);
    CHECK(r.absolute.empty());
}

TEST_CASE("plain chatter yields no match") {
    CHECK(parseHint("nice one!!", 6, 6).empty());
}
