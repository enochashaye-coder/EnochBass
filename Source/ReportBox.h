#pragma once
// v3.14 / v2.14 "Report a problem" (identical in Enoch Solo and Enoch Bass).
// A small box that opens from the REPORT button: write what went wrong in a few words, press Save report, and Enoch writes one zip
// (Documents/Enoch Reports) with this take as a MIDI file, the plugin state, the key-switch profiles in use and the MIDI check.
// Attach that zip when you report the bug: it holds everything needed to reproduce the take exactly.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"

class ReportBox : public juce::Component
{
public:
    explicit ReportBox (EnochSoloProcessor& p) : proc (p)
    {
        title.setText ("REPORT A PROBLEM", juce::dontSendNotification);
        title.setFont (EnochLookAndFeel::fnt (12.f, "SemiBold", 0.08f));
        addAndMakeVisible (title);
        intro.setText ("What went wrong? (e.g. \"the legato on bar 3 does not slur in EZbass\", \"the key-switch on beat 2 plays the wrong sound\")",
                       juce::dontSendNotification);
        intro.setFont (EnochLookAndFeel::fnt (11.f));
        intro.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        intro.setMinimumHorizontalScale (1.f);
        addAndMakeVisible (intro);
        text.setMultiLine (true, true);
        text.setReturnKeyStartsNewLine (true);
        text.setTextToShowWhenEmpty ("Describe the problem, the bar and the instrument / library...", EnochLookAndFeel::dim());
        addAndMakeVisible (text);
        save.onClick = [this] { doSave(); };
        save.setTooltip ("Save a zip with this take, the settings, the key-switch profiles and the MIDI check, to attach to your report");
        addAndMakeVisible (save);
        show.onClick = [this] { if (last.existsAsFile()) last.revealToUser(); else EnochSoloProcessor::reportsFolder().revealToUser(); };
        show.setTooltip ("Open the folder with the report in Explorer");
        addAndMakeVisible (show);
        status.setFont (EnochLookAndFeel::fnt (11.f));
        status.setText ("Saved to Documents\\Enoch Reports. The take, settings, profiles and MIDI check go in one zip.", juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        addAndMakeVisible (status);
        setSize (520, 230);
    }

    /** used by the automatic tests too */
    juce::File doSave()
    {
        juce::String msg;
        last = proc.writeProblemReport (text.getText(), msg);
        status.setText (msg + (last.existsAsFile() ? "  -  attach this file to your message." : ""), juce::dontSendNotification);
        if (last.existsAsFile()) show.setButtonText ("Show the file");
        return last;
    }
    juce::TextEditor& textBox() { return text; }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        title.setBounds (r.removeFromTop (18));
        intro.setBounds (r.removeFromTop (32));
        auto bottom = r.removeFromBottom (26);
        save.setBounds (bottom.removeFromRight (120).reduced (2, 0));
        show.setBounds (bottom.removeFromRight (120).reduced (2, 0));
        status.setBounds (r.removeFromBottom (34));
        text.setBounds (r.reduced (0, 4));
    }

private:
    EnochSoloProcessor& proc;
    juce::Label title, intro, status;
    juce::TextEditor text;
    juce::TextButton save { "Save report" }, show { "Open folder" };
    juce::File last;
};
