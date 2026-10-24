#include "VideoIO.h"
#include <juce_graphics/juce_graphics.h>
#include <cstdio>
#include <cstring>

#if JUCE_WINDOWS
 #define AU_POPEN _popen
 #define AU_PCLOSE _pclose
#else
 #define AU_POPEN popen
 #define AU_PCLOSE pclose
#endif

namespace au
{
std::unique_ptr<VideoWriter> createMediaFoundationWriter();
std::unique_ptr<VideoSource> createMediaFoundationSource();

std::string findFfmpeg()
{
    static const std::string cached = []
    {
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        const auto dll = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getParentDirectory();
       #if JUCE_WINDOWS
        const char* name = "ffmpeg.exe";
       #else
        const char* name = "ffmpeg";
       #endif
        for (auto dir : { exe, dll, dll.getParentDirectory(), juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Auralis") })
            if (dir.getChildFile (name).existsAsFile()) return dir.getChildFile (name).getFullPathName().toStdString();
        const auto path = juce::SystemStats::getEnvironmentVariable ("PATH", {});
       #if JUCE_WINDOWS
        const auto parts = juce::StringArray::fromTokens (path, ";", "");
       #else
        const auto parts = juce::StringArray::fromTokens (path, ":", "");
       #endif
        for (auto& p : parts)
        {
            const juce::File f = juce::File (p.trim().unquoted()).getChildFile (name);
            if (p.isNotEmpty() && f.existsAsFile()) return f.getFullPathName().toStdString();
        }
        return std::string();
    }();
    return cached;
}

static std::string q (const std::string& s)
{
   #if JUCE_WINDOWS
    return "\"" + s + "\"";
   #else
    std::string o = "'";
    for (char c : s) { if (c == '\'') o += "'\\''"; else o += c; }
    return o + "'";
   #endif
}

// ============================================================================ ffmpeg writer
class FfmpegWriter : public VideoWriter
{
public:
    bool open (const VideoWriterSettings& s, std::string& err) override
    {
        st = s;
        const std::string ff = findFfmpeg();
        if (ff.empty()) { err = "ffmpeg not found"; return false; }
        char buf[256];
        std::snprintf (buf, sizeof buf, " -f rawvideo -pix_fmt bgra -s %dx%d -r %.6f -i - ", s.width, s.height, s.fps);
        std::string cmd = q (ff) + " -y -hide_banner -loglevel error" + buf;
        if (! s.audioFile.empty())
        {
            char ab[128]; std::snprintf (ab, sizeof ab, " -ss %.6f -t %.6f -i ", s.audioStart, s.duration);
            cmd += ab + q (s.audioFile) + " -map 0:v -map 1:a -c:a aac -b:a 320k";
        }
        char vb[160]; std::snprintf (vb, sizeof vb, " -c:v libx264 -preset medium -crf 17 -pix_fmt yuv420p -profile:v high -movflags +faststart -shortest ");
        cmd += vb + q (s.path);
       #if JUCE_WINDOWS
        cmd = "\"" + cmd + "\"";
        pipe = AU_POPEN (cmd.c_str(), "wb");
       #else
        pipe = AU_POPEN (cmd.c_str(), "w");
       #endif
        if (! pipe) { err = "could not start ffmpeg"; return false; }
        return true;
    }
    bool writeFrame (const uint8_t* px) override
    {
        const size_t n = (size_t) st.width * (size_t) st.height * 4;
        return pipe && std::fwrite (px, 1, n, pipe) == n;
    }
    bool writeAudio (const float* const*, int, int) override { return true; }
    bool finish (std::string& err) override
    {
        if (! pipe) return false;
        const int rc = AU_PCLOSE (pipe); pipe = nullptr;
        if (rc != 0) { err = "ffmpeg exited with code " + std::to_string (rc); return false; }
        return true;
    }
    const char* name() const override { return "ffmpeg"; }
    ~FfmpegWriter() override { if (pipe) AU_PCLOSE (pipe); }
private:
    VideoWriterSettings st;
    FILE* pipe = nullptr;
};

// ============================================================================ ffmpeg reader (fixed 30 fps, scaled to <= 1280 wide)
class FfmpegSource : public VideoSource
{
public:
    bool open (const std::string& p, std::string& err) override
    {
        path = p;
        const std::string ff = findFfmpeg();
        if (ff.empty()) { err = "ffmpeg not found"; return false; }
        std::string probe = ff; // ffprobe usually sits next to ffmpeg
        auto pos = probe.rfind ("ffmpeg");
        if (pos != std::string::npos) probe.replace (pos, 6, "ffprobe");
        std::string cmd = q (probe) + " -v error -select_streams v:0 -show_entries stream=width,height:format=duration -of default=nw=1 " + q (p);
       #if JUCE_WINDOWS
        cmd = "\"" + cmd + "\"";
       #endif
        FILE* f = AU_POPEN (cmd.c_str(), "r");
        if (! f) { err = "ffprobe failed"; return false; }
        char line[256]; int sw = 0, sh = 0;
        while (std::fgets (line, sizeof line, f))
        {
            if (std::strncmp (line, "width=", 6) == 0) sw = std::atoi (line + 6);
            else if (std::strncmp (line, "height=", 7) == 0) sh = std::atoi (line + 7);
            else if (std::strncmp (line, "duration=", 9) == 0) dur = std::atof (line + 9);
        }
        AU_PCLOSE (f);
        if (sw <= 0 || sh <= 0) { err = "could not read video size"; return false; }
        w = std::min (1280, sw) & ~1; h = (int) ((double) w * sh / sw) & ~1;
        frame.assign ((size_t) w * h * 4, 0);
        return true;
    }
    double duration() const override { return dur; }
    const uint8_t* frameAt (double t, int& ow, int& oh, bool& changed) override
    {
        ow = w; oh = h; changed = false;
        if (dur > 0) { t = std::fmod (t, dur); if (t < 0) t += dur; }
        const double fps = 30.0;
        const long long want = (long long) (t * fps);
        if (! pipe || want < cur || want - cur > 90) restart (want / fps);
        while (pipe && cur < want)
        {
            if (std::fread (frame.data(), 1, frame.size(), pipe) != frame.size()) { AU_PCLOSE (pipe); pipe = nullptr; break; }
            ++cur; changed = true;
        }
        if (! everRead && pipe) { if (std::fread (frame.data(), 1, frame.size(), pipe) == frame.size()) { ++cur; changed = true; } everRead = true; }
        return frame.data();
    }
    ~FfmpegSource() override { if (pipe) AU_PCLOSE (pipe); }
private:
    void restart (double t)
    {
        if (pipe) { AU_PCLOSE (pipe); pipe = nullptr; }
        char b[256]; std::snprintf (b, sizeof b, " -v error -ss %.4f -i ", t);
        char vf[160]; std::snprintf (vf, sizeof vf, " -an -vf scale=%d:%d,fps=30 -f rawvideo -pix_fmt bgra -", w, h);
        std::string cmd = q (findFfmpeg()) + b + q (path) + vf;
       #if JUCE_WINDOWS
        cmd = "\"" + cmd + "\"";
        pipe = AU_POPEN (cmd.c_str(), "rb");
       #else
        pipe = AU_POPEN (cmd.c_str(), "r");
       #endif
        cur = (long long) (t * 30.0) - 1; everRead = false;
    }
    std::string path; double dur = 0; int w = 0, h = 0;
    FILE* pipe = nullptr; long long cur = -1; bool everRead = false;
    std::vector<uint8_t> frame;
};

// ============================================================================ still image
class ImageSource : public VideoSource
{
public:
    bool open (const std::string& p, std::string& err) override
    {
        auto img = juce::ImageFileFormat::loadFrom (juce::File (juce::String::fromUTF8 (p.c_str())));
        if (! img.isValid()) { err = "could not load image"; return false; }
        const int maxSide = 2048;
        if (img.getWidth() > maxSide || img.getHeight() > maxSide)
        {
            const double s = (double) maxSide / std::max (img.getWidth(), img.getHeight());
            img = img.rescaled (std::max (2, (int) (img.getWidth() * s)), std::max (2, (int) (img.getHeight() * s)), juce::Graphics::highResamplingQuality);
        }
        img = img.convertedToFormat (juce::Image::ARGB);
        w = img.getWidth(); h = img.getHeight();
        px.resize ((size_t) w * h * 4);
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto c = bd.getPixelColour (x, y);
                uint8_t* d = &px[((size_t) y * w + x) * 4];
                d[0] = c.getBlue(); d[1] = c.getGreen(); d[2] = c.getRed(); d[3] = 255;
            }
        return true;
    }
    double duration() const override { return 0; }
    const uint8_t* frameAt (double, int& ow, int& oh, bool& changed) override { ow = w; oh = h; changed = first; first = false; return px.data(); }
private:
    int w = 0, h = 0; bool first = true; std::vector<uint8_t> px;
};

std::unique_ptr<VideoSource> createVideoSource (const std::string& path)
{
    const auto ext = juce::File (juce::String::fromUTF8 (path.c_str())).getFileExtension().toLowerCase();
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif") return std::make_unique<ImageSource>();
   #if JUCE_WINDOWS
    if (auto mf = createMediaFoundationSource()) return mf;
   #endif
    return std::make_unique<FfmpegSource>();
}

std::unique_ptr<VideoWriter> createVideoWriter (bool preferFfmpeg)
{
   #if JUCE_WINDOWS
    if (! (preferFfmpeg && ! findFfmpeg().empty()))
        if (auto mf = createMediaFoundationWriter()) return mf;
   #endif
    (void) preferFfmpeg;
    return std::make_unique<FfmpegWriter>();
}

#if ! JUCE_WINDOWS
std::unique_ptr<VideoWriter> createMediaFoundationWriter() { return nullptr; }
std::unique_ptr<VideoSource> createMediaFoundationSource() { return nullptr; }
#endif
}
