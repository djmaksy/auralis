#pragma once
#include <juce_opengl/juce_opengl.h>
#include "../Engine/VisualEngine.h"
#include "../Export/ExportJob.h"
#include "../Plugin/PluginProcessor.h"

namespace au
{
// Live preview: pulls frames from the processor's LiveHub and draws them letterboxed.
class LiveView : public juce::Component, private juce::OpenGLRenderer
{
public:
    LiveView (AuralisProcessor& p, int maxRenderSide);
    ~LiveView() override;
    void resized() override;
    void paint (juce::Graphics&) override {}
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
    std::function<void()> onDoubleClick;
    juce::String getError() const { const juce::ScopedLock sl (errLock); return error; }

    // rectangle of the frame inside this component (logical px)
    juce::Rectangle<int> frameRect() const;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    AuralisProcessor& proc;
    juce::OpenGLContext ctx;
    VisualEngine engine;
    int maxSide;
    std::atomic<int> w { 1 }, h { 1 };
    juce::CriticalSection errLock;
    juce::String error;
};

// Offline export with live preview and progress. Owns its GL context and window.
class ExportWindow : public juce::DocumentWindow, private juce::Timer
{
public:
    using Done = std::function<void (bool ok, juce::File out, juce::String err)>;
    static void launch (ExportRequest r, bool russian, Done onDone = {});
    static void closeAll();
    ~ExportWindow() override;
    void closeButtonPressed() override;

private:
    ExportWindow (ExportRequest r, bool russian, Done onDone);
    void timerCallback() override;
    class Content;
    std::unique_ptr<Content> content;
    Done done;
    bool reported = false;
};

// Full-screen output (D key). Closing: D, Escape or double-click.
class FullscreenWindow : public juce::TopLevelWindow
{
public:
    FullscreenWindow (AuralisProcessor& p, std::function<void()> onClose);
    ~FullscreenWindow() override;
    bool keyPressed (const juce::KeyPress&) override;
    void resized() override;
private:
    std::unique_ptr<LiveView> view;
    std::function<void()> closeFn;
};
}
