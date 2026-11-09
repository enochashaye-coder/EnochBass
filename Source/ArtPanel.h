#pragma once
// v3.11 / v2.11 ARTICULATION page of the SOUND & FEEL drawer (identical in Enoch Solo and Enoch Bass).
// What the articulation planner (Engine/ArtBrain.h) is allowed to do, family by family, and what it did in this take:
//  - INTENSITY: the ARTICULATION knob (how expressive the whole performance is)
//  - VARIATION: how much close choices and repeated riffs may differ from take to take
//  - one row per family the instrument has: amount (100% = the style's natural amount) and a lock (Auto / Never / Always where it fits)
//  - PROFESSIONAL BALANCE: everything back to the natural, musically balanced setting
//  - THIS TAKE: the share of the main line's notes each family uses now (hover a note in the piano roll to see why it got its articulation)
//  - v3.14 WHY: the most common reasons behind this take's articulations, and PHRASES: how articulated each phrase is (hover for details)
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"
#include "Engine/Phrases.h"
#include <map>

class ArtPanel : public juce::Component, public juce::TooltipClient, private juce::Timer
{
public:
    explicit ArtPanel (EnochSoloProcessor& p) : proc (p)
    {
        using namespace enochsolo::artbrain;
        row (intensity, intensityL, "artAmount", "INTENSITY", "How expressive the whole performance is (the ARTICULATION knob). 60% = natural");
        row (variation, variationL, "artVary", "VARIATION", "0% = the same choices every time for the same notes. Higher = more variety between close choices and repeated riffs");
        for (int i = 0; i < F_Count; ++i)
        {
            auto* r = fams.add (new FamRow());
            r->fam = i;
            juce::String tip (familyTip (i));
            row (r->amt, r->name, ("fa" + juce::String (i)).toRawUTF8(), familyShort (i), (tip + "  100% = what this style naturally uses, 0% = none, 200% = twice as much where it fits").toRawUTF8());
            if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("fl" + juce::String (i))))
            {
                r->lock.addItemList (cp->choices, 1);
                catts.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (*cp, r->lock));
            }
            r->lock.setTooltip ("Auto: Enoch decides by the music.  Never: this articulation is not used at all (also not by the style's licks).  "
                                "Always: used wherever the music allows it");
            addChildComponent (r->lock);
        }
        balance.setTooltip ("Professional Balance: intensity 60%, variation 30%, every family at its natural amount and on Auto. "
                            "The plan then uses each articulation only where the phrase calls for it, in the proportions of the style");
        balance.onClick = [this] { professionalBalance(); };
        addAndMakeVisible (balance);
        hint.setText ("Hover a note in the piano roll to see why it got its articulation.", juce::dontSendNotification);
        hint.setFont (EnochLookAndFeel::fnt (11.f));
        hint.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        addAndMakeVisible (hint);
        startTimerHz (4);
        timerCallback();
    }

    ~ArtPanel() override { stopTimer(); }

    void professionalBalance()
    {
        auto set = [this] (const juce::String& id, float plain)
        {
            if (auto* pr = proc.apvts.getParameter (id))
            { pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->convertTo0to1 (plain)); pr->endChangeGesture(); }
        };
        set ("artAmount", 60.f); set ("artVary", 30.f);
        for (int i = 0; i < enochsolo::artbrain::F_Count; ++i) { set ("fa" + juce::String (i), 100.f); set ("fl" + juce::String (i), 0.f); }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace enochsolo::artbrain;
        auto u = usageRect.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (u, 5.f);
        auto b = u.reduced (8.f, 6.f);
        // v3.14 right column: why, and phrase by phrase
        {
            auto w = whyRect.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (w, 5.f);
            auto c = w.reduced (8.f, 6.f);
            g.setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f));
            g.setColour (EnochLookAndFeel::dim());
            g.drawText ("WHY  (most common reasons)", c.removeFromTop (15.f), juce::Justification::centredLeft);
            g.setFont (EnochLookAndFeel::fnt (10.5f));
            const int nWhy = juce::jmin ((int) reasons.size(), juce::jlimit (2, 6, (int) ((c.getHeight() - 15.f - 6.f - 15.f - 18.f) / 15.f)));
            for (int i = 0; i < nWhy; ++i)
            {
                auto line = c.removeFromTop (15.f);
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawText (juce::String (reasons[(size_t) i].second) + "x", line.removeFromLeft (30.f), juce::Justification::centredLeft);
                g.setColour (EnochLookAndFeel::dim());
                g.drawFittedText (whyShort (reasons[(size_t) i].first), line.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            }
            if (nWhy == 0) { g.setColour (EnochLookAndFeel::dim()); g.drawText ("(all notes plain)", c.removeFromTop (15.f), juce::Justification::centredLeft); }
            c.removeFromTop (6.f);
            g.setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f));
            g.setColour (EnochLookAndFeel::dim());
            g.drawText ("PHRASES  (share articulated)", c.removeFromTop (15.f), juce::Justification::centredLeft);
            phraseRects.clear();
            auto strip = c.removeFromTop (juce::jlimit (8.f, 34.f, c.getHeight()));
            const int np = (int) phrases.size();
            if (np > 0)
            {
                const float cw = strip.getWidth() / (float) np;
                for (int i = 0; i < np; ++i)
                {
                    auto cell = juce::Rectangle<float> (strip.getX() + cw * (float) i, strip.getY(), cw, strip.getHeight()).reduced (1.f, 0.f);
                    phraseRects.push_back (cell.toNearestInt());
                    g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRect (cell);
                    const float sh = phrases[(size_t) i].notes > 0 ? (float) phrases[(size_t) i].art / (float) phrases[(size_t) i].notes : 0.f;
                    g.setColour (juce::Colour (0xff3fd8ff).withAlpha (0.85f));
                    g.fillRect (cell.withTrimmedTop (cell.getHeight() * (1.f - juce::jlimit (0.f, 1.f, sh))));
                    if (cw >= 24.f)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.setFont (EnochLookAndFeel::fnt (9.5f));
                        g.drawText (juce::String (i + 1), cell.withHeight (12.f), juce::Justification::centred);
                    }
                }
            }
        }
        g.setFont (EnochLookAndFeel::fnt (11.f, "SemiBold", 0.05f));
        g.setColour (EnochLookAndFeel::dim());
        g.drawText ("THIS TAKE  (" + juce::String (total) + " notes)", b.removeFromTop (15.f), juce::Justification::centredLeft);
        std::vector<int> shown;
        for (int i = 0; i < F_Count; ++i) if (familyFor (inst, i) && i != F_Repeat) shown.push_back (i);
        shown.push_back (F_Count);              // plain
        const float rh = juce::jmin (15.f, b.getHeight() / (float) shown.size());
        g.setFont (EnochLookAndFeel::fnt (10.5f));
        for (int i : shown)
        {
            auto line = b.removeFromTop (rh);
            const float share = total > 0 ? (float) counts[(size_t) i] / (float) total : 0.f;
            auto nameR = line.removeFromLeft (78.f);
            auto pctR = line.removeFromRight (40.f);
            g.setColour (EnochLookAndFeel::dim());
            g.drawText (i == F_Count ? "PLAIN" : familyShort (i), nameR, juce::Justification::centredLeft);
            auto bar = line.reduced (2.f, rh * 0.22f);
            g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRect (bar);
            g.setColour (i == F_Count ? juce::Colour (0xff8a8f99) : juce::Colour (0xff3fd8ff).withAlpha (0.85f));
            g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.f, 1.f, share)));
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawText (juce::String (juce::roundToInt (share * 100.f)) + "%", pctR, juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (26);
        balance.setBounds (top.removeFromRight (200).reduced (2, 1));
        hint.setBounds (top);
        r.removeFromTop (4);
        usageRect = r.removeFromLeft (juce::jmin (260, r.getWidth() / 4));
        r.removeFromLeft (8);
        whyRect = r.removeFromLeft (juce::jmin (250, r.getWidth() / 3));
        r.removeFromLeft (12);
        // the rows the instrument uses: intensity, variation, then the families (two columns)
        std::vector<std::tuple<juce::Label*, juce::Slider*, juce::ComboBox*>> rows;
        rows.push_back ({ &intensityL, &intensity, nullptr });
        rows.push_back ({ &variationL, &variation, nullptr });
        for (auto* f : fams)
        {
            const bool on = enochsolo::artbrain::familyFor (inst, f->fam);
            f->name.setVisible (on); f->amt.setVisible (on); f->lock.setVisible (on);
            if (on) rows.push_back ({ &f->name, &f->amt, &f->lock });
        }
        const int perCol = (int) (rows.size() + 1) / 2;
        const int rh = juce::jmax (18, juce::jmin (28, r.getHeight() / juce::jmax (1, perCol)));
        auto left = r.removeFromLeft (r.getWidth() / 2).withTrimmedRight (8), right = r;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto& col = (int) i < perCol ? left : right;
            auto line = col.removeFromTop (rh).reduced (0, 2);
            auto [l, s, c] = rows[i];
            l->setBounds (line.removeFromLeft (76));
            if (c != nullptr) c->setBounds (line.removeFromRight (78).reduced (1, 0));
            s->setBounds (line.reduced (2, 0));
        }
    }

    juce::String getTooltip() override
    {
        const auto pos = getMouseXYRelative();
        for (size_t i = 0; i < phraseRects.size() && i < phrases.size(); ++i)
            if (phraseRects[i].contains (pos))
            {
                const auto& ph = phrases[i];
                juce::String t = "Phrase " + juce::String ((int) i + 1) + " (bar " + juce::String (ph.bar + 1) + "): " + juce::String (ph.art) + " of " + juce::String (ph.notes) + " notes articulated";
                if (ph.top.isNotEmpty()) t << "  -  mostly: " << ph.top;
                return t;
            }
        for (size_t i = 0; i < reasons.size() && i < 6; ++i)
            if (whyRect.contains (pos) && pos.y >= whyRect.getY() + 21 + 15 * (int) i && pos.y < whyRect.getY() + 36 + 15 * (int) i)
                return juce::String (enochsolo::artbrain::whyText (reasons[i].first));
        return {};
    }

    /** a few words for each reason (the full sentence is in the tooltip and on the note in the piano roll) */
    static juce::String whyShort (int w)
    {
        using namespace enochsolo::artbrain;
        switch (w)
        {
            case W_Generator: return "part of the style's lick";   case W_HammerUp: return "hammer-on: quick step up";
            case W_PullDown: return "pull-off: quick step down";     case W_SlurStep: return "legato: close steps";
            case W_SlurBow: return "slur: one bow / breath";         case W_SlideTarget: return "slide into a chord tone";
            case W_SlideShift: return "slide across a shift";        case W_SlideIntoPhrase: return "slide into the phrase";
            case W_SlideOut: return "slide out of an ending";        case W_GhostGroove: return "groove ghost note";
            case W_GhostPickup: return "ghosted pickup";              case W_DeadPunch: return "dead note for punch";
            case W_PalmPassage: return "palm-muted passage";          case W_VibLong: return "vibrato on a long note";
            case W_VibPeak: return "vibrato at the peak / end";       case W_BendPeak: return "bend to the peak";
            case W_HarmEnding: return "harmonic at an ending";        case W_SlapLow: return "slap: low root / fifth";
            case W_PopHigh: return "pop: octave / high note";         case W_ShortOffbeat: return "staccato off-beat";
            case W_PizzPassage: return "pizzicato passage";           case W_AccentChange: return "accent on a chord change";
            case W_Growl: return "growl at the climax";               case W_Shake: return "shake on a peak";
            case W_Subtone: return "subtone: soft and low";           case W_Tremolo: return "tremolo on a peak";
            case W_Scoop: return "scoop into the phrase";             case W_Fall: return "fall off the ending";
            case W_Doit: return "doit at a peak ending";              case W_Riff: return "riff repeats: same as before";
            case W_Breath: return "shortened for a breath";           case W_Forced: return "set to Always";
            default: return juce::String (whyText (w));
        }
    }

private:
    struct FamRow { int fam = 0; juce::Label name; juce::Slider amt; juce::ComboBox lock; };
    struct Phrase { int bar = 0, notes = 0, art = 0; juce::String top; };

    static const char* familyTip (int f)
    {
        using namespace enochsolo::artbrain;
        switch (f)
        {
            case F_Legato:   return "Hammer-ons up, pull-offs down and slurs, on small steps close together while the previous note still rings.";
            case F_Slide:    return "Slides into chord tones at chord changes, across position shifts, into phrases and out of exposed endings (scoops and falls on horns, glissando on violin).";
            case F_Ghost:    return "Ghost and dead notes, only where the groove needs them (funk, neo-soul, gospel, Afrobeats...), never on the main beats.";
            case F_Palm:     return "Palm-muted passages for driving, repeated rhythms in styles that mute.";
            case F_Vibrato:  return "Vibrato on notes long enough to hear it, most at the peak and the end of a phrase.";
            case F_Bend:     return "Bends up into a chord tone on peaks and long notes (guitar).";
            case F_Harmonic: return "Harmonics on long chord tones at endings, for colour. Used sparingly.";
            case F_Slap:     return "Slap (low roots and fifths) and pop (octaves, high notes) passages, in styles that slap.";
            case F_Short:    return "Staccato (and pizzicato passages on violin) on short off-beat notes followed by space.";
            case F_Accent:   return "Accents on the downbeats where the chord changes.";
            case F_Colour:   return "Growl and shake on climaxes, subtone on soft low notes (horns), tremolo (violin).";
            case F_Repeat:   return "Repetition keys for fast repeated notes, when the instrument's profile has one (no machine-gun retrigger). Always = every repeat within a beat.";
            default:         return "";
        }
    }

    void row (juce::Slider& s, juce::Label& l, const char* id, const char* name, const char* tip)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
        s.setTooltip (juce::String (tip) + "  (double-click = default)");
        if (auto* p = proc.apvts.getParameter (id))
        {
            satts.push_back (std::make_unique<juce::SliderParameterAttachment> (*p, s));
            s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        }
        s.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
        s.updateText();
        l.setText (name, juce::dontSendNotification);
        l.setFont (EnochLookAndFeel::fnt (10.5f, "SemiBold", 0.05f));
        l.setTooltip (tip);
        addAndMakeVisible (s); addAndMakeVisible (l);
    }

    void timerCallback() override
    {
        int now = enochsolo::I_Guitar;
        if (auto* v = proc.apvts.getRawParameterValue ("inst")) now = juce::jlimit (0, (int) enochsolo::I_Count - 1, juce::roundToInt (v->load()));
        if (juce::String (JucePlugin_Name).containsIgnoreCase ("bass")) now = juce::jmin ((int) enochsolo::I_Count - 1, 5);
        if (now != inst) { inst = now; resized(); }
        std::array<int, enochsolo::artbrain::F_Count + 1> c {};
        int t = 0;
        std::vector<std::pair<int, int>> rs;
        std::vector<Phrase> ps;
        const void* src = nullptr;
        if (auto snap = proc.getSnapshot())
        {
            c = enochsolo::artbrain::usage (snap->pattern.raw);
            for (auto& n : snap->pattern.raw) if (n.track == 0 && n.voice == 0 && ! n.mute) ++t;
            src = snap.get();
            if (src != lastSnap) summarise (snap->pattern, rs, ps);
        }
        if (src != lastSnap) { lastSnap = src; reasons = rs; phrases = ps; repaint (whyRect); }
        if (c != counts || t != total) { counts = c; total = t; repaint (usageRect); }
    }

    /** reasons this take (most common first), and each phrase (the same phrases as the piano roll's phrase strip) */
    static void summarise (const enochsolo::Pattern& pat, std::vector<std::pair<int, int>>& rs, std::vector<Phrase>& ps)
    {
        using namespace enochsolo::artbrain;
        auto plainWhy = [] (int w) { return w == W_None || w == W_Plain || w == W_PlainStrong; };
        std::map<int, int> cnt;
        const auto found = enochsolo::findPhrases (pat);
        std::vector<std::map<int, int>> per (found.size());
        for (auto& f : found) { Phrase p; p.bar = f.bar0; ps.push_back (p); }
        for (auto& n : pat.raw)
        {
            if (n.track != 0 || n.voice != 0 || n.mute) continue;
            if (! plainWhy (n.why)) ++cnt[n.why];
            for (size_t i = 0; i < found.size(); ++i)
                if (n.start >= found[i].r0 - 1e-6 && n.start < found[i].r1 - 1e-6)
                {
                    ++ps[i].notes;
                    if (familyOf (n.art) >= 0 || n.vib) ++ps[i].art;
                    if (! plainWhy (n.why)) ++per[i][n.why];
                    break;
                }
        }
        for (size_t i = 0; i < ps.size(); ++i)
        {
            int best = -1, bc = 0; for (auto& [w, k] : per[i]) if (k > bc) { bc = k; best = w; }
            if (best >= 0) ps[i].top = whyShort (best);
        }
        for (auto& [w, k] : cnt) rs.push_back ({ w, k });
        std::sort (rs.begin(), rs.end(), [] (auto& x, auto& y) { return x.second > y.second; });
        if (ps.size() > 32) ps.resize (32);
    }

    EnochSoloProcessor& proc;
    const void* lastSnap = nullptr;
    std::vector<std::pair<int, int>> reasons;
    std::vector<Phrase> phrases;
    std::vector<juce::Rectangle<int>> phraseRects;
    juce::Rectangle<int> whyRect;
    int inst = -1, total = 0;
    std::array<int, enochsolo::artbrain::F_Count + 1> counts {};
    juce::Rectangle<int> usageRect;
    juce::Slider intensity, variation; juce::Label intensityL, variationL, hint;
    juce::OwnedArray<FamRow> fams;
    juce::TextButton balance { "PROFESSIONAL BALANCE" };
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> satts;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> catts;
};
