#pragma once
// v3.6 OUTPUT drawer (identical in Enoch Solo and Enoch Bass): master volume, part levels, compressor, Safe Output limiter,
// and a peak meter with peak hold, gain reduction and a clip light. Only the built-in sound is affected (MIDI is unchanged).
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"

class OutputPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> paintPlate;

    explicit OutputPanel (EnochSoloProcessor& p) : proc (p)
    {
        const bool bass = juce::String (JucePlugin_Name).containsIgnoreCase ("bass");
        knob ("masterVol",  "MASTER",      "Master volume of the built-in sound (dB). The default leaves headroom", " dB");
        knob ("partLvl1",   bass ? "BASS" : "SOLO",                 "Level of the main part (it sets how hard its notes are played)", "%");
        knob ("partLvl2",   bass ? "LOG DRUM" : "ACCOMP.",          "Level of the second part (it sets how hard its notes are played)", "%");
        knob ("partLvl3",   "CHORD PAD",   "Level of the chord pad part (it sets how hard its notes are played)", "%");
        knob ("compThresh", "THRESHOLD",   "Compressor: level above which it turns the sound down", " dB");
        knob ("compRatio",  "RATIO",       "Compressor: how strongly (3 = 3 dB over becomes 1 dB over)", ":1");
        knob ("compAttack", "ATTACK",      "Compressor: how fast it reacts", " ms");
        knob ("compRelease","RELEASE",     "Compressor: how fast it lets go", " ms");
        knob ("compKnee",   "KNEE",        "Compressor: soft (high) or hard (0) onset", " dB");
        knob ("compMakeup", "MAKEUP",      "Compressor: gain added back after compressing", " dB");
        knob ("limCeiling", "CEILING",     "Safe Output: the highest level the sound can reach (dBFS)", " dB");
        knob ("limRelease", "LIM RELEASE", "Safe Output: how fast the limiter lets go", " ms");
        toggle (compOn, "compOn", "Compressor on / off (off by default)");
        toggle (limOn,  "limOn",  "Safe Output: a limiter that keeps every note below the ceiling, so nothing clips. On by default and transparent unless pushed");
        title.setText ("OUTPUT", juce::dontSendNotification);
        title.setFont (EnochLookAndFeel::fnt (13.f, "Bold", 0.08f));
        addAndMakeVisible (title);
        hint.setText ("Built-in sound only. Click the meter to reset the clip light.", juce::dontSendNotification);
        hint.setFont (EnochLookAndFeel::fnt (11.5f));
        hint.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        addAndMakeVisible (hint);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.f);
        if (paintPlate) paintPlate (g, b); else { g.setColour (juce::Colour (0xff24262b)); g.fillRoundedRectangle (b, 7.f); }
        // meter: two bars (dB scale -48..0), peak hold ticks, gain reduction bar, clip light
        auto m = meterRect.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (m, 4.f);
        auto bars = m.reduced (6.f).withTrimmedBottom (18.f);
        const float bw = (bars.getWidth() - 12.f) / 3.f;
        auto yOfDb = [&] (float db) { return bars.getBottom() - bars.getHeight() * juce::jlimit (0.f, 1.f, (db + 48.f) / 48.f); };
        for (int c = 0; c < 2; ++c)
        {
            auto col = juce::Rectangle<float> (bars.getX() + (float) c * (bw + 6.f), bars.getY(), bw, bars.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRect (col);
            const float db = juce::Decibels::gainToDecibels (shown[c], -60.f), y = yOfDb (db);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3ec46d), 0.f, bars.getBottom(), juce::Colour (0xffe0c341), 0.f, yOfDb (-6.f), false));
            g.fillRect (col.withTop (y));
            if (db > -6.f) { g.setColour (juce::Colour (0xffe07a3a)); g.fillRect (col.withTop (y).withBottom (yOfDb (-6.f))); }
            g.setColour (juce::Colours::white.withAlpha (0.9f)); g.fillRect (col.getX(), yOfDb (juce::Decibels::gainToDecibels (hold[c], -60.f)) - 1.f, col.getWidth(), 2.f);
        }
        auto grCol = juce::Rectangle<float> (bars.getX() + 2.f * (bw + 6.f), bars.getY(), bw, bars.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRect (grCol);
        const float grH = bars.getHeight() * juce::jlimit (0.f, 1.f, -grShown / 24.f);
        g.setColour (juce::Colour (0xff4aa3df)); g.fillRect (grCol.withHeight (grH));
        auto foot = m.reduced (6.f).removeFromBottom (14.f);
        g.setFont (EnochLookAndFeel::fnt (10.f, "SemiBold"));
        g.setColour (EnochLookAndFeel::dim()); g.drawText ("L  R  GR", foot.withTrimmedRight (20.f), juce::Justification::centredLeft);
        auto led = foot.removeFromRight (14.f).reduced (1.f);
        g.setColour (clipLit ? juce::Colours::red : juce::Colours::red.withAlpha (0.18f)); g.fillEllipse (led);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 10);
        auto head = r.removeFromTop (28);
        title.setBounds (head.removeFromLeft (90));
        limOn.setBounds (head.removeFromRight (140).reduced (2, 1));      // the two switches sit in the title row (room for their names)
        compOn.setBounds (head.removeFromRight (140).reduced (2, 1));
        hint.setBounds (head);
        r.removeFromTop (6);
        meterRect = r.removeFromRight (96).reduced (4, 0);
        r.removeFromRight (10);
        // row of knobs: master + parts | compressor (toggle + 6) | limiter (toggle + 2)
        const int gap = 18;
        const int kw = juce::jmax (52, (r.getWidth() - 2 * gap) / (int) knobs.size());
        auto place = [&] (int i) { auto c = r.removeFromLeft (kw); auto& k = knobs[(size_t) i]; k.label->setBounds (c.removeFromTop (14)); k.value->setBounds (c.removeFromBottom (16)); k.slider->setBounds (c.reduced (2, 0)); };
        for (int i = 0; i < 4; ++i) place (i);
        r.removeFromLeft (gap);                                          // compressor knobs
        for (int i = 4; i < 10; ++i) place (i);
        r.removeFromLeft (gap);                                          // Safe Output knobs
        for (int i = 10; i < 12; ++i) place (i);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (meterRect.contains (e.getPosition())) { proc.resetClip(); clipLit = false; hold[0] = hold[1] = 0.f; repaint (meterRect); }
    }

private:
    struct K { std::unique_ptr<juce::Slider> slider; std::unique_ptr<juce::Label> label, value; std::unique_ptr<juce::SliderParameterAttachment> att; };

    void knob (const char* id, const char* name, const char* tip, const char* suffix)
    {
        K k;
        k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        k.slider->setTooltip (tip);
        k.value = std::make_unique<juce::Label>();
        k.value->setFont (EnochLookAndFeel::fnt (10.5f));
        k.value->setJustificationType (juce::Justification::centred);
        k.value->setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        const juce::String suf (suffix);
        auto* sl = k.slider.get(); auto* vl = k.value.get();
        k.slider->onValueChange = [sl, vl, suf]
        {
            const double v = sl->getValue();
            vl->setText ((std::abs (v) < 10.0 && ! suf.contains ("%") ? juce::String::formatted ("%.1f", v) : juce::String::formatted ("%.0f", v)) + suf, juce::dontSendNotification);
        };
        if (auto* pr = proc.apvts.getParameter (id))
        {
            k.att = std::make_unique<juce::SliderParameterAttachment> (*pr, *k.slider);
            k.slider->setDoubleClickReturnValue (true, (double) dynamic_cast<juce::RangedAudioParameter*> (pr)->convertFrom0to1 (pr->getDefaultValue()));
        }
        if (k.slider->onValueChange) k.slider->onValueChange();
        k.label = std::make_unique<juce::Label> (juce::String(), name);
        k.label->setFont (EnochLookAndFeel::fnt (10.5f, "SemiBold", 0.05f));
        k.label->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*k.slider); addAndMakeVisible (*k.label); addAndMakeVisible (*k.value);
        knobs.push_back (std::move (k));
    }

    void toggle (juce::ToggleButton& t, const char* id, const char* tip)
    {
        t.setButtonText (juce::String (id) == "limOn" ? "SAFE OUTPUT" : "COMPRESSOR");
        t.setTooltip (tip);
        if (auto* pr = proc.apvts.getParameter (id)) batts.push_back (std::make_unique<juce::ButtonParameterAttachment> (*pr, t));
        addAndMakeVisible (t);
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        for (int c = 0; c < 2; ++c)
        {
            const float pk = proc.outPeak (c);
            shown[c] = std::max (pk, shown[c] * 0.86f);
            if (pk >= hold[c]) { hold[c] = pk; holdUntil[c] = juce::Time::getMillisecondCounter() + 1500; }
            else if (juce::Time::getMillisecondCounter() > holdUntil[c]) hold[c] *= 0.95f;
        }
        grShown = std::min (proc.outGainReductionDb(), grShown * 0.8f);
        clipLit = proc.outClipped();
        repaint (meterRect);
    }

    EnochSoloProcessor& proc;
    std::vector<K> knobs;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> batts;
    juce::ToggleButton compOn, limOn;
    juce::Label title, hint;
    juce::Rectangle<int> meterRect;
    float shown[2] = { 0.f, 0.f }, hold[2] = { 0.f, 0.f }, grShown = 0.f;
    juce::uint32 holdUntil[2] = { 0, 0 };
    bool clipLit = false;
};
