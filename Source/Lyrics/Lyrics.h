#pragma once
#include <string>
#include <vector>

namespace au
{
struct LyricWord { double start = 0, end = 0; std::string text; };

struct LyricLine
{
    double start = 0, end = 0;
    std::vector<LyricWord> words;
    std::string text() const;
};

struct Lyrics
{
    std::vector<LyricLine> lines;
    bool synced = false;
    bool empty() const { return lines.empty(); }

    // Index of the line being sung at time t (or the next one if in a gap). -1 if none/before everything and no lines.
    int lineAt (double t) const;
    // Index of the word in the line that is "currently lit" at t, -1 if none has started yet.
    static int wordAt (const LyricLine& line, double t);
};

// Parsers (UTF-8). Both tolerate \r\n.
Lyrics parseLrc (const std::string& text);                   // [mm:ss.xx] lines, optional <mm:ss.xx> word tags
Lyrics parsePlain (const std::string& text);                 // one line per row, no timing
void distributeEvenly (Lyrics& l, double duration);          // weak fallback for un-timed text
Lyrics fromRecognized (const std::vector<LyricWord>& words); // group recognised words into lines by pauses

// Place the words of `text` on the timeline using recognised words as anchors (DP alignment).
Lyrics alignToRecognized (const Lyrics& text, const std::vector<LyricWord>& recognized, double duration);

std::string toLrc (const Lyrics& l, bool wordTimes = true);           // enhanced LRC
std::string toPlainText (const Lyrics& l);

// Unicode helpers
std::u32string toU32 (const std::string& utf8);
std::string toUtf8 (const std::u32string& s);
std::u32string normalizeWord (const std::u32string& w);       // lowercase, strip punctuation
double wordSimilarity (const std::u32string& a, const std::u32string& b); // 0..1

enum class Script { Latin, Cyrillic, Armenian, Georgian, Arabic, Other };
Script dominantScript (const std::string& utf8);
bool isRightToLeft (const std::string& utf8);
}
