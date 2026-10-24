#pragma once
#include "Analyzer.h"
#include <vector>

namespace au
{
// Turns a stream of audio blocks into SoundFrames at exact, sample-locked frame boundaries.
// The result depends only on the audio and the fps, never on block size or wall-clock time.
class FrameProducer
{
public:
    FrameProducer (double sampleRate, double fps);

    void reset (double startTimelineSeconds = 0.0);

    // hostBpm/hostPpqAtBlockStart/beatsPerBar are optional (ppq < 0 = unknown).
    template <class OnFrame>
    void push (const float* L, const float* R, int n, double timelineAtBlockStart,
               double hostBpm, double ppqAtBlockStart, int beatsPerBar, OnFrame&& onFrame)
    {
        for (int i = 0; i < n; ++i)
        {
            ringL[(size_t) writePos] = L[i];
            ringR[(size_t) writePos] = R ? R[i] : L[i];
            writePos = (writePos + 1) & mask;
            ++totalSamples;

            if (totalSamples >= nextBoundary)
            {
                const double tl = timelineAtBlockStart + (double) (i + 1) / sampleRate;
                const double ppq = ppqAtBlockStart >= 0.0 ? ppqAtBlockStart + (double) (i + 1) / sampleRate * hostBpm / 60.0 : -1.0;
                analyzer.setHost (hostBpm, ppq, beatsPerBar);
                buildFrame (tl);
                onFrame (frame);
                ++frameIndex;
                nextBoundary = (long long) std::ceil ((double) (frameIndex + 1) * sampleRate / fps);
            }
        }
    }

    double getFps() const { return fps; }
    double getSampleRate() const { return sampleRate; }
    int getFrameIndex() const { return frameIndex; }

private:
    void buildFrame (double timeline);

    double sampleRate, fps;
    Analyzer analyzer;
    int mask, writePos = 0;
    std::vector<float> ringL, ringR, winL, winR;
    long long totalSamples = 0, nextBoundary = 0;
    int frameIndex = 0;
    SoundFrame frame;
};
}
