#pragma once
// v3.14.1 / v2.14.1 TIMING page of the routing panel (identical in Enoch Solo and Enoch Bass).
// Three different timings, three controls:
//  - LIVE MONITOR OFFSET (this route): what Enoch sends live to this instrument goes out early (or late) by this much, so a monitored
//    instrument that the DAW plays late lines up with the project. Never moves the take or the MIDI you drag.
//  - RECORDING OFFSET (this route): used instead while the DAW records, so MIDI recorded from Enoch's output lands on the grid (0 ms).
//  - MIDI TIMING OFFSET (all routes): moves every live note for feel; it moves dragged MIDI only when "Apply to dragged MIDI" is on.
// MEASURE: route the instrument's audio into Enoch's side-chain input; Enoch plays 8 short notes and measures how late they come back.
// BY EAR: Enoch plays a click and the instrument's note alternately; move the Live Monitor Offset until they sound as one.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"

class TimingPanel : public juce::Component, private juce::Timer
{
public:
    explicit TimingPanel (EnochSoloProcessor& p) : proc (p)
    {
        title.setFont (EnochLookAndFeel::fnt (13.f, "Bold", 0.06f)); addAndMakeVisible (title);
        intro.setFont (EnochLookAndFeel::fnt (11.5f)); intro.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        intro.setText ("Monitored external instruments can sound late: the DAW plays its timeline early to make up for latency, but not a track you monitor live. "
                       "These offsets move only what Enoch sends live. The take and the MIDI you drag stay on the grid.", juce::dontSendNotification);
        intro.setMinimumHorizontalScale (1.f); addAndMakeVisible (intro);

        sliderRow (mon, monL, monV, "LIVE MONITOR OFFSET", -1000.0, 200.0,
                   "Live Monitor Offset for this route: negative = Enoch sends this instrument's live MIDI that much earlier, so it lines up while you "
                   "monitor it. Never changes the take or the dragged MIDI. Double-click = 0");
        sliderRow (rec, recL, recV, "RECORDING OFFSET", -1000.0, 200.0,
                   "Used instead of the Live Monitor Offset while the DAW is recording. Keep 0 when you record Enoch's MIDI onto a track (it lands on the grid). "
                   "If you record this instrument's audio and it lands late, set how late. Double-click = 0");
        mon.onValueChange = [this] { if (! updating) storeRoute(); };
        rec.onValueChange = [this] { if (! updating) storeRoute(); };

        sliderRow (timing, timingL, timingV, "MIDI TIMING OFFSET", -100.0, 100.0,
                   "Every route: moves all live notes early (-) or late (+) for feel. Moves dragged MIDI only with 'Apply to dragged MIDI'. Double-click = 0");
        if (auto* pr = proc.apvts.getParameter ("timingOffsetMs"))
            timingAtt = std::make_unique<juce::SliderParameterAttachment> (*pr, timing);
        timing.onValueChange = [this] { showValue (timing, timingV); };
        toDrag.setButtonText ("Apply the MIDI Timing Offset (bottom row) to dragged MIDI too");
        toDrag.setTooltip ("Off (default): the MIDI you drag is exactly the take, on the grid. On: it is moved by the MIDI Timing Offset too.");
        if (auto* pr = proc.apvts.getParameter ("timingToDrag"))
            dragAtt = std::make_unique<juce::ButtonParameterAttachment> (*pr, toDrag);
        addAndMakeVisible (toDrag);

        measure.setTooltip ("Route this instrument's audio into Enoch's side-chain input first (in Studio One: the side-chain button at the top of "
                            "Enoch's window). Enoch plays 8 short notes on this route and measures how late they come back.");
        measure.onClick = [this] { proc.startMeasure (part, false); };
        byEar.setTooltip ("Enoch plays a click and this instrument's note, about once a second. Move the Live Monitor Offset until they are one sound. "
                          "Click again to stop.");
        byEar.onClick = [this] { if (proc.measureState() == 4) proc.stopMeasure(); else proc.startMeasure (part, true); };
        accept.setTooltip ("Set the Live Monitor Offset to the measured delay (sent that much earlier)");
        accept.onClick = [this] { if (proc.measureState() == 2) { mon.setValue (-std::round (proc.measuredMs() * 10.0) / 10.0, juce::sendNotificationSync); } };
        reset.setTooltip ("Live Monitor Offset and Recording Offset of this route back to 0");
        reset.onClick = [this] { mon.setValue (0.0, juce::sendNotificationSync); rec.setValue (0.0, juce::sendNotificationSync); };
        back.onClick = [this] { proc.stopMeasure(); if (onBack) onBack(); };
        for (auto* b : { &measure, &byEar, &accept, &reset, &back }) addAndMakeVisible (b);
        result.setFont (EnochLookAndFeel::fnt (12.f, "SemiBold")); addAndMakeVisible (result);
        diag.setFont (EnochLookAndFeel::fnt (11.5f)); diag.setJustificationType (juce::Justification::topLeft);
        diag.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f)); addAndMakeVisible (diag);
        warn.setFont (EnochLookAndFeel::fnt (11.5f, "SemiBold")); warn.setJustificationType (juce::Justification::topLeft);
        warn.setColour (juce::Label::textColourId, juce::Colour (0xffffb74d)); addAndMakeVisible (warn);
        startTimerHz (8);
    }
    ~TimingPanel() override { stopTimer(); }

    std::function<void()> onBack;

    void setPart (int p)
    {
        part = juce::jlimit (0, EnochSoloProcessor::kParts - 1, p);
        const auto rt = proc.getRoute (proc.previewSlot(), part);
        updating = true;
        mon.setValue (rt.monitorMs, juce::dontSendNotification); rec.setValue (rt.recordMs, juce::dontSendNotification);
        updating = false;
        showValue (mon, monV); showValue (rec, recV); showValue (timing, timingV);
        juce::String dest = rt.dest == EnochSoloProcessor::RD_Builtin ? "built-in sound" : rt.dest == EnochSoloProcessor::RD_Hardware ? "hardware port"
                          : rt.dest == EnochSoloProcessor::RD_Off ? "off" : "external VST (your DAW)";
        title.setText ("TIMING  -  " + EnochSoloProcessor::partNames()[part] + "  ->  " + dest + ", channel " + juce::String (rt.channel)
                       + (rt.profile.isNotEmpty() ? "  (" + rt.profile + ")" : juce::String()), juce::dontSendNotification);
        refreshNow = true; timerCallback();
    }

    void paint (juce::Graphics& g) override
    {
        // alignment meter: where the instrument sounds against the beat (measured delay minus the early send), on a -50..+300 ms ruler
        auto m = meterRect.toFloat();
        if (m.isEmpty()) return;
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (m, 5.f);
        auto in = m.reduced (12.f, 8.f);
        g.setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f)); g.setColour (EnochLookAndFeel::dim());
        g.drawText ("ALIGNMENT  (where this instrument sounds against the beat)", in.removeFromTop (16.f), juce::Justification::centredLeft);
        auto ruler = in.removeFromTop (34.f);
        const float lo = -50.f, hi = 300.f;
        auto X = [&] (float ms) { return ruler.getX() + (juce::jlimit (lo, hi, ms) - lo) / (hi - lo) * ruler.getWidth(); };
        g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRect (ruler.withHeight (2.f).withY (ruler.getCentreY()));
        g.setFont (EnochLookAndFeel::fnt (10.5f));
        for (float t : { -50.f, 0.f, 50.f, 100.f, 150.f, 200.f, 250.f, 300.f })
        {
            g.setColour (t == 0.f ? juce::Colours::white : juce::Colours::white.withAlpha (0.35f));
            g.fillRect (X (t) - 0.5f, ruler.getCentreY() - (t == 0.f ? 10.f : 5.f), 1.f, t == 0.f ? 20.f : 10.f);
            g.drawText (juce::String ((int) t), juce::Rectangle<float> (X (t) - 20.f, ruler.getBottom() - 10.f, 40.f, 12.f), juce::Justification::centred);
        }
        if (proc.measureState() == 2 || haveMeasured)
        {
            const float sounds = lastMeasured + (float) mon.getValue() + (float) timing.getValue();   // what you hear, after the early send
            const bool ok = std::abs (sounds) <= 3.f;
            g.setColour (ok ? juce::Colour (0xff4cd964) : juce::Colour (0xffffb74d));
            g.fillEllipse (X (sounds) - 6.f, ruler.getCentreY() - 6.f, 12.f, 12.f);
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.drawEllipse (X (lastMeasured) - 5.f, ruler.getCentreY() - 5.f, 10.f, 10.f, 1.2f);
            g.setFont (EnochLookAndFeel::fnt (11.f));
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawText ((ok ? "On the beat: " : "Sounds ") + juce::String (sounds, 1) + " ms" + (ok ? "" : (sounds > 0 ? " late" : " early"))
                        + "   (hollow ring = measured without offset)", in.removeFromTop (16.f), juce::Justification::centredLeft);
        }
        else
        {
            g.setFont (EnochLookAndFeel::fnt (11.f)); g.setColour (EnochLookAndFeel::dim());
            g.drawText ("Press MEASURE (side-chain) to see where it sounds, or use BY EAR.", in.removeFromTop (16.f), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (24);
        back.setBounds (top.removeFromRight (90).reduced (2, 0));
        title.setBounds (top);
        intro.setBounds (r.removeFromTop (34));
        r.removeFromTop (4);
        auto left = r.removeFromLeft (r.getWidth() * 55 / 100).withTrimmedRight (12), right = r;
        auto rowOf = [&] (juce::Rectangle<int>& area, juce::Label& l, juce::Slider& s, juce::Label& v)
        {
            auto line = area.removeFromTop (28);
            l.setBounds (line.removeFromLeft (150)); v.setBounds (line.removeFromRight (90)); s.setBounds (line.reduced (2, 3));
        };
        rowOf (left, monL, mon, monV);
        rowOf (left, recL, rec, recV);
        {
            auto line = left.removeFromTop (30).reduced (0, 2);
            measure.setBounds (line.removeFromLeft (110).reduced (2, 0));
            byEar.setBounds (line.removeFromLeft (110).reduced (2, 0));
            accept.setBounds (line.removeFromLeft (90).reduced (2, 0));
            reset.setBounds (line.removeFromLeft (80).reduced (2, 0));
        }
        result.setBounds (left.removeFromTop (22));
        left.removeFromTop (4);
        meterRect = left.removeFromTop (92);
        // the MIDI Timing Offset slider lives in the panel's bottom row (always visible); here only its drag option
        timing.setVisible (false); timingL.setVisible (false); timingV.setVisible (false);
        toDrag.setBounds (right.removeFromTop (24));
        right.removeFromTop (4);
        diag.setBounds (right.removeFromTop (juce::jmin (right.getHeight() - 40, 150)));
        warn.setBounds (right);
    }

    // used by the automatic tests
    juce::Slider& monitorSlider() { return mon; }
    juce::String diagnosticsText() const { return diag.getText() + "\n" + warn.getText(); }

private:
    void sliderRow (juce::Slider& s, juce::Label& l, juce::Label& v, const char* name, double lo, double hi, const char* tip)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRange (lo, hi, 0.1); s.setDoubleClickReturnValue (true, 0.0); s.setTooltip (tip);
        l.setText (name, juce::dontSendNotification); l.setFont (EnochLookAndFeel::fnt (11.f, "Bold", 0.06f)); l.setTooltip (tip);
        l.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        v.setFont (EnochLookAndFeel::fnt (12.f, "SemiBold")); v.setJustificationType (juce::Justification::centredRight);
        v.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (s); addAndMakeVisible (l); addAndMakeVisible (v);
    }
    static void showValue (juce::Slider& s, juce::Label& v)
    {
        const double x = s.getValue();
        v.setText (std::abs (x) < 0.05 ? juce::String ("0 ms") : juce::String::formatted ("%.1f ms %s", std::abs (x), x < 0 ? "early" : "late"), juce::dontSendNotification);
    }
    void storeRoute()
    {
        auto rt = proc.getRoute (proc.previewSlot(), part);
        rt.monitorMs = mon.getValue(); rt.recordMs = rec.getValue();
        proc.setRoute (proc.previewSlot(), part, rt);
        showValue (mon, monV); showValue (rec, recV);
        if (onChanged) onChanged();
        timerCallback();                      // diagnostics and the alignment meter follow at once
    }

    void timerCallback() override
    {
        if (! isVisible()) return;
        const int st = proc.measureState();
        if (st == 2 && proc.measuringPart() == part) { lastMeasured = proc.measuredMs(); haveMeasured = true; }
        juce::String res;
        switch (st)
        {
            case 1: res = "Measuring... (8 short notes on this route)"; break;
            case 2: res = "Measured: " + juce::String (proc.measuredMs(), 1) + " ms late  (spread " + juce::String (proc.measuredSpreadMs(), 1) + " ms, "
                          + juce::String (proc.measuredHits()) + " of 8 notes heard).  ACCEPT sends this route that much earlier."; break;
            case 3: res = "Nothing came back on the side-chain. Route this instrument's audio into Enoch's side-chain input, or use BY EAR."; break;
            case 4: res = "By ear: a click and the instrument's note, about once a second. Move the Live Monitor Offset until they are one sound."; break;
            default: res = haveMeasured ? "Last measurement: " + juce::String (lastMeasured, 1) + " ms late." : juce::String(); break;
        }
        result.setText (res, juce::dontSendNotification);
        byEar.setButtonText (st == 4 ? "STOP" : "BY EAR");
        accept.setEnabled (st == 2);
        measure.setEnabled (st != 1 && st != 4);

        const auto rt = proc.getRoute (proc.previewSlot(), part);
        const double srate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        const int blk = proc.blockSizeNow();
        juce::String d;
        d << "Host: " << proc.hostName() << "\n"
          << "Sample rate " << juce::String (srate / 1000.0, 1) << " kHz, block " << blk << " samples (" << juce::String (blk * 1000.0 / srate, 1) << " ms)\n"
          << "Latency Enoch reports: 0 samples (Enoch has no look-ahead)\n"
          << "This route: channel " << rt.channel << (rt.profile.isNotEmpty() ? ", " + rt.profile : juce::String()) << "\n"
          << "Live Monitor " << juce::String (rt.monitorMs, 1) << " ms, Recording " << juce::String (rt.recordMs, 1) << " ms, MIDI Timing "
          << juce::String (timing.getValue(), 1) << " ms\n"
          << "DAW is " << (proc.hostIsRecording() ? "RECORDING (Recording Offset in use)" : proc.hostIsPlaying() ? "playing (Live Monitor Offset in use)" : "stopped") << "\n"
          << "Side-chain audio: " << (proc.sideChainConnected() ? "arriving" : "none yet") << "\n"
          << "Notes sent late this playback: " << proc.lateEventsThisPlay();
        diag.setText (d, juce::dontSendNotification);
        juce::String w;
        if (haveMeasured && lastMeasured > 50.f)
            w << "This instrument comes back " << juce::String (lastMeasured, 0) << " ms late. Studio One: lower Dropout Protection, a smaller block size, "
              << "or switch Monitor off on its track when you only play back (it then plays in time with no offset).\n";
        if (haveMeasured && proc.measuredSpreadMs() > 3.f) w << "The delay changes from note to note (spread over 3 ms): set a fixed Dropout Protection.\n";
        if (proc.lateEventsThisPlay() > 0)
            w << proc.lateEventsThisPlay() << " note(s) were due before you pressed Play with this early offset and went out at once. Start a bar earlier for a clean count-in.\n";
        w << "The audio interface and Dropout Protection cannot be read by a plugin: check them in Studio One's Options > Audio Setup.";
        warn.setText (w, juce::dontSendNotification);
        repaint (meterRect);
    }

public:
    std::function<void()> onChanged;
private:
    EnochSoloProcessor& proc;
    int part = 0; bool updating = false, haveMeasured = false, refreshNow = false; float lastMeasured = 0.f;
    juce::Label title, intro, monL, monV, recL, recV, timingL, timingV, result, diag, warn;
    juce::Slider mon, rec, timing;
    juce::ToggleButton toDrag;
    juce::TextButton measure { "MEASURE" }, byEar { "BY EAR" }, accept { "ACCEPT" }, reset { "RESET" }, back { "< Back" };
    std::unique_ptr<juce::SliderParameterAttachment> timingAtt;
    std::unique_ptr<juce::ButtonParameterAttachment> dragAtt;
    juce::Rectangle<int> meterRect;
};
