#include "ExportJob.h"
#include <cmath>

namespace au
{
ExportJob::ExportJob (ExportRequest r) : req (std::move (r)) { formats.registerBasicFormats(); }

ExportJob::~ExportJob()
{
    if (writer && ! finished) { std::string e; writer->finish (e); }
    reader.reset();
    if (req.deleteAudioWhenDone) req.audioFile.deleteFile();
}

void ExportJob::fail (const juce::String& e)
{
    errorText = e; hasFailed = true; finished = true;
    if (writer) { std::string ignore; writer->finish (ignore); writer.reset(); }
}

bool ExportJob::begin (juce::String& err)
{
    reader.reset (formats.createReaderFor (req.audioFile));
    if (! reader) { err = "Cannot read audio: " + req.audioFile.getFullPathName(); fail (err); return false; }

    const double sr = reader->sampleRate;
    startSample = (long long) std::llround (std::max (0.0, req.startSec) * sr);
    endSample = req.lengthSec > 0 ? std::min ((long long) reader->lengthInSamples, startSample + (long long) std::llround (req.lengthSec * sr))
                                  : (long long) reader->lengthInSamples;
    if (endSample - startSample < (long long) (sr * 0.2)) { err = "Audio is too short"; fail (err); return false; }
    readPos = startSample;

    const double seconds = (double) (endSample - startSample) / sr;
    totalFrames = (long long) std::ceil (seconds * req.fps - 1e-9);
    producer = std::make_unique<FrameProducer> (sr, req.fps);

    req.width &= ~1; req.height &= ~1;
    pixels.resize ((size_t) req.width * (size_t) req.height * 4);
    req.outFile.getParentDirectory().createDirectory();

    VideoWriterSettings vs;
    vs.path = req.outFile.getFullPathName().toStdString();
    vs.width = req.width; vs.height = req.height; vs.fps = req.fps;
    vs.videoBitrate = req.videoBitrateMbps > 0 ? req.videoBitrateMbps * 1000000
                                               : (int) std::min (80.0e6, (double) req.width * req.height * req.fps * 0.20);
    vs.constantQuality = req.videoBitrateMbps <= 0;
    vs.sampleRate = sr; vs.channels = 2;
    vs.audioFile = req.audioFile.getFullPathName().toStdString();
    vs.audioStart = (double) startSample / sr; vs.duration = seconds;

    writer = createVideoWriter (req.preferFfmpeg);
    std::string e;
    if (! writer->open (vs, e))
    {
        // Media Foundation can refuse some sizes; fall back to ffmpeg when present
        if (! findFfmpeg().empty() && std::string (writer->name()) != "ffmpeg")
        {
            writer = createVideoWriter (true);
            std::string e2;
            if (! writer->open (vs, e2)) { err = juce::String (e) + " / " + juce::String (e2); fail (err); return false; }
        }
        else { err = e; fail (err); return false; }
    }
    block.setSize (2, 1024);
    started = true;
    return true;
}

void ExportJob::finish()
{
    if (finished) return;
    std::string e;
    const bool ok = writer->finish (e);
    writer.reset();
    finished = true;
    if (! ok && ! cancelled) { errorText = e; hasFailed = true; }
    if (cancelled) req.outFile.deleteFile();
}

bool ExportJob::step (VisualEngine& engine, double budgetMs)
{
    if (! started || finished) return false;
    if (! engineReset) { engine.reset(); engineReset = true; }
    if (cancelled) { finish(); return false; }

    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const double sr = reader->sampleRate;
    bool writeError = false;

    auto onFrame = [&] (const SoundFrame& f)
    {
        if (framesDone >= totalFrames || writeError) return;
        engine.render (f, req.state, req.width, req.height);
        engine.renderer().readPixelsBGRA (pixels.data());
        if (! writer->writeFrame (pixels.data())) writeError = true;
        ++framesDone;
    };

    while (framesDone < totalFrames && juce::Time::getMillisecondCounterHiRes() - t0 < budgetMs)
    {
        const int n = (int) std::min<long long> (block.getNumSamples(), std::max<long long> (0, endSample - readPos));
        const int count = n > 0 ? n : block.getNumSamples();
        block.clear();
        if (n > 0)
        {
            reader->read (&block, 0, n, readPos, true, reader->numChannels > 1);
            if (reader->numChannels == 1) block.copyFrom (1, 0, block, 0, 0, n);
            const float* chans[2] = { block.getReadPointer (0), block.getReadPointer (1) };
            if (! writer->writeAudio (chans, 2, n)) { fail ("Audio encoding failed"); return false; }
        }
        const double rel = (double) (readPos - startSample) / sr;
        const double ppq = req.ppqStart >= 0 ? req.ppqStart + rel * req.hostBpm / 60.0 : -1.0;
        producer->push (block.getReadPointer (0), block.getReadPointer (1), count, req.timelineStart + rel, req.hostBpm, ppq, req.beatsPerBar, onFrame);
        readPos += count;
        if (writeError) { fail ("Video encoding failed (disk full or encoder error)"); return false; }
        if (cancelled) break;
    }
    if (framesDone >= totalFrames || cancelled) { finish(); return false; }
    return true;
}
}
