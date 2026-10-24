#pragma once
#include "../Render/Renderer.h"
#include "../Core/SceneState.h"
#include "../Export/VideoIO.h"

namespace au
{
// Everything that turns (SoundFrame, SceneState) into pixels: modulation, lyrics/title particles,
// the video layer and the renderer. One instance per GL context.
class VisualEngine
{
public:
    bool init (juce::String& err);
    void release();
    void reset();                 // clears all time-dependent state (used before an export)

    void render (const SoundFrame& sf, const SceneState& st, int width, int height);
    Renderer& renderer() { return r; }
    bool isReady() const { return r.isReady(); }

private:
    void updateText (const SoundFrame& sf, const SceneState& st, const Scene& eff, int width, int height);
    void updateVideo (const SoundFrame& sf, const Scene& eff);

    Renderer r;
    TextState ts;
    std::string textKey;
    unsigned textSeed = 0;
    float boxCache[4] { 0, 0, 0, 0 };

    std::unique_ptr<VideoSource> video;
    std::string videoPath;
    bool videoFailed = false;
    int lastBeatCount = -1;
    double jumpOffset = 0;
};
}
