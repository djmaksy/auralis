#include "../Source/Core/FrameProducer.h"
#include "../Source/Core/Params.h"
#include "../Source/Lyrics/Lyrics.h"
#include "../Source/Render/ParticleSims.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

using namespace au;
static int failures = 0;
#define CHECK(c) do { if (! (c)) { std::printf ("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static std::vector<SoundFrame> run (double sr, double fps, int block, int seconds)
{
    FrameProducer fp (sr, fps);
    std::vector<SoundFrame> frames;
    std::vector<float> L ((size_t) block), R ((size_t) block);
    long long n = 0; const long long total = (long long) (sr * seconds);
    while (n < total)
    {
        const int cnt = (int) std::min<long long> (block, total - n);
        for (int i = 0; i < cnt; ++i)
        {
            const double t = (double) (n + i) / sr;
            // 100 Hz kick every 0.5 s with decay + 2 kHz tone
            const double ph = std::fmod (t, 0.5);
            const double kick = std::sin (2 * 3.14159265 * 100.0 * t) * std::exp (-ph * 18.0) * 0.8;
            const double hat = std::sin (2 * 3.14159265 * 2000.0 * t) * 0.1;
            L[(size_t) i] = (float) (kick + hat); R[(size_t) i] = (float) (kick + hat);
        }
        fp.push (L.data(), R.data(), cnt, (double) n / sr, 120.0, (double) n / sr * 2.0, 4, [&] (const SoundFrame& f) { frames.push_back (f); });
        n += cnt;
    }
    return frames;
}

int main()
{
    // --- params & presets
    CHECK (factoryPresets().size() == 62);
    std::set<std::string> names;
    for (auto& p : factoryPresets()) { names.insert (p.en); CHECK (! p.ru.empty()); }
    CHECK (names.size() == 62);
    for (int i = 0; i < kParamCount; ++i)
    {
        const auto& pi = info ((P) i);
        CHECK (pi.def >= pi.mn && pi.def <= pi.mx);
        CHECK (indexOfKey (pi.key) == i);
        if (pi.kind == Kind::Choice) CHECK ((int) splitChoices (pi.choicesEn).size() == (int) pi.mx + 1 && (int) splitChoices (pi.choicesRu).size() == (int) pi.mx + 1);
    }
    for (auto& p : factoryPresets())
        for (int i = 0; i < kParamCount; ++i) CHECK (p.scene.v[(size_t) i] >= info ((P) i).mn - 1e-4f && p.scene.v[(size_t) i] <= info ((P) i).mx + 1e-4f);

    // --- analysis: determinism independent of block size
    auto a = run (48000, 30, 512, 4), b = run (48000, 30, 333, 4), c = run (48000, 30, 4096, 4);
    CHECK (a.size() == 120 && b.size() == 120 && c.size() == 120);
    bool same = a.size() == b.size() && a.size() == c.size();
    for (size_t i = 0; same && i < a.size(); ++i)
        same = std::memcmp (a[i].bands, b[i].bands, sizeof a[i].bands) == 0 && std::memcmp (a[i].bands, c[i].bands, sizeof a[i].bands) == 0
               && a[i].beatCount == b[i].beatCount && a[i].beat == c[i].beat;
    CHECK (same);

    // kick energy shows up in bass band, hat in the 2 kHz region; beats ~ 2 per second
    float bassMax = 0, trebleMax = 0; int beats = a.back().beatCount;
    for (auto& f : a) { bassMax = std::max (bassMax, f.bass); trebleMax = std::max (trebleMax, f.treble); }
    std::printf ("bassMax=%.2f trebMax=%.2f beats=%d (expect ~8) level=%.2f\n", bassMax, trebleMax, beats, a[60].level);
    CHECK (bassMax > 0.35f);
    CHECK (trebleMax > 0.03f);
    CHECK (a[100].width < 0.05f);
    CHECK (beats >= 6 && beats <= 10);
    int peakBand = 0; for (int i = 0; i < 64; ++i) if (a[100].bands[i] > a[100].bands[peakBand]) peakBand = i;
    float bm = a[100].bands[peakBand]; CHECK (bm > 0.3f);
    CHECK (std::fabs (a[59].beatPhase - 0.0f) < 0.1f || a[59].beatPhase > 0.9f); // 2 beats/s -> frame 60 at 2.0s is a beat boundary
    for (auto& f : a) for (float v : f.bands) CHECK (v >= 0.f && v <= 1.f);

    // --- lyrics: LRC
    auto l = parseLrc ("[ar:Someone]\n[00:01.50]Hello brave world\n[00:05.00]Привет мир\n[00:08.00]<00:08.00>Mir <00:09.00>drugoy\n");
    CHECK (l.lines.size() == 3 && l.synced);
    CHECK (std::fabs (l.lines[0].start - 1.5) < 1e-6);
    CHECK (l.lines[0].words.size() == 3 && l.lines[1].words[0].text == "Привет");
    CHECK (l.lines[2].words.size() == 2 && std::fabs (l.lines[2].words[1].start - 9.0) < 1e-6);
    CHECK (l.lineAt (5.5) == 1 && l.lineAt (0.2) == -1);
    CHECK (Lyrics::wordAt (l.lines[2], 8.5) == 0 && Lyrics::wordAt (l.lines[2], 9.1) == 1);

    // --- scripts
    CHECK (dominantScript ("Hello") == Script::Latin);
    CHECK (dominantScript ("Привет") == Script::Cyrillic);
    CHECK (dominantScript ("Սևան") == Script::Armenian);
    CHECK (dominantScript ("სევანი") == Script::Georgian);
    CHECK (dominantScript ("سيفان") == Script::Arabic && isRightToLeft ("سيفان"));
    CHECK (toUtf8 (toU32 ("Սևան სევანი Привет 🙂")) == "Սևան სევანი Привет 🙂");
    CHECK (toUtf8 (normalizeWord (toU32 ("Ёлка,"))) == "елка");

    // --- alignment: recognised words with typos and a missed word
    auto txt = parsePlain ("Hello darling world\nsee the light tonight\n");
    std::vector<LyricWord> rec = { { 10.0, 10.4, "hello" }, { 10.5, 11.0, "darlin" }, { 11.1, 11.6, "word" },
                                   { 20.0, 20.3, "see" }, { 20.4, 20.6, "the" }, { 21.0, 21.5, "tonight" } };
    auto al = alignToRecognized (txt, rec, 60.0);
    CHECK (al.synced && al.lines.size() == 2);
    CHECK (std::fabs (al.lines[0].words[0].start - 10.0) < 1e-6);
    CHECK (std::fabs (al.lines[0].words[2].start - 11.1) < 1e-6);
    CHECK (std::fabs (al.lines[1].words[0].start - 20.0) < 1e-6);
    CHECK (al.lines[1].words[2].start >= 20.6 - 1e-9 && al.lines[1].words[2].start < 21.0);   // "light" interpolated
    CHECK (std::fabs (al.lines[1].words[3].start - 21.0) < 1e-6);
    {
        auto back = parseLrc (toLrc (al));
        CHECK (back.lines.size() == al.lines.size() && std::fabs (back.lines[1].words[3].start - 21.0) < 0.011);
        CHECK (toPlainText (back) == "Hello darling world\nsee the light tonight\n");
    }
    auto fr = fromRecognized (rec);
    CHECK (fr.lines.size() == 2);

    // --- particle sims: finite, deterministic, right sizes
    for (int mode = 0; mode < 5; ++mode)
        for (int attr = 0; attr < (mode == 2 ? 3 : 1); ++attr)
        {
            Scene sc; sc[P::d3Mode] = (float) mode; sc[P::d3Attr] = (float) attr; sc[P::d3Count] = 0.3f;
            Sim3D s1, s2;
            for (int f = 0; f < 90; ++f)
            {
                const SoundFrame& sf = a[(size_t) f];
                s1.update (sf, sc); s2.update (sf, sc);
            }
            bool fin = ! s1.verts.empty(), eq = s1.verts.size() == s2.verts.size();
            for (size_t i = 0; i < s1.verts.size(); ++i)
            {
                const auto& v = s1.verts[i];
                fin = fin && std::isfinite (v.x + v.y + v.z + v.r + v.g + v.b + v.size);
                eq = eq && std::memcmp (&v, &s2.verts[i], sizeof v) == 0;
            }
            std::printf ("mode %d attr %d -> %zu verts\n", mode, attr, s1.verts.size());
            CHECK (fin); CHECK (eq);
        }
    {
        TextSim ts; std::vector<TextTarget> t; for (int i = 0; i < 500; ++i) t.push_back ({ (float) (i % 50) * 0.02f - 0.5f, (float) (i / 50) * 0.02f, i / 100 });
        ts.setTargets (t, 1); Scene sc;
        for (int f = 0; f < 120; ++f) ts.update (a[(size_t) f], sc, 2);
        double err = 0; for (size_t i = 0; i < t.size(); ++i) err += std::hypot (ts.verts[i].x - t[i].x, ts.verts[i].y - t[i].y);
        err /= (double) t.size(); std::printf ("text settle error %.4f\n", err);
        CHECK (err < 0.05);
    }

    std::printf (failures ? "%d FAILURES\n" : "ALL OK\n", failures);
    return failures ? 1 : 0;
}
