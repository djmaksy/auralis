#include "Analyzer.h"
#include <algorithm>
#include <cmath>

namespace au
{
namespace
{
constexpr float kFloorDb = -72.f, kRangeDb = 56.f;
inline float clamp01 (float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float follow (float s, float v, double dt, float attack, float release)
{
    const float tc = v > s ? attack : release;
    const float k = 1.f - std::exp ((float) (-dt / tc));
    return s + (v - s) * k;
}
}

Analyzer::Analyzer (double sampleRate)
    : sr (sampleRate), fft (kFftSize), window ((size_t) kFftSize), re ((size_t) kFftSize), im ((size_t) kFftSize),
      mag ((size_t) kFftSize / 2), binLo (SoundFrame::kBands), binHi (SoundFrame::kBands), bandTilt (SoundFrame::kBands)
{
    for (int i = 0; i < kFftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * 3.14159265358979323846 * i / (kFftSize - 1));

    const double fMin = 32.0, fMax = std::min (16000.0, sr * 0.45);
    const double binHz = sr / kFftSize;
    for (int b = 0; b < SoundFrame::kBands; ++b)
    {
        const double f0 = fMin * std::pow (fMax / fMin, (double) b / SoundFrame::kBands);
        const double f1 = fMin * std::pow (fMax / fMin, (double) (b + 1) / SoundFrame::kBands);
        int lo = (int) std::floor (f0 / binHz), hi = (int) std::ceil (f1 / binHz);
        lo = std::max (1, lo);
        hi = std::max (lo + 1, std::min (hi, kFftSize / 2 - 1));
        binLo[(size_t) b] = lo; binHi[(size_t) b] = hi;
        const double fc = std::sqrt (f0 * f1);
        bandTilt[(size_t) b] = (float) (4.5 * std::log2 (fc / 1000.0)); // dB, makes highs visible
    }
    reset();
}

void Analyzer::reset()
{
    std::fill (std::begin (smoothed), std::end (smoothed), 0.f);
    bassSm = midSm = trebSm = levelSm = widthSm = 0;
    prevBassRaw = fluxAvg = beatEnv = 0; sinceBeat = 10.f; beatCount = 0;
}

void Analyzer::setHost (double bpm, double ppq, int beatsPerBar)
{
    hostBpm = bpm > 1.0 ? bpm : 120.0;
    hostPpq = ppq;
    bpb = beatsPerBar > 0 ? beatsPerBar : 4;
}

void Analyzer::analyze (const float* L, const float* R, double dt, SoundFrame& out)
{
    dt = std::max (1.0e-4, std::min (dt, 0.25));

    double sumMid = 0, sumSide = 0, sumRms = 0;
    for (int i = 0; i < kFftSize; ++i)
    {
        const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]);
        re[(size_t) i] = m * window[(size_t) i];
        im[(size_t) i] = 0.f;
        if (i >= kFftSize - 1024) { sumMid += (double) m * m; sumSide += (double) s * s; sumRms += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]); }
    }
    fft.forward (re.data(), im.data());

    constexpr float norm = 4.f / kFftSize; // full-scale sine == 1.0
    for (int k = 1; k < kFftSize / 2; ++k)
        mag[(size_t) k] = std::sqrt (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]) * norm;

    float raw[SoundFrame::kBands];
    for (int b = 0; b < SoundFrame::kBands; ++b)
    {
        float m = 0.f;
        for (int k = binLo[(size_t) b]; k <= binHi[(size_t) b]; ++k) m = std::max (m, mag[(size_t) k]);
        const float db = 20.f * std::log10 (m + 1.0e-7f) + bandTilt[(size_t) b];
        raw[b] = clamp01 ((db - kFloorDb) / kRangeDb);
    }

    for (int b = 0; b < SoundFrame::kBands; ++b)
        smoothed[b] = follow (smoothed[b], raw[b], dt, 0.020f, 0.160f);

    auto avg = [&] (const float* a, int lo, int hi) { float s = 0; for (int i = lo; i < hi; ++i) s += a[i]; return s / (float) (hi - lo); };
    const float bassRaw = avg (raw, 0, 12), midRaw = avg (raw, 12, 40), trebRaw = avg (raw, 40, 64);

    bassSm = follow (bassSm, bassRaw, dt, 0.025f, 0.20f);
    midSm  = follow (midSm,  midRaw,  dt, 0.025f, 0.20f);
    trebSm = follow (trebSm, trebRaw, dt, 0.020f, 0.15f);

    const float rms = (float) std::sqrt (sumRms / 1024.0);
    const float lvDb = 20.f * std::log10 (rms + 1.0e-7f);
    levelSm = follow (levelSm, clamp01 ((lvDb + 54.f) / 48.f), dt, 0.03f, 0.25f);

    // Onset: positive flux of the low band against a running average.
    const float flux = std::max (0.f, bassRaw - prevBassRaw);
    prevBassRaw = bassRaw;
    fluxAvg += (flux - fluxAvg) * (1.f - std::exp ((float) (-dt / 0.35)));
    sinceBeat += (float) dt;
    float pulse = 0.f;
    if (flux > fluxAvg * 2.2f + 0.035f && sinceBeat > 0.13f && bassRaw > 0.25f)
    {
        pulse = 1.f; sinceBeat = 0.f; ++beatCount;
    }
    beatEnv = std::max (pulse, beatEnv * std::exp ((float) (-dt / 0.20)));

    const float wd = (float) (sumSide / (sumMid + sumSide + 1.0e-9));
    widthSm = follow (widthSm, clamp01 (wd * 2.f), dt, 0.1f, 0.3f);

    for (int b = 0; b < SoundFrame::kBands; ++b) out.bands[b] = smoothed[b];
    out.level = levelSm; out.bass = bassSm; out.mid = midSm; out.treble = clamp01 (trebSm * 1.6f);
    out.beat = beatEnv; out.beatCount = beatCount; out.width = widthSm;
    out.tempo = (float) hostBpm;
    if (hostPpq >= 0.0)
    {
        out.beatPhase = (float) (hostPpq - std::floor (hostPpq));
        const double bar = hostPpq / bpb;
        out.barPhase = (float) (bar - std::floor (bar));
    }
    else
    {
        out.beatPhase = out.barPhase = 0.f;
    }
    out.dt = dt;
}
}
