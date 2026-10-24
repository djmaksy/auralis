#include "Recognizer.h"
#include <juce_events/juce_events.h>
#if AURALIS_WITH_WHISPER
 #include "whisper.h"
#endif

namespace au
{
const std::vector<RecognitionLanguage>& recognitionLanguages()
{
    static const std::vector<RecognitionLanguage> l =
    {
        { "auto", "Auto-detect", "Автоопределение" },
        { "en", "English", "Английский" },   { "ru", "Russian", "Русский" },     { "hy", "Armenian", "Армянский" },
        { "ka", "Georgian", "Грузинский" },  { "ar", "Arabic", "Арабский" },     { "uk", "Ukrainian", "Украинский" },
        { "be", "Belarusian", "Белорусский" },{ "kk", "Kazakh", "Казахский" },   { "az", "Azerbaijani", "Азербайджанский" },
        { "uz", "Uzbek", "Узбекский" },      { "tr", "Turkish", "Турецкий" },    { "de", "German", "Немецкий" },
        { "fr", "French", "Французский" },   { "es", "Spanish", "Испанский" },   { "it", "Italian", "Итальянский" },
        { "pt", "Portuguese", "Португальский" }, { "pl", "Polish", "Польский" },
    };
    return l;
}

std::vector<float> prepareForRecognition (const juce::AudioBuffer<float>& buf, double sr)
{
    const int n = buf.getNumSamples();
    std::vector<float> mid ((size_t) n);
    const float* L = buf.getReadPointer (0);
    const float* R = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : L;
    for (int i = 0; i < n; ++i) mid[(size_t) i] = 0.5f * (L[i] + R[i]);

    juce::IIRFilter hp, lp;
    hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 120.0));
    lp.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, std::min (7000.0, sr * 0.45)));
    hp.processSamples (mid.data(), n); lp.processSamples (mid.data(), n);

    const double ratio = sr / 16000.0;
    std::vector<float> out ((size_t) std::floor (n / ratio));
    juce::LagrangeInterpolator li;
    li.process (ratio, mid.data(), out.data(), (int) out.size(), n, 0);
    float peak = 1e-6f; for (float v : out) peak = std::max (peak, std::abs (v));
    for (auto& v : out) v *= 0.9f / peak;
    return out;
}

bool loadAudioForRecognition (const juce::File& f, std::vector<float>& mono16k, double& seconds, juce::String& err)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (! r) { err = "Cannot read " + f.getFileName(); return false; }
    const auto len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 60 * 15));
    juce::AudioBuffer<float> b (2, len);
    r->read (&b, 0, len, 0, true, r->numChannels > 1);
    if (r->numChannels == 1) b.copyFrom (1, 0, b, 0, 0, len);
    mono16k = prepareForRecognition (b, r->sampleRate);
    seconds = len / r->sampleRate;
    return true;
}

juce::File WhisperModels::folder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Auralis").getChildFile ("models");
    d.createDirectory();
    return d;
}
juce::File WhisperModels::defaultModel() { return folder().getChildFile ("ggml-small.bin"); }
juce::URL WhisperModels::downloadUrl() { return juce::URL ("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin"); }
bool WhisperModels::available()
{
   #if AURALIS_WITH_WHISPER
    return true;
   #else
    return false;
   #endif
}

LyricsRecognizer::LyricsRecognizer() : juce::Thread ("Auralis lyrics recognition") {}
LyricsRecognizer::~LyricsRecognizer() { cancelFlag = true; stopThread (30000); }

bool LyricsRecognizer::start (Job j, Done d)
{
    if (isThreadRunning()) return false;
    job = std::move (j); done = std::move (d); cancelFlag = false; prog = 0.f;
    startThread (juce::Thread::Priority::low);
    return true;
}

void LyricsRecognizer::run()
{
    std::vector<LyricWord> words;
    juce::String err;
    bool ok = false;
#if AURALIS_WITH_WHISPER
    whisper_context_params cp = whisper_context_default_params();
    cp.use_gpu = true;
    whisper_context* ctx = whisper_init_from_file_with_params (job.modelPath.c_str(), cp);
    if (! ctx) err = "Cannot load recognition model";
    else
    {
        whisper_full_params p = whisper_full_default_params (WHISPER_SAMPLING_BEAM_SEARCH);
        p.beam_search.beam_size = 3;
        p.language = job.language.c_str();
        p.translate = false;
        p.token_timestamps = true;
        p.max_len = 1;
        p.split_on_word = true;
        p.print_progress = p.print_realtime = p.print_timestamps = p.print_special = false;
        p.no_context = false;
        p.suppress_blank = true;
        p.n_threads = std::max (1, juce::SystemStats::getNumPhysicalCpus());
        if (! job.prompt.empty()) p.initial_prompt = job.prompt.c_str();
        p.progress_callback = [] (whisper_context*, whisper_state*, int progress, void* user) { static_cast<LyricsRecognizer*> (user)->prog = (float) progress / 100.f; };
        p.progress_callback_user_data = this;
        p.abort_callback = [] (void* user) { return static_cast<LyricsRecognizer*> (user)->cancelFlag.load(); };
        p.abort_callback_user_data = this;

        const int rc = whisper_full (ctx, p, job.mono16k.data(), (int) job.mono16k.size());
        if (cancelFlag) err = "Cancelled";
        else if (rc != 0) err = "Recognition failed (" + juce::String (rc) + ")";
        else
        {
            const int ns = whisper_full_n_segments (ctx);
            for (int i = 0; i < ns; ++i)
            {
                juce::String t = juce::String::fromUTF8 (whisper_full_get_segment_text (ctx, i)).trim();
                if (t.isEmpty() || t.startsWithChar ('[') || t.startsWithChar ('(') || t.startsWithChar ('*') || t == "-" ) continue;
                LyricWord w;
                w.start = (double) whisper_full_get_segment_t0 (ctx, i) / 100.0;
                w.end = (double) whisper_full_get_segment_t1 (ctx, i) / 100.0;
                w.text = t.toStdString();
                if (! words.empty() && (t == "," || t == "." || t == "!" || t == "?")) { words.back().text += w.text; continue; }
                words.push_back (w);
            }
            ok = ! words.empty();
            if (! ok) err = "No vocal recognised";
        }
        whisper_free (ctx);
    }
#else
    err = "This build has no speech recognition (built without whisper.cpp)";
#endif
    prog = 1.f;
    auto cb = done;
    juce::MessageManager::callAsync ([cb, ok, words, err] { if (cb) cb (ok, words, err); });
}
}
