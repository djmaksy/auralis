#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Plugin/PluginProcessor.h"

namespace au
{
// Waveform of the loaded track with the synced lyrics underneath and a playhead. Click to seek (app).
class Timeline : public juce::Component, public juce::ChangeListener
{
public:
    explicit Timeline (AuralisProcessor& p);
    ~Timeline() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override { mouseDown (e); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
private:
    void visibleRange (double& a, double& b) const;
    AuralisProcessor& proc;
};
}
