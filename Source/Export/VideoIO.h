#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace au
{
// Decodes a user video (or still image) into top-down BGRA frames for the video layer.
class VideoSource
{
public:
    virtual ~VideoSource() = default;
    virtual bool open (const std::string& path, std::string& err) = 0;
    virtual double duration() const = 0;          // seconds (0 for a still image)
    // Returns the frame shown at time t (seconds into the clip). `changed` is set when the pixels differ from the last call.
    virtual const uint8_t* frameAt (double t, int& w, int& h, bool& changed) = 0;
};
std::unique_ptr<VideoSource> createVideoSource (const std::string& path);   // picks image / platform decoder

struct VideoWriterSettings
{
    std::string path;           // .mp4
    int width = 1920, height = 1080;
    double fps = 30;
    int videoBitrate = 12000000;
    double sampleRate = 48000;
    int channels = 2;
    std::string audioFile;      // used by the ffmpeg backend (audio is muxed from this file)
    double audioStart = 0;      // seconds into audioFile
    double duration = 0;        // seconds
};

// Encodes BGRA frames + PCM audio into an H.264/AAC MP4.
class VideoWriter
{
public:
    virtual ~VideoWriter() = default;
    virtual bool open (const VideoWriterSettings& s, std::string& err) = 0;
    virtual bool writeFrame (const uint8_t* bgraTopDown) = 0;
    virtual bool writeAudio (const float* const* channels, int numChannels, int numSamples) = 0;
    virtual bool finish (std::string& err) = 0;
    virtual const char* name() const = 0;
};

// On Windows: Media Foundation (built in, no extra install). Elsewhere, or when preferFfmpeg is set
// and ffmpeg is found: ffmpeg through a pipe.
std::unique_ptr<VideoWriter> createVideoWriter (bool preferFfmpeg);
std::string findFfmpeg();       // "" if not found
}
