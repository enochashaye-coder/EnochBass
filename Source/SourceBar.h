#pragma once
// v2.7  SOUND SOURCE row + NOW PLAYING display (main UI).
// Identical file in Enoch Solo and Enoch Bass. Every instrument keeps its own source, one-shot and SoundFont;
// the four buttons are bound to that instrument's "src<n>" parameter, so switching instruments shows that instrument's choice.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EnochLook.h"
#include "FolderMemory.h"

/** the green / amber lamp + text window that reads back what the audio engine is really playing */
class NowPlayingLCD : public juce::Component, public juce::SettableTooltipClient
{
public:
    void set (const juce::String& t, bool match, bool warn)
    {
        if (t == text && match == ok && warn == warning) return;
        text = t; ok = match; warning = warn; setTooltip (t); repaint();
    }
    void paint (juce::Graphics& g) override
    {
        using LF = EnochLookAndFeel;
        const auto& th = LF::of (*this);
        auto r = getLocalBounds().toFloat().reduced (1.f);
        LF::recess (g, r, 5.f, juce::Colour (0xff07090a));
        auto in = r.reduced (8.f, 0.f);
        // lamp: green = the engine plays exactly what the panel shows; amber = a fallback / warning; red = mismatch (switch still settling)
        const auto lampC = ! ok ? LF::red() : (warning ? juce::Colour (0xffffb020) : LF::green());
        auto lamp = in.removeFromLeft (12.f).withSizeKeepingCentre (9.f, 9.f);
        g.setColour (lampC.withAlpha (0.35f)); g.fillEllipse (lamp.expanded (3.f));
        g.setColour (lampC); g.fillEllipse (lamp);
        g.setColour (juce::Colours::white.withAlpha (0.5f)); g.fillEllipse (lamp.reduced (2.5f).translated (-1.f, -1.f));
        in.removeFromLeft (8.f);
        LF::ledText (g, "NOW PLAYING", in.removeFromLeft (92.f), LF::fnt (11.f, "Bold", 0.08f), juce::Justification::centredLeft, th.accent.withAlpha (0.7f));
        auto main = text, warn = juce::String();
        const int bang = text.indexOf ("!!");
        if (bang >= 0) { main = text.substring (0, bang).trimEnd(); warn = text.substring (bang + 2).trim(); }
        if (warn.isNotEmpty())
        {
            const float ww = juce::jmin (in.getWidth() * 0.55f, (float) juce::GlyphArrangement::getStringWidthInt (LF::fnt (11.5f, "SemiBold"), warn) + 12.f);
            LF::ledText (g, warn, in.removeFromRight (ww), LF::fnt (11.5f, "SemiBold"), juce::Justification::centredRight, juce::Colour (0xffffb020));
        }
        LF::ledText (g, main, in, LF::fnt (12.5f, "SemiBold"), juce::Justification::centredLeft, th.accent.interpolatedWith (juce::Colours::white, 0.25f));
    }
private:
    juce::String text { "starting..." }; bool ok = true, warning = false;
};

class SourceBar : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit SourceBar (EnochSoloProcessor& p) : proc (p)
    {
        title.setText ("SOUND SOURCE", juce::dontSendNotification);
        title.setFont (EnochLookAndFeel::fnt (12.f, "Bold", 0.06f));
        addAndMakeVisible (title);
        const char* names[] = { "BUILT-IN", "ONE-SHOT", "SOUNDFONT", "EXTERNAL" };
        const char* tips[] = {
            "Play the preview with Enoch's own modelled instrument (choose it in SOUND)",
            "Play the preview with a single-note recording you load for THIS instrument only (WAV / AIFF / FLAC / MP3)",
            "Play the preview through a SoundFont (.sf2) loaded for THIS instrument. If the SoundFont has no matching program, "
            "the built-in engine plays and NOW PLAYING shows a warning - it never plays the wrong instrument",
            "Silence the built-in player and send the notes out as MIDI to the instrument on your track (route the plugin's MIDI output in your DAW)" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = srcBtn.add (new juce::TextButton (names[i]));
            b->setTooltip (tips[i]);
            b->setClickingTogglesState (false);
            b->getProperties().set ("seg", true);
            b->onClick = [this, i] { setSource (i); };
            addAndMakeVisible (b);
        }
        loadBtn.setTooltip ("Load a one-shot or a SoundFont for the instrument that is selected now (or drop the file on this row)");
        loadBtn.onClick = [this] { loadMenu(); };
        clearBtn.setTooltip ("Remove the one-shot / SoundFont of the selected source from this instrument (the built-in engine takes over)");
        clearBtn.onClick = [this] { clearCurrent(); };
        addAndMakeVisible (loadBtn); addAndMakeVisible (clearBtn);
        presetBox.setTooltip ("Which program of the SoundFont plays this instrument. Auto = the General MIDI program for the instrument; "
                              "if the SoundFont does not have it, the built-in engine plays instead (amber lamp).");
        presetBox.onChange = [this]
        {
            if (updating) return;
            const int id = presetBox.getSelectedId();
            if (id > 0) proc.setSoundFontPreset (id - 2);      // id 1 = auto (-1), id 2.. = preset 0..
        };
        addChildComponent (presetBox);
        rootBox.setTooltip ("The note your one-shot was recorded at. Auto finds it; set it by hand if the preview sounds out of tune.");
        if (auto* rp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("smpRoot")))
        {
            rootBox.addItemList (rp->choices, 1);
            rootAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*rp, rootBox);
        }
        addChildComponent (rootBox);
        info.setFont (EnochLookAndFeel::fnt (12.f));
        info.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        info.setMinimumHorizontalScale (0.7f);
        addAndMakeVisible (info);
        addAndMakeVisible (lcd);
        startTimerHz (12);
        timerCallback();
    }
    ~SourceBar() override { stopTimer(); }

    /** row 1: source buttons, load / clear, preset or root, file info.  row 2: NOW PLAYING */
    void resized() override
    {
        auto r = getLocalBounds();
        auto row = r.removeFromTop (28);
        r.removeFromTop (4);
        lcd.setBounds (r.removeFromTop (26));
        title.setBounds (row.removeFromLeft (104));
        const int bw = juce::jmin (102, juce::jmax (70, row.getWidth() / 9));
        for (auto* b : srcBtn) b->setBounds (row.removeFromLeft (bw).reduced (2, 1));
        row.removeFromLeft (8);
        loadBtn.setBounds (row.removeFromLeft (76).reduced (2, 1));
        clearBtn.setBounds (row.removeFromLeft (62).reduced (2, 1));
        row.removeFromLeft (6);
        // the preset list (SoundFont) or root note (one-shot) only takes room when it is shown
        const int s = source();
        if (s == 2 && proc.hasSoundFont()) { presetBox.setBounds (row.removeFromLeft (juce::jmin (230, row.getWidth() / 2)).reduced (2, 1)); row.removeFromLeft (6); }
        if (s == 1 && proc.hasOneShot())   { rootBox.setBounds (row.removeFromLeft (110).reduced (2, 1)); row.removeFromLeft (6); }
        layoutKey = s * 4 + (proc.hasSoundFont() ? 2 : 0) + (proc.hasOneShot() ? 1 : 0);
        info.setBounds (row);
    }

    // drop a SoundFont or a one-shot recording on the row: it loads for the instrument that is selected now
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) { const auto e = juce::File (f).getFileExtension().toLowerCase(); if (e == ".sf2" || e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac" || e == ".mp3") return true; }
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dropHover = false; repaint();
        for (auto& path : files)
        {
            const juce::File f (path); const auto e = f.getFileExtension().toLowerCase(); juce::String m;
            if (e == ".sf2") { msg = proc.loadSoundFont (f, m) ? juce::String() : m; presetKey = {}; break; }
            if (e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac" || e == ".mp3") { msg = proc.loadOneShot (f, m) ? juce::String() : m; break; }
        }
        timerCallback();
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! dropHover) return;
        g.setColour (EnochLookAndFeel::of (*this).accent.withAlpha (0.18f)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.f);
        g.setColour (EnochLookAndFeel::of (*this).accent); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.f), 6.f, 2.f);
    }
    bool dropHover = false;

    int slot() const { return proc.previewSlot(); }
    int source() const
    {
        if (auto* p = proc.apvts.getRawParameterValue ("src" + juce::String (slot()))) return (int) std::lround (p->load());
        return 0;
    }

private:
    void setSource (int s)
    {
        if (auto* pr = proc.apvts.getParameter ("src" + juce::String (slot())))
        {
            pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->convertTo0to1 ((float) s)); pr->endChangeGesture();
        }
        // choosing ONE-SHOT / SOUNDFONT with nothing loaded yet goes straight to the file browser
        if (s == 1 && ! proc.hasOneShot()) chooseOneShot();
        if (s == 2 && ! proc.hasSoundFont()) chooseSoundFont();
        timerCallback();
    }
    void loadMenu()
    {
        const int s = source();
        if (s == 1) { chooseOneShot(); return; }
        if (s == 2) { chooseSoundFont(); return; }
        juce::PopupMenu m;
        m.addItem (1, "Load one-shot (single note recording)...");
        m.addItem (2, "Load SoundFont (.sf2)...");
        juce::Component::SafePointer<SourceBar> self (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadBtn), [self] (int r)
        {
            if (self == nullptr) return;
            if (r == 1) self->chooseOneShot();
            if (r == 2) self->chooseSoundFont();
        });
    }
    void clearCurrent()
    {
        const int s = source();
        if (s == 1 || (s != 2 && proc.hasOneShot() && ! proc.hasSoundFont())) proc.clearOneShot();
        else if (s == 2 || proc.hasSoundFont()) proc.clearSoundFont();
        timerCallback();
    }
    void chooseOneShot()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a single note recording for this instrument", enochsolo::lastFolder ("oneshot", juce::File::getSpecialLocation (juce::File::userMusicDirectory)), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        juce::Component::SafePointer<SourceBar> self (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File() || self == nullptr) return;
            enochsolo::rememberFolder ("oneshot", f);
            self->msg = "Loading " + f.getFileName() + " ...";
            juce::MessageManager::callAsync ([self, f]
            {
                if (self == nullptr) return;
                juce::String m;
                self->msg = self->proc.loadOneShot (f, m) ? juce::String() : m;
                self->timerCallback();
            });
        });
    }
    void chooseSoundFont()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a SoundFont (.sf2) for this instrument", enochsolo::lastFolder ("soundfont", juce::File::getSpecialLocation (juce::File::userMusicDirectory)), "*.sf2");
        juce::Component::SafePointer<SourceBar> self (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File() || self == nullptr) return;
            enochsolo::rememberFolder ("soundfont", f);
            self->msg = "Loading " + f.getFileName() + " ...";
            juce::MessageManager::callAsync ([self, f]
            {
                if (self == nullptr) return;
                juce::String m;
                self->msg = self->proc.loadSoundFont (f, m) ? juce::String() : m;
                self->presetKey = {};
                self->timerCallback();
            });
        });
    }

    void timerCallback() override
    {
        const int s = source();
        if (s * 4 + (proc.hasSoundFont() ? 2 : 0) + (proc.hasOneShot() ? 1 : 0) != layoutKey) resized();
        for (int i = 0; i < srcBtn.size(); ++i) srcBtn[i]->setToggleState (i == s, juce::dontSendNotification);
        const bool sfMode = s == 2, osMode = s == 1;
        presetBox.setVisible (sfMode && proc.hasSoundFont());
        rootBox.setVisible (osMode && proc.hasOneShot());
        clearBtn.setEnabled (proc.hasOneShot() || proc.hasSoundFont());
        if (sfMode && proc.hasSoundFont())
        {
            const auto names = proc.soundFontPresets();
            const juce::String key = juce::String (slot()) + "|" + proc.soundFontName() + "|" + juce::String (names.size());
            if (key != presetKey)
            {
                presetKey = key;
                updating = true;
                presetBox.clear (juce::dontSendNotification);
                presetBox.addItem ("Auto (General MIDI program)", 1);
                if (names.size() > 0) presetBox.addSectionHeading ("Presets in this SoundFont");
                for (int i = 0; i < names.size(); ++i) presetBox.addItem (names[i], i + 2);
                updating = false;
            }
            const int want = proc.soundFontPreset() + 2;
            if (presetBox.getSelectedId() != want) { updating = true; presetBox.setSelectedId (want, juce::dontSendNotification); updating = false; }
        }
        juce::String t = msg;
        if (t.isEmpty())
        {
            const juce::String os = proc.hasOneShot() ? "One-shot: " + proc.oneShotInfo() : juce::String ("no one-shot");
            const juce::String sf = proc.hasSoundFont() ? "SoundFont: " + proc.soundFontName() : juce::String ("no SoundFont");
            t = osMode ? os : sfMode ? sf : s == 3 ? juce::String ("Notes go out as MIDI to your track")
                                                   : "This instrument: " + os + "  |  " + sf;
        }
        if (t != info.getText()) { info.setText (t, juce::dontSendNotification); info.setTooltip (t); }
        bool match = true, warn = false;
        const auto np = proc.nowPlayingText (match, warn);
        lcd.set (np, match, warn);
    }

    EnochSoloProcessor& proc;
    juce::Label title, info;
    juce::OwnedArray<juce::TextButton> srcBtn;
    juce::TextButton loadBtn { "Load..." }, clearBtn { "Clear" };
    juce::ComboBox presetBox, rootBox;
    std::unique_ptr<juce::ComboBoxParameterAttachment> rootAtt;
    NowPlayingLCD lcd;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String msg, presetKey;
    bool updating = false;
    int layoutKey = -1;
};
