#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Plugin/PluginProcessor.h"

namespace au
{
// Right-hand panel: one collapsible section per layer, each parameter as a slider / combo / toggle.
// Right-click a slider for modulation (bands, hits, loudness, tempo LFOs); double-click resets it.
class LayerPanel : public juce::Component
{
public:
    explicit LayerPanel (AuralisProcessor& p);
    ~LayerPanel() override;
    void refresh();                 // pull values from the processor
    void relabel();                 // after RU/EN switch
    void resized() override;
    void paint (juce::Graphics&) override;
    std::function<void()> onChooseVideo;

private:
    struct Row;
    struct Section;
    class Inner;
    AuralisProcessor& proc;
    juce::Viewport viewport;
    std::unique_ptr<Inner> inner;
};
}
