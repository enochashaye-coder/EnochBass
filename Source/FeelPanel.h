#pragma once
// v2.7 batch 2  SOUND & FEEL drawer (identical in Enoch Solo and Enoch Bass).
// Global sound controls of the preview engine + glide + phrasing engine, and the generation speed / fastest subdivision.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"
#include <map>

class FeelPanel : public juce::Component
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> paintPlate;

    explicit FeelPanel (EnochSoloProcessor& p) : proc (p)
    {
        title.setText ("SOUND & FEEL", juce::dontSendNotification);
        title.setFont (EnochLookAndFeel::fnt (13.f, "Bold", 0.08f));
        addAndMakeVisible (title);
        hint.setText ("These shape how the preview plays (all sounds). Speed and fastest notes rewrite the take.", juce::dontSendNotification);
        hint.setFont (EnochLookAndFeel::fnt (11.5f));
        hint.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        addAndMakeVisible (hint);

        knob ("realism",   "REALISM",    "CLEAN", "REAL",  "Model detail: key / finger / damper / bow noises, string and soundboard resonance, analog drift");
        knob ("sndHuman",  "HUMANIZE",   "EXACT", "LOOSE", "How different each note is: a few cents of intonation, level and attack");
        knob ("dynamics",  "DYNAMICS",   "EVEN",  "WIDE",  "Velocity range of the preview: 50% = as played, lower = more even, higher = bigger contrast");
        knob ("exprResp",  "EXPRESSION", "LOW",   "HIGH",  "How much expression (CC11) moves level and tone on winds, strings and synths. 50% = as before");
        knob ("bright",    "BRIGHTNESS", "DARK",  "BRIGHT","Tone tilt of the preview");
        knob ("mechNoise", "MECH NOISE", "NONE",  "MORE",  "Keys, valves, fingers on strings, dampers, pedal, bow changes, breaths. 50% = natural");
        knob ("micPos",    "MIC",        "CLOSE", "FAR",   "Microphone distance: close = fuller lows and air, far = darker with more room");
        knob ("glideTime", "GLIDE TIME", "FAST",  "SLOW",  "How long a glide takes: fast and subtle at the left, slow slides at the right");

        combo (roomBox, "roomType", "ROOM", "Reverb room of the preview (the REVERB amount stays in Advanced)");
        combo (glideBox, "glideMode", "GLIDE", "Off: legato notes change pitch at once (slides still slide).  Legato glide: legato articulations glide (default).\n"
                                               "Always glide: overlapping notes and notes that follow within 1/4 s glide.  Overlap glide: any overlap glides.\n"
                                               "Portamento: every note slides in from the previous one. Articulations are never changed.");
        combo (outBox, "outMode", "OUTPUT", "Stereo or mono preview output");
        combo (speedBox, "speed", "SPEED", "Double time: notes of an 8th or longer are split and the line walks to the next note at twice the rate (per bar: bar panel)");
        combo (subBox, "subdiv", "FASTEST NOTES", "Fast runs into the next note at 1/16 triplets, 1/32 or 1/32 triplets. How many follows each bar's Notes Density");
        humanBtn.setTooltip ("Phrasing engine: phrase arcs, breaths for winds, bow direction for strings, string and fret choice for guitar and bass, "
                             "lighter fast runs. Seeded: the same take always plays the same way");
        if (auto* bp = proc.apvts.getParameter ("humanPlay")) btnAtt = std::make_unique<juce::ButtonParameterAttachment> (*bp, humanBtn);
        addAndMakeVisible (humanBtn);
        resetBtn.setTooltip ("Put the sound controls back to their defaults (Speed and Fastest notes are kept)");
        resetBtn.onClick = [this]
        {
            for (auto* id : { "realism", "sndHuman", "dynamics", "exprResp", "bright", "mechNoise", "micPos", "glideTime", "roomType", "glideMode", "outMode", "humanPlay" })
                if (auto* pr = proc.apvts.getParameter (id)) { pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->getDefaultValue()); pr->endChangeGesture(); }
        };
        addAndMakeVisible (resetBtn);

        // ---- page 2: modulation matrix + creative effect
        for (int t = 0; t < 2; ++t)
        {
            auto* b = tabs.add (new juce::TextButton (t == 0 ? "SOUND & FEEL" : "MODULATION & FX"));
            b->getProperties().set ("seg", true);
            b->setClickingTogglesState (false);
            b->onClick = [this, t] { page = t; showPage(); };
            addAndMakeVisible (b);
        }
        for (int k = 1; k <= 4; ++k)
        {
            const juce::String n (k);
            auto* row = rows.add (new Row());
            row->num.setText (n, juce::dontSendNotification);
            row->num.setFont (EnochLookAndFeel::fnt (13.f, "Bold"));
            row->num.setJustificationType (juce::Justification::centred);
            addChoice (row->src, ("mm" + n + "Src").toRawUTF8());
            addChoice (row->dst, ("mm" + n + "Dst").toRawUTF8());
            row->src.setTooltip ("Modulation source " + n + ": LFOs, velocity, mod wheel, aftertouch, expression, key, a per-note envelope or a random value per note");
            row->dst.setTooltip ("What it moves: pitch (+-2 semitones at 100%), brightness, level, pan, extra vibrato or drive");
            row->amt.setSliderStyle (juce::Slider::LinearHorizontal);
            row->amt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
            row->amt.setTooltip ("Amount (negative turns it upside down). Double-click = 0");
            if (auto* pa = proc.apvts.getParameter ("mm" + n + "Amt")) { satts.push_back (std::make_unique<juce::SliderParameterAttachment> (*pa, row->amt)); row->amt.setDoubleClickReturnValue (true, 0.0); }
            row->amt.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
            row->amt.setTextValueSuffix ("");
            row->amt.updateText();
            for (juce::Component* c : { (juce::Component*) &row->num, (juce::Component*) &row->src, (juce::Component*) &row->dst, (juce::Component*) &row->amt }) addChildComponent (c);
        }
        knob2 (lfo1, lfo1L, "lfo1Rate", "LFO 1", "Rate of LFO 1 (vibrato-speed range)");
        knob2 (lfo2, lfo2L, "lfo2Rate", "LFO 2", "Rate of LFO 2 (slow sweeps)");
        knob2 (fxAmtK, fxAmtL, "fxAmt", "FX AMOUNT", "How much of the creative effect");
        addChoice (fxBox, "fxMode");
        fxBox.setTooltip ("Creative effect on the preview: tape wobble, lo-fi, wide, telephone / radio, warm saturation");
        fxLbl.setText ("CREATIVE FX", juce::dontSendNotification); fxLbl.setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f));
        addChildComponent (fxBox); addChildComponent (fxLbl);
        showPage();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.f);
        if (paintPlate) paintPlate (g, b);
        else { g.setColour (juce::Colour (0xff24262b)); g.fillRoundedRectangle (b, 7.f); }
        g.setColour (EnochLookAndFeel::secSound().withAlpha (0.6f));     // the sound section's colour
        g.fillRect (b.reduced (18.f, 0.f).withY (b.getY() + 44.f).withHeight (1.f));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 10);
        auto head = r.removeFromTop (30);
        title.setBounds ({});
        for (auto* t : tabs) t->setBounds (head.removeFromLeft (150).reduced (2, 2));
        head.removeFromLeft (8);
        resetBtn.setBounds (head.removeFromRight (90).reduced (2, 2));
        humanBtn.setBounds (head.removeFromRight (230).reduced (2, 2));
        hint.setBounds (head);
        r.removeFromTop (10);
        {   // page 2 layout
            auto r2 = r;
            auto right2 = r2.removeFromRight (juce::jmin (380, r2.getWidth() * 2 / 5));
            const int rh = juce::jmin (32, r2.getHeight() / 4);
            for (auto* row : rows)
            {
                auto rr = r2.removeFromTop (rh).reduced (0, 3);
                row->num.setBounds (rr.removeFromLeft (24));
                row->src.setBounds (rr.removeFromLeft (rr.getWidth() * 2 / 5).reduced (2, 0));
                row->dst.setBounds (rr.removeFromLeft (rr.getWidth() / 2).reduced (2, 0));
                row->amt.setBounds (rr.reduced (2, 2));
            }
            right2.removeFromLeft (16);
            auto top = right2.removeFromTop (28);
            fxLbl.setBounds (top.removeFromLeft (90)); fxBox.setBounds (top.reduced (2, 2));
            right2.removeFromTop (6);
            const int kw = right2.getWidth() / 3;
            for (auto [k, l] : { std::pair<juce::Slider*, juce::Label*> { &lfo1, &lfo1L }, { &lfo2, &lfo2L }, { &fxAmtK, &fxAmtL } })
            {
                auto col = right2.removeFromLeft (kw);
                l->setBounds (col.removeFromTop (14));
                k->setBounds (col.withSizeKeepingCentre (juce::jmin (kw - 6, 70), juce::jmin (col.getHeight(), 90)));
            }
        }
        // left: 8 knobs; right: the five choice boxes in a column
        auto right = r.removeFromRight (juce::jmin (300, r.getWidth() / 3));
        const int rowH = juce::jmin (34, right.getHeight() / 5);
        for (auto* c : { &roomBox, &glideBox, &outBox, &speedBox, &subBox })
        {
            auto row = right.removeFromTop (rowH);
            auto& l = *labels.at (c);
            l.setBounds (row.removeFromLeft (110));
            c->setBounds (row.reduced (2, 4));
        }
        const int kw = r.getWidth() / juce::jmax (1, (int) knobs.size());
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            auto col = r.removeFromLeft (kw);
            klabels[i]->setBounds (col.removeFromTop (14));
            knobs[i]->setBounds (col.withSizeKeepingCentre (juce::jmin (kw - 6, 76), juce::jmin (col.getHeight(), 96)));
        }
    }

private:
    struct Row { juce::Label num; juce::ComboBox src, dst; juce::Slider amt; };
    void showPage()
    {
        for (int t = 0; t < tabs.size(); ++t) tabs[t]->setToggleState (t == page, juce::dontSendNotification);
        const bool p1 = page == 0;
        for (auto& k : knobs) k->setVisible (p1);
        for (auto& l : klabels) l->setVisible (p1);
        for (auto& kv : labels) kv.second->setVisible (p1);
        for (auto* c : { &roomBox, &glideBox, &outBox, &speedBox, &subBox }) c->setVisible (p1);
        humanBtn.setVisible (p1); hint.setVisible (p1); resetBtn.setVisible (p1);
        for (auto* row : rows) for (juce::Component* c : { (juce::Component*) &row->num, (juce::Component*) &row->src, (juce::Component*) &row->dst, (juce::Component*) &row->amt }) c->setVisible (! p1);
        for (juce::Component* c : { (juce::Component*) &lfo1, (juce::Component*) &lfo2, (juce::Component*) &fxAmtK, (juce::Component*) &lfo1L, (juce::Component*) &lfo2L, (juce::Component*) &fxAmtL, (juce::Component*) &fxBox, (juce::Component*) &fxLbl })
            c->setVisible (! p1);
    }
    void addChoice (juce::ComboBox& c, const char* id)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        {
            c.addItemList (cp->choices, 1);
            catts.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (*cp, c));
        }
    }
    void knob2 (juce::Slider& s, juce::Label& l, const char* id, const char* name, const char* tip)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.getProperties().set ("lcd", true);
        s.setTooltip (tip);
        if (auto* p = proc.apvts.getParameter (id)) { satts.push_back (std::make_unique<juce::SliderParameterAttachment> (*p, s)); s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue())); }
        const bool hz = juce::String (id).startsWith ("lfo");
        s.textFromValueFunction = [hz] (double v) { return hz ? juce::String (v, v < 1.0 ? 2 : 1) + " Hz" : juce::String (juce::roundToInt (v)) + "%"; };
        s.updateText();
        l.setText (name, juce::dontSendNotification); l.setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f)); l.setJustificationType (juce::Justification::centred);
        addChildComponent (s); addChildComponent (l);
    }
    int page = 0;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::OwnedArray<Row> rows;
    juce::Slider lfo1, lfo2, fxAmtK; juce::Label lfo1L, lfo2L, fxAmtL, fxLbl; juce::ComboBox fxBox;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> satts;
    void knob (const char* id, const char* name, const char* lo, const char* hi, const char* tip)
    {
        auto* p = proc.apvts.getParameter (id);
        if (p == nullptr) return;
        auto s = std::make_unique<juce::Slider>();
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->setMouseDragSensitivity (230);
        s->setVelocityModeParameters (0.4, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        s->getProperties().set ("lcd", true);
        s->getProperties().set ("lo", lo); s->getProperties().set ("hi", hi);
        s->setTooltip (juce::String (tip) + "  (double-click = default, Shift = fine)");
        atts.push_back (std::make_unique<juce::SliderParameterAttachment> (*p, *s));
        s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        s->textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
        s->updateText();
        auto l = std::make_unique<juce::Label> (juce::String(), name);
        l->setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f));
        l->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*s); addAndMakeVisible (*l);
        knobs.push_back (std::move (s)); klabels.push_back (std::move (l));
    }
    void combo (juce::ComboBox& c, const char* id, const char* name, const char* tip)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        {
            c.addItemList (cp->choices, 1);
            catts.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (*cp, c));
        }
        c.setTooltip (tip);
        addAndMakeVisible (c);
        auto l = std::make_unique<juce::Label> (juce::String(), name);
        l->setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f));
        l->setTooltip (tip);
        addAndMakeVisible (*l);
        labels[&c] = std::move (l);
    }

    EnochSoloProcessor& proc;
    juce::Label title, hint;
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>> klabels;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> atts;
    juce::ComboBox roomBox, glideBox, outBox, speedBox, subBox;
    std::map<juce::ComboBox*, std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> catts;
    juce::ToggleButton humanBtn { "Human Play (phrasing engine)" };
    juce::TextButton resetBtn { "Reset" };
    std::unique_ptr<juce::ButtonParameterAttachment> btnAtt;
};
