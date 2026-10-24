// Custom standalone application: frameless window in the plug-in's style, audio device / ASIO in the gear,
// and a command-line renderer (Auralis --render song.wav --out video.mp4 ...).
#include "../Plugin/PluginProcessor.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "../UI/Editor.h"
#include "../UI/GlViews.h"
#include "../UI/Look.h"

namespace au
{
class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (juce::StandalonePluginHolder& h)
        : juce::DocumentWindow ("Auralis by Dj Maksy", colours::bg, 0), holder (h)
    {
        setUsingNativeTitleBar (false);
        setTitleBarHeight (0);
        setDropShadowEnabled (true);
        editor.reset (holder.processor->createEditorIfNeeded());
        setContentNonOwned (editor.get(), true);
        setResizable (true, false);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) AuralisEditor::kW / AuralisEditor::kH);
        setResizeLimits (AuralisEditor::kW / 2, AuralisEditor::kH / 2, AuralisEditor::kW * 3, AuralisEditor::kH * 3);

        const auto saved = holder.settings ? holder.settings->getValue ("windowBounds") : juce::String();
        const auto user = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        if (saved.isNotEmpty() && user.contains (juce::Rectangle<int>::fromString (saved).getCentre()))
            setBounds (juce::Rectangle<int>::fromString (saved));
        else
        {
            // first launch: fit the screen
            int w = (int) (user.getWidth() * 0.92), hgt = w * AuralisEditor::kH / AuralisEditor::kW;
            if (hgt > user.getHeight() * 0.92) { hgt = (int) (user.getHeight() * 0.92); w = hgt * AuralisEditor::kW / AuralisEditor::kH; }
            centreWithSize (w, hgt);
        }
        setVisible (true);
    }
    ~MainWindow() override
    {
        if (holder.settings) holder.settings->setValue ("windowBounds", getBounds().toString());
        clearContentComponent();
        if (editor) holder.processor->editorBeingDeleted (editor.get());
        editor.reset();
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
private:
    juce::StandalonePluginHolder& holder;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};

class AuralisApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Auralis"; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&look);
        const auto args = getCommandLineParameterArray();
        if (args.contains ("--render")) { runCli (args); return; }
        if (args.contains ("--simulate-bounce")) { simulateBounce (args); return; }

        juce::PropertiesFile::Options o;
        o.applicationName = "Auralis"; o.filenameSuffix = ".settings"; o.folderName = "Auralis";
        o.osxLibrarySubFolder = "Application Support";
        holder = std::make_unique<juce::StandalonePluginHolder> (new juce::PropertiesFile (o), true);
        holder->shouldMuteInput.setValue (false);
        if (auto* p = dynamic_cast<AuralisProcessor*> (holder->processor.get()))
            p->showAudioSettings = [this] { holder->showAudioSettingsDialog(); };
        window = std::make_unique<MainWindow> (*holder);
        for (auto& a : args) openPath (a);
    }

    void shutdown() override
    {
        ExportWindow::closeAll();
        window.reset();
        if (holder) { holder->savePluginState(); holder.reset(); }
        cliProc.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String& cmd) override
    {
        for (auto& a : juce::StringArray::fromTokens (cmd, true)) openPath (a.unquoted());
    }

private:
    void openPath (const juce::String& s)
    {
        if (s.startsWith ("-") || ! juce::File::isAbsolutePath (s)) return;
        juce::File f (s);
        if (! f.existsAsFile() || ! holder) return;
        if (auto* ed = dynamic_cast<AuralisEditor*> (holder->processor->getActiveEditor())) ed->openFile (f);
    }

    static juce::String arg (const juce::StringArray& a, const char* key, const juce::String& def = {})
    {
        const int i = a.indexOf (key);
        return i >= 0 && i + 1 < a.size() ? a[i + 1] : def;
    }

    void runCli (const juce::StringArray& a)
    {
        auto die = [this] (const juce::String& msg) { std::fprintf (stderr, "%s\n", msg.toRawUTF8()); setApplicationReturnValue (2); quit(); };
        const juce::File in (juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--render")));
        const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--out", in.getFileNameWithoutExtension() + ".mp4")));
        if (! in.existsAsFile()) { die ("input not found: " + in.getFullPathName()); return; }

        cliProc = std::make_unique<AuralisProcessor>();
        auto& p = *cliProc;
        p.launchExport = nullptr;

        const auto presetArg = arg (a, "--preset", "1");
        const auto& fp = factoryPresets();
        int idx = presetArg.getIntValue() - 1;
        for (int i = 0; i < (int) fp.size(); ++i) if (presetArg.equalsIgnoreCase (juce::String (fp[(size_t) i].en))) idx = i;
        idx = juce::jlimit (0, (int) fp.size() - 1, idx);
        const auto title = arg (a, "--title", "AURALIS").toStdString();
        const auto video = arg (a, "--video");
        p.editState ([&] (SceneState& s)
        {
            s.scene = fp[(size_t) idx].scene; s.scene.title = title;
            if (video.isNotEmpty()) { s.scene.videoPath = juce::File::getCurrentWorkingDirectory().getChildFile (video).getFullPathName().toStdString(); s.scene[P::vdOn] = 1.f; }
            if (a.contains ("--framed")) s.scene[P::vdMode] = 1.f;
        });
        const auto lyr = arg (a, "--lyrics");
        if (lyr.isNotEmpty())
        {
            const juce::File lf (juce::File::getCurrentWorkingDirectory().getChildFile (lyr));
            const auto text = lf.loadFileAsString().toStdString();
            if (lf.hasFileExtension (".lrc")) p.setLyrics (parseLrc (text));
            else { auto l = parsePlain (text); distributeEvenly (l, 60.0); p.setLyrics (l); }
        }
        ExportPrefs pr;
        const auto asp = arg (a, "--aspect", "16:9");
        pr.aspect = asp == "9:16" ? 1 : (asp == "1:1" ? 2 : 0);
        const int res = arg (a, "--res", "1080").getIntValue();
        pr.resolution = res >= 2160 ? 3 : (res >= 1440 ? 2 : (res >= 1080 ? 1 : 0));
        const int fps = arg (a, "--fps", "30").getIntValue();
        const int fl[] = { 24, 25, 30, 50, 60 };
        for (int i = 0; i < 5; ++i) if (fl[i] == fps) pr.fps = i;
        p.setPrefs (pr);

        auto req = p.makeExportRequest (in, out);
        req.startSec = arg (a, "--start", "0").getDoubleValue();
        req.lengthSec = arg (a, "--seconds", "-1").getDoubleValue();
        req.timelineStart = req.startSec;
        req.preferFfmpeg = a.contains ("--ffmpeg");
        req.videoBitrateMbps = arg (a, "--bitrate", "0").getIntValue();
        if (a.contains ("--text-instant")) req.state.scene[P::txAnim] = 2.f;
        std::printf ("Auralis: rendering %s -> %s  (%dx%d @ %.0f fps, preset %d \"%s\")\n", in.getFileName().toRawUTF8(), out.getFullPathName().toRawUTF8(),
                     req.width, req.height, req.fps, idx + 1, fp[(size_t) idx].en.c_str());
        std::fflush (stdout);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        ExportWindow::launch (req, false, [this, t0] (bool ok, juce::File f, juce::String err)
        {
            const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
            if (ok) std::printf ("done in %.1f s: %s\n", secs, f.getFullPathName().toRawUTF8());
            else std::fprintf (stderr, "failed: %s\n", err.toRawUTF8());
            std::fflush (stdout); std::fflush (stderr);
            setApplicationReturnValue (ok ? 0 : 1);
            juce::MessageManager::callAsync ([this] { quit(); });
        });
    }

    // Test hook: feeds a file through processBlock in non-realtime mode, exactly like a DAW audio export,
    // and lets the plug-in's own bounce detection produce the video.
    void simulateBounce (const juce::StringArray& a)
    {
        const juce::File in (juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--simulate-bounce")));
        const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--out", "bounce.mp4")));
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (in));
        if (! r) { setApplicationReturnValue (2); quit(); return; }
        cliProc = std::make_unique<AuralisProcessor>();
        auto& p = *cliProc;
        ExportPrefs pr; pr.resolution = 0; pr.autoOnBounce = true; p.setPrefs (pr);
        p.editState ([] (SceneState& s) { s.scene = factoryPresets()[45].scene; s.scene.title = "BOUNCE"; });
        p.launchExport = [this, out] (ExportRequest req)
        {
            req.outFile = out;
            std::printf ("bounce captured: %.2f s -> rendering video\n", 0.0); std::fflush (stdout);
            ExportWindow::launch (req, false, [this] (bool ok, juce::File f, juce::String err)
            {
                std::printf (ok ? "bounce video: %s\n" : "bounce failed: %s\n", ok ? f.getFullPathName().toRawUTF8() : err.toRawUTF8());
                std::fflush (stdout);
                setApplicationReturnValue (ok ? 0 : 1);
                juce::MessageManager::callAsync ([this] { quit(); });
            });
        };
        p.setRateAndBufferSizeDetails (r->sampleRate, 512);
        p.prepareToPlay (r->sampleRate, 512);
        p.setNonRealtime (true);
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer midi;
        for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += 512)
        {
            const int n = (int) std::min<juce::int64> (512, r->lengthInSamples - pos);
            b.setSize (2, n, false, false, true);
            r->read (&b, 0, n, pos, true, true);
            p.processBlock (b, midi);
        }
        p.setNonRealtime (false);     // host finished exporting; the plug-in's timer picks it up
    }

    Look look;
    std::unique_ptr<juce::StandalonePluginHolder> holder;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<AuralisProcessor> cliProc;
};
}

juce::JUCEApplicationBase* juce_CreateApplication() { return new au::AuralisApp(); }
