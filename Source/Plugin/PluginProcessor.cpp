#include "PluginProcessor.h"
#include "../UI/Editor.h"
#include "../UI/GlViews.h"

namespace au
{
AuralisProcessor::AuralisProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formats.registerBasicFormats();
    const auto& fp = factoryPresets();
    state.scene = fp.front().scene;
    presetName = fp.front().en;
    state.scene.title = "AURALIS";
    launchExport = [this] (ExportRequest r) { ExportWindow::launch (std::move (r), russian); };
    startTimerHz (4);
}

AuralisProcessor::~AuralisProcessor()
{
    stopTimer();
    transport.setSource (nullptr);
    const juce::ScopedLock sl (bounceLock);
    bounceWriter.reset();
}

bool AuralisProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return l.getMainInputChannelSet() == out || l.getMainInputChannelSet().isDisabled();
}

void AuralisProcessor::prepareToPlay (double sr, int bs)
{
    sampleRate = sr; blockSize = bs;
    hub.prepare (sr);
    playBuf.setSize (2, std::max (bs, 4096));
    const juce::ScopedLock sl (transportLock);
    transport.prepareToPlay (bs, sr);
}

void AuralisProcessor::releaseResources()
{
    const juce::ScopedLock sl (transportLock);
    transport.releaseResources();
}

void AuralisProcessor::setNonRealtime (bool nr) noexcept
{
    AudioProcessor::setNonRealtime (nr);
    nonRealtimeFlag = nr;
}

void AuralisProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    double timeline = 0, bpm = 120, ppq = -1; int bpb = 4;

    if (isStandalone())
    {
        const juce::ScopedTryLock sl (transportLock);
        bool havePlayback = false;
        if (sl.isLocked() && readerSource)
        {
            if (playBuf.getNumSamples() < n) playBuf.setSize (2, n, false, false, true);
            juce::AudioSourceChannelInfo info (&playBuf, 0, n);
            transport.getNextAudioBlock (info);
            timeline = transport.getCurrentPosition() - n / sampleRate;
            for (int c = 0; c < nch; ++c) buffer.copyFrom (c, 0, playBuf, std::min (c, 1), 0, n);
            havePlayback = true;
        }
        if (! havePlayback)
        {
            // visualise the input (microphone / line in) but keep the output silent to avoid feedback
            const float* L = buffer.getReadPointer (0);
            const float* R = nch > 1 ? buffer.getReadPointer (1) : L;
            hub.push (L, R, n, juce::Time::getMillisecondCounterHiRes() * 0.001, 120.0, -1.0, 4);
            buffer.clear();
            return;
        }
    }
    else if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto t = pos->getTimeInSeconds()) timeline = *t;
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto ts = pos->getTimeSignature()) bpb = std::max (1, ts->numerator);
        }
    }
    lastHostTime = timeline;

    const float* L = buffer.getReadPointer (0);
    const float* R = nch > 1 ? buffer.getReadPointer (1) : L;

    if (! isStandalone() && isNonRealtime())
    {
        const juce::ScopedLock sl (bounceLock);
        if (! bounceActive && exportPrefs.autoOnBounce)
        {
            bounceFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("auralis-bounce-" + juce::String (juce::Time::currentTimeMillis()) + ".wav");
            juce::WavAudioFormat wav;
            if (auto* os = bounceFile.createOutputStream().release())
            {
                bounceWriter.reset (wav.createWriterFor (os, sampleRate, 2, 32, {}, 0));
                if (! bounceWriter) delete os;
            }
            bounceActive = bounceWriter != nullptr;
            bounceSamples = 0; bounceTimelineStart = timeline; bounceBpm = bpm; bouncePpq = ppq; bounceBpb = bpb;
        }
        if (bounceActive && bounceWriter)
        {
            const float* ch[2] = { L, R };
            bounceWriter->writeFromFloatArrays (ch, 2, n);
            bounceSamples += n;
            lastBounceBlock = juce::Time::getMillisecondCounterHiRes();
        }
    }
    else
        hub.push (L, R, n, timeline, bpm, ppq, bpb);
}

void AuralisProcessor::timerCallback()
{
    bool shouldFinish = false;
    {
        const juce::ScopedLock sl (bounceLock);
        if (bounceActive)
        {
            const double idle = juce::Time::getMillisecondCounterHiRes() - lastBounceBlock.load();
            shouldFinish = ! nonRealtimeFlag.load() || idle > 2500.0;
        }
    }
    if (shouldFinish) finishBounce();
}

void AuralisProcessor::finishBounce()
{
    juce::File f; long long samples = 0; double tl = 0, bpm = 120, ppq = -1; int bpb = 4;
    {
        const juce::ScopedLock sl (bounceLock);
        bounceWriter.reset();
        bounceActive = false;
        f = bounceFile; samples = bounceSamples; tl = bounceTimelineStart; bpm = bounceBpm; ppq = bouncePpq; bpb = bounceBpb;
    }
    if (samples < (long long) (sampleRate * 1.0)) { f.deleteFile(); return; }
    auto req = makeExportRequest (f, defaultOutputFile());
    req.deleteAudioWhenDone = true;
    req.timelineStart = tl; req.hostBpm = bpm; req.ppqStart = ppq; req.beatsPerBar = bpb;
    if (launchExport) launchExport (req);
    else f.deleteFile();
}

ExportRequest AuralisProcessor::makeExportRequest (const juce::File& audio, const juce::File& out) const
{
    ExportRequest r;
    r.audioFile = audio; r.outFile = out;
    const auto p = prefs();
    p.frameSize (r.width, r.height);
    r.fps = ExportPrefs::fpsValue (p.fps);
    r.videoBitrateMbps = ExportPrefs::bitrateMbps (p.bitrate);
    r.state = snapshot();
    return r;
}

juce::File AuralisProcessor::defaultOutputFile() const
{
    const auto p = prefs();
    juce::File dir = p.folder.empty() ? juce::File::getSpecialLocation (juce::File::userMoviesDirectory).getChildFile ("Auralis")
                                      : juce::File (juce::String::fromUTF8 (p.folder.c_str()));
    dir.createDirectory();
    juce::String base = juce::String::fromUTF8 (p.fileName.c_str()).trim();
    if (base.isEmpty()) base = "Auralis";
    base = juce::File::createLegalFileName (base);
    return dir.getNonexistentChildFile (base, ".mp4", false);
}

// ---------------------------------------------------------------- state
SceneState AuralisProcessor::snapshot() const { std::lock_guard<std::mutex> lk (stateMtx); return state; }
void AuralisProcessor::editState (const std::function<void (SceneState&)>& fn)
{
    { std::lock_guard<std::mutex> lk (stateMtx); fn (state); }
    ++stateVersion;
}
ExportPrefs AuralisProcessor::prefs() const { std::lock_guard<std::mutex> lk (stateMtx); return exportPrefs; }
void AuralisProcessor::setPrefs (const ExportPrefs& p) { { std::lock_guard<std::mutex> lk (stateMtx); exportPrefs = p; } ++stateVersion; }

void AuralisProcessor::setLyrics (Lyrics l)
{
    auto sp = std::make_shared<const Lyrics> (std::move (l));
    editState ([&] (SceneState& s) { s.lyrics = sp; });
}

// ---------------------------------------------------------------- track
bool AuralisProcessor::loadTrack (const juce::File& f, juce::String& err)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (! r) { err = "Unsupported audio file"; return false; }
    const double fileRate = r->sampleRate;
    auto src = std::make_unique<juce::AudioFormatReaderSource> (r.release(), true);
    {
        const juce::ScopedLock sl (transportLock);
        transport.stop();
        transport.setSource (nullptr);
        readerSource = std::move (src);
        transport.setSource (readerSource.get(), 0, nullptr, fileRate, 2);
        transport.setPosition (0);
    }
    trackFile = f;
    thumb.setSource (new juce::FileInputSource (f));
    ++stateVersion;
    return true;
}
double AuralisProcessor::trackLength() const { return readerSource ? transport.getLengthInSeconds() : 0.0; }
double AuralisProcessor::trackPosition() const { return readerSource ? transport.getCurrentPosition() : lastHostTime.load(); }
void AuralisProcessor::setTrackPosition (double s) { const juce::ScopedLock sl (transportLock); transport.setPosition (std::max (0.0, s)); }
void AuralisProcessor::setPlaying (bool p)
{
    const juce::ScopedLock sl (transportLock);
    if (! readerSource) return;
    if (p) { if (transport.hasStreamFinished()) transport.setPosition (0); transport.start(); }
    else transport.stop();
}
bool AuralisProcessor::isPlaying() const { return readerSource && transport.isPlaying(); }

// ---------------------------------------------------------------- serialisation
static juce::var lyricsToVar (const Lyrics& l)
{
    juce::Array<juce::var> lines;
    for (auto& ln : l.lines)
    {
        juce::Array<juce::var> words;
        for (auto& w : ln.words)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("s", w.start); o->setProperty ("e", w.end); o->setProperty ("t", juce::String::fromUTF8 (w.text.c_str()));
            words.add (juce::var (o));
        }
        auto* lo = new juce::DynamicObject(); lo->setProperty ("w", words);
        lines.add (juce::var (lo));
    }
    auto* o = new juce::DynamicObject(); o->setProperty ("lines", lines); o->setProperty ("synced", l.synced);
    return juce::var (o);
}

static Lyrics lyricsFromVar (const juce::var& v)
{
    Lyrics l;
    if (auto* arr = v["lines"].getArray())
        for (auto& lv : *arr)
        {
            LyricLine ln;
            if (auto* wa = lv["w"].getArray())
                for (auto& wv : *wa) ln.words.push_back ({ (double) wv["s"], (double) wv["e"], wv["t"].toString().toStdString() });
            if (ln.words.empty()) continue;
            ln.start = ln.words.front().start; ln.end = ln.words.back().end;
            l.lines.push_back (ln);
        }
    l.synced = (bool) v["synced"];
    return l;
}

juce::var sceneToVar (const SceneState& s)
{
    auto* o = new juce::DynamicObject();
    auto* params = new juce::DynamicObject();
    for (int i = 0; i < kParamCount; ++i) params->setProperty (info ((P) i).key, s.scene.v[(size_t) i]);
    o->setProperty ("params", juce::var (params));
    auto* mods = new juce::DynamicObject();
    for (int i = 0; i < kParamCount; ++i)
        if (s.mods[(size_t) i].src != 0)
        {
            juce::Array<juce::var> m { s.mods[(size_t) i].src, s.mods[(size_t) i].amt };
            mods->setProperty (info ((P) i).key, m);
        }
    o->setProperty ("mods", juce::var (mods));
    o->setProperty ("title", juce::String::fromUTF8 (s.scene.title.c_str()));
    o->setProperty ("video", juce::String::fromUTF8 (s.scene.videoPath.c_str()));
    return juce::var (o);
}

void sceneFromVar (const juce::var& v, SceneState& s, bool includeText)
{
    s.scene.resetToDefaults();
    if (auto* po = v["params"].getDynamicObject())
        for (auto& kv : po->getProperties())
        {
            const int idx = indexOfKey (kv.name.toString().toStdString());
            if (idx >= 0) s.scene.v[(size_t) idx] = juce::jlimit (info ((P) idx).mn, info ((P) idx).mx, (float) kv.value);
        }
    s.mods = {};
    if (auto* mo = v["mods"].getDynamicObject())
        for (auto& kv : mo->getProperties())
        {
            const int idx = indexOfKey (kv.name.toString().toStdString());
            if (idx >= 0 && kv.value.isArray() && kv.value.size() == 2)
                s.mods[(size_t) idx] = { juce::jlimit (0, (int) ModSource::COUNT - 1, (int) kv.value[0]), juce::jlimit (-1.f, 1.f, (float) kv.value[1]) };
        }
    if (includeText)
    {
        s.scene.title = v["title"].toString().toStdString();
        s.scene.videoPath = v["video"].toString().toStdString();
    }
}

juce::var AuralisProcessor::toVar() const
{
    const auto s = snapshot(); const auto p = prefs();
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("scene", sceneToVar (s));
    o->setProperty ("preset", juce::String::fromUTF8 (presetName.c_str()));
    o->setProperty ("russian", russian);
    if (s.lyrics) o->setProperty ("lyrics", lyricsToVar (*s.lyrics));
    o->setProperty ("lyricsOffset", s.lyricsOffset);
    o->setProperty ("rawLyrics", juce::String::fromUTF8 (rawLyricsText.c_str()));
    o->setProperty ("recLang", recognitionLanguage);
    auto* e = new juce::DynamicObject();
    e->setProperty ("aspect", p.aspect); e->setProperty ("res", p.resolution); e->setProperty ("fps", p.fps);
    e->setProperty ("bitrate", p.bitrate);
    e->setProperty ("auto", p.autoOnBounce); e->setProperty ("folder", juce::String::fromUTF8 (p.folder.c_str()));
    e->setProperty ("name", juce::String::fromUTF8 (p.fileName.c_str()));
    o->setProperty ("export", juce::var (e));
    if (trackFile != juce::File()) o->setProperty ("track", trackFile.getFullPathName());
    return juce::var (o);
}

void AuralisProcessor::fromVar (const juce::var& v)
{
    if (! v.isObject()) return;
    SceneState s;
    sceneFromVar (v["scene"], s, true);
    if (v.hasProperty ("lyrics")) s.lyrics = std::make_shared<const Lyrics> (lyricsFromVar (v["lyrics"]));
    s.lyricsOffset = (double) v["lyricsOffset"];
    editState ([&] (SceneState& st) { st = s; });
    presetName = v["preset"].toString().toStdString();
    russian = v.hasProperty ("russian") ? (bool) v["russian"] : true;
    rawLyricsText = v["rawLyrics"].toString().toStdString();
    recognitionLanguage = (int) v["recLang"];
    ExportPrefs p;
    const auto e = v["export"];
    if (e.isObject())
    {
        p.aspect = juce::jlimit (0, 2, (int) e["aspect"]); p.resolution = juce::jlimit (0, 3, (int) e["res"]);
        p.fps = juce::jlimit (0, 4, (int) e["fps"]); p.autoOnBounce = (bool) e["auto"];
        p.bitrate = juce::jlimit (0, ExportPrefs::kBitrateCount - 1, (int) e["bitrate"]);
        p.folder = e["folder"].toString().toStdString(); p.fileName = e["name"].toString().toStdString();
    }
    setPrefs (p);
    if (v.hasProperty ("track"))
    {
        juce::File f (v["track"].toString()); juce::String err;
        if (f.existsAsFile() && isStandalone()) loadTrack (f, err);
    }
}

void AuralisProcessor::getStateInformation (juce::MemoryBlock& mb)
{
    const auto json = juce::JSON::toString (toVar(), true);
    mb.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void AuralisProcessor::setStateInformation (const void* data, int size)
{
    const auto json = juce::String::fromUTF8 (static_cast<const char*> (data), size);
    fromVar (juce::JSON::parse (json));
}

juce::AudioProcessorEditor* AuralisProcessor::createEditor() { return new AuralisEditor (*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new au::AuralisProcessor(); }
