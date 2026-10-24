#pragma once
#include "Fft.h"
#include "SoundFrame.h"
#include <vector>

namespace au
{
// Stateful spectrum / transient analyser. Call once per video frame, in order.
// For identical input it produces identical output (this is what makes exports repeatable).
class Analyzer
{
public:
    static constexpr int kFftSize = 4096;

    explicit Analyzer (double sampleRate);

    void reset();

    // L / R: the last kFftSize samples ending at the frame boundary (oldest first).
    void analyze (const float* L, const float* R, double dt, SoundFrame& out);

    void setHost (double bpm, double ppq, int beatsPerBar);

private:
    double sr;
    Fft fft;
    std::vector<float> window, re, im, mag;
    std::vector<int> binLo, binHi;
    std::vector<float> bandTilt;
    float smoothed[SoundFrame::kBands] {};
    float bassSm = 0, midSm = 0, trebSm = 0, levelSm = 0, widthSm = 0;
    float prevBassRaw = 0, fluxAvg = 0, beatEnv = 0, sinceBeat = 10.f;
    int beatCount = 0;
    double hostBpm = 120.0, hostPpq = -1.0;
    int bpb = 4;
};
}
