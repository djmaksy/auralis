#include "SceneState.h"
#include "SoundFrame.h"
#include <cmath>

namespace au
{
float modSourceValue (ModSource s, const SoundFrame& sf)
{
    switch (s)
    {
        case ModSource::Bass:    return sf.bass;
        case ModSource::Mid:     return sf.mid;
        case ModSource::Treble:  return sf.treble;
        case ModSource::Beat:    return sf.beat;
        case ModSource::Level:   return sf.level;
        case ModSource::BeatLfo: return 0.5f - 0.5f * std::cos (sf.beatPhase * 6.2831853f);
        case ModSource::BarLfo:  return 0.5f - 0.5f * std::cos (sf.barPhase * 6.2831853f);
        default:                 return 0.f;
    }
}

Scene applyModulation (const SceneState& st, const SoundFrame& sf)
{
    Scene s = st.scene;
    for (int i = 0; i < kParamCount; ++i)
    {
        const auto& m = st.mods[(size_t) i];
        if (m.src == 0 || m.amt == 0.f) continue;
        const auto& pi = info ((P) i);
        if (pi.kind != Kind::Slider) continue;
        float v = s.v[(size_t) i] + m.amt * modSourceValue ((ModSource) m.src, sf) * (pi.mx - pi.mn);
        s.v[(size_t) i] = v < pi.mn ? pi.mn : (v > pi.mx ? pi.mx : v);
    }
    return s;
}
}
