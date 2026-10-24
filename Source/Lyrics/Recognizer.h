#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "Lyrics.h"
#include <atomic>
#include <functional>

namespace au
{
struct RecognitionLanguage { const char* code; const char* en; const char* ru; };
const std::vector<RecognitionLanguage>& recognitionLanguages();   // "auto" + 17 languages

// Mono 16 kHz, vocal band emphasised (mid channel, 120 Hz..7 kHz).
std::vector<float> prepareForRecognition (const juce::AudioBuffer<float>& buf, double sampleRate);
bool loadAudioForRecognition (const juce::File& f, std::vector<float>& mono16k, double& seconds, juce::String& err);

struct WhisperModels
{
    static juce::File folder();
    static juce::File defaultModel();          // ggml-small.bin in the models folder
    static juce::URL downloadUrl();
    static bool available();                   // compiled with whisper
};

// Speech-to-text with word timestamps on a background thread (whisper.cpp).
class LyricsRecognizer : private juce::Thread
{
public:
    struct Job { std::vector<float> mono16k; std::string language = "auto"; std::string modelPath; std::string prompt; };
    using Done = std::function<void (bool ok, std::vector<LyricWord> words, juce::String error)>;

    LyricsRecognizer();
    ~LyricsRecognizer() override;

    bool start (Job job, Done onDone);       // false if already running
    void cancel() { cancelFlag = true; }
    bool isBusy() const { return isThreadRunning(); }
    float progress() const { return prog.load(); }

private:
    void run() override;
    Job job;
    Done done;
    std::atomic<bool> cancelFlag { false };
    std::atomic<float> prog { 0.f };
};
}
