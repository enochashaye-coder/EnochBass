#pragma once
// v2.7  Amapiano LOG DRUM control panel (Enoch Bass). Opens as a drawer over the lower part of the roll from the
// LOG DRUM button in the footer. The six type buttons pick one of the "Amapiano log drums" sounds; the knobs are relative
// to that type (100% = the type's own voicing), so every type keeps its character and the knobs shape it.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"
#include "Sounds.h"

class LogDrumPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> paintPlate;

    explicit LogDrumPanel (EnochSoloProcessor& p) : proc (p)
    {
        // the six types = the "Amapiano log drums" group of the bass sound list
        const auto& list = enochsolo::snd::patchList (enochsolo::I_Bass);
        for (int i = 0; i < (int) list.size(); ++i)
            if (list[(size_t) i].kind == enochsolo::snd::K_LogDrum)
            {
                typeIdx.push_back (i);
                juce::String n (list[(size_t) i].name);
                n = n.fromFirstOccurrenceOf ("Log drum: ", false, false);
                juce::String shortName = n.upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("-", false, false).toUpperCase();
                if (shortName == "PERCUSSIVE") shortName = "PERC";
                auto* b = typeBtn.add (new juce::TextButton (shortName));
                b->getProperties().set ("seg", true);
                b->setClickingTogglesState (false);
                b->setTooltip ("Log drum type: " + n);
                const int idx = i;
                b->onClick = [this, idx] { pickType (idx); };
                addAndMakeVisible (b);
            }
        title.setText ("AMAPIANO LOG DRUM", juce::dontSendNotification);
        title.setFont (EnochLookAndFeel::fnt (13.f, "Bold", 0.08f));
        addAndMakeVisible (title);
        state.setFont (EnochLookAndFeel::fnt (12.f));
        state.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        addAndMakeVisible (state);

        //            id          label        lo text   hi text   unit      tip
        knob ("ldTune",   "TUNE",     "-12", "+12", "st",  "Tune the log drum in semitones (the played note still sets the pitch)");
        knob ("ldFine",   "FINE",     "-50", "+50", "ct",  "Fine tune in cents - match the drum to the track's key centre");
        knob ("ldDrop",   "PITCH DROP", "0", "2x",  "%",   "How far the pitch falls into the note (the Amapiano 'dooon'). 100% = the type's own drop");
        knob ("ldDropT",  "DROP TIME", "FAST", "SLOW", "%", "How long the pitch drop takes");
        knob ("ldAttack", "ATTACK",   "0", "20", "ms",     "Attack: 0 = hardest transient, higher = softer start");
        knob ("ldPunch",  "PUNCH",    "0", "2x", "%",      "Transient punch / knock level");
        knob ("ldDecay",  "DECAY",    "SHORT", "LONG", "%", "Length of the drum. Short for percussive patterns, long for melodic log drum lines");
        knob ("ldSub",    "SUB",      "-24", "+6", "dB",   "Sub level (it shares one phase-locked oscillator with the body, so it never cancels)");
        knob ("ldBody",   "BODY",     "0", "2x", "%",      "Wooden body resonance (the harmonic modes above the fundamental)");
        knob ("ldTone",   "TONE",     "DARK", "BRIGHT", "%", "Brightness of the upper layers (the sub stays clean)");
        knob ("ldDrive",  "DRIVE",    "0", "100", "%",     "Saturation for grit and loudness");
        knob ("ldWidth",  "WIDTH",    "MONO", "WIDE", "%", "Stereo width of the upper layers only - the low end stays mono");
        knob ("ldVel",    "VELOCITY", "FLAT", "FULL", "%", "How much the velocity changes level and brightness");
        knob ("ldGlide",  "GLIDE",    "0", "500", "ms",    "Pitch glide time for overlapping notes and slides");
        knob ("ldHuman",  "HUMAN",    "0", "100", "%",     "Small seeded variations of pitch, drop and level, so repeated hits are not identical");
        knob ("ldRoom",   "ROOM",     "DRY", "WET", "%",   "Room / reverb amount used while a log drum is playing");
        knob ("ldOut",    "OUTPUT",   "-24", "+6", "dB",   "Output level of the log drum");

        lMono.setText ("MONO BELOW", juce::dontSendNotification);
        lMono.setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f));
        lMono.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lMono);
        if (auto* mp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("ldMono")))
        {
            monoBox.addItemList (mp->choices, 1);
            comboAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*mp, monoBox);
        }
        monoBox.setTooltip ("Everything below this frequency is summed to mono (keeps the sub solid in clubs and on phones)");
        addAndMakeVisible (monoBox);
        scBtn.setTooltip ("Keeps the log drum peak under -1 dBFS with a soft ceiling so it sits well against a sidechained kick");
        if (auto* bp = proc.apvts.getParameter ("ldSC")) btnAtt = std::make_unique<juce::ButtonParameterAttachment> (*bp, scBtn);
        addAndMakeVisible (scBtn);
        resetBtn.setTooltip ("Put every log drum knob back to 100% of the selected type");
        resetBtn.onClick = [this]
        {
            for (auto* id : { "ldTune", "ldFine", "ldDrop", "ldDropT", "ldAttack", "ldPunch", "ldDecay", "ldSub", "ldBody", "ldTone", "ldDrive",
                              "ldWidth", "ldVel", "ldGlide", "ldHuman", "ldRoom", "ldOut", "ldMono", "ldSC" })
                if (auto* pr = proc.apvts.getParameter (id))
                { pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->getDefaultValue()); pr->endChangeGesture(); }
        };
        addAndMakeVisible (resetBtn);
        startTimerHz (6);
        timerCallback();
    }
    ~LogDrumPanel() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.f);
        if (paintPlate) paintPlate (g, b);
        else { g.setColour (juce::Colour (0xff24262b)); g.fillRoundedRectangle (b, 7.f); }
        // a thin ember rule under the header
        g.setColour (EnochLookAndFeel::of (*this).accent.withAlpha (0.45f));
        g.fillRect (b.reduced (18.f, 0.f).withY (b.getY() + 44.f).withHeight (1.f));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 10);
        auto head = r.removeFromTop (30);
        title.setBounds (head.removeFromLeft (170));
        resetBtn.setBounds (head.removeFromRight (90).reduced (2, 2));
        scBtn.setBounds (head.removeFromRight (190).reduced (2, 2));
        monoBox.setBounds (head.removeFromRight (90).reduced (2, 3));
        lMono.setBounds (head.removeFromRight (80));
        head.removeFromRight (6);
        const int tw = juce::jmin (118, head.getWidth() / juce::jmax (1, typeBtn.size()));
        for (auto* b : typeBtn) b->setBounds (head.removeFromLeft (tw).reduced (2, 2));
        r.removeFromTop (8);
        state.setBounds (r.removeFromBottom (16));
        // 17 knobs: two rows (9 + 8)
        const int perRow = 9, rows = 2;
        const int rh = r.getHeight() / rows;
        for (int row = 0; row < rows; ++row)
        {
            auto rr = r.removeFromTop (rh);
            const int n0 = row * perRow, n1 = juce::jmin ((int) knobs.size(), n0 + perRow);
            const int kw = rr.getWidth() / perRow;
            if (row == 1) rr.removeFromLeft (kw / 2);
            for (int i = n0; i < n1; ++i)
            {
                auto col = rr.removeFromLeft (kw);
                labels[(size_t) i]->setBounds (col.removeFromTop (14));
                knobs[(size_t) i]->setBounds (col.withSizeKeepingCentre (juce::jmin (kw - 6, 74), col.getHeight()));
            }
        }
    }

private:
    void knob (const char* id, const char* name, const char* lo, const char* hi, const char* unit, const char* tip)
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
        const juce::String u (unit);
        s->textFromValueFunction = [u] (double v)
        {
            if (u == "st") { const int o = juce::roundToInt (v); return (o > 0 ? "+" : "") + juce::String (o) + " st"; }
            if (u == "ct" || u == "dB") return (v > 0.05 ? "+" : "") + juce::String (v, u == "dB" ? 1 : 0) + " " + u;
            if (u == "ms") return juce::String (v, v < 10.0 ? 1 : 0) + " ms";
            if (u == "%" && v < 0) return juce::String (juce::roundToInt (v)) + "%";
            return juce::String (juce::roundToInt (v)) + "%";
        };
        s->updateText();
        auto l = std::make_unique<juce::Label> (juce::String(), name);
        l->setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f));
        l->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*s); addAndMakeVisible (*l);
        knobs.push_back (std::move (s)); labels.push_back (std::move (l));
    }
    int currentSound() const
    {
        if (auto* p = proc.apvts.getRawParameterValue ("sound" + juce::String ((int) enochsolo::I_Bass))) return (int) std::lround (p->load());
        return -1;
    }
    void pickType (int idx)
    {
        if (auto* pr = proc.apvts.getParameter ("sound" + juce::String ((int) enochsolo::I_Bass)))
        { pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->convertTo0to1 ((float) idx)); pr->endChangeGesture(); }
        timerCallback();
    }
    void timerCallback() override
    {
        const int cur = currentSound();
        bool isLog = false;
        for (int i = 0; i < typeBtn.size(); ++i) { const bool on = typeIdx[(size_t) i] == cur; typeBtn[i]->setToggleState (on, juce::dontSendNotification); isLog |= on; }
        const juce::String t = isLog ? juce::String ("These knobs shape the selected log drum type.  Tip: Bass genre Amapiano writes call-and-response log drum phrases; "
                                                     "overlap two notes or use a Slide articulation for a glide.")
                                     : juce::String ("PREVIEW SOUND is not an Amapiano log drum - pick a type above to hear these controls "
                                                     "(the knobs are kept and apply as soon as a log drum is selected).");
        if (t != state.getText()) state.setText (t, juce::dontSendNotification);
        for (auto& k : knobs) k->setAlpha (isLog ? 1.f : 0.55f);
    }

    EnochSoloProcessor& proc;
    juce::Label title, state, lMono;
    juce::OwnedArray<juce::TextButton> typeBtn;
    std::vector<int> typeIdx;
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> atts;
    juce::ComboBox monoBox;
    juce::ToggleButton scBtn { "Sidechain-friendly output" };
    juce::TextButton resetBtn { "Reset knobs" };
    std::unique_ptr<juce::ComboBoxParameterAttachment> comboAtt;
    std::unique_ptr<juce::ButtonParameterAttachment> btnAtt;
};
