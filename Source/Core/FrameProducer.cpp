#include "FrameProducer.h"
#include <cmath>

namespace au
{
FrameProducer::FrameProducer (double sr, double f)
    : sampleRate (sr), fps (f), analyzer (sr), mask (Analyzer::kFftSize * 2 - 1),
      ringL ((size_t) Analyzer::kFftSize * 2), ringR ((size_t) Analyzer::kFftSize * 2),
      winL ((size_t) Analyzer::kFftSize), winR ((size_t) Analyzer::kFftSize)
{
    reset();
}

void FrameProducer::reset (double)
{
    analyzer.reset();
    std::fill (ringL.begin(), ringL.end(), 0.f);
    std::fill (ringR.begin(), ringR.end(), 0.f);
    writePos = 0; totalSamples = 0; frameIndex = 0;
    nextBoundary = (long long) std::ceil (sampleRate / fps);
    frame = SoundFrame();
}

void FrameProducer::buildFrame (double timeline)
{
    for (int i = 0; i < Analyzer::kFftSize; ++i)
    {
        const int idx = (writePos - Analyzer::kFftSize + i) & mask;
        winL[(size_t) i] = ringL[(size_t) idx];
        winR[(size_t) i] = ringR[(size_t) idx];
    }
    analyzer.analyze (winL.data(), winR.data(), 1.0 / fps, frame);
    frame.frameIndex = frameIndex;
    frame.time = timeline;
    frame.animTime = (double) (frameIndex + 1) / fps;
}
}
