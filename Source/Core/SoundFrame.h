#pragma once

namespace au
{
// Everything the renderer knows about the sound at one video frame.
struct SoundFrame
{
    static constexpr int kBands = 64;

    float bands[kBands] {};      // smoothed, log-spaced, 0..1
    float level  = 0.f;          // overall loudness 0..1
    float bass   = 0.f;          // smoothed 0..1
    float mid    = 0.f;
    float treble = 0.f;
    float beat   = 0.f;          // decaying envelope, jumps to 1 on an onset
    int   beatCount = 0;         // number of onsets so far (deterministic seed for jumps)
    float width  = 0.f;          // stereo width 0..1
    float tempo  = 120.f;        // bpm from host (or 120)
    float beatPhase = 0.f;       // 0..1 inside a beat
    float barPhase  = 0.f;       // 0..1 inside a bar

    double time = 0.0;           // timeline seconds (lyrics clock)
    double animTime = 0.0;       // monotonic animation clock, seconds
    double dt = 1.0 / 60.0;      // seconds since previous frame
    int frameIndex = 0;
};
}
