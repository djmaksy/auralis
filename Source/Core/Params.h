#pragma once
#include <array>
#include <string>
#include <vector>

namespace au
{
// Every scene parameter. Order matters only for the table in Params.cpp.
enum class P : int
{
    // BACKGROUND
    bgOn, bgMode, bgIntensity, bgSpeed, bgHue, bgSat, bgReact,
    // SPECTRUM
    spOn, spMode, spSize, spThick, spHue, spSat, spGain,
    // 3D
    d3On, d3Mode, d3Count, d3Size, d3Speed, d3Hue, d3Sat, d3React, d3Cam, d3Attr,
    // TEXT
    txOn, txSize, txPosY, txHue, txSat, txSungHue, txShowLyrics, txBright, txPanel,
    // ORNAMENT
    orOn, orMode, orSize, orHue, orSat, orReact, orSpin,
    // OPTICS
    opBloom, opBokeh, opChroma, opVignette, opExposure,
    // VIDEO LAYER
    vdOn, vdMode, vdOpacity, vdBright, vdContrast, vdSat, vdGrain, vdScan, vdAberr, vdSlow, vdBeatJump,
    COUNT
};

enum class Kind { Slider, Toggle, Choice };

struct ParamInfo
{
    P id;
    const char* key;      // stable id used in saved files
    const char* group;    // BACKGROUND, SPECTRUM, ...
    const char* en;
    const char* ru;
    float mn, mx, def;
    Kind kind;
    const char* choicesEn; // '|' separated
    const char* choicesRu;
};

constexpr int kParamCount = (int) P::COUNT;
const ParamInfo& info (P p);
const std::vector<std::string>& groups();           // ordered group ids
const char* groupLabel (const std::string& group, bool russian);
std::vector<std::string> splitChoices (const char* s);
int indexOfKey (const std::string& key);             // -1 if unknown

struct Scene
{
    std::array<float, kParamCount> v {};
    std::string title = "AURALIS";
    std::string videoPath;

    Scene() { resetToDefaults(); }
    float& operator[] (P p) { return v[(size_t) p]; }
    float operator[] (P p) const { return v[(size_t) p]; }
    bool on (P p) const { return v[(size_t) p] >= 0.5f; }
    int  choice (P p) const { return (int) (v[(size_t) p] + 0.5f); }
    void resetToDefaults();
};

struct Preset
{
    std::string en, ru;
    Scene scene;
};

const std::vector<Preset>& factoryPresets();         // exactly 62
}
