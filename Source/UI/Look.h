#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace au
{
namespace colours
{
const juce::Colour bg       { 0xff0c0f14 };
const juce::Colour panel    { 0xff141922 };
const juce::Colour panel2   { 0xff1b212c };
const juce::Colour line     { 0xff2a3240 };
const juce::Colour text     { 0xffdfe5ee };
const juce::Colour dim      { 0xff8892a2 };
const juce::Colour accent   { 0xffff9a3c };
const juce::Colour accent2  { 0xff4fd1c5 };
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (13.f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.f)); }
};
}
