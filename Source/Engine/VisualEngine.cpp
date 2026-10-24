#include "VisualEngine.h"
#include "../Render/TextLayout.h"
#include <cmath>

namespace au
{
bool VisualEngine::init (juce::String& err) { return r.init (err); }
void VisualEngine::release() { r.release(); video.reset(); videoPath.clear(); }

void VisualEngine::reset()
{
    r.sim = Sim3D();
    r.text.clear();
    textKey.clear(); textSeed = 0;
    ts = TextState();
    lastBeatCount = -1; jumpOffset = 0;
    video.reset(); videoPath.clear(); videoFailed = false;
    r.clearVideo();
}

static std::vector<std::string> splitWs (const std::string& s)
{
    std::vector<std::string> out; std::string cur;
    for (char c : s) { if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { if (! cur.empty()) out.push_back (cur); cur.clear(); } else cur += c; }
    if (! cur.empty()) out.push_back (cur);
    return out;
}

void VisualEngine::updateText (const SoundFrame& sf, const SceneState& st, const Scene& eff, int W, int H)
{
    ts.visible = false;
    if (! eff.on (P::txOn)) return;

    const double t = sf.time + st.lyricsOffset;
    std::vector<std::string> words;
    std::string key;
    int lit = 1 << 20;

    if (st.lyrics && ! st.lyrics->empty() && eff.on (P::txShowLyrics))
    {
        const int li = st.lyrics->lineAt (t + 0.25);           // form a little before the voice
        if (li >= 0)
        {
            const auto& line = st.lyrics->lines[(size_t) li];
            const double nextStart = (size_t) li + 1 < st.lyrics->lines.size() ? st.lyrics->lines[(size_t) li + 1].start : 1e9;
            if (t < line.end + 1.6 || t < nextStart - 0.25)
            {
                for (auto& w : line.words) words.push_back (w.text);
                key = "L" + std::to_string (li);
                lit = Lyrics::wordAt (line, t);
            }
        }
    }
    if (words.empty() && ! st.scene.title.empty())
    {
        words = splitWs (st.scene.title);
        key = "T" + st.scene.title;
    }
    if (words.empty()) { if (! textKey.empty()) { r.text.clear(); textKey.clear(); } return; }

    const float mn = (float) std::min (W, H);
    const float fw = (float) W / mn, fh = (float) H / mn;
    char sz[112]; std::snprintf (sz, sizeof sz, "|%.3f|%.3f|%.3f|%.3f|%d", eff[P::txSize], st.scene[P::txPosY], fw, fh, st.scene.choice (P::txAnim));
    key += sz;

    if (key != textKey)
    {
        textKey = key;
        auto lay = layoutWords (words, fw, fh, st.scene[P::txSize], st.scene[P::txPosY], 9000);
        std::copy (std::begin (lay.box), std::end (lay.box), std::begin (boxCache));
        r.text.setTargets (std::move (lay.targets), ++textSeed, lay.dotSize, st.scene.choice (P::txAnim));
    }
    ts.visible = r.text.hasTargets();
    ts.litWord = lit;
    std::copy (std::begin (boxCache), std::end (boxCache), std::begin (ts.box));
}

void VisualEngine::updateVideo (const SoundFrame& sf, const Scene& eff)
{
    if (! eff.on (P::vdOn) || eff.videoPath.empty()) { r.clearVideo(); return; }
    if (eff.videoPath != videoPath)
    {
        videoPath = eff.videoPath; videoFailed = false;
        video = createVideoSource (videoPath);
        std::string err;
        if (! video || ! video->open (videoPath, err)) { videoFailed = true; video.reset(); juce::Logger::writeToLog ("Video: " + juce::String (err)); }
        r.clearVideo();
    }
    if (! video) return;

    const double dur = video->duration();
    if (dur > 0 && sf.beatCount != lastBeatCount)
    {
        if (lastBeatCount >= 0 && hash1 ((unsigned) sf.beatCount * 2654435761u) < eff[P::vdBeatJump])
            jumpOffset = hash1 ((unsigned) sf.beatCount * 97u + 13u) * dur;
        lastBeatCount = sf.beatCount;
    }
    const double vt = sf.time * eff[P::vdSlow] + jumpOffset;
    int w = 0, h = 0; bool changed = false;
    const uint8_t* px = video->frameAt (vt, w, h, changed);
    if (px && w > 0 && h > 0 && changed) r.setVideoFrame (px, w, h);
}

void VisualEngine::render (const SoundFrame& sf, const SceneState& st, int W, int H)
{
    const Scene eff = applyModulation (st, sf);
    updateText (sf, st, eff, W, H);
    updateVideo (sf, eff);
    r.render (sf, eff, W, H, ts);
}
}
