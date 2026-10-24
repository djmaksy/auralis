#include "Editor.h"
#include "GlViews.h"
#include "LayerPanel.h"
#include "Timeline.h"
#include "Strings.h"

namespace au
{
// ============================================================================ helpers
static bool hasExt (const juce::File& f, std::initializer_list<const char*> exts)
{
    const auto e = f.getFileExtension().toLowerCase();
    for (auto* x : exts) if (e == x) return true;
    return false;
}
static const std::initializer_list<const char*> audioExts { ".wav", ".mp3", ".flac", ".aif", ".aiff", ".ogg", ".m4a", ".aac", ".wma" };
static const std::initializer_list<const char*> videoExts { ".mp4", ".mov", ".mkv", ".webm", ".avi", ".m4v", ".wmv", ".png", ".jpg", ".jpeg", ".gif" };

static juce::String fmtTime (double s)
{
    const int t = (int) std::max (0.0, s);
    return juce::String::formatted ("%d:%02d", t / 60, t % 60);
}

juce::File AuralisEditor::presetDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Auralis").getChildFile ("Presets");
    d.createDirectory();
    return d;
}
juce::Array<juce::File> AuralisEditor::userPresets() const
{
    auto a = presetDir().findChildFiles (juce::File::findFiles, false, "*.auralis");
    a.sort();
    return a;
}

// ============================================================================ logo: "AURALIS by DJ Maksy"
void AuralisEditor::Logo::paint (juce::Graphics& g)
{
    const float h = (float) getHeight();
    // mark: a ring of particles with a glowing core
    const float r = h * 0.36f, cx = r + 3.f, cy = h * 0.5f;
    for (int i = 0; i < 40; ++i)
    {
        const float a = (float) i / 40.f * juce::MathConstants<float>::twoPi;
        const float rr = r * (1.f + 0.13f * std::sin (a * 8.f));
        const float d = 1.2f + 0.9f * (0.5f + 0.5f * std::sin (a * 3.f));
        g.setColour (colours::accent.withAlpha (0.55f + 0.45f * (0.5f + 0.5f * std::cos (a * 5.f))));
        g.fillEllipse (cx + rr * std::cos (a) - d * 0.5f, cy + rr * std::sin (a) - d * 0.5f, d, d);
    }
    g.setGradientFill (juce::ColourGradient (colours::accent2, cx, cy, colours::accent2.withAlpha (0.f), cx + r * 0.6f, cy, true));
    g.fillEllipse (cx - r * 0.6f, cy - r * 0.6f, r * 1.2f, r * 1.2f);

    const float tx = cx + r + 9.f;
    g.setColour (colours::accent);
    g.setFont (juce::FontOptions (h * 0.48f, juce::Font::bold));
    g.drawText ("AURALIS", juce::Rectangle<float> (tx, h * 0.02f, (float) getWidth() - tx, h * 0.56f), juce::Justification::bottomLeft);
    g.setColour (colours::text.withAlpha (0.75f));
    g.setFont (juce::FontOptions (h * 0.27f, juce::Font::italic));
    g.drawText ("by DJ Maksy", juce::Rectangle<float> (tx + 1.f, h * 0.58f, (float) getWidth() - tx, h * 0.36f), juce::Justification::topLeft);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (h * 0.24f));
    g.drawText ("v" JucePlugin_VersionString, juce::Rectangle<float> (tx + 1.f, h * 0.58f, (float) getWidth() - tx - 2.f, h * 0.36f), juce::Justification::topRight);
}

// ============================================================================ lyrics dialog
class LyricsDialog : public juce::Component
{
public:
    LyricsDialog (const juce::String& text, bool ru, std::function<void (juce::String, bool)> cb) : done (std::move (cb))
    {
        addAndMakeVisible (hint); hint.setText (tr ("editLyrics", ru), juce::dontSendNotification);
        hint.setFont (juce::FontOptions (13.f)); hint.setColour (juce::Label::textColourId, colours::dim);
        addAndMakeVisible (editor);
        editor.setMultiLine (true, true); editor.setReturnKeyStartsNewLine (true);
        editor.setFont (juce::FontOptions (16.f)); editor.setText (text, false);
        for (auto* b : { &apply, &applySync, &cancel }) addAndMakeVisible (*b);
        apply.setButtonText (tr ("apply", ru)); applySync.setButtonText (tr ("applySync", ru)); cancel.setButtonText (tr ("cancel", ru));
        apply.onClick = [this] { finish (true, false); };
        applySync.onClick = [this] { finish (true, true); };
        cancel.onClick = [this] { finish (false, false); };
        setSize (620, 520);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        hint.setBounds (r.removeFromTop (40));
        auto b = r.removeFromBottom (32);
        cancel.setBounds (b.removeFromRight (100)); b.removeFromRight (8);
        apply.setBounds (b.removeFromRight (120)); b.removeFromRight (8);
        applySync.setBounds (b.removeFromRight (230));
        r.removeFromBottom (8);
        editor.setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (colours::panel); }
private:
    void finish (bool ok, bool sync)
    {
        auto cb = done; auto txt = editor.getText();
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (0);
        if (ok && cb) cb (txt, sync);
    }
    juce::Label hint; juce::TextEditor editor; juce::TextButton apply, applySync, cancel;
    std::function<void (juce::String, bool)> done;
};

// ============================================================================ content (fixed design size, scaled)
class AuralisEditor::Content : public juce::Component
{
public:
    explicit Content (AuralisEditor& e) : ed (e) {}
    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bg);
        g.setColour (colours::panel); g.fillRect (0, 0, kW, 52);
        g.setColour (colours::line); g.drawHorizontalLine (52, 0.f, (float) kW);
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (ed.dragOver)
        {
            g.setColour (colours::accent.withAlpha (0.12f)); g.fillRect (getLocalBounds());
            g.setColour (colours::accent); g.drawRect (getLocalBounds(), 3);
            g.setFont (juce::FontOptions (22.f));
            g.drawText (tr ("dropHint", ed.proc.russian), getLocalBounds(), juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (ed.proc.isStandalone() && e.y < 52)
            if (auto* top = getTopLevelComponent()) ed.dragger.startDraggingComponent (top, e.getEventRelativeTo (top));
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (ed.proc.isStandalone())
            if (auto* top = getTopLevelComponent()) ed.dragger.dragComponent (top, e.getEventRelativeTo (top), nullptr);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (ed.proc.isStandalone() && e.y < 52)
            if (auto* peer = getPeer()) peer->setFullScreen (! peer->isFullScreen());
    }
    AuralisEditor& ed;
};

// ============================================================================ editor
AuralisEditor::AuralisEditor (AuralisProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    content = std::make_unique<Content> (*this);
    content->setSize (kW, kH);
    addAndMakeVisible (*content);
    auto add = [this] (juce::Component& c) { content->addAndMakeVisible (c); };

    logo.setInterceptsMouseClicks (false, false);
    add (logo);

    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &delBtn, &exportBtn, &langBtn, &fullBtn, &gearBtn, &minBtn, &closeBtn,
                     &openBtn, &playBtn, &lyricsBtn, &loadLyricsBtn, &saveLrcBtn, &recBtn }) add (*b);
    for (auto* c : { &presetBox, &aspectBox, &resBox, &fpsBox, &langBox }) add (*c);
    add (timeLabel); add (statusLabel); add (titleLabel); add (titleEdit); add (offsetSlider); add (offsetLabel);

    exportBtn.setColour (juce::TextButton::buttonColourId, colours::accent);
    exportBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) selectPreset (presetBox.getSelectedId()); };
    saveBtn.onClick = [this] { savePreset(); };
    delBtn.onClick = [this] { deleteUserPreset(); };
    exportBtn.onClick = [this] { showExportMenu(); };
    langBtn.onClick = [this] { proc.russian = ! proc.russian; relabel(); };
    fullBtn.onClick = [this] { toggleFullscreen(); };
    gearBtn.onClick = [this] { if (proc.showAudioSettings) proc.showAudioSettings(); };
    minBtn.onClick = [this] { if (auto* peer = getPeer()) peer->setMinimised (true); };
    closeBtn.onClick = [] { if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); };
    gearBtn.setVisible (proc.isStandalone()); minBtn.setVisible (proc.isStandalone()); closeBtn.setVisible (proc.isStandalone());
    gearBtn.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9a\x99"));
    fullBtn.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9b\xb6"));

    aspectBox.addItemList ({ "16:9", "9:16", "1:1" }, 1);
    resBox.addItemList ({ "720p", "1080p", "1440p", "4K" }, 1);
    fpsBox.addItemList ({ "24 fps", "25 fps", "30 fps", "50 fps", "60 fps" }, 1);
    auto prefsChanged = [this]
    {
        auto pr = proc.prefs();
        pr.aspect = aspectBox.getSelectedItemIndex(); pr.resolution = resBox.getSelectedItemIndex(); pr.fps = fpsBox.getSelectedItemIndex();
        proc.setPrefs (pr);
        resized();
    };
    aspectBox.onChange = prefsChanged; resBox.onChange = prefsChanged; fpsBox.onChange = prefsChanged;

    openBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("openTrack", proc.russian), juce::File(), "*.wav;*.mp3;*.flac;*.aif;*.aiff;*.ogg;*.m4a;*.aac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) openFile (fc.getResult()); });
    };
    playBtn.onClick = [this] { proc.setPlaying (! proc.isPlaying()); };
    lyricsBtn.onClick = [this] { editLyrics(); };
    loadLyricsBtn.onClick = [this] { loadLyricsFile(); };
    saveLrcBtn.onClick = [this] { saveLrc(); };
    recBtn.onClick = [this] { if (recognizer.isBusy()) recognizer.cancel(); else if (download) download.reset(), downloadProgress = -1.f; else startRecognition(); };
    langBox.onChange = [this] { proc.recognitionLanguage = langBox.getSelectedItemIndex(); };

    titleEdit.setFont (juce::FontOptions (14.f));
    titleEdit.onTextChange = [this] { const auto t = titleEdit.getText().toStdString(); proc.editState ([&] (SceneState& s) { s.scene.title = t; }); };
    titleLabel.setFont (juce::FontOptions (12.5f)); titleLabel.setColour (juce::Label::textColourId, colours::dim);
    offsetLabel.setFont (juce::FontOptions (12.5f)); offsetLabel.setColour (juce::Label::textColourId, colours::dim);
    offsetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    offsetSlider.setRange (-3.0, 3.0, 0.01); offsetSlider.setDoubleClickReturnValue (true, 0.0);
    offsetSlider.setTextValueSuffix (" s");
    offsetSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    offsetSlider.onValueChange = [this] { const double v = offsetSlider.getValue(); proc.editState ([&] (SceneState& s) { s.lyricsOffset = v; }); };
    timeLabel.setFont (juce::FontOptions (13.f)); timeLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setFont (juce::FontOptions (12.5f)); statusLabel.setJustificationType (juce::Justification::centredRight);

    view = std::make_unique<LiveView> (proc, 1920);
    view->onDoubleClick = [this] { toggleFullscreen(); };
    add (*view);
    layers = std::make_unique<LayerPanel> (proc);
    layers->onChooseVideo = [this] { chooseVideo(); };
    add (*layers);
    timeline = std::make_unique<Timeline> (proc);
    add (*timeline);

    relabel();
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / kH);
    setSize (kW, kH);
    startTimerHz (20);
}

AuralisEditor::~AuralisEditor()
{
    stopTimer();
    recognizer.cancel();
    fullscreen.reset();
    view.reset();
    setLookAndFeel (nullptr);
}

void AuralisEditor::paint (juce::Graphics& g) { g.fillAll (colours::bg); }

void AuralisEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content->setTransform (juce::AffineTransform::scale (s));
    content->setBounds (0, 0, kW, kH);

    // header
    juce::Rectangle<int> h (0, 0, kW, 52);
    h = h.reduced (12, 10);
    logo.setBounds (h.removeFromLeft (150).withTrimmedTop (-6).withTrimmedBottom (-6)); h.removeFromLeft (6);
    prevBtn.setBounds (h.removeFromLeft (28)); h.removeFromLeft (4);
    presetBox.setBounds (h.removeFromLeft (212)); h.removeFromLeft (4);
    nextBtn.setBounds (h.removeFromLeft (28)); h.removeFromLeft (8);
    saveBtn.setBounds (h.removeFromLeft (92)); h.removeFromLeft (4);
    delBtn.setBounds (h.removeFromLeft (80));
    if (proc.isStandalone()) { closeBtn.setBounds (h.removeFromRight (32)); h.removeFromRight (4); minBtn.setBounds (h.removeFromRight (32)); h.removeFromRight (10);
                               gearBtn.setBounds (h.removeFromRight (36)); h.removeFromRight (6); }
    fullBtn.setBounds (h.removeFromRight (36)); h.removeFromRight (6);
    langBtn.setBounds (h.removeFromRight (48)); h.removeFromRight (14);
    exportBtn.setBounds (h.removeFromRight (100)); h.removeFromRight (8);
    fpsBox.setBounds (h.removeFromRight (82)); h.removeFromRight (4);
    resBox.setBounds (h.removeFromRight (80)); h.removeFromRight (4);
    aspectBox.setBounds (h.removeFromRight (70));

    // right panel
    const int panelW = 392;
    layers->setBounds (kW - panelW - 10, 62, panelW, kH - 72);

    // preview + timeline + controls
    juce::Rectangle<int> left (12, 62, kW - panelW - 34, kH - 72);
    auto row2 = left.removeFromBottom (30); left.removeFromBottom (6);
    auto row1 = left.removeFromBottom (30); left.removeFromBottom (6);
    timeline->setBounds (left.removeFromBottom (74)); left.removeFromBottom (8);
    view->setBounds (left);

    openBtn.setBounds (row1.removeFromLeft (112)); row1.removeFromLeft (4);
    playBtn.setBounds (row1.removeFromLeft (70)); row1.removeFromLeft (4);
    timeLabel.setBounds (row1.removeFromLeft (96)); row1.removeFromLeft (8);
    lyricsBtn.setBounds (row1.removeFromLeft (82)); row1.removeFromLeft (4);
    loadLyricsBtn.setBounds (row1.removeFromLeft (150)); row1.removeFromLeft (4);
    saveLrcBtn.setBounds (row1.removeFromLeft (108)); row1.removeFromLeft (4);
    recBtn.setBounds (row1);

    titleLabel.setBounds (row2.removeFromLeft (64));
    titleEdit.setBounds (row2.removeFromLeft (190).reduced (0, 2)); row2.removeFromLeft (10);
    offsetLabel.setBounds (row2.removeFromLeft (52));
    offsetSlider.setBounds (row2.removeFromLeft (150)); row2.removeFromLeft (8);
    langBox.setBounds (row2.removeFromLeft (160).reduced (0, 2)); row2.removeFromLeft (8);
    statusLabel.setBounds (row2);
}

void AuralisEditor::relabel()
{
    const bool ru = proc.russian;
    saveBtn.setButtonText (tr ("save", ru)); delBtn.setButtonText (tr ("delete", ru));
    exportBtn.setButtonText (tr ("export", ru)); langBtn.setButtonText (ru ? "RU" : "EN");
    openBtn.setButtonText (tr ("openTrack", ru)); lyricsBtn.setButtonText (tr ("lyrics", ru));
    loadLyricsBtn.setButtonText (tr ("loadLyrics", ru)); saveLrcBtn.setButtonText (tr ("saveLrc", ru));
    recBtn.setButtonText (tr ("recognise", ru));
    titleLabel.setText (tr ("title", ru), juce::dontSendNotification);
    offsetLabel.setText (tr ("offset", ru), juce::dontSendNotification);
    fullBtn.setTooltip (tr ("fullscreen", ru)); gearBtn.setTooltip (tr ("audioSettings", ru));
    langBox.clear (juce::dontSendNotification);
    int id = 1;
    for (auto& l : recognitionLanguages()) langBox.addItem (juce::String::fromUTF8 (ru ? l.ru : l.en), id++);
    langBox.setSelectedItemIndex (proc.recognitionLanguage, juce::dontSendNotification);
    buildPresetMenu();
    if (layers) layers->relabel();
    lastVersion = -1;
}

void AuralisEditor::buildPresetMenu()
{
    const bool ru = proc.russian;
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading (tr ("factory", ru));
    const auto& fp = factoryPresets();
    int selected = 0;
    for (int i = 0; i < (int) fp.size(); ++i)
    {
        presetBox.addItem (juce::String (i + 1) + ".  " + juce::String::fromUTF8 ((ru ? fp[(size_t) i].ru : fp[(size_t) i].en).c_str()), i + 1);
        if (fp[(size_t) i].en == proc.presetName) selected = i + 1;
    }
    presetFiles.clear();
    const auto ups = userPresets();
    if (! ups.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading (tr ("user", ru));
        for (int i = 0; i < ups.size(); ++i)
        {
            presetFiles.push_back (ups[i]);
            presetBox.addItem (ups[i].getFileNameWithoutExtension(), 1000 + i);
            if ("user:" + ups[i].getFileNameWithoutExtension().toStdString() == proc.presetName) selected = 1000 + i;
        }
    }
    presetBox.setSelectedId (selected, juce::dontSendNotification);
    delBtn.setEnabled (selected >= 1000);
}

void AuralisEditor::selectPreset (int id)
{
    if (id >= 1 && id <= (int) factoryPresets().size())
    {
        const auto& pr = factoryPresets()[(size_t) id - 1];
        proc.editState ([&] (SceneState& s)
        {
            const auto title = s.scene.title; const auto vid = s.scene.videoPath; const bool vOn = s.scene.on (P::vdOn);
            const float vm = s.scene[P::vdMode];
            s.scene = pr.scene; s.scene.title = title; s.scene.videoPath = vid;
            s.scene[P::vdOn] = vOn ? 1.f : 0.f; s.scene[P::vdMode] = vm;
            s.mods = {};
        });
        proc.presetName = pr.en;
    }
    else if (id >= 1000 && id - 1000 < (int) presetFiles.size())
    {
        const auto f = presetFiles[(size_t) (id - 1000)];
        const auto v = juce::JSON::parse (f);
        proc.editState ([&] (SceneState& s)
        {
            const auto title = s.scene.title; const auto vid = s.scene.videoPath;
            sceneFromVar (v["scene"], s, false);
            s.scene.title = title; s.scene.videoPath = vid;
        });
        proc.presetName = "user:" + f.getFileNameWithoutExtension().toStdString();
    }
    presetBox.setSelectedId (id, juce::dontSendNotification);
    delBtn.setEnabled (id >= 1000);
}

void AuralisEditor::stepPreset (int d)
{
    juce::Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i) ids.add (presetBox.getItemId (i));
    if (ids.isEmpty()) return;
    int idx = ids.indexOf (presetBox.getSelectedId());
    idx = idx < 0 ? 0 : (idx + d + ids.size()) % ids.size();
    selectPreset (ids[idx]);
}

void AuralisEditor::savePreset()
{
    auto* aw = new juce::AlertWindow (tr ("save", proc.russian), tr ("presetName", proc.russian), juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", proc.presetName.rfind ("user:", 0) == 0 ? juce::String (proc.presetName.substr (5)) : juce::String());
    aw->addButton (tr ("save", proc.russian), 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton (tr ("cancel", proc.russian), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        const auto name = juce::File::createLegalFileName (aw->getTextEditorContents ("name").trim());
        if (r == 1 && name.isNotEmpty())
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("scene", sceneToVar (proc.snapshot()));
            presetDir().getChildFile (name + ".auralis").replaceWithText (juce::JSON::toString (juce::var (o)));
            proc.presetName = "user:" + name.toStdString();
            buildPresetMenu();
        }
    }), true);
}

void AuralisEditor::deleteUserPreset()
{
    const int id = presetBox.getSelectedId();
    if (id < 1000 || id - 1000 >= (int) presetFiles.size()) return;
    presetFiles[(size_t) (id - 1000)].deleteFile();
    proc.presetName = factoryPresets().front().en;
    buildPresetMenu();
}

void AuralisEditor::setStatus (const juce::String& s, bool err)
{
    status = s; statusError = err; statusTime = juce::Time::getMillisecondCounterHiRes();
    statusLabel.setColour (juce::Label::textColourId, err ? juce::Colour (0xffff6b6b) : colours::accent2);
    statusLabel.setText (s, juce::dontSendNotification);
}

// ---------------------------------------------------------------- files
void AuralisEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false; repaint();
    for (auto& f : files) openFile (juce::File (f));
}

void AuralisEditor::openFile (const juce::File& f)
{
    const bool ru = proc.russian;
    if (hasExt (f, audioExts))
    {
        juce::String err;
        if (proc.loadTrack (f, err)) setStatus (f.getFileName());
        else setStatus (err, true);
    }
    else if (hasExt (f, videoExts))
    {
        const auto path = f.getFullPathName().toStdString();
        proc.editState ([&] (SceneState& s) { s.scene.videoPath = path; s.scene[P::vdOn] = 1.f; });
        setStatus (f.getFileName());
    }
    else if (hasExt (f, { ".lrc" }))
    {
        auto l = parseLrc (f.loadFileAsString().toStdString());
        if (l.empty()) { setStatus (".lrc: 0 lines", true); return; }
        proc.rawLyricsText = toPlainText (l);
        proc.setLyrics (std::move (l));
        setStatus (f.getFileName());
    }
    else if (hasExt (f, { ".txt" }))
    {
        proc.rawLyricsText = f.loadFileAsString().toStdString();
        auto l = parsePlain (proc.rawLyricsText);
        distributeEvenly (l, proc.trackLength() > 0 ? proc.trackLength() : 180.0);
        proc.setLyrics (std::move (l));
        setStatus (tr ("recognise", ru) + "  →");
    }
    else if (hasExt (f, { ".auralis" }))
    {
        f.copyFileTo (presetDir().getChildFile (f.getFileName()));
        buildPresetMenu();
        for (int i = 0; i < (int) presetFiles.size(); ++i) if (presetFiles[(size_t) i].getFileName() == f.getFileName()) selectPreset (1000 + i);
    }
}

void AuralisEditor::chooseVideo()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("videoFile", proc.russian), juce::File(), "*.mp4;*.mov;*.mkv;*.webm;*.avi;*.m4v;*.wmv;*.png;*.jpg;*.jpeg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) openFile (fc.getResult()); });
}

void AuralisEditor::loadLyricsFile()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("loadLyrics", proc.russian), juce::File(), "*.lrc;*.txt");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) openFile (fc.getResult()); });
}

void AuralisEditor::saveLrc()
{
    const auto st = proc.snapshot();
    if (! st.lyrics || st.lyrics->empty()) return;
    chooser = std::make_unique<juce::FileChooser> (tr ("saveLrc", proc.russian), proc.getTrack().withFileExtension (".lrc"), "*.lrc");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, st] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        Lyrics shifted = *st.lyrics;
        for (auto& ln : shifted.lines) { ln.start -= st.lyricsOffset; ln.end -= st.lyricsOffset; for (auto& w : ln.words) { w.start -= st.lyricsOffset; w.end -= st.lyricsOffset; } }
        f.withFileExtension (".lrc").replaceWithText (juce::String::fromUTF8 (toLrc (shifted).c_str()));
    });
}

void AuralisEditor::editLyrics()
{
    const auto st = proc.snapshot();
    juce::String text = juce::String::fromUTF8 (proc.rawLyricsText.c_str());
    if (text.isEmpty() && st.lyrics) text = juce::String::fromUTF8 (toPlainText (*st.lyrics).c_str());
    juce::DialogWindow::LaunchOptions o;
    juce::Component::SafePointer<AuralisEditor> safe (this);
    o.content.setOwned (new LyricsDialog (text, proc.russian, [safe] (juce::String t, bool sync)
    {
        if (! safe) return;
        auto& self = *safe;
        self.proc.rawLyricsText = t.toStdString();
        auto l = parsePlain (self.proc.rawLyricsText);
        distributeEvenly (l, self.proc.trackLength() > 0 ? self.proc.trackLength() : 180.0);
        self.proc.setLyrics (std::move (l));
        if (sync) self.startRecognition();
    }));
    o.dialogTitle = tr ("lyrics", proc.russian);
    o.dialogBackgroundColour = colours::panel;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.componentToCentreAround = this;
    o.launchAsync();
}

// ---------------------------------------------------------------- recognition
struct DownloadListener : juce::URL::DownloadTaskListener
{
    std::function<void (bool)> onFinish; std::atomic<float>* progressOut = nullptr;
    void finished (juce::URL::DownloadTask*, bool ok) override { auto f = onFinish; juce::MessageManager::callAsync ([f, ok] { if (f) f (ok); }); }
    void progress (juce::URL::DownloadTask*, juce::int64 done, juce::int64 total) override { if (progressOut && total > 0) *progressOut = (float) ((double) done / (double) total); }
};
static DownloadListener& dlListener() { static DownloadListener l; return l; }

void AuralisEditor::startRecognition()
{
    const bool ru = proc.russian;
    if (! WhisperModels::available()) { setStatus ("Whisper: n/a", true); return; }
    const auto model = WhisperModels::defaultModel();
    if (model.existsAsFile()) { runRecognition(); return; }

    juce::Component::SafePointer<AuralisEditor> safe (this);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Whisper", tr ("modelAsk", ru), "OK", tr ("cancel", ru), this,
        juce::ModalCallbackFunction::create ([safe, model] (int r)
    {
        if (! safe || r != 1) return;
        auto& self = *safe;
        const auto part = model.withFileExtension (".part");
        dlListener().progressOut = &self.downloadProgress;
        dlListener().onFinish = [safe, model, part] (bool ok)
        {
            if (! safe) return;
            safe->download.reset(); safe->downloadProgress = -1.f;
            if (ok && part.getSize() > 50 * 1024 * 1024 && part.moveFileTo (model)) safe->runRecognition();
            else { part.deleteFile(); safe->setStatus ("Download failed", true); }
        };
        self.downloadProgress = 0.f;
        self.download = WhisperModels::downloadUrl().downloadToFile (part, juce::URL::DownloadTaskOptions().withListener (&dlListener()));
        if (! self.download) { self.downloadProgress = -1.f; self.setStatus ("Download failed", true); }
    }));
}

void AuralisEditor::runRecognition()
{
    const bool ru = proc.russian;
    const auto track = proc.getTrack();
    const std::string lang = recognitionLanguages()[(size_t) juce::jlimit (0, (int) recognitionLanguages().size() - 1, langBox.getSelectedItemIndex())].code;
    std::string prompt = juce::String::fromUTF8 (proc.rawLyricsText.c_str()).replaceCharacters ("\r\n", "  ").substring (0, 220).toStdString();
    juce::Component::SafePointer<AuralisEditor> safe (this);

    auto launch = [safe, lang, prompt] (std::vector<float> audio, double seconds)
    {
        if (! safe) return;
        LyricsRecognizer::Job job;
        job.mono16k = std::move (audio); job.language = lang; job.prompt = prompt;
        job.modelPath = WhisperModels::defaultModel().getFullPathName().toStdString();
        safe->recognizer.start (std::move (job), [safe, seconds] (bool ok, std::vector<LyricWord> words, juce::String err)
        {
            if (! safe) return;
            auto& self = *safe;
            if (! ok) { self.setStatus (err, true); return; }
            Lyrics l = self.proc.rawLyricsText.empty() ? fromRecognized (words)
                                                       : alignToRecognized (parsePlain (self.proc.rawLyricsText), words, seconds);
            if (self.proc.rawLyricsText.empty()) self.proc.rawLyricsText = toPlainText (l);
            self.proc.setLyrics (std::move (l));
            self.setStatus (tr ("recognised", self.proc.russian) + " (" + juce::String ((int) words.size()) + ")");
        });
    };

    if (track.existsAsFile())
    {
        setStatus (tr ("recognising", ru) + "…");
        juce::Thread::launch ([safe, track, launch]
        {
            std::vector<float> a; double secs = 0; juce::String err;
            const bool ok = loadAudioForRecognition (track, a, secs, err);
            juce::MessageManager::callAsync ([safe, ok, a = std::move (a), secs, err, launch]() mutable
            {
                if (! safe) return;
                if (! ok) { safe->setStatus (err, true); return; }
                launch (std::move (a), secs);
            });
        });
        return;
    }
    auto cap = proc.hub.getCapture();
    if (cap.size() < 16000 * 5) { setStatus (tr ("noAudio", ru), true); return; }
    // normalise and band-limit the capture like a file
    juce::AudioBuffer<float> b (1, (int) cap.size());
    b.copyFrom (0, 0, cap.data(), (int) cap.size());
    auto prepared = prepareForRecognition (b, 16000.0);
    launch (std::move (prepared), (double) cap.size() / 16000.0);
}

// ---------------------------------------------------------------- export
void AuralisEditor::showExportMenu()
{
    const bool ru = proc.russian;
    const auto pr = proc.prefs();
    juce::PopupMenu m;
    m.addItem (1, tr ("renderTrack", ru), proc.getTrack().existsAsFile());
    if (! proc.isStandalone()) m.addItem (2, tr ("autoBounce", ru), true, pr.autoOnBounce);
    juce::PopupMenu br;
    for (int i = 0; i < ExportPrefs::kBitrateCount; ++i)
        br.addItem (100 + i, i == 0 ? tr ("bitrateAuto", ru) : juce::String (ExportPrefs::bitrateMbps (i)) + " Mbps", true, pr.bitrate == i);
    m.addSeparator();
    m.addSubMenu (tr ("bitrate", ru) + ":  " + (pr.bitrate == 0 ? tr ("bitrateAuto", ru) : juce::String (ExportPrefs::bitrateMbps (pr.bitrate)) + " Mbps"), br);
    m.addItem (3, tr ("outFolder", ru));
    if (! proc.isStandalone()) m.addItem (4, tr ("fileName", ru));
    m.addItem (5, tr ("openFolder", ru));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportBtn), [this] (int r)
    {
        auto p = proc.prefs();
        if (r >= 100 && r < 100 + ExportPrefs::kBitrateCount) { p.bitrate = r - 100; proc.setPrefs (p); }
        else if (r == 1) renderTrack();
        else if (r == 2) { p.autoOnBounce = ! p.autoOnBounce; proc.setPrefs (p); }
        else if (r == 3)
        {
            chooser = std::make_unique<juce::FileChooser> (tr ("outFolder", proc.russian), proc.defaultOutputFile().getParentDirectory());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().isDirectory()) { auto pp = proc.prefs(); pp.folder = fc.getResult().getFullPathName().toStdString(); proc.setPrefs (pp); }
            });
        }
        else if (r == 4)
        {
            auto* aw = new juce::AlertWindow (tr ("fileName", proc.russian), {}, juce::MessageBoxIconType::NoIcon, this);
            aw->addTextEditor ("n", juce::String::fromUTF8 (p.fileName.c_str()));
            aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton (tr ("cancel", proc.russian), 0, juce::KeyPress (juce::KeyPress::escapeKey));
            aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int res)
            {
                if (res == 1) { auto pp = proc.prefs(); pp.fileName = aw->getTextEditorContents ("n").trim().toStdString(); proc.setPrefs (pp); }
            }), true);
        }
        else if (r == 5) proc.defaultOutputFile().getParentDirectory().startAsProcess();
    });
}

void AuralisEditor::renderTrack()
{
    const auto track = proc.getTrack();
    if (! track.existsAsFile()) { setStatus (tr ("needTrack", proc.russian), true); return; }
    auto def = proc.defaultOutputFile().getParentDirectory().getNonexistentChildFile (track.getFileNameWithoutExtension(), ".mp4", false);
    chooser = std::make_unique<juce::FileChooser> (tr ("render", proc.russian), def, "*.mp4");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, track] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        proc.setPlaying (false);
        ExportWindow::launch (proc.makeExportRequest (track, f.withFileExtension (".mp4")), proc.russian);
    });
}

// ---------------------------------------------------------------- misc
void AuralisEditor::toggleFullscreen()
{
    if (fullscreen) { fullscreen.reset(); return; }
    fullscreen = std::make_unique<FullscreenWindow> (proc, [this] { fullscreen.reset(); grabKeyboardFocus(); });
}

bool AuralisEditor::keyPressed (const juce::KeyPress& k)
{
    if (titleEdit.hasKeyboardFocus (true)) return false;
    const auto c = k.getTextCharacter();
    if (c == 'd' || c == 'D' || c == L'в' || c == L'В') { toggleFullscreen(); return true; }
    if (k == juce::KeyPress::spaceKey && proc.getTrack().existsAsFile()) { proc.setPlaying (! proc.isPlaying()); return true; }
    if (k == juce::KeyPress::leftKey) { stepPreset (-1); return true; }
    if (k == juce::KeyPress::rightKey) { stepPreset (1); return true; }
    return false;
}

void AuralisEditor::timerCallback()
{
    const bool ru = proc.russian;
    if (proc.stateVersion.load() != lastVersion)
    {
        lastVersion = proc.stateVersion.load();
        layers->refresh();
        const auto st = proc.snapshot();
        if (! titleEdit.hasKeyboardFocus (true)) titleEdit.setText (juce::String::fromUTF8 (st.scene.title.c_str()), false);
        offsetSlider.setValue (st.lyricsOffset, juce::dontSendNotification);
        const auto pr = proc.prefs();
        aspectBox.setSelectedItemIndex (pr.aspect, juce::dontSendNotification);
        resBox.setSelectedItemIndex (pr.resolution, juce::dontSendNotification);
        fpsBox.setSelectedItemIndex (pr.fps, juce::dontSendNotification);
        if (presetBox.getNumItems() == 0) buildPresetMenu();
        resized();
        timeline->repaint();
    }
    const bool hasTrack = proc.getTrack().existsAsFile();
    playBtn.setEnabled (hasTrack && proc.isStandalone());
    playBtn.setButtonText (proc.isPlaying() ? tr ("pause", ru) : tr ("play", ru));
    timeLabel.setText (fmtTime (proc.trackPosition()) + (hasTrack ? " / " + fmtTime (proc.trackLength()) : juce::String()), juce::dontSendNotification);
    timeline->repaint();

    if (recognizer.isBusy())
    {
        recBtn.setButtonText (tr ("cancel", ru) + "  (" + juce::String (juce::roundToInt (recognizer.progress() * 100)) + "%)");
        statusLabel.setText (tr ("recognising", ru) + "…", juce::dontSendNotification);
    }
    else if (downloadProgress.load() >= 0.f)
    {
        recBtn.setButtonText (tr ("cancel", ru));
        statusLabel.setColour (juce::Label::textColourId, colours::accent2);
        statusLabel.setText (tr ("downloading", ru) + "  " + juce::String (juce::roundToInt (downloadProgress.load() * 100)) + "%", juce::dontSendNotification);
    }
    else
    {
        recBtn.setButtonText (tr ("recognise", ru));
        if (status.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() - statusTime > 9000) { status.clear(); statusLabel.setText ({}, juce::dontSendNotification); }
        if (auto e = view->getError(); e.isNotEmpty()) { statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff6b6b)); statusLabel.setText (tr ("glError", ru), juce::dontSendNotification); }
    }
}
}
