#include "LiveHub.h"
#include <cmath>

namespace au
{
LiveHub::LiveHub() : chunks ((size_t) kChunks) {}

void LiveHub::prepare (double sampleRate)
{
    sr.store (sampleRate > 0 ? sampleRate : 48000.0);
    ++generation;
}

void LiveHub::push (const float* L, const float* R, int n, double timeline, double bpm, double ppq, int bpb)
{
    int off = 0;
    const double rate = sr.load();
    while (off < n)
    {
        const int cnt = std::min (kChunk, n - off);
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 + n2 == 0) return;                       // visual side is not consuming: drop
        Chunk& c = chunks[(size_t) (n1 ? s1 : s2)];
        std::memcpy (c.L, L + off, sizeof (float) * (size_t) cnt);
        std::memcpy (c.R, (R ? R : L) + off, sizeof (float) * (size_t) cnt);
        c.n = cnt; c.timeline = timeline + off / rate; c.bpm = bpm;
        c.ppq = ppq >= 0 ? ppq + off / rate * bpm / 60.0 : -1.0; c.bpb = bpb;
        fifo.finishedWrite (1);
        off += cnt;
    }
}

SoundFrame LiveHub::pull()
{
    std::lock_guard<std::mutex> lk (mtx);
    const double rate = sr.load();
    if (! producer || producerGen != generation.load())
    {
        producer = std::make_unique<FrameProducer> (rate, kFps);
        producerGen = generation.load();
        fifo.reset();
    }
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    auto onFrame = [this] (const SoundFrame& f) { latest = f; };

    int s1, n1, s2, n2;
    fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
    auto consume = [&] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const Chunk& c = chunks[(size_t) (start + i)];
            producer->push (c.L, c.R, c.n, c.timeline, c.bpm, c.ppq, c.bpb, onFrame);
            lastTimeline = c.timeline + c.n / rate;
            // 16 kHz mono capture (simple decimation with box filter)
            const double step = rate / 16000.0;
            const size_t base = (size_t) std::max (0.0, c.timeline * 16000.0);
            if (base < (size_t) (16000 * 60 * 15))
            {
                const size_t need = base + (size_t) (c.n / step) + 2;
                if (capture.size() < need) capture.resize (need, 0.f);
                for (int k = 0; (double) k * step < c.n; ++k)
                {
                    const int a = (int) (k * step), b = std::min (c.n, (int) ((k + 1) * step));
                    float acc = 0; for (int j = a; j < std::max (a + 1, b); ++j) acc += 0.5f * (c.L[j] + c.R[j]);
                    capture[base + (size_t) k] = acc / (float) std::max (1, b - a);
                }
            }
        }
    };
    consume (s1, n1); consume (s2, n2);
    fifo.finishedRead (n1 + n2);

    if (n1 + n2 > 0) { lastPushWall = now; lastSynthWall = now; }
    else if (now - lastPushWall > 0.15)
    {
        // host stopped calling us: keep the picture alive with silence
        const double dt = std::min (0.1, now - lastSynthWall);
        lastSynthWall = now;
        static thread_local std::vector<float> zeros;
        const int n = (int) (dt * rate);
        zeros.assign ((size_t) std::max (1, n), 0.f);
        producer->push (zeros.data(), zeros.data(), n, lastTimeline, latest.tempo, -1.0, 4, onFrame);
    }
    return latest;
}

std::vector<float> LiveHub::getCapture() const { std::lock_guard<std::mutex> lk (mtx); return capture; }
void LiveHub::clearCapture() { std::lock_guard<std::mutex> lk (mtx); capture.clear(); }
double LiveHub::captureSeconds() const { std::lock_guard<std::mutex> lk (mtx); return (double) capture.size() / 16000.0; }
}
