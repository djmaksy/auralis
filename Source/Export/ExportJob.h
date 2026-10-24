#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Engine/VisualEngine.h"
#include "../Core/FrameProducer.h"
#include "VideoIO.h"
#include <atomic>

namespace au
{
struct ExportRequest
{
    juce::File audioFile;
    bool deleteAudioWhenDone = false;
    double timelineStart = 0;      // timeline seconds of the first audio sample (lyrics clock)
    double hostBpm = 120, ppqStart = -1; int beatsPerBar = 4;
    double startSec = 0, lengthSec = -1;   // part of the file to render (-1 = to the end)
    juce::File outFile;
    int width = 1920, height = 1080;
    double fps = 30;
    int videoBitrateMbps = 0;      // 0 = automatic (quality-based)
    SceneState state;
    bool preferFfmpeg = false;
};

// Frame-accurate offline render: audio file -> analysis -> GL frames -> MP4 (with the same audio).
// begin() on any thread; step() on the GL thread of the context that owns `engine`.
class ExportJob
{
public:
    explicit ExportJob (ExportRequest r);
    ~ExportJob();

    bool begin (juce::String& err);
    bool step (VisualEngine& engine, double budgetMs);     // false when finished (check failed())
    void cancel() { cancelled = true; }

    double progress() const { return totalFrames > 0 ? (double) framesDone / (double) totalFrames : 0.0; }
    bool failed() const { return hasFailed; }
    bool isDone() const { return finished; }
    bool wasCancelled() const { return cancelled; }
    const juce::String& error() const { return errorText; }
    const ExportRequest& request() const { return req; }
    long long frames() const { return framesDone; }
    long long totalFrameCount() const { return totalFrames; }
    const char* backendName() const { return writer ? writer->name() : ""; }

private:
    void fail (const juce::String& e);
    void finish();

    ExportRequest req;
    juce::AudioFormatManager formats;
    std::unique_ptr<juce::AudioFormatReader> reader;
    std::unique_ptr<FrameProducer> producer;
    std::unique_ptr<VideoWriter> writer;
    juce::AudioBuffer<float> block;
    std::vector<uint8_t> pixels;
    long long startSample = 0, endSample = 0, readPos = 0;
    long long framesDone = 0, totalFrames = 0;
    bool started = false, finished = false, hasFailed = false, engineReset = false;
    std::atomic<bool> cancelled { false };
    juce::String errorText;
};
}
