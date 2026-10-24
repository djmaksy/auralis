#include "GlViews.h"
#include "Strings.h"
#include "Look.h"

using namespace juce::gl;

namespace au
{
static juce::Rectangle<int> fitAspect (juce::Rectangle<int> area, float aspect)
{
    int w = area.getWidth(), h = (int) std::round ((float) w / aspect);
    if (h > area.getHeight()) { h = area.getHeight(); w = (int) std::round ((float) h * aspect); }
    return juce::Rectangle<int> (w, h).withCentre (area.getCentre());
}

// ============================================================================ LiveView
LiveView::LiveView (AuralisProcessor& p, int maxRenderSide) : proc (p), maxSide (maxRenderSide)
{
    setOpaque (true);
    ctx.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    ctx.setRenderer (this);
    ctx.setComponentPaintingEnabled (false);
    ctx.setContinuousRepainting (true);
    ctx.attachTo (*this);
}
LiveView::~LiveView() { ctx.detach(); }
void LiveView::resized() { w = getWidth(); h = getHeight(); }

juce::Rectangle<int> LiveView::frameRect() const { return fitAspect (getLocalBounds(), proc.prefs().aspectRatio()); }

void LiveView::newOpenGLContextCreated()
{
    juce::String err;
    if (! engine.init (err)) { const juce::ScopedLock sl (errLock); error = err; juce::Logger::writeToLog (err); }
}
void LiveView::openGLContextClosing() { engine.release(); }

void LiveView::renderOpenGL()
{
    const float scale = (float) ctx.getRenderingScale();
    const int vw = juce::roundToInt ((float) w.load() * scale), vh = juce::roundToInt ((float) h.load() * scale);
    const SoundFrame sf = proc.hub.pull();
    if (! engine.isReady()) { juce::OpenGLHelpers::clear (colours::bg); return; }

    const SceneState st = proc.snapshot();
    const float aspect = proc.prefs().aspectRatio();
    auto r = fitAspect ({ 0, 0, vw, vh }, aspect);
    int rw = r.getWidth(), rh = r.getHeight();
    const int longSide = std::max (rw, rh);
    if (longSide > maxSide) { rw = rw * maxSide / longSide; rh = rh * maxSide / longSide; }
    engine.render (sf, st, std::max (16, rw), std::max (16, rh));

    glBindFramebuffer (GL_FRAMEBUFFER, ctx.getFrameBufferID());
    glViewport (0, 0, vw, vh);
    glClearColor (0, 0, 0, 1); glClear (GL_COLOR_BUFFER_BIT);
    engine.renderer().drawToScreen (vw, vh, r.getX(), vh - r.getBottom(), r.getWidth(), r.getHeight());
}

// ============================================================================ ExportWindow
class ExportWindow::Content : public juce::Component, private juce::OpenGLRenderer
{
public:
    Content (ExportRequest r, bool ru_) : job (std::move (r)), ru (ru_), bar (progressValue)
    {
        aspect = (float) job.request().width / (float) job.request().height;
        addAndMakeVisible (preview);
        preview.setOpaque (true);
        addAndMakeVisible (bar);
        addAndMakeVisible (info);
        addAndMakeVisible (cancelBtn);
        addAndMakeVisible (showBtn);
        showBtn.setVisible (false);
        info.setFont (juce::FontOptions (13.f));
        info.setColour (juce::Label::textColourId, colours::text);
        cancelBtn.setButtonText (tr ("cancel", ru));
        showBtn.setButtonText (tr ("showFile", ru));
        cancelBtn.onClick = [this] { if (finishedFlag) closeWindow(); else job.cancel(); };
        showBtn.onClick = [this] { job.request().outFile.revealToUser(); };

        juce::String err;
        if (! job.begin (err)) { failedEarly = true; errorText = err; }
        ctx.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
        ctx.setRenderer (this);
        ctx.setComponentPaintingEnabled (false);
        ctx.setContinuousRepainting (true);
        ctx.setSwapInterval (0);
        ctx.attachTo (preview);
        setSize (760, 560);
    }
    ~Content() override { job.cancel(); ctx.detach(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto bottom = r.removeFromBottom (34);
        cancelBtn.setBounds (bottom.removeFromRight (110)); bottom.removeFromRight (8);
        showBtn.setBounds (bottom.removeFromRight (130)); bottom.removeFromRight (8);
        bar.setBounds (bottom);
        r.removeFromBottom (6);
        info.setBounds (r.removeFromBottom (22));
        r.removeFromBottom (6);
        preview.setBounds (fitAspect (r, aspect));
        pw = preview.getWidth(); ph = preview.getHeight();
    }
    void paint (juce::Graphics& g) override { g.fillAll (colours::bg); }

    void updateUi()
    {
        progressValue = job.progress();
        const auto& rq = job.request();
        juce::String s;
        if (failedEarly || job.failed()) s = tr ("failed", ru) + ": " + (failedEarly ? errorText : job.error());
        else if (job.isDone()) s = job.wasCancelled() ? tr ("cancel", ru) : tr ("done", ru) + ": " + rq.outFile.getFileName();
        else
        {
            const double el = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
            const double eta = job.progress() > 0.01 ? el / job.progress() - el : 0;
            s << tr ("rendering", ru) << "  " << rq.width << "x" << rq.height << " @ " << juce::String (rq.fps, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")
              << " fps  ·  " << (int) job.frames() << " / " << (int) job.totalFrameCount()
              << "  ·  " << juce::String (job.progress() * 100.0, 1) << "%  ·  ~" << (int) eta << " s  ·  "
              << (rq.videoBitrateMbps > 0 ? juce::String (rq.videoBitrateMbps) + " Mbps" : juce::String ("auto")) << "  ·  " << job.backendName();
        }
        info.setText (s, juce::dontSendNotification);
        const bool fin = isFinished();
        if (fin && ! finishedFlag)
        {
            finishedFlag = true;
            cancelBtn.setButtonText (tr ("close", ru));
            showBtn.setVisible (! failedEarly && ! job.failed() && ! job.wasCancelled());
        }
    }
    bool isFinished() const { return failedEarly || job.isDone(); }
    bool succeeded() const { return isFinished() && ! failedEarly && ! job.failed() && ! job.wasCancelled(); }
    juce::String err() const { return failedEarly ? errorText : job.error(); }
    juce::File outFile() const { return job.request().outFile; }
    void closeWindow() { if (auto* w = findParentComponentOfClass<ExportWindow>()) w->closeButtonPressed(); }

private:
    void newOpenGLContextCreated() override
    {
        juce::String e;
        if (! engine.init (e)) { failedEarly = true; errorText = e; job.cancel(); }
    }
    void openGLContextClosing() override { engine.release(); }
    void renderOpenGL() override
    {
        const float sc = (float) ctx.getRenderingScale();
        const int vw = juce::roundToInt ((float) pw.load() * sc), vh = juce::roundToInt ((float) ph.load() * sc);
        if (engine.isReady() && ! failedEarly && ! job.isDone())
            job.step (engine, 60.0);
        glBindFramebuffer (GL_FRAMEBUFFER, ctx.getFrameBufferID());
        glViewport (0, 0, vw, vh);
        glClearColor (0, 0, 0, 1); glClear (GL_COLOR_BUFFER_BIT);
        if (engine.isReady()) engine.renderer().drawToScreen (vw, vh, 0, 0, vw, vh);
        if (job.isDone()) ctx.setContinuousRepainting (false);
    }

    ExportJob job;
    bool ru;
    double progressValue = 0;
    juce::Component preview;
    juce::ProgressBar bar;
    juce::Label info;
    juce::TextButton cancelBtn, showBtn;
    juce::OpenGLContext ctx;
    VisualEngine engine;
    float aspect = 16.f / 9.f;
    std::atomic<int> pw { 1 }, ph { 1 };
    std::atomic<bool> failedEarly { false };
    juce::String errorText;
    bool finishedFlag = false;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
};

static juce::OwnedArray<ExportWindow>& openWindows() { static juce::OwnedArray<ExportWindow> w; return w; }

void ExportWindow::launch (ExportRequest r, bool ru, Done d)
{
    openWindows().add (new ExportWindow (std::move (r), ru, std::move (d)));
}

void ExportWindow::closeAll() { openWindows().clear (true); }

ExportWindow::ExportWindow (ExportRequest r, bool ru, Done d)
    : juce::DocumentWindow ("Auralis — " + tr ("render", ru), colours::bg, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
      done (std::move (d))
{
    content = std::make_unique<Content> (std::move (r), ru);
    setUsingNativeTitleBar (true);
    setContentNonOwned (content.get(), true);
    setResizable (true, false);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
    toFront (true);
    startTimerHz (10);
}

ExportWindow::~ExportWindow() { stopTimer(); clearContentComponent(); content.reset(); }

void ExportWindow::timerCallback()
{
    content->updateUi();
    if (content->isFinished() && ! reported)
    {
        reported = true;
        if (done) done (content->succeeded(), content->outFile(), content->err());
    }
}

void ExportWindow::closeButtonPressed()
{
    if (! content->isFinished())
    {
        // cancel first; window closes on the next press or when the job stops
        content->updateUi();
    }
    stopTimer();
    if (! reported && done) { reported = true; done (false, content->outFile(), "Cancelled"); }
    juce::MessageManager::callAsync ([this] { openWindows().removeObject (this, true); });
}

// ============================================================================ FullscreenWindow
FullscreenWindow::FullscreenWindow (AuralisProcessor& p, std::function<void()> onClose)
    : juce::TopLevelWindow ("Auralis", true), closeFn (std::move (onClose))
{
    view = std::make_unique<LiveView> (p, 3840);
    view->onDoubleClick = [this] { if (closeFn) closeFn(); };
    addAndMakeVisible (*view);
    setWantsKeyboardFocus (true);
    setOpaque (true);
    const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea;
    setBounds (area);
    setVisible (true);
    juce::Desktop::getInstance().setKioskModeComponent (this, false);
    grabKeyboardFocus();
}
FullscreenWindow::~FullscreenWindow()
{
    if (juce::Desktop::getInstance().getKioskModeComponent() == this)
        juce::Desktop::getInstance().setKioskModeComponent (nullptr);
}
void FullscreenWindow::resized() { if (view) view->setBounds (getLocalBounds()); }
bool FullscreenWindow::keyPressed (const juce::KeyPress& k)
{
    if (k.getKeyCode() == juce::KeyPress::escapeKey || k.getTextCharacter() == 'd' || k.getTextCharacter() == 'D'
        || k.getTextCharacter() == L'в' || k.getTextCharacter() == L'В')
    { if (closeFn) juce::MessageManager::callAsync (closeFn); return true; }
    return false;
}
}
