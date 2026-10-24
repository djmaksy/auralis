#pragma once
#include "Params.h"
#include "../Lyrics/Lyrics.h"
#include <array>
#include <memory>

namespace au
{
enum class ModSource : int { None, Bass, Mid, Treble, Beat, Level, BeatLfo, BarLfo, COUNT };

struct ModSlot { int src = 0; float amt = 0.f; };

// Everything needed to draw a frame apart from the sound. Cheap to copy.
struct SceneState
{
    Scene scene;
    std::array<ModSlot, kParamCount> mods {};
    std::shared_ptr<const Lyrics> lyrics;
    double lyricsOffset = 0.0;
};

float modSourceValue (ModSource s, const struct SoundFrame& sf);
Scene applyModulation (const SceneState& st, const struct SoundFrame& sf);

struct ExportPrefs
{
    int aspect = 0;        // 0 = 16:9, 1 = 9:16, 2 = 1:1
    int resolution = 1;    // short side: 0 = 720, 1 = 1080, 2 = 1440, 3 = 2160
    int fps = 2;           // 0=24 1=25 2=30 3=50 4=60
    int bitrate = 0;       // index into bitrateMbps(): 0 = auto
    bool autoOnBounce = true;
    std::string folder;    // empty = Videos/Auralis
    std::string fileName = "Auralis";

    static int shortSide (int r) { const int s[] = { 720, 1080, 1440, 2160 }; return s[r < 0 ? 0 : (r > 3 ? 3 : r)]; }
    static int bitrateMbps (int b) { const int v[] = { 0, 4, 8, 12, 16, 25, 40, 60, 80, 120 }; return v[b < 0 ? 0 : (b > 9 ? 9 : b)]; }
    static constexpr int kBitrateCount = 10;
    static double fpsValue (int f) { const double v[] = { 24, 25, 30, 50, 60 }; return v[f < 0 ? 0 : (f > 4 ? 4 : f)]; }
    void frameSize (int& w, int& h) const
    {
        const int s = shortSide (resolution);
        if (aspect == 0) { h = s; w = (s * 16 / 9 + 1) & ~1; }
        else if (aspect == 1) { w = s; h = (s * 16 / 9 + 1) & ~1; }
        else { w = h = s; }
    }
    float aspectRatio() const { return aspect == 0 ? 16.f / 9.f : (aspect == 1 ? 9.f / 16.f : 1.f); }
};
}
