#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Plugin/PluginProcessor.h"
#include "../Lyrics/Recognizer.h"
#include "Look.h"

namespace au
{
class LiveView;
class LayerPanel;
class Timeline;
class FullscreenWindow;

class AuralisEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    static constexpr int kW = 1280, kH = 780;
    explicit AuralisEditor (AuralisProcessor&);
    ~AuralisEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

    void openFile (const juce::File& f);
    std::function<void()> showAudioSettings;     // standalone gear

private:
    class Content;
    void timerCallback() override;
    void relabel();
    void buildPresetMenu();
    void selectPreset (int index);
    void stepPreset (int delta);
    void savePreset();
    void deleteUserPreset();
    void showExportMenu();
    void renderTrack();
    void editLyrics();
    void loadLyricsFile();
    void saveLrc();
    void startRecognition();
    void runRecognition();
    void toggleFullscreen();
    void chooseVideo();
    void setStatus (const juce::String& s, bool error = false);
    juce::Array<juce::File> userPresets() const;
    static juce::File presetDir();

    AuralisProcessor& proc;
    Look look;
    std::unique_ptr<Content> content;
    std::unique_ptr<LiveView> view;
    std::unique_ptr<LayerPanel> layers;
    std::unique_ptr<Timeline> timeline;
    std::unique_ptr<FullscreenWindow> fullscreen;

    juce::Label logo;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn, delBtn, exportBtn, langBtn, fullBtn, gearBtn, minBtn { "-" }, closeBtn { "x" };
    juce::ComboBox presetBox, aspectBox, resBox, fpsBox;
    juce::TextButton openBtn, playBtn, lyricsBtn, loadLyricsBtn, saveLrcBtn, recBtn;
    juce::ComboBox langBox;
    juce::Slider offsetSlider;
    juce::Label timeLabel, statusLabel, titleLabel, offsetLabel;
    juce::TextEditor titleEdit;

    LyricsRecognizer recognizer;
    std::unique_ptr<juce::URL::DownloadTask> download;
    std::atomic<float> downloadProgress { -1.f };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComponentDragger dragger;
    int lastVersion = -1;
    bool dragOver = false;
    juce::String status; bool statusError = false; double statusTime = 0;
    std::vector<juce::File> presetFiles;    // user presets backing the combo items
};
}
