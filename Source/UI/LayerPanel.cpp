#include "LayerPanel.h"
#include "Look.h"
#include "Strings.h"

namespace au
{
static const char* modKeys[] = { "modNone", "bass", "mid", "treble", "beat", "level", "beatLfo", "barLfo" };

struct LayerPanel::Row : public juce::Component
{
    Row (AuralisProcessor& pr, P id) : proc (pr), param (id)
    {
        const auto& pi = info (id);
        addAndMakeVisible (label);
        label.setFont (juce::FontOptions (12.5f));
        label.setColour (juce::Label::textColourId, colours::dim);
        label.setInterceptsMouseClicks (false, false);
        if (pi.kind == Kind::Slider)
        {
            addAndMakeVisible (slider);
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setRange (pi.mn, pi.mx, 0.0);
            slider.setDoubleClickReturnValue (true, pi.def);
            slider.setPopupDisplayEnabled (true, true, nullptr);
            slider.onValueChange = [this] { const float v = (float) slider.getValue(); proc.editState ([&] (SceneState& s) { s.scene[param] = v; }); };
            slider.addMouseListener (this, false);
        }
        else if (pi.kind == Kind::Choice)
        {
            addAndMakeVisible (combo);
            combo.onChange = [this] { const float v = (float) (combo.getSelectedItemIndex()); if (v >= 0) proc.editState ([&] (SceneState& s) { s.scene[param] = v; }); };
        }
        else
        {
            addAndMakeVisible (toggle);
            toggle.onClick = [this] { const float v = toggle.getToggleState() ? 1.f : 0.f; proc.editState ([&] (SceneState& s) { s.scene[param] = v; }); };
        }
        relabel();
    }

    void relabel()
    {
        const bool ru = proc.russian;
        const auto& pi = info (param);
        juce::String t = juce::String::fromUTF8 (ru ? pi.ru : pi.en);
        const auto st = proc.snapshot();
        const auto& m = st.mods[(size_t) param];
        if (m.src != 0) t << "  ~ " << tr (modKeys[m.src], ru) << " " << juce::String (juce::roundToInt (m.amt * 100)) << "%";
        label.setText (t, juce::dontSendNotification);
        slider.getProperties().set ("mod", m.src != 0);
        if (pi.kind == Kind::Choice)
        {
            const int sel = combo.getSelectedItemIndex();
            combo.clear (juce::dontSendNotification);
            int id = 1;
            for (auto& c : splitChoices (ru ? pi.choicesRu : pi.choicesEn)) combo.addItem (juce::String::fromUTF8 (c.c_str()), id++);
            combo.setSelectedItemIndex (sel < 0 ? 0 : sel, juce::dontSendNotification);
        }
        if (pi.kind == Kind::Toggle) toggle.setButtonText ({});
        repaint();
    }

    void refresh (const SceneState& st)
    {
        const float v = st.scene[param];
        const auto& pi = info (param);
        if (pi.kind == Kind::Slider) { if (! slider.isMouseButtonDown()) slider.setValue (v, juce::dontSendNotification); }
        else if (pi.kind == Kind::Choice) combo.setSelectedItemIndex ((int) (v + 0.5f), juce::dontSendNotification);
        else toggle.setToggleState (v >= 0.5f, juce::dontSendNotification);
        const auto& m = st.mods[(size_t) param];
        if ((m.src != 0) != (bool) slider.getProperties().getWithDefault ("mod", false) || lastMod.src != m.src || lastMod.amt != m.amt)
        { lastMod = m; relabel(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent != &slider || ! e.mods.isPopupMenu()) return;
        const bool ru = proc.russian;
        const auto cur = proc.snapshot().mods[(size_t) param];
        juce::PopupMenu m, depth;
        m.addSectionHeader (tr ("modSource", ru));
        for (int i = 0; i < (int) ModSource::COUNT; ++i) m.addItem (100 + i, tr (modKeys[i], ru), true, cur.src == i);
        const int depths[] = { -100, -50, -25, 10, 25, 50, 75, 100 };
        for (int d : depths) depth.addItem (200 + d + 100, juce::String (d) + "%", true, juce::roundToInt (cur.amt * 100) == d);
        m.addSubMenu (tr ("modDepth", ru), depth);
        m.addSeparator();
        m.addItem (1, tr ("resetValue", ru));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider), [this] (int r)
        {
            if (r == 1) { slider.setValue (info (param).def, juce::sendNotification); return; }
            proc.editState ([&] (SceneState& s)
            {
                auto& md = s.mods[(size_t) param];
                if (r >= 100 && r < 100 + (int) ModSource::COUNT) { md.src = r - 100; if (md.src != 0 && md.amt == 0.f) md.amt = 0.25f; if (md.src == 0) md.amt = 0; }
                else if (r >= 200) { md.amt = (float) (r - 300) / 100.f; if (md.src == 0) md.src = (int) ModSource::Bass; }
            });
            relabel();
        });
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromLeft ((int) (r.getWidth() * 0.44f)));
        r.removeFromLeft (4);
        slider.setBounds (r); combo.setBounds (r.reduced (0, 3)); toggle.setBounds (r);
    }

    AuralisProcessor& proc;
    P param;
    juce::Label label;
    juce::Slider slider;
    juce::ComboBox combo;
    juce::ToggleButton toggle;
    ModSlot lastMod;
};

struct LayerPanel::Section : public juce::Component
{
    Section (AuralisProcessor& pr, const std::string& g, LayerPanel& owner) : proc (pr), group (g)
    {
        for (int i = 0; i < kParamCount; ++i)
        {
            const auto& pi = info ((P) i);
            if (group != pi.group) continue;
            if (std::string (pi.key).find (".on") != std::string::npos && pi.kind == Kind::Toggle && onParam < 0) { onParam = i; continue; }
            rows.push_back (std::make_unique<Row> (proc, (P) i));
            addAndMakeVisible (*rows.back());
        }
        if (onParam >= 0)
        {
            addAndMakeVisible (onToggle);
            onToggle.onClick = [this] { const float v = onToggle.getToggleState() ? 1.f : 0.f; proc.editState ([&] (SceneState& s) { s.scene.v[(size_t) onParam] = v; }); };
        }
        addAndMakeVisible (header);
        header.setFont (juce::FontOptions (13.f, juce::Font::bold));
        header.setColour (juce::Label::textColourId, colours::text);
        header.setInterceptsMouseClicks (false, false);
        if (group == "VIDEO")
        {
            addAndMakeVisible (fileBtn); addAndMakeVisible (clearBtn); addAndMakeVisible (fileLabel);
            fileLabel.setFont (juce::FontOptions (11.5f)); fileLabel.setColour (juce::Label::textColourId, colours::dim);
            fileBtn.onClick = [&owner] { if (owner.onChooseVideo) owner.onChooseVideo(); };
            clearBtn.onClick = [this] { proc.editState ([] (SceneState& s) { s.scene.videoPath.clear(); s.scene[P::vdOn] = 0.f; }); };
        }
        relabel();
    }

    int heightNeeded() const { return 34 + (expanded ? (int) rows.size() * 26 + (group == "VIDEO" ? 30 : 0) + 6 : 0); }

    void relabel()
    {
        const bool ru = proc.russian;
        header.setText (juce::String (expanded ? juce::CharPointer_UTF8 ("\xe2\x96\xbe  ") : juce::CharPointer_UTF8 ("\xe2\x96\xb8  ")) + groupLabel (group, ru), juce::dontSendNotification);
        for (auto& r : rows) r->relabel();
        fileBtn.setButtonText (tr ("videoFile", ru)); clearBtn.setButtonText (tr ("clear", ru));
    }

    void refresh (const SceneState& st)
    {
        if (onParam >= 0) onToggle.setToggleState (st.scene.v[(size_t) onParam] >= 0.5f, juce::dontSendNotification);
        for (auto& r : rows) r->refresh (st);
        if (group == "VIDEO")
        {
            const auto f = juce::File (juce::String::fromUTF8 (st.scene.videoPath.c_str()));
            fileLabel.setText (st.scene.videoPath.empty() ? tr ("noVideo", proc.russian) : f.getFileName(), juce::dontSendNotification);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.y < 34 && e.x < getWidth() - 50) { expanded = ! expanded; relabel(); if (auto* p = getParentComponent()) p->resized(); }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (colours::panel2); g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.f, 2.f), 6.f);
        if (onParam >= 0 && onToggle.getToggleState()) { g.setColour (colours::accent); g.fillRoundedRectangle (4.f, 10.f, 3.f, 14.f, 1.5f); }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 2);
        auto top = r.removeFromTop (32);
        if (onParam >= 0) onToggle.setBounds (top.removeFromRight (40).reduced (0, 6));
        header.setBounds (top);
        for (auto& row : rows) { row->setVisible (expanded); if (expanded) row->setBounds (r.removeFromTop (26)); }
        const bool showVid = expanded && group == "VIDEO";
        fileBtn.setVisible (showVid); clearBtn.setVisible (showVid); fileLabel.setVisible (showVid);
        if (showVid)
        {
            auto b = r.removeFromTop (28).reduced (0, 2);
            fileBtn.setBounds (b.removeFromLeft (70)); b.removeFromLeft (6);
            clearBtn.setBounds (b.removeFromLeft (70)); b.removeFromLeft (6);
            fileLabel.setBounds (b);
        }
    }

    AuralisProcessor& proc;
    std::string group;
    int onParam = -1;
    bool expanded = true;
    juce::Label header;
    juce::ToggleButton onToggle;
    std::vector<std::unique_ptr<Row>> rows;
    juce::TextButton fileBtn, clearBtn;
    juce::Label fileLabel;
};

class LayerPanel::Inner : public juce::Component
{
public:
    Inner (AuralisProcessor& p, LayerPanel& owner)
    {
        for (auto& g : groups())
        {
            sections.push_back (std::make_unique<Section> (p, g, owner));
            addAndMakeVisible (*sections.back());
        }
        // collapse the less used groups by default
        for (auto& s : sections) if (s->group == "VIDEO" || s->group == "ORNAMENT") s->expanded = false;
        for (auto& s : sections) s->relabel();
    }
    void resized() override
    {
        int y = 0;
        for (auto& s : sections) { const int h = s->heightNeeded(); s->setBounds (0, y, getWidth(), h); y += h + 4; }
        if (getHeight() != y) setSize (getWidth(), y);
    }
    std::vector<std::unique_ptr<Section>> sections;
};

LayerPanel::LayerPanel (AuralisProcessor& p) : proc (p)
{
    inner = std::make_unique<Inner> (p, *this);
    viewport.setViewedComponent (inner.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    refresh();
}
LayerPanel::~LayerPanel() = default;

void LayerPanel::refresh()
{
    const auto st = proc.snapshot();
    for (auto& s : inner->sections) s->refresh (st);
}
void LayerPanel::relabel() { for (auto& s : inner->sections) s->relabel(); refresh(); }
void LayerPanel::paint (juce::Graphics& g) { g.fillAll (colours::panel); }
void LayerPanel::resized()
{
    viewport.setBounds (getLocalBounds().reduced (6));
    inner->setSize (viewport.getWidth() - 10, inner->getHeight());
    inner->resized();
}
}
