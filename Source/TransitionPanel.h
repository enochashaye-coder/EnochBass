#pragma once
// v2.14.2 Enoch Bass: TRANSITIONS page of the SOUND & FEEL drawer (Engine/Transitions.h).
// How the bass line moves from one bar into the next, and how sections end. All on Auto by default.
//  - AMOUNT: how often a bar line gets a transition (50% = the strong moments: chord changes into a new phrase or section, resolutions)
//  - STYLE: how big they are (Auto follows each bar's density: sparse bars a single note, busy bars runs and fills)
//  - PLACEMENT, TYPE: limit where they go, or ask for one kind
//  - HUMANIZE: velocity variation of the transition notes;  ARTICULATION: Plain (no slides / ghosts) or allow slides and hammer-ons
// In the piano roll: right-click a bar's chord for Try another transition, Lock transition and the bar's SECTION (Intro, Verse, Chorus...).
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"

class TransitionPanel : public juce::Component
{
public:
    explicit TransitionPanel (EnochSoloProcessor& p) : proc (p)
    {
        slider (amount, amountL, "trAmount", "AMOUNT", "How often a bar line gets a transition. 50% = the strong moments (a chord change into a new phrase or section, "
                "a resolution); 100% = wherever one fits; 0% = none (the line simply goes on).");
        slider (human, humanL, "trHuman", "HUMANIZE", "How much the velocity of transition notes varies.");
        combo (style, styleL, "trStyle", "STYLE", "Auto: the size follows each bar's density (Bar Notes Density): sparse bars get one approach note or space, "
               "busy bars runs and fills. Subtle .. Virtuosic set it for the whole take.");
        combo (place, placeL, "trPlace", "PLACEMENT", "Auto: where the music needs it. Or only at the end of every bar, before chord changes, before section changes, "
               "or before fills (the end of each 4-bar phrase).");
        combo (type, typeL, "trType", "TYPE", "Auto: the genre chooses (funk ghost and dead-note pickups, gospel chromatic walk-ups, jazz approach notes, Afrobeats "
               "anticipation, Amapiano space, Kizomba slides, pop octaves). Or one kind everywhere.");
        combo (art, artL, "trArt", "ARTICULATION", "Auto: slides, hammer-ons and ghost notes where the routed library has them. Plain: none. "
               "Allow: runs also played with hammer-ons and pull-offs.");
        auto def = [this] (const char* id) { if (auto* pr = proc.apvts.getParameter (id)) { pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->getDefaultValue()); pr->endChangeGesture(); } };
        autoBtn.setTooltip ("Everything back to Auto, Amount 50%");
        autoBtn.onClick = [def] { for (auto* id : { "trAmount", "trStyle", "trPlace", "trType", "trHuman", "trArt" }) def (id); };
        addAndMakeVisible (autoBtn);
        help.setText ("Right-click a bar's chord in the piano roll: Try another transition, Lock transition, and the bar's Section (Intro, Verse, Pre-chorus, "
                      "Chorus, Bridge, Solo, Outro). The lead-in to a chorus is the biggest; intros and outros end with space; the last bar resolves "
                      "(or turns around to bar 1 when Loop is on).", juce::dontSendNotification);
        help.setFont (EnochLookAndFeel::fnt (11.5f)); help.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        help.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (help);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (26);
        autoBtn.setBounds (top.removeFromRight (140).reduced (2, 1));
        r.removeFromTop (4);
        auto left = r.removeFromLeft (r.getWidth() / 2).withTrimmedRight (12), right = r;
        auto line = [] (juce::Rectangle<int>& a) { return a.removeFromTop (30).reduced (0, 3); };
        { auto l = line (left); amountL.setBounds (l.removeFromLeft (110)); amount.setBounds (l); }
        { auto l = line (left); styleL.setBounds (l.removeFromLeft (110)); style.setBounds (l.removeFromLeft (220)); }
        { auto l = line (left); placeL.setBounds (l.removeFromLeft (110)); place.setBounds (l.removeFromLeft (220)); }
        { auto l = line (right); typeL.setBounds (l.removeFromLeft (110)); type.setBounds (l.removeFromLeft (220)); }
        { auto l = line (right); artL.setBounds (l.removeFromLeft (110)); art.setBounds (l.removeFromLeft (240)); }
        { auto l = line (right); humanL.setBounds (l.removeFromLeft (110)); human.setBounds (l); }
        help.setBounds (left.getUnion (right).withTrimmedTop (6));
    }

private:
    void slider (juce::Slider& s, juce::Label& l, const char* id, const char* name, const char* tip)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18); s.setTooltip (tip);
        if (auto* pr = proc.apvts.getParameter (id)) { satts.push_back (std::make_unique<juce::SliderParameterAttachment> (*pr, s)); s.setDoubleClickReturnValue (true, pr->convertFrom0to1 (pr->getDefaultValue())); }
        s.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; }; s.updateText();
        label (l, name, tip); addAndMakeVisible (s);
    }
    void combo (juce::ComboBox& c, juce::Label& l, const char* id, const char* name, const char* tip)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        { c.addItemList (cp->choices, 1); catts.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (*cp, c)); }
        c.setTooltip (tip); label (l, name, tip); addAndMakeVisible (c);
    }
    void label (juce::Label& l, const char* name, const char* tip)
    {
        l.setText (name, juce::dontSendNotification); l.setFont (EnochLookAndFeel::fnt (10.5f, "SemiBold", 0.05f)); l.setTooltip (tip);
        l.setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addAndMakeVisible (l);
    }

    EnochSoloProcessor& proc;
    juce::Slider amount, human;
    juce::ComboBox style, place, type, art;
    juce::Label amountL, humanL, styleL, placeL, typeL, artL, help;
    juce::TextButton autoBtn { "ALL ON AUTO" };
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> satts;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> catts;
};
