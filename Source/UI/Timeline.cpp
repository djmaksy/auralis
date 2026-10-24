#include "Timeline.h"
#include "Look.h"

namespace au
{
Timeline::Timeline (AuralisProcessor& p) : proc (p) { const_cast<juce::AudioThumbnail&> (proc.thumbnail()).addChangeListener (this); }
Timeline::~Timeline() { const_cast<juce::AudioThumbnail&> (proc.thumbnail()).removeChangeListener (this); }

void Timeline::visibleRange (double& a, double& b) const
{
    const double len = proc.trackLength();
    if (len > 0) { a = 0; b = len; return; }
    const double t = proc.trackPosition();
    a = std::max (0.0, t - 20.0); b = a + 40.0;
}

void Timeline::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel); g.fillRoundedRectangle (r, 6.f);
    double a, b; visibleRange (a, b);
    const auto toX = [&] (double t) { return (float) ((t - a) / (b - a)) * r.getWidth(); };

    auto wave = getLocalBounds().reduced (4, 4).removeFromTop (getHeight() / 2);
    if (proc.trackLength() > 0 && proc.thumbnail().getTotalLength() > 0)
    {
        g.setColour (colours::dim.withAlpha (0.55f));
        const_cast<juce::AudioThumbnail&> (proc.thumbnail()).drawChannels (g, wave, a, b, 1.0f);
    }
    else
    {
        g.setColour (colours::line);
        g.drawHorizontalLine (wave.getCentreY(), (float) wave.getX(), (float) wave.getRight());
    }

    const auto st = proc.snapshot();
    auto lyr = getLocalBounds().reduced (4, 4).removeFromBottom (getHeight() / 2 - 6);
    g.setFont (juce::FontOptions (11.f));
    if (st.lyrics)
    {
        const double off = st.lyricsOffset;
        for (auto& ln : st.lyrics->lines)
        {
            const double s = ln.start - off, e = ln.end - off;
            if (e < a || s > b) continue;
            const float x0 = toX (s), x1 = std::max (x0 + 2.f, toX (e));
            g.setColour (colours::accent.withAlpha (0.18f));
            g.fillRoundedRectangle (x0, (float) lyr.getY(), x1 - x0, (float) lyr.getHeight(), 3.f);
            for (auto& w : ln.words)
            {
                const float wx = toX (w.start - off);
                g.setColour (colours::accent.withAlpha (0.8f));
                g.drawVerticalLine ((int) wx, (float) lyr.getY(), (float) lyr.getY() + 4.f);
            }
            g.setColour (colours::text.withAlpha (0.85f));
            g.drawText (juce::String::fromUTF8 (ln.text().c_str()), juce::Rectangle<float> (x0 + 3.f, (float) lyr.getY() + 3.f, std::max (10.f, x1 - x0 - 4.f), (float) lyr.getHeight() - 3.f),
                        juce::Justification::centredLeft, true);
        }
    }
    const double t = proc.trackPosition();
    g.setColour (juce::Colours::white);
    g.fillRect (toX (t) - 0.75f, 0.f, 1.5f, r.getHeight());
}

void Timeline::mouseDown (const juce::MouseEvent& e)
{
    if (proc.trackLength() <= 0) return;
    double a, b; visibleRange (a, b);
    proc.setTrackPosition (a + (b - a) * juce::jlimit (0.0, 1.0, (double) e.x / getWidth()));
}
}
