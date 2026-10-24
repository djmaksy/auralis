#include "Lyrics.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <cstdio>

namespace au
{
// ---------------------------------------------------------------- unicode
std::u32string toU32 (const std::string& s)
{
    std::u32string out;
    for (size_t i = 0; i < s.size();)
    {
        const unsigned char c = (unsigned char) s[i];
        char32_t cp; int len;
        if (c < 0x80) { cp = c; len = 1; }
        else if ((c >> 5) == 6)  { cp = c & 0x1F; len = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; len = 3; }
        else if ((c >> 3) == 30) { cp = c & 0x07; len = 4; }
        else { ++i; continue; }
        for (int k = 1; k < len && i + (size_t) k < s.size(); ++k) cp = (cp << 6) | ((unsigned char) s[i + (size_t) k] & 0x3F);
        out.push_back (cp); i += (size_t) len;
    }
    return out;
}

std::string toUtf8 (const std::u32string& s)
{
    std::string out;
    for (char32_t c : s)
    {
        if (c < 0x80) out += (char) c;
        else if (c < 0x800) { out += (char) (0xC0 | (c >> 6)); out += (char) (0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out += (char) (0xE0 | (c >> 12)); out += (char) (0x80 | ((c >> 6) & 0x3F)); out += (char) (0x80 | (c & 0x3F)); }
        else { out += (char) (0xF0 | (c >> 18)); out += (char) (0x80 | ((c >> 12) & 0x3F)); out += (char) (0x80 | ((c >> 6) & 0x3F)); out += (char) (0x80 | (c & 0x3F)); }
    }
    return out;
}

static bool isLetterOrDigit (char32_t c)
{
    if (c < 0x80) return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    if (c >= 0xC0 && c <= 0x24F) return c != 0xD7 && c != 0xF7;           // Latin-1 supplement / extended
    if (c >= 0x370 && c <= 0x52F) return true;                           // Greek, Cyrillic
    if (c >= 0x531 && c <= 0x587) return true;                           // Armenian
    if (c >= 0x10A0 && c <= 0x10FF) return true;                         // Georgian
    if (c >= 0x2D00 && c <= 0x2D2F) return true;
    if (c >= 0x621 && c <= 0x64A) return true;                           // Arabic letters
    if (c >= 0x660 && c <= 0x669) return true;
    if (c >= 0x670 && c <= 0x6D3) return true;
    return false;
}

static char32_t lower (char32_t c)
{
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    if (c >= 0x531 && c <= 0x556) return c + 48;
    if (c >= 0x1C90 && c <= 0x1CBF) return c - 0x1C90 + 0x10D0;         // Georgian Mtavruli -> Mkhedruli
    return c;
}

std::u32string normalizeWord (const std::u32string& w)
{
    std::u32string out;
    for (char32_t c : w)
    {
        if (c == 0x451 || c == 0x401) c = 0x435;                         // ё -> е
        if (isLetterOrDigit (c)) out.push_back (lower (c));
    }
    return out;
}

double wordSimilarity (const std::u32string& a, const std::u32string& b)
{
    if (a.empty() || b.empty()) return 0.0;
    if (a == b) return 1.0;
    const size_t n = a.size(), m = b.size();
    std::vector<int> prev (m + 1), cur (m + 1);
    for (size_t j = 0; j <= m; ++j) prev[j] = (int) j;
    for (size_t i = 1; i <= n; ++i)
    {
        cur[0] = (int) i;
        for (size_t j = 1; j <= m; ++j)
            cur[j] = std::min ({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1) });
        std::swap (prev, cur);
    }
    return 1.0 - (double) prev[m] / (double) std::max (n, m);
}

Script dominantScript (const std::string& utf8)
{
    int cnt[6] {};
    for (char32_t c : toU32 (utf8))
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xC0 && c <= 0x24F)) ++cnt[0];
        else if (c >= 0x400 && c <= 0x52F) ++cnt[1];
        else if (c >= 0x531 && c <= 0x58F) ++cnt[2];
        else if ((c >= 0x10A0 && c <= 0x10FF) || (c >= 0x1C90 && c <= 0x1CBF)) ++cnt[3];
        else if ((c >= 0x600 && c <= 0x6FF) || (c >= 0x750 && c <= 0x77F) || (c >= 0xFB50 && c <= 0xFEFF)) ++cnt[4];
    }
    int best = 0;
    for (int i = 1; i < 5; ++i) if (cnt[i] > cnt[best]) best = i;
    if (cnt[best] == 0) return Script::Other;
    return (Script) best;
}

bool isRightToLeft (const std::string& utf8) { return dominantScript (utf8) == Script::Arabic; }

// ---------------------------------------------------------------- model
std::string LyricLine::text() const
{
    std::string s;
    for (size_t i = 0; i < words.size(); ++i) { if (i) s += ' '; s += words[i].text; }
    return s;
}

int Lyrics::lineAt (double t) const
{
    if (lines.empty()) return -1;
    int best = -1;
    for (int i = 0; i < (int) lines.size(); ++i)
    {
        if (lines[(size_t) i].start <= t) best = i; else break;
    }
    return best;
}

int Lyrics::wordAt (const LyricLine& line, double t)
{
    int idx = -1;
    for (int i = 0; i < (int) line.words.size(); ++i)
        if (line.words[(size_t) i].start <= t) idx = i; else break;
    return idx;
}

// ---------------------------------------------------------------- parsing
namespace
{
std::vector<std::string> splitLines (const std::string& s)
{
    std::vector<std::string> out; std::string cur;
    for (char c : s)
    {
        if (c == '\n') { if (! cur.empty() && cur.back() == '\r') cur.pop_back(); out.push_back (cur); cur.clear(); }
        else cur += c;
    }
    if (! cur.empty()) { if (cur.back() == '\r') cur.pop_back(); out.push_back (cur); }
    return out;
}

std::string trim (const std::string& s)
{
    size_t a = 0, b = s.size();
    // strip UTF-8 BOM
    if (s.size() >= 3 && (unsigned char) s[0] == 0xEF && (unsigned char) s[1] == 0xBB && (unsigned char) s[2] == 0xBF) a = 3;
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr (a, b - a);
}

std::vector<std::string> splitWords (const std::string& s)
{
    std::vector<std::string> out; std::string cur;
    for (char c : s)
    {
        if (c == ' ' || c == '\t') { if (! cur.empty()) { out.push_back (cur); cur.clear(); } }
        else cur += c;
    }
    if (! cur.empty()) out.push_back (cur);
    return out;
}

// "mm:ss.xx" / "mm:ss" / "h:mm:ss.xx" -> seconds, or -1
double parseStamp (const std::string& s)
{
    double parts[3] {}; int n = 0; std::string cur;
    for (size_t i = 0; i <= s.size(); ++i)
    {
        if (i == s.size() || s[i] == ':')
        {
            if (cur.empty() || n >= 3) return -1;
            char* end = nullptr; parts[n++] = std::strtod (cur.c_str(), &end);
            if (end == cur.c_str()) return -1;
            cur.clear();
        }
        else if ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' ) cur += s[i];
        else return -1;
    }
    if (n == 2) return parts[0] * 60 + parts[1];
    if (n == 3) return parts[0] * 3600 + parts[1] * 60 + parts[2];
    return -1;
}

// Spread words over [start, end] proportionally to their length.
void spreadWords (LyricLine& line)
{
    double total = 0; for (auto& w : line.words) total += (double) toU32 (w.text).size() + 1.0;
    double t = line.start;
    const double span = std::max (0.05, line.end - line.start);
    for (auto& w : line.words)
    {
        const double share = ((double) toU32 (w.text).size() + 1.0) / total * span;
        w.start = t; w.end = t + share; t += share;
    }
}
}

Lyrics parseLrc (const std::string& text)
{
    Lyrics out;
    struct Raw { double t; std::string body; };
    std::vector<Raw> raws;

    for (auto row : splitLines (text))
    {
        row = trim (row);
        if (row.empty() || row[0] != '[') continue;
        std::vector<double> stamps; size_t pos = 0; bool meta = false;
        while (pos < row.size() && row[pos] == '[')
        {
            const size_t close = row.find (']', pos);
            if (close == std::string::npos) break;
            const double t = parseStamp (row.substr (pos + 1, close - pos - 1));
            if (t < 0) { meta = true; break; }       // [ar:..] [ti:..] etc.
            stamps.push_back (t); pos = close + 1;
        }
        if (meta || stamps.empty()) continue;
        for (double t : stamps) raws.push_back ({ t, trim (row.substr (pos)) });
    }
    std::stable_sort (raws.begin(), raws.end(), [] (const Raw& a, const Raw& b) { return a.t < b.t; });

    for (size_t i = 0; i < raws.size(); ++i)
    {
        LyricLine line; line.start = raws[i].t;
        line.end = i + 1 < raws.size() ? raws[i + 1].t : raws[i].t + 4.0;
        std::string body = raws[i].body;

        // enhanced LRC: <mm:ss.xx>word
        if (body.find ('<') != std::string::npos)
        {
            size_t p = 0; double curT = -1; std::string cur;
            auto flush = [&]
            {
                const std::string w = trim (cur);
                if (! w.empty() && curT >= 0) { LyricWord lw; lw.start = curT; lw.text = w; line.words.push_back (lw); }
                cur.clear();
            };
            while (p < body.size())
            {
                if (body[p] == '<')
                {
                    const size_t c = body.find ('>', p);
                    const double t = c == std::string::npos ? -1 : parseStamp (body.substr (p + 1, c - p - 1));
                    if (t >= 0) { flush(); curT = t; p = c + 1; continue; }
                }
                cur += body[p++];
            }
            flush();
            for (size_t k = 0; k < line.words.size(); ++k)
                line.words[k].end = k + 1 < line.words.size() ? line.words[k + 1].start : line.end;
            if (! line.words.empty()) line.start = line.words.front().start;
        }
        else
        {
            if (body.empty()) continue;               // instrumental gap marker
            for (auto& w : splitWords (body)) { LyricWord lw; lw.text = w; line.words.push_back (lw); }
            spreadWords (line);
        }
        if (! line.words.empty()) out.lines.push_back (std::move (line));
    }
    out.synced = ! out.lines.empty();
    return out;
}

Lyrics parsePlain (const std::string& text)
{
    Lyrics out;
    for (auto row : splitLines (text))
    {
        row = trim (row);
        if (row.empty()) continue;
        LyricLine line;
        for (auto& w : splitWords (row)) { LyricWord lw; lw.text = w; line.words.push_back (lw); }
        out.lines.push_back (std::move (line));
    }
    out.synced = false;
    return out;
}

void distributeEvenly (Lyrics& l, double duration)
{
    if (l.lines.empty()) return;
    double total = 0;
    for (auto& ln : l.lines) for (auto& w : ln.words) total += (double) toU32 (w.text).size() + 1.0;
    double t = 0;
    for (auto& ln : l.lines)
    {
        ln.start = t;
        for (auto& w : ln.words)
        {
            const double d = ((double) toU32 (w.text).size() + 1.0) / total * duration;
            w.start = t; w.end = t + d; t += d;
        }
        ln.end = t;
    }
    l.synced = true;
}

Lyrics fromRecognized (const std::vector<LyricWord>& words)
{
    Lyrics out;
    LyricLine cur;
    for (size_t i = 0; i < words.size(); ++i)
    {
        const auto& w = words[i];
        const bool gap = ! cur.words.empty() && (w.start - cur.words.back().end > 0.9);
        const bool tooLong = cur.words.size() >= 8;
        if (gap || tooLong) { cur.start = cur.words.front().start; cur.end = cur.words.back().end; out.lines.push_back (std::move (cur)); cur = {}; }
        cur.words.push_back (w);
    }
    if (! cur.words.empty()) { cur.start = cur.words.front().start; cur.end = cur.words.back().end; out.lines.push_back (std::move (cur)); }
    out.synced = ! out.lines.empty();
    return out;
}

Lyrics alignToRecognized (const Lyrics& text, const std::vector<LyricWord>& rec, double duration)
{
    // flatten
    struct Ref { size_t line, word; std::u32string norm; };
    std::vector<Ref> flat;
    for (size_t i = 0; i < text.lines.size(); ++i)
        for (size_t j = 0; j < text.lines[i].words.size(); ++j)
            flat.push_back ({ i, j, normalizeWord (toU32 (text.lines[i].words[j].text)) });

    Lyrics out = text;
    if (flat.empty()) return out;
    if (rec.empty()) { distributeEvenly (out, duration); return out; }

    std::vector<std::u32string> rn; rn.reserve (rec.size());
    for (auto& w : rec) rn.push_back (normalizeWord (toU32 (w.text)));

    const size_t n = flat.size(), m = rec.size();
    constexpr double gap = -0.35, minSim = 0.45;
    std::vector<double> dp ((n + 1) * (m + 1), 0.0);
    std::vector<unsigned char> bt ((n + 1) * (m + 1), 0);
    auto at = [&] (size_t i, size_t j) -> size_t { return i * (m + 1) + j; };
    for (size_t i = 1; i <= n; ++i) { dp[at (i, 0)] = dp[at (i - 1, 0)] + gap; bt[at (i, 0)] = 1; }
    for (size_t j = 1; j <= m; ++j) { dp[at (0, j)] = dp[at (0, j - 1)] + gap; bt[at (0, j)] = 2; }
    for (size_t i = 1; i <= n; ++i)
        for (size_t j = 1; j <= m; ++j)
        {
            const double sim = wordSimilarity (flat[i - 1].norm, rn[j - 1]);
            const double diag = dp[at (i - 1, j - 1)] + (sim >= minSim ? sim * 2.0 - 0.2 : -1.0);
            const double up = dp[at (i - 1, j)] + gap, left = dp[at (i, j - 1)] + gap;
            double best = diag; unsigned char b = 0;
            if (up > best) { best = up; b = 1; }
            if (left > best) { best = left; b = 2; }
            dp[at (i, j)] = best; bt[at (i, j)] = b;
        }

    std::vector<int> match (n, -1);
    for (size_t i = n, j = m; i > 0 || j > 0;)
    {
        const unsigned char b = bt[at (i, j)];
        if (i > 0 && j > 0 && b == 0)
        {
            if (wordSimilarity (flat[i - 1].norm, rn[j - 1]) >= minSim) match[i - 1] = (int) (j - 1);
            --i; --j;
        }
        else if (i > 0 && (b == 1 || j == 0)) --i;
        else --j;
    }

    // Anchor times; interpolate unmatched words between anchors (by index).
    std::vector<double> st (n, -1.0), en (n, -1.0);
    for (size_t i = 0; i < n; ++i) if (match[i] >= 0) { st[i] = rec[(size_t) match[i]].start; en[i] = rec[(size_t) match[i]].end; }

    bool any = false; for (double s : st) if (s >= 0) { any = true; break; }
    if (! any) { distributeEvenly (out, duration); return out; }

    // Leading / trailing / inner gaps.
    size_t i = 0;
    while (i < n)
    {
        if (st[i] >= 0) { ++i; continue; }
        size_t j = i; while (j < n && st[j] < 0) ++j;
        const double t0 = i > 0 ? en[i - 1] : std::max (0.0, st[j < n ? j : 0] - 0.5 * (double) (j - i));
        const double t1 = j < n ? st[j] : std::min (duration, en[i - 1] + 0.5 * (double) (j - i));
        const double step = (t1 - t0) / (double) (j - i);
        for (size_t k = i; k < j; ++k) { st[k] = t0 + step * (double) (k - i); en[k] = st[k] + step; }
        i = j;
    }
    // enforce monotonic order
    for (size_t k = 1; k < n; ++k) { if (st[k] < st[k - 1]) st[k] = st[k - 1]; if (en[k] < st[k]) en[k] = st[k] + 0.05; }

    for (size_t k = 0; k < n; ++k)
    {
        auto& w = out.lines[flat[k].line].words[flat[k].word];
        w.start = st[k]; w.end = en[k];
    }
    for (auto& ln : out.lines)
    {
        ln.start = ln.words.front().start; ln.end = ln.words.back().end;
    }
    out.synced = true;
    return out;
}

static std::string stamp (double t)
{
    if (t < 0) t = 0;
    const int cs = (int) std::llround (t * 100.0);
    char b[32]; std::snprintf (b, sizeof b, "%02d:%02d.%02d", cs / 6000, (cs / 100) % 60, cs % 100);
    return b;
}

std::string toLrc (const Lyrics& l, bool wordTimes)
{
    std::string out;
    for (auto& ln : l.lines)
    {
        out += "[" + stamp (ln.start) + "]";
        for (size_t i = 0; i < ln.words.size(); ++i)
        {
            if (wordTimes) out += "<" + stamp (ln.words[i].start) + ">";
            out += ln.words[i].text;
            if (i + 1 < ln.words.size()) out += ' ';
        }
        out += "\n";
    }
    return out;
}

std::string toPlainText (const Lyrics& l)
{
    std::string out;
    for (auto& ln : l.lines) { out += ln.text(); out += "\n"; }
    return out;
}
}
