#pragma once
// v3.6 TAP RHYTHM drawer (identical in Enoch Solo and Enoch Bass). Tap a rhythm with the mouse (higher on the pad = louder),
// any computer key (hold for longer notes) or a MIDI keyboard / pad (velocity and note length are used). USE RHYTHM turns the
// taps into a rhythm template: the take is written on that rhythm, and every TRY ANOTHER keeps it while the notes change.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"

class TapPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> paintPlate;

    explicit TapPanel (EnochSoloProcessor& p) : proc (p)
    {
        setWantsKeyboardFocus (true);
        title.setText ("TAP RHYTHM", juce::dontSendNotification);
        title.setFont (EnochLookAndFeel::fnt (13.f, "Bold", 0.08f));
        addAndMakeVisible (title);
        hint.setText ("Tap the pad (higher = louder), press any key (hold = longer), or play your MIDI keyboard. Then USE RHYTHM.", juce::dontSendNotification);
        hint.setFont (EnochLookAndFeel::fnt (11.5f));
        hint.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        addAndMakeVisible (hint);
        status.setFont (EnochLookAndFeel::fnt (12.f, "SemiBold"));
        addAndMakeVisible (status);
        clearBtn.setTooltip ("Start again");
        clearBtn.onClick = [this] { hits.clear(); updateStatus(); repaint(); };
        useBtn.setTooltip ("Write the take on this rhythm. TRY ANOTHER then keeps the rhythm and changes the notes (Undo and < go back)");
        useBtn.onClick = [this]
        {
            std::vector<EnochSoloProcessor::TapHit> h;
            for (auto& x : hits) { EnochSoloProcessor::TapHit t; t.ms = x.ms; t.durMs = x.durMs > 0 ? x.durMs : 120.0; t.loud = x.loud; h.push_back (t); }
            proc.useTappedRhythm (h, tempo());
            updateStatus();
        };
        stopBtn.setTooltip ("Stop following the tapped rhythm (or a loaded reference)");
        stopBtn.onClick = [this] { proc.clearReference(); updateStatus(); };
        for (auto* b : { &clearBtn, &useBtn, &stopBtn }) addAndMakeVisible (*b);
        updateStatus();
    }

    void visibilityChanged() override
    {
        proc.setTapArmed (isVisible());          // MIDI notes become taps only while this drawer is open
        if (isVisible()) { EnochSoloProcessor::TapMidi tmp[64]; while (proc.popTaps (tmp, 64) > 0) {} startTimerHz (30); grabKeyboardFocus(); }
        else stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.f);
        if (paintPlate) paintPlate (g, b); else { g.setColour (juce::Colour (0xff24262b)); g.fillRoundedRectangle (b, 7.f); }
        auto pad = padRect.toFloat();
        const auto accent = EnochLookAndFeel::of (*this).accent;
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (pad, 6.f);
        g.setColour (hasKeyboardFocus (true) ? accent.withAlpha (0.8f) : accent.withAlpha (0.3f)); g.drawRoundedRectangle (pad.reduced (0.5f), 6.f, 1.5f);
        if (hits.empty())
        {
            g.setColour (EnochLookAndFeel::dim()); g.setFont (EnochLookAndFeel::fnt (14.f, "SemiBold"));
            g.drawText ("TAP HERE", pad, juce::Justification::centred);
            return;
        }
        // time line: beats of the current tempo, then every tap (height = how loud, width = how long)
        const double t0 = hits.front().ms, beatMs = 60000.0 / tempo();
        const double span = std::max (beatMs * 4.0, (hits.back().ms + std::max (100.0, hits.back().durMs) - t0) + beatMs * 0.5);
        auto inner = pad.reduced (8.f);
        auto xOf = [&] (double ms) { return inner.getX() + (float) ((ms - t0) / span) * inner.getWidth(); };
        for (double t = t0; t <= t0 + span; t += beatMs)
        { g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawVerticalLine ((int) xOf (t), inner.getY(), inner.getBottom()); }
        for (auto& h : hits)
        {
            const float x = xOf (h.ms), w = std::max (3.f, xOf (h.ms + (h.durMs > 0 ? h.durMs : juce::Time::getMillisecondCounterHiRes() - h.ms)) - x);
            const float hh = inner.getHeight() * (0.25f + 0.75f * h.loud);
            g.setColour (accent.withAlpha (0.35f + 0.6f * h.loud));
            g.fillRoundedRectangle (x, inner.getBottom() - hh, w, hh, 2.f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 10);
        auto head = r.removeFromTop (28);
        title.setBounds (head.removeFromLeft (120));
        stopBtn.setBounds (head.removeFromRight (120).reduced (2, 2));
        useBtn.setBounds (head.removeFromRight (120).reduced (2, 2));
        clearBtn.setBounds (head.removeFromRight (80).reduced (2, 2));
        hint.setBounds (head);
        status.setBounds (r.removeFromBottom (20));
        padRect = r.reduced (0, 6);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (! padRect.contains (e.getPosition())) return;
        const float loud = juce::jlimit (0.1f, 1.f, 1.f - (float) (e.y - padRect.getY()) / (float) std::max (1, padRect.getHeight()));
        startHit (loud); mouseHit = (int) hits.size() - 1;
    }
    void mouseUp (const juce::MouseEvent&) override { endHit (mouseHit); mouseHit = -1; }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown() || k.getModifiers().isAltDown()) return false;
        if (keyHit < 0) { startHit (0.7f); keyHit = (int) hits.size() - 1; }
        return true;
    }
    bool keyStateChanged (bool isKeyDown) override
    {
        if (! isKeyDown && keyHit >= 0) { endHit (keyHit); keyHit = -1; return true; }
        return false;
    }

private:
    struct Hit { double ms = 0, durMs = -1; float loud = 0.7f; };

    double tempo() const { const double b = proc.getHostBpm(); return b > 20.0 ? b : 120.0; }

    void startHit (float loud, double ms = -1.0)
    {
        if (ms < 0) ms = juce::Time::getMillisecondCounterHiRes();
        if (! hits.empty() && ms - hits.back().ms > 4000.0) hits.clear();      // a pause of 4 s starts a new take
        if (hits.size() >= 256) return;
        hits.push_back ({ ms, -1.0, loud });
        updateStatus(); repaint();
    }
    void endHit (int i, double ms = -1.0)
    {
        if (i < 0 || i >= (int) hits.size()) return;
        if (ms < 0) ms = juce::Time::getMillisecondCounterHiRes();
        hits[(size_t) i].durMs = std::max (30.0, ms - hits[(size_t) i].ms);
        repaint();
    }

    void timerCallback() override
    {
        EnochSoloProcessor::TapMidi ev[64];
        const int n = proc.popTaps (ev, 64);
        for (int i = 0; i < n; ++i)
        {
            if (ev[i].on) { startHit (juce::jlimit (0.1f, 1.f, (float) ev[i].vel / 127.f), ev[i].ms); midiHit = (int) hits.size() - 1; }
            else { endHit (midiHit, ev[i].ms); midiHit = -1; }
        }
        if (keyHit >= 0 || mouseHit >= 0 || midiHit >= 0) repaint();
    }

    void updateStatus()
    {
        juce::String s;
        if (hits.empty()) s = "No taps yet.";
        else
        {
            const double beats = (hits.back().ms - hits.front().ms) / (60000.0 / tempo());
            s << (int) hits.size() << " taps over " << juce::String::formatted ("%.1f", beats) << " beats at " << juce::roundToInt (tempo()) << " BPM";
            if (hits.size() < 3) s << "  (tap at least 3)";
        }
        const auto ref = proc.referenceName();
        if (ref.isNotEmpty()) s << "     Following: " << ref;
        status.setText (s, juce::dontSendNotification);
        useBtn.setEnabled (hits.size() >= 3);
        stopBtn.setEnabled (ref.isNotEmpty());
    }

    EnochSoloProcessor& proc;
    std::vector<Hit> hits;
    int mouseHit = -1, keyHit = -1, midiHit = -1;
    juce::Rectangle<int> padRect;
    juce::Label title, hint, status;
    juce::TextButton clearBtn { "CLEAR" }, useBtn { "USE RHYTHM" }, stopBtn { "STOP FOLLOWING" };
};
