#include "core/hint_parser.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "core/path_gen.hpp"  // tileAt

namespace mg {
namespace {

bool isDigits(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

// Split into lowercase alphanumeric tokens (runs of [a-z0-9]); everything else
// is a separator. "A3" stays one token; "row 2" becomes ["row","2"].
std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> toks;
    std::string cur;
    for (char ch : text) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) {
            cur.push_back(static_cast<char>(std::tolower(c)));
        } else if (!cur.empty()) {
            toks.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) toks.push_back(cur);
    return toks;
}

void addAbsolute(HintMatch& m, int row0, int col0, int rows, int cols) {
    if (row0 >= 0 && row0 < rows && col0 >= 0 && col0 < cols)
        m.absolute.push_back(tileAt(row0, col0, cols));
}

void addDirection(HintMatch& m, Direction d) {
    if (std::find(m.directions.begin(), m.directions.end(), d) == m.directions.end())
        m.directions.push_back(d);
}

// A token like "a3" → column letter + row digits. Returns true if it parsed as
// a chess coordinate (single leading letter followed by digits).
bool tryChessCoord(const std::string& tok, int rows, int cols, HintMatch& m) {
    if (tok.size() < 2) return false;
    if (!std::isalpha(static_cast<unsigned char>(tok[0]))) return false;
    if (!std::isdigit(static_cast<unsigned char>(tok[1]))) return false;
    // Whole remainder must be digits.
    for (std::size_t i = 1; i < tok.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(tok[i]))) return false;
    int col0 = tok[0] - 'a';
    int row0 = std::stoi(tok.substr(1)) - 1;  // 1-based row
    addAbsolute(m, row0, col0, rows, cols);
    return true;
}

}  // namespace

HintMatch parseHint(std::string_view text, int rows, int cols) {
    HintMatch m;
    auto toks = tokenize(text);

    int rowNum = -1, colNum = -1;  // 1-based, from "row N" / "col N"
    for (std::size_t i = 0; i < toks.size(); ++i) {
        const std::string& t = toks[i];

        // "row N" / "col N" / "column N"
        if ((t == "row") && i + 1 < toks.size() && isDigits(toks[i + 1])) {
            rowNum = std::stoi(toks[i + 1]);
            continue;
        }
        if ((t == "col" || t == "column") && i + 1 < toks.size() && isDigits(toks[i + 1])) {
            colNum = std::stoi(toks[i + 1]);
            continue;
        }

        // Direction words (including corner words top/bottom).
        if (t == "up" || t == "top")    { addDirection(m, Direction::Up);    continue; }
        if (t == "down" || t == "bottom"){ addDirection(m, Direction::Down);  continue; }
        if (t == "left")                { addDirection(m, Direction::Left);  continue; }
        if (t == "right")               { addDirection(m, Direction::Right); continue; }

        // Chess coordinate token (e.g. "a3").
        tryChessCoord(t, rows, cols, m);
    }

    if (rowNum > 0 && colNum > 0)
        addAbsolute(m, rowNum - 1, colNum - 1, rows, cols);

    return m;
}

}  // namespace mg
