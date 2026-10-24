#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/FrameProducer.h"
#include <atomic>
#include <mutex>

namespace au
{
// Audio thread -> visual thread bridge. The audio thread pushes blocks (lock-free); any GL thread
// pulls the newest SoundFrame. Also keeps a 16 kHz mono capture of what was played, indexed by
// timeline position, so lyrics can be recognised from the host's audio.
class LiveHub
{
public:
    static constexpr int kChunk = 512, kChunks = 512;
    static constexpr double kFps = 60.0;

    LiveHub();
    void prepare (double sampleRate);

    // audio thread
    void push (const float* L, const float* R, int n, double timeline, double bpm, double ppq, int bpb);

    // any non-audio thread
    SoundFrame pull();

    std::vector<float> getCapture() const;          // copy of the 16 kHz capture
    void clearCapture();
    double captureSeconds() const;

private:
    struct Chunk { float L[kChunk], R[kChunk]; int n; double timeline, bpm, ppq; int bpb; };
    std::vector<Chunk> chunks;
    juce::AbstractFifo fifo { kChunks };
    std::atomic<double> sr { 48000.0 };
    std::atomic<int> generation { 0 };

    mutable std::mutex mtx;
    std::unique_ptr<FrameProducer> producer;
    int producerGen = -1;
    SoundFrame latest;
    double lastPushWall = 0, lastSynthWall = 0, lastTimeline = 0;
    std::vector<float> capture;
    double capPhase = 0;
};
}
