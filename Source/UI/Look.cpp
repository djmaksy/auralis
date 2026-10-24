#include "Look.h"

namespace au
{
Look::Look()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, panel2);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextEditor::backgroundColourId, panel2);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, line);
    setColour (juce::ScrollBar::thumbColourId, line.brighter (0.2f));
    setColour (juce::ProgressBar::foregroundColourId, accent);
    setColour (juce::ProgressBar::backgroundColourId, panel2);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, line);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::DocumentWindow::textColourId, text);
    setColour (juce::ListBox::backgroundColourId, panel);
}

void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const juce::Rectangle<float> track ((float) x, cy - 2.f, (float) w, 4.f);
    g.setColour (colours::line); g.fillRoundedRectangle (track, 2.f);
    const bool modulated = s.getProperties().getWithDefault ("mod", false);
    const auto col = modulated ? colours::accent2 : colours::accent;
    const float zero = s.getMinimum() < 0 ? (float) x + (float) w * (float) ((0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : (float) x;
    g.setColour (col.withAlpha (s.isEnabled() ? 0.9f : 0.3f));
    g.fillRoundedRectangle (juce::Rectangle<float> (std::min (zero, pos), cy - 2.f, std::abs (pos - zero), 4.f), 2.f);
    g.setColour (s.isMouseOverOrDragging() ? juce::Colours::white : colours::text);
    g.fillEllipse (pos - 6.f, cy - 6.f, 12.f, 12.f);
    g.setColour (col); g.drawEllipse (pos - 6.f, cy - 6.f, 12.f, 12.f, 1.5f);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& c, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto col = b.getToggleState() ? colours::accent : c;
    if (down) col = col.brighter (0.15f); else if (over) col = col.brighter (0.08f);
    g.setColour (col); g.fillRoundedRectangle (r, 5.f);
    g.setColour (colours::line); g.drawRoundedRectangle (r, 5.f, 1.f);
}

void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const float h = 16.f, w = 30.f;
    const juce::Rectangle<float> sw (2.f, ((float) b.getHeight() - h) * 0.5f, w, h);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::accent : colours::line.brighter (over ? 0.2f : 0.f));
    g.fillRoundedRectangle (sw, h * 0.5f);
    g.setColour (on ? juce::Colours::black : colours::dim);
    const float k = on ? sw.getRight() - h + 2.f : sw.getX() + 2.f;
    g.fillEllipse (k, sw.getY() + 2.f, h - 4.f, h - 4.f);
    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (13.f));
        g.drawText (b.getButtonText(), juce::Rectangle<int> ((int) w + 8, 0, b.getWidth() - (int) w - 8, b.getHeight()), juce::Justification::centredLeft);
    }
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c)
{
    const juce::Rectangle<float> r (0.5f, 0.5f, (float) w - 1.f, (float) h - 1.f);
    g.setColour (c.findColour (juce::ComboBox::backgroundColourId)); g.fillRoundedRectangle (r, 5.f);
    g.setColour (colours::line); g.drawRoundedRectangle (r, 5.f, 1.f);
    juce::Path a; const float ax = (float) w - 14.f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.f, ay - 2.f, ax + 4.f, ay - 2.f, ax, ay + 3.f);
    g.setColour (colours::dim); g.fillPath (a);
}
}
