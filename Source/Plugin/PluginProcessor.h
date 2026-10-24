#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Engine/LiveHub.h"
#include "../Core/SceneState.h"
#include "../Export/ExportJob.h"
#include <mutex>

namespace au
{
juce::var sceneToVar (const SceneState& s);
void sceneFromVar (const juce::var& v, SceneState& s, bool includeText);

class AuralisProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    AuralisProcessor();
    ~AuralisProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Auralis"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void setNonRealtime (bool) noexcept override;

    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }

    // ---- scene state (thread-safe snapshot)
    SceneState snapshot() const;
    void editState (const std::function<void (SceneState&)>& fn);
    ExportPrefs prefs() const;
    void setPrefs (const ExportPrefs& p);
    std::string presetName;
    bool russian = true;
    std::atomic<int> stateVersion { 0 };

    // ---- lyrics
    void setLyrics (Lyrics l);
    std::string rawLyricsText;               // what the user pasted (kept for re-alignment)
    int recognitionLanguage = 0;

    // ---- track (standalone playback / plugin drop)
    bool loadTrack (const juce::File& f, juce::String& err);
    juce::File getTrack() const { return trackFile; }
    double trackLength() const;
    double trackPosition() const;
    void setTrackPosition (double s);
    void setPlaying (bool);
    bool isPlaying() const;
    const juce::AudioThumbnail& thumbnail() const { return thumb; }

    LiveHub hub;
    juce::AudioFormatManager formats;

    // ---- export
    ExportRequest makeExportRequest (const juce::File& audio, const juce::File& out) const;
    juce::File defaultOutputFile() const;
    std::function<void (ExportRequest)> launchExport;   // opens an export window
    std::function<void()> showAudioSettings;            // standalone only
    std::atomic<double> lastHostTime { 0.0 };

    juce::var toVar() const;
    void fromVar (const juce::var&);

private:
    void timerCallback() override;
    void finishBounce();

    mutable std::mutex stateMtx;
    SceneState state;
    ExportPrefs exportPrefs;

    // standalone player
    juce::File trackFile;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioThumbnailCache thumbCache { 4 };
    juce::AudioThumbnail thumb { 512, formats, thumbCache };
    juce::CriticalSection transportLock;
    double sampleRate = 48000;
    int blockSize = 512;
    juce::AudioBuffer<float> playBuf;

    // offline bounce capture -> automatic video render
    juce::CriticalSection bounceLock;
    std::unique_ptr<juce::AudioFormatWriter> bounceWriter;
    juce::File bounceFile;
    bool bounceActive = false;
    long long bounceSamples = 0;
    double bounceTimelineStart = 0, bounceBpm = 120, bouncePpq = -1; int bounceBpb = 4;
    std::atomic<double> lastBounceBlock { 0.0 };
    std::atomic<bool> nonRealtimeFlag { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuralisProcessor)
};
}
