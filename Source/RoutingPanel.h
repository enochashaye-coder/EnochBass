#pragma once
#include "StepCombo.h"   // v3.4: routing dropdowns step with the arrows, keys and wheel
// v3.0 ROUTING drawer (identical in Enoch Solo and Enoch Bass): where every part plays, on which MIDI channel, with which mapping profile,
// plus the profile editor (articulation keys with Learn and Test, transpose, legato overlap, repetition key, program) and rig templates.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "PluginProcessor.h"
#include "EnochLook.h"
#include "FolderMemory.h"
#include <set>

class RoutingPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> paintPlate;
    std::function<void()> onEditorToggled;                   // v3.8: the window gives the profile editor more height
    bool isEditingProfile() const { return editing; }

    explicit RoutingPanel (EnochSoloProcessor& p) : proc (p)
    {
        title.setText ("ROUTING", juce::dontSendNotification);
        title.setFont (EnochLookAndFeel::fnt (13.f, "Bold", 0.08f));
        addAndMakeVisible (title);
        hint.setFont (EnochLookAndFeel::fnt (11.5f)); hint.setColour (juce::Label::textColourId, EnochLookAndFeel::dim());
        hint.setText ("Studio One: set each instrument track's Input to Enoch and the channel below. Dragged MIDI uses the same channels.", juce::dontSendNotification);
        hint.setMinimumHorizontalScale (0.7f);
        addAndMakeVisible (hint);
        for (auto* h : { &hPart, &hDest, &hCh, &hProf, &hProg, &hTr, &hPort })
        { h->setFont (EnochLookAndFeel::fnt (10.5f, "SemiBold", 0.06f)); h->setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addAndMakeVisible (*h); }
        hPart.setText ("PART", juce::dontSendNotification); hDest.setText ("PLAYS ON", juce::dontSendNotification); hCh.setText ("MIDI CH", juce::dontSendNotification);
        hProf.setText ("MAPPING PROFILE", juce::dontSendNotification); hProg.setText ("PROGRAM", juce::dontSendNotification); hTr.setText ("OCTAVE", juce::dontSendNotification);
        hPort.setText ("HARDWARE PORT", juce::dontSendNotification);

        const auto names = EnochSoloProcessor::partNames();
        for (int i = 0; i < EnochSoloProcessor::kParts; ++i)
        {
            auto* r = rows.add (new Row());
            r->name.setText (names[i], juce::dontSendNotification); r->name.setFont (EnochLookAndFeel::fnt (12.f, "SemiBold"));
            r->dest.addItemList ({ "Built-in sound", "External VST", "External VST + built-in", "Hardware MIDI port", "Off" }, 1);
            for (int c = 1; c <= 16; ++c) r->ch.addItem (juce::String (c), c);
            r->prog.addItem ("-", 1); for (int pgm = 0; pgm < 128; ++pgm) r->prog.addItem (juce::String (pgm + 1), pgm + 2);
            r->tr.addItemList ({ "-2 oct", "-1 oct", "as written", "+1 oct", "+2 oct" }, 1);
            r->dest.setTooltip ("Built-in: only the preview plays. External VST: the notes leave on the MIDI channel (the preview goes quiet). "
                                "Hardware: straight to a MIDI port. Off: the part is not sent and not exported.");
            r->ch.setTooltip ("MIDI channel of this part. Set the same channel on the instrument's track in your DAW (or in Kontakt).");
            r->prof.setTooltip ("What the receiving instrument expects: key-switches, legato by overlap, strings, repetition keys, range, velocity curve.");
            r->prog.setTooltip ("Program change sent when playback starts (for GM synths and hardware). '-' sends none.");
            r->tr.setTooltip ("Extra octave shift for this route (on top of the profile's own).");
            r->edit.setTooltip ("Edit this profile: articulation keys with Learn and Test, transpose, legato overlap, repetition key. Saved as a profile file.");
            r->test.setTooltip ("Play a short test on this part's channel (a C, then E and G) so you can check the routing in your DAW.");
            const int idx = i;
            for (juce::ComboBox* c : { &r->dest, &r->ch, &r->prof, &r->prog, &r->tr, &r->port }) c->onChange = [this, idx] { if (! updating) store (idx); };
            r->edit.onClick = [this, idx] { openEditor (idx); };
            r->test.onClick = [this, idx] { testRoute (idx); };
            for (juce::Component* c : { (juce::Component*) &r->name, (juce::Component*) &r->dest, (juce::Component*) &r->ch, (juce::Component*) &r->prof, (juce::Component*) &r->prog,
                                        (juce::Component*) &r->tr, (juce::Component*) &r->port, (juce::Component*) &r->edit, (juce::Component*) &r->test }) addAndMakeVisible (c);
        }
        saveRig.onClick = [this] { saveRigFile(); };
        loadRig.onClick = [this] { loadRigFile(); };
        saveRig.setTooltip ("Save the routing of every instrument as a rig file (e.g. 'My Studio One rig')");
        loadRig.setTooltip ("Load a rig file");
        addAndMakeVisible (saveRig); addAndMakeVisible (loadRig);
        quickBox.setTextWhenNothingSelected ("Quick setup...");
        quickBox.setTooltip ("One click for the usual rigs (EZkeys Full, EZbass + log drum, Ilya Efimov ...). Every row stays editable afterwards.");
        quickBox.onChange = [this] { if (! updating && quickBox.getSelectedId() > 0) { applyQuick (quickBox.getSelectedId()); } };
        addAndMakeVisible (quickBox);
        // v3.3 / v2.3: MIDI Delay for external instruments (parameter midiDelayMs, 0.1 ms steps)
        delayLbl.setText ("MIDI DELAY", juce::dontSendNotification); delayLbl.setFont (EnochLookAndFeel::fnt (11.f, "Bold", 0.06f));
        delayLbl.setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addAndMakeVisible (delayLbl);
        delaySlider.setRange (-200.0, 200.0, 0.1);    // v3.7: below 0 sends the MIDI early delaySlider.setSliderStyle (juce::Slider::LinearHorizontal);
        delaySlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        delayVal.setFont (EnochLookAndFeel::fnt (12.f, "SemiBold")); delayVal.setJustificationType (juce::Justification::centredRight);
        delayVal.setColour (juce::Label::textColourId, juce::Colours::white); addAndMakeVisible (delayVal);
        delaySlider.onValueChange = [this] { const double v = delaySlider.getValue(); delayVal.setText (v < 0.0 ? juce::String::formatted ("%.1f ms early", -v) : juce::String::formatted ("%.1f ms", v), juce::dontSendNotification); };
        delaySlider.setTooltip ("MIDI DELAY: moves the MIDI sent to your external instruments. Right of 0 = later. Left of 0 = EARLY, for an instrument the DAW plays late while you monitor it live (EZbass with Monitor on). Double-click = 0");
        delayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "midiDelayMs", delaySlider);
        addAndMakeVisible (delaySlider);
        // v3.3 / v2.3 Built-in Delay: holds the built-in sounds back to line them up with a late external instrument (parameter internalDelayMs)
        internalLbl.setText ("BUILT-IN DELAY", juce::dontSendNotification); internalLbl.setFont (EnochLookAndFeel::fnt (11.f, "Bold", 0.06f));
        internalLbl.setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addAndMakeVisible (internalLbl);
        internalSlider.setRange (0.0, 400.0, 0.1); internalSlider.setSliderStyle (juce::Slider::LinearHorizontal); internalSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        internalSlider.setTooltip ("Built-in Delay: holds the built-in sounds back this many ms, to line them up with an external instrument that plays late (e.g. EZbass). 0 = off. Steps of 0.1 ms.");
        internalVal.setFont (EnochLookAndFeel::fnt (12.f, "SemiBold")); internalVal.setJustificationType (juce::Justification::centredRight);
        internalVal.setColour (juce::Label::textColourId, juce::Colours::white); addAndMakeVisible (internalVal);
        internalSlider.onValueChange = [this] { internalVal.setText (juce::String::formatted ("%.1f ms", internalSlider.getValue()), juce::dontSendNotification); };
        internalAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "internalDelayMs", internalSlider);
        internalVal.setText (juce::String::formatted ("%.1f ms", internalSlider.getValue()), juce::dontSendNotification);
        addAndMakeVisible (internalSlider);
        delayVal.setText (juce::String::formatted ("%.1f ms", delaySlider.getValue()), juce::dontSendNotification);
        status.setFont (EnochLookAndFeel::fnt (11.5f)); status.setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addAndMakeVisible (status);

        // ---- profile editor page
        edTitle.setFont (EnochLookAndFeel::fnt (13.f, "Bold", 0.06f)); addChildComponent (edTitle);
        edName.setTextToShowWhenEmpty ("Profile name", EnochLookAndFeel::dim()); addChildComponent (edName);
        for (int t = -24; t <= 24; t += 12) edTr.addItem ((t > 0 ? "+" : "") + juce::String (t / 12) + " oct", t + 25);
        edTr.setTooltip ("Notes are sent this many octaves away (basses mapped an octave up need +1)"); addChildComponent (edTr);
        edOverlap.setRange (0, 120, 1); edOverlap.setTextValueSuffix (" ms"); edOverlap.setSliderStyle (juce::Slider::LinearHorizontal);
        edOverlap.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20); edOverlap.setTooltip ("Legato: how far a note overlaps the next legato note (libraries that detect legato by overlap)");
        addChildComponent (edOverlap);
        edRep.addItem ("No repetition key", 1); for (int n = 0; n < 128; ++n) edRep.addItem ("Repetition key " + noteName (n), n + 2);
        edRep.setTooltip ("Fast repeats of one note alternate with this key (no machine-gun retrigger)"); addChildComponent (edRep);
        edPre.setRange (0, 100, 1); edPre.setTextValueSuffix (" ms"); edPre.setSliderStyle (juce::Slider::LinearHorizontal); edPre.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        edPre.setTooltip ("How long before its note a key-switch goes down (never on the note's own tick). 20 ms suits most libraries."); addChildComponent (edPre);
        edOvLeg.setTooltip ("Tick when the instrument plays ANY two overlapping notes as legato (EZbass, Ilya Efimov with auto legato on). Enoch then lets only the notes it "
                            "means to join overlap; every other note ends a moment before the next one."); addChildComponent (edOvLeg);
        edDetach.addItem ("No 'no legato' key", 1); for (int n = 0; n < 128; ++n) edDetach.addItem ("No legato key " + noteName (n), n + 2);
        edDetach.setTooltip ("A key held while notes must sound together without legato (EZbass A-1 polyphony)"); addChildComponent (edDetach);
        lPre.setFont (EnochLookAndFeel::fnt (10.5f, "SemiBold", 0.06f)); lPre.setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); lPre.setText ("PRE-ROLL", juce::dontSendNotification); addChildComponent (lPre);
        for (auto* l : { &lTr, &lOv, &lMode }) { l->setFont (EnochLookAndFeel::fnt (10.5f, "SemiBold", 0.06f)); l->setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addChildComponent (*l); }
        lTr.setText ("TRANSPOSE", juce::dontSendNotification); lOv.setText ("LEGATO OVERLAP", juce::dontSendNotification); lMode.setText ("MODE KEYS", juce::dontSendNotification);
        // v3.8: mode keys (sent once when playback starts), custom keys, paste a key list, test every key, start a new profile
        edMode.setTextToShowWhenEmpty ("e.g. G0@120, F#0@40  (sent once at the start)", EnochLookAndFeel::dim());
        edMode.setTooltip ("Mode keys sent once when playback starts, to put the instrument in a known state: note@velocity, comma separated. "
                           "Notes as names (Kontakt numbering, C3 = 60: G0, F#0, D#1) or numbers (31@120). On/off mode keys usually take 101-127 = on, 1-100 = off.");
        addChildComponent (edMode);
        addKeyBtn.setTooltip ("Add one of the instrument's own keys with its own name (Pop, Down stroke, Set fret position 3...). It appears in the piano roll's Articulation menu, so you can place it on notes by hand.");
        pasteBtn.setTooltip ("Paste the instrument's key-switch list (one key per line, as the manual or Kontakt shows it: 'Slide up 1 slow/fast (velo)  G#0 (32)'). Enoch fills in the keys it recognises and adds the rest as custom keys.");
        testAllBtn.setTooltip ("Play every key of this profile in turn on the part's MIDI channel, each followed by a test note, so you can hear that each one switches the right sound.");
        newBtn.setTooltip ("Start a new, empty profile for this instrument (any instrument, popular or not)");
        addKeyBtn.onClick = [this] { addCustomRow ("New key", -1, 100, false); layoutArts(); edStatus.setText ("Custom key added: name it, set its key (or press Learn), then Save profile.", juce::dontSendNotification); };
        pasteBtn.onClick = [this] { showPaste (true); };
        testAllBtn.onClick = [this] { startTestAll(); };
        newBtn.onClick = [this] { newProfile(); };
        for (auto* b : { &addKeyBtn, &pasteBtn, &testAllBtn, &newBtn }) addChildComponent (*b);
        artView.setViewedComponent (&artHolder, false); artView.setScrollBarsShown (true, false); addChildComponent (artView);
        for (auto a : editArts())
        {
            auto* r = arts.add (new ArtRow());
            r->art = a;
            r->name.setText (enochsolo::artName (a), juce::dontSendNotification); r->name.setFont (EnochLookAndFeel::fnt (11.5f));
            wireRow (*r);
        }
        edSave.onClick = [this] { saveEditor(); };
        edBack.onClick = [this] { stopTestAll(); showEditor (false); };
        edSave.setTooltip ("Save as a profile file (in the Libraries folder). It appears in every MAPPING PROFILE list for this instrument.");
        addChildComponent (edSave); addChildComponent (edBack);
        edStatus.setFont (EnochLookAndFeel::fnt (11.5f)); edStatus.setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addChildComponent (edStatus);
        // paste page
        pasteBox.setMultiLine (true, false); pasteBox.setReturnKeyStartsNewLine (true);
        pasteBox.setTextToShowWhenEmpty ("Paste or type the key list here, one key per line, e.g.\n  Select string E6   A-1 (21)\n  Slide up 1 slow/fast (velo)   G#0 (32)\n  Mute / X-Note (velo)   B0 (35)", EnochLookAndFeel::dim());
        addChildComponent (pasteBox);
        pasteOk.onClick = [this] { importList (pasteBox.getText()); showPaste (false); };
        pasteCancel.onClick = [this] { showPaste (false); };
        addChildComponent (pasteOk); addChildComponent (pasteCancel);
        refresh();
        startTimerHz (8);
    }
    ~RoutingPanel() override { stopTimer(); ++testToken; flushPending (true); }
    /** picks a quick setup by its menu number (also used by the tests) */
    void chooseQuick (int id) { fillQuick (proc.previewSlot()); applyQuick (id); }
    juce::StringArray quickNames() { fillQuick (proc.previewSlot()); juce::StringArray a; for (auto& q : quicks) a.add (q.label); return a; }
    // v3.8 hooks used by the automatic tests (they drive the editor exactly as the buttons do)
    juce::String testImport (const juce::String& text, bool fresh)
    {
        openEditor (0); if (fresh) newProfile();
        importList (text);
        juce::String out;
        for (auto* a : arts) if ((a->shown || a->custom) && a->key.getSelectedId() > 1)
            out << a->label() << "=" << (a->key.getSelectedId() - 2) << "@" << (int) a->vel.getValue() << (a->hold.getToggleState() ? "h" : "") << "; ";
        return out;
    }
    int testAllCount() { startTestAll(); return (int) testQueue.size(); }
    juce::String editorStatus() const { return edStatus.getText(); }
    bool testSave (const juce::String& name, const juce::String& modeKeys) { edName.setText (name, false); edMode.setText (modeKeys, false); saveEditor(); return ! editing; }
    juce::String routeStatus() const { return status.getText(); }

    void refresh()
    {
        updating = true;
        const int slot = proc.previewSlot();
        fillQuick (slot);
        const int inst = juce::jmin (slot, (int) enochsolo::I_Count - 1);
        auto ports = juce::MidiOutput::getAvailableDevices();
        for (int i = 0; i < rows.size(); ++i)
        {
            auto* r = rows[i];
            const auto rt = proc.getRoute (slot, i);
            r->dest.setSelectedId (rt.dest + 1, juce::dontSendNotification);
            r->ch.setSelectedId (rt.channel, juce::dontSendNotification);
            r->prof.clear (juce::dontSendNotification);
            r->prof.addItem (i == 0 ? "Instrument of choice (main panel)" : "Plain notes", 1);
            const auto& ll = proc.getLibraries (inst);
            for (size_t k = 1; k < ll.size(); ++k) r->prof.addItem (juce::String (ll[k]->name), (int) k + 2);
            int sel = 1;
            for (size_t k = 1; k < ll.size(); ++k) if (juce::String (ll[k]->name) == rt.profile) sel = (int) k + 2;
            r->prof.setSelectedId (sel, juce::dontSendNotification);
            r->prog.setSelectedId (rt.program < 0 ? 1 : rt.program + 2, juce::dontSendNotification);
            r->tr.setSelectedId (juce::jlimit (1, 5, rt.transpose / 12 + 3), juce::dontSendNotification);
            r->port.clear (juce::dontSendNotification);
            r->port.addItem ("(choose a port)", 1);
            for (int k = 0; k < ports.size(); ++k) r->port.addItem (ports[k].name, k + 2);
            int ps = 1; for (int k = 0; k < ports.size(); ++k) if (ports[k].identifier == rt.port) ps = k + 2;
            r->port.setSelectedId (ps, juce::dontSendNotification);
            r->port.setEnabled (rt.dest == EnochSoloProcessor::RD_Hardware);
            portIds = ports;
        }
        updating = false;
        shownSlot = slot;
        status.setText (proc.anyRouted (slot) ? "Routing is on for " + instLabel (slot) + "." : "All parts play on the built-in sound (as before). Choose External VST to drive your own instrument.", juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.f);
        if (paintPlate) paintPlate (g, b);
        else { g.setColour (juce::Colour (0xff1a1f29)); g.fillRoundedRectangle (b, 7.f); }
        g.setColour (EnochLookAndFeel::secExport().withAlpha (0.65f));
        g.fillRect (b.reduced (18.f, 0.f).withY (b.getY() + 44.f).withHeight (1.f));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 10);
        auto head = r.removeFromTop (30);
        title.setBounds (head.removeFromLeft (110));
        edTitle.setBounds (head.removeFromLeft (360));
        loadRig.setBounds (head.removeFromRight (96).reduced (2, 2));
        saveRig.setBounds (head.removeFromRight (96).reduced (2, 2));
        quickBox.setBounds (head.removeFromRight (230).reduced (4, 2));
        edBack.setBounds (loadRig.getBounds()); edSave.setBounds (saveRig.getBounds().withX (saveRig.getX() - 30).withWidth (124));
        hint.setBounds (juce::Rectangle<int> (title.getRight() + 4, head.getY(), juce::jmax (0, quickBox.getX() - title.getRight() - 12), head.getHeight()));
        r.removeFromTop (10);
        auto statusRow = r.removeFromBottom (24);
        auto delayArea = statusRow.removeFromRight (700);
        auto internalArea = delayArea.removeFromRight (340);
        delayLbl.setBounds (delayArea.removeFromLeft (100).reduced (0, 3));
        delayVal.setBounds (delayArea.removeFromRight (72).reduced (0, 3));
        delaySlider.setBounds (delayArea.reduced (2, 2));
        internalLbl.setBounds (internalArea.removeFromLeft (110).reduced (0, 3));
        internalVal.setBounds (internalArea.removeFromRight (72).reduced (0, 3));
        internalSlider.setBounds (internalArea.reduced (2, 2));
        status.setBounds (statusRow.reduced (0, 3)); edStatus.setBounds (status.getBounds());
        // routing page
        {
            auto rr = r;
            auto hdr = rr.removeFromTop (16);
            const int w[] = { 200, 160, 64, 290, 74, 92, 170 };
            juce::Label* hs[] = { &hPart, &hDest, &hCh, &hProf, &hProg, &hTr, &hPort };
            const double k = std::min (1.0, (rr.getWidth() - 132) / 1050.0);
            auto cols = [&] (juce::Rectangle<int> line, std::function<void (int, juce::Rectangle<int>)> fn) { for (int c = 0; c < 7; ++c) fn (c, line.removeFromLeft ((int) (w[c] * k))); return line; };
            cols (hdr, [&] (int c, juce::Rectangle<int> b) { hs[c]->setBounds (b); });
            for (auto* row : rows)
            {
                auto line = rr.removeFromTop (34).reduced (0, 3);
                auto rest = cols (line, [&] (int c, juce::Rectangle<int> b)
                {
                    juce::Component* comp[] = { &row->name, &row->dest, &row->ch, &row->prof, &row->prog, &row->tr, &row->port };
                    comp[c]->setBounds (b.reduced (2, 0));
                });
                row->edit.setBounds (rest.removeFromLeft (62).reduced (2, 0));
                row->test.setBounds (rest.removeFromLeft (62).reduced (2, 0));
            }
        }
        // editor page
        {
            auto er = r;
            auto top = er.removeFromTop (28);
            edName.setBounds (top.removeFromLeft (300).reduced (2, 2));
            lTr.setBounds (top.removeFromLeft (80)); edTr.setBounds (top.removeFromLeft (100).reduced (2, 2));
            lOv.setBounds (top.removeFromLeft (110)); edOverlap.setBounds (top.removeFromLeft (220).reduced (2, 2));
            edRep.setBounds (top.removeFromLeft (220).reduced (2, 2));
            er.removeFromTop (4);
            auto row2 = er.removeFromTop (26);
            newBtn.setBounds (row2.removeFromRight (110).reduced (2, 1));
            testAllBtn.setBounds (row2.removeFromRight (84).reduced (2, 1));
            pasteBtn.setBounds (row2.removeFromRight (110).reduced (2, 1));
            addKeyBtn.setBounds (row2.removeFromRight (118).reduced (2, 1));
            lMode.setBounds (row2.removeFromLeft (80)); edMode.setBounds (row2.reduced (2, 1));
            er.removeFromTop (4);
            auto row3 = er.removeFromTop (26);                                         // v3.12 performance settings
            lPre.setBounds (row3.removeFromLeft (80)); edPre.setBounds (row3.removeFromLeft (220).reduced (2, 2));
            edOvLeg.setBounds (row3.removeFromLeft (250).reduced (6, 1)); edDetach.setBounds (row3.removeFromLeft (220).reduced (2, 1));
            er.removeFromTop (4);
            artView.setBounds (er);
            pasteBox.setBounds (er.withTrimmedBottom (30));
            auto pb = er.removeFromBottom (26);
            pasteCancel.setBounds (pb.removeFromRight (90).reduced (2, 1)); pasteOk.setBounds (pb.removeFromRight (150).reduced (2, 1));
            layoutArts();
        }
    }

private:
    struct Row { juce::Label name; StepCombo dest, ch, prof, prog, tr, port; juce::TextButton edit { "Edit" }, test { "Test" }; };
    struct ArtRow
    {
        int art = 0; bool shown = true, custom = false;
        juce::Label name; juce::TextEditor cname;           // Enoch's articulation name, or the custom key's own (editable) name
        juce::ComboBox key; juce::Slider vel; juce::ToggleButton hold; juce::TextButton learn { "Learn" }, test { "Test" }, remove { "x" };
        juce::ComboBox when;                                 // v3.12: before the note / while the note sounds / the key is the sound
        juce::String label() const { return custom ? (cname.getText().trim().isNotEmpty() ? cname.getText().trim() : juce::String ("Custom key")) : name.getText(); }
    };

    static juce::String noteName (int n)                                          // Kontakt / Toontrack numbering: middle C = C3 = 60
    {
        static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (nn[n % 12]) + juce::String (n / 12 - 2);
    }
    static juce::String instLabel (int slot) { return slot == 5 ? juce::String ("the bass") : juce::String (enochsolo::instName (slot)); }
    static std::vector<int> editArts()
    {
        using namespace enochsolo;
        std::vector<int> v;                                                          // a row for every articulation; openEditor shows the instrument's own
        for (int a = 0; a < (int) A_Count; ++a) v.push_back (a);
        return v;
    }

    void store (int part)
    {
        const int slot = proc.previewSlot();
        auto* r = rows[part];
        EnochSoloProcessor::PartRoute rt = proc.getRoute (slot, part);
        rt.dest = juce::jlimit (0, 4, r->dest.getSelectedId() - 1);
        rt.channel = juce::jlimit (1, 16, r->ch.getSelectedId());
        rt.profile = r->prof.getSelectedId() > 1 ? r->prof.getText() : juce::String();
        rt.program = r->prog.getSelectedId() > 1 ? r->prog.getSelectedId() - 2 : -1;
        rt.transpose = (r->tr.getSelectedId() - 3) * 12;
        const int pi = r->port.getSelectedId() - 2;
        rt.port = (pi >= 0 && pi < portIds.size()) ? portIds[pi].identifier : juce::String();
        proc.setRoute (slot, part, rt);
        refresh();
    }

    void testRoute (int part)
    {
        const auto rt = proc.getRoute (proc.previewSlot(), part);
        const int ch = rt.channel;
        std::vector<juce::MidiMessage> m;
        for (int n : { 60, 64, 67 }) m.push_back (juce::MidiMessage::noteOn (ch, n, (juce::uint8) 96));
        sendLater (m, 0);
        std::vector<juce::MidiMessage> off;
        for (int n : { 60, 64, 67 }) off.push_back (juce::MidiMessage::noteOff (ch, n));
        sendLater (off, 600);
        status.setText ("Test sent on MIDI ch " + juce::String (ch) + ": a C major chord.", juce::dontSendNotification);
    }
    // v3.8: timed test messages wait in this panel's own list, sent from its timer: nothing stays scheduled after the window closes
    // (a note-off still waiting is sent at once when the panel goes away, so no test note keeps sounding)
    struct Pending { juce::uint32 due = 0; std::vector<juce::MidiMessage> msgs; };
    std::vector<Pending> pending;
    void sendLater (std::vector<juce::MidiMessage> m, int ms)
    {
        if (ms <= 0) { proc.sendTest (m); return; }
        pending.push_back ({ juce::Time::getMillisecondCounter() + (juce::uint32) ms, std::move (m) });
        if (! isTimerRunning() || getTimerInterval() > 40) startTimer (25);
    }
    void flushPending (bool all)
    {
        const auto now = juce::Time::getMillisecondCounter();
        for (size_t i = 0; i < pending.size();)
            if (all || (juce::int32) (now - pending[i].due) >= 0) { proc.sendTest (pending[i].msgs); pending.erase (pending.begin() + (long) i); }
            else ++i;
    }

    // ---- editor
    int editPart = 0;
    ArtRow* armed = nullptr; uint32_t armCount = 0;
    void wireRow (ArtRow& r)
    {
        r.key.addItem ("-", 1); for (int n = 0; n < 128; ++n) r.key.addItem (noteName (n) + "  (" + juce::String (n) + ")", n + 2);
        r.vel.setRange (1, 127, 1); r.vel.setValue (100); r.vel.setSliderStyle (juce::Slider::LinearBar); r.vel.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 18);
        r.vel.setTooltip ("Key-switch velocity (Ilya Efimov: on/off keys 101-127 = on, 1-100 = off; 'A / B (velo)' keys: low = A, high = B)");
        r.hold.setButtonText ("hold"); r.hold.setTooltip ("Held from before the note until it ends (legato, slide, harmonic, slap keys). Several notes in a row with the same held key keep it down through the run.");
        r.when.addItem ("before note", 1); r.when.addItem ("during note", 2); r.when.addItem ("is the sound", 3); r.when.setSelectedId (1, juce::dontSendNotification);
        r.when.setTooltip ("When the key goes down. Before note: a pre-roll before the note (most keys). During note: pressed while the note is held, e.g. EZbass F0 / G0 "
                           "slide the held note down / up. Is the sound: the key plays the sound itself and no pitched note is sent, e.g. EZbass E0 ghost tap.");
        r.learn.setTooltip ("Press, then play the key-switch on your MIDI keyboard");
        r.test.setTooltip ("Send this key-switch and a test note on the part's channel");
        r.remove.setTooltip ("Remove this custom key");
        ArtRow* rp = &r;
        r.learn.onClick = [this, rp] { armed = rp; armCount = proc.inNoteCount.load(); for (auto* x : arts) x->learn.setToggleState (x == rp, juce::dontSendNotification); edStatus.setText ("Learn: play the key-switch for " + rp->label() + " on your keyboard", juce::dontSendNotification); };
        r.learn.setClickingTogglesState (false);
        r.test.onClick = [this, rp] { testArt (*rp); };
        r.remove.onClick = [this, rp] { juce::Component::SafePointer<RoutingPanel> self (this); juce::MessageManager::callAsync ([self, rp] { if (self != nullptr) self->removeRow (rp); }); };
        r.cname.setFont (EnochLookAndFeel::fnt (11.5f));
        for (juce::Component* c : { (juce::Component*) &r.name, (juce::Component*) &r.cname, (juce::Component*) &r.key, (juce::Component*) &r.vel, (juce::Component*) &r.hold,
                                    (juce::Component*) &r.when, (juce::Component*) &r.learn, (juce::Component*) &r.test, (juce::Component*) &r.remove })
            artHolder.addChildComponent (c);
    }
    ArtRow* addCustomRow (const juce::String& name, int note, int vel, bool hold, int artNo = -1)
    {
        std::set<int> used; for (auto* a : arts) if (a->custom) used.insert (a->art);
        if (artNo < 0) { artNo = enochsolo::kCustomArt; while (used.count (artNo)) ++artNo; }
        if (! enochsolo::isCustomArt (artNo) || used.count (artNo)) { edStatus.setText ("No room for more custom keys (" + juce::String (enochsolo::kMaxCustom) + " at most).", juce::dontSendNotification); return nullptr; }
        auto* r = arts.add (new ArtRow());
        r->art = artNo; r->custom = true; r->cname.setText (name, false);
        wireRow (*r);
        r->key.setSelectedId (note >= 0 ? note + 2 : 1, juce::dontSendNotification);
        r->vel.setValue (vel, juce::dontSendNotification); r->hold.setToggleState (hold, juce::dontSendNotification);
        return r;
    }
    void removeRow (ArtRow* rp)
    {
        if (armed == rp) armed = nullptr;
        stopTestAll();
        for (int i = 0; i < arts.size(); ++i) if (arts[i] == rp && rp->custom) { arts.remove (i); break; }
        layoutArts();
    }
    void clearCustomRows() { armed = nullptr; for (int i = arts.size(); --i >= 0;) if (arts[i]->custom) arts.remove (i); }
    void layoutArts()
    {
        juce::Array<ArtRow*> vis; for (auto* a : arts) if (a->shown || a->custom) vis.add (a);
        // Enoch's articulations first, then the instrument's own keys
        std::stable_sort (vis.begin(), vis.end(), [] (ArtRow* a, ArtRow* b) { return (int) a->custom < (int) b->custom; });
        const int w = juce::jmax (200, artView.getWidth() - artView.getScrollBarThickness() - 2);
        const int colW = w / 2, rowH = 26, perCol = (vis.size() + 1) / 2;
        artHolder.setSize (w, juce::jmax (rowH, perCol * rowH + 4));
        for (auto* a : arts)
            for (juce::Component* c : { (juce::Component*) &a->name, (juce::Component*) &a->cname, (juce::Component*) &a->key, (juce::Component*) &a->vel, (juce::Component*) &a->hold,
                                        (juce::Component*) &a->when, (juce::Component*) &a->learn, (juce::Component*) &a->test, (juce::Component*) &a->remove }) c->setVisible (false);
        for (int i = 0; i < vis.size(); ++i)
        {
            auto* a = vis[i];
            const int col = i < perCol ? 0 : 1, rowI = i < perCol ? i : i - perCol;
            auto line = juce::Rectangle<int> (col * colW, rowI * rowH, colW, rowH).reduced (4, 1);
            const double sc = juce::jlimit (0.7, 1.0, (colW - 8) / 552.0);
            auto take = [&] (int w) { return line.removeFromLeft ((int) (w * sc)); };
            if (a->custom) { a->cname.setBounds (take (116).reduced (1, 1)); a->remove.setBounds (line.removeFromRight (22).reduced (1, 1)); }
            else a->name.setBounds (take (116));
            a->key.setBounds (take (108).reduced (1, 0));
            a->vel.setBounds (take (54).reduced (1, 1));
            a->hold.setBounds (take (70));
            a->when.setBounds (take (100).reduced (1, 0));
            a->learn.setBounds (take (54).reduced (1, 0));
            a->test.setBounds (take (46).reduced (1, 0));
            if (editing && ! pasting)
                for (juce::Component* c : { (juce::Component*) (a->custom ? (juce::Component*) &a->cname : (juce::Component*) &a->name), (juce::Component*) &a->key, (juce::Component*) &a->vel,
                                            (juce::Component*) &a->hold, (juce::Component*) &a->when, (juce::Component*) &a->learn, (juce::Component*) &a->test })
                    c->setVisible (true);
            if (editing && ! pasting && a->custom) a->remove.setVisible (true);
        }
    }
    void showEditor (bool on)
    {
        editing = on; pasting = false;
        for (auto* r : rows) for (juce::Component* c : { (juce::Component*) &r->name, (juce::Component*) &r->dest, (juce::Component*) &r->ch, (juce::Component*) &r->prof, (juce::Component*) &r->prog,
                                                         (juce::Component*) &r->tr, (juce::Component*) &r->port, (juce::Component*) &r->edit, (juce::Component*) &r->test }) c->setVisible (! on);
        for (auto* h : { &hPart, &hDest, &hCh, &hProf, &hProg, &hTr, &hPort, &hint, &status }) h->setVisible (! on);
        saveRig.setVisible (! on); loadRig.setVisible (! on); quickBox.setVisible (! on);
        for (juce::Component* c : { (juce::Component*) &edTitle, (juce::Component*) &edName, (juce::Component*) &edTr, (juce::Component*) &edOverlap, (juce::Component*) &edRep,
                                    (juce::Component*) &lTr, (juce::Component*) &lOv, (juce::Component*) &lMode, (juce::Component*) &edMode, (juce::Component*) &edPre, (juce::Component*) &edOvLeg, (juce::Component*) &edDetach, (juce::Component*) &lPre, (juce::Component*) &addKeyBtn, (juce::Component*) &pasteBtn,
                                    (juce::Component*) &testAllBtn, (juce::Component*) &newBtn, (juce::Component*) &edSave, (juce::Component*) &edBack, (juce::Component*) &edStatus,
                                    (juce::Component*) &artView }) c->setVisible (on);
        for (juce::Component* c : { (juce::Component*) &pasteBox, (juce::Component*) &pasteOk, (juce::Component*) &pasteCancel }) c->setVisible (false);
        armed = nullptr;
        layoutArts();
        if (! on) { stopTestAll(); refresh(); }
        if (onEditorToggled) onEditorToggled();
    }
    void showPaste (bool on)
    {
        pasting = on;
        for (juce::Component* c : { (juce::Component*) &pasteBox, (juce::Component*) &pasteOk, (juce::Component*) &pasteCancel }) c->setVisible (on && editing);
        artView.setVisible (! on && editing);
        if (on) { pasteBox.clear(); pasteBox.grabKeyboardFocus(); edStatus.setText ("Paste the key list (from the manual, or typed from Kontakt's key-switch page), then Import.", juce::dontSendNotification); }
        layoutArts();
    }
    void loadIntoEditor (const enochsolo::Library& l, int inst, int part)
    {
        base = l;
        edName.setText (juce::String (base.name), false);
        edTr.setSelectedId (juce::jlimit (-24, 24, base.transpose) / 12 * 12 + 25, juce::dontSendNotification);
        edOverlap.setValue (base.overlapMs, juce::dontSendNotification);
        edRep.setSelectedId (base.repKey < 0 ? 1 : base.repKey + 2, juce::dontSendNotification);
        juce::String mk; for (size_t i = 0; i < base.init.size(); ++i) mk << (i ? ", " : "") << noteName (base.init[i].note) << "@" << base.init[i].vel;
        edMode.setText (mk, false);
        clearCustomRows();
        // only the articulations this instrument plays (the chord pad: piano's), plus any the profile already maps
        const auto& own = enochsolo::artList (part == 2 ? (int) enochsolo::I_Piano : inst);
        for (auto* a : arts)
        {
            bool mapped = false; for (auto& x : base.arts) if (x.art == a->art && ! x.press.empty()) mapped = true;
            a->shown = mapped || std::find (own.begin(), own.end(), a->art) != own.end();
            const enochsolo::LibArt* la = enochsolo::libFind (base, a->art);
            a->key.setSelectedId (la && ! la->press.empty() ? la->press[0].note + 2 : 1, juce::dontSendNotification);
            a->vel.setValue (la && ! la->press.empty() ? la->press[0].vel : 100, juce::dontSendNotification);
            a->hold.setToggleState (la && la->hold, juce::dontSendNotification);
            a->when.setSelectedId (la ? la->when + 1 : 1, juce::dontSendNotification);
        }
        for (auto& x : base.arts)
            if (enochsolo::isCustomArt (x.art))
                if (auto* cr = addCustomRow (juce::String (x.name.empty() ? std::string ("Custom key") : x.name), x.press.empty() ? -1 : x.press[0].note, x.press.empty() ? 100 : x.press[0].vel, x.hold, x.art))
                    cr->when.setSelectedId (x.when + 1, juce::dontSendNotification);
        edPre.setValue (base.preRollMs, juce::dontSendNotification);
        edOvLeg.setToggleState (base.overlapLegato, juce::dontSendNotification);
        edDetach.setSelectedId (base.detachKey < 0 ? 1 : base.detachKey + 2, juce::dontSendNotification);
        layoutArts();
    }
    void openEditor (int part)
    {
        editPart = part;
        const int slot = proc.previewSlot();
        const int inst = juce::jmin (slot, (int) enochsolo::I_Count - 1);
        const auto rt = proc.getRoute (slot, part);
        std::shared_ptr<const enochsolo::Library> lib = rt.profile.isNotEmpty() ? proc.libraryByName (inst, rt.profile) : (part == 0 ? proc.currentLibrary() : nullptr);
        enochsolo::Library l = lib ? *lib : enochsolo::Library();
        if (! lib) { l.inst = inst; l.name = "My " + std::string (slot == 5 ? "bass" : enochsolo::instName (inst)) + " profile"; l.verify = 2; }
        if (l.standard) { l = enochsolo::Library(); l.inst = inst; l.name = "My profile"; }
        if (lib && lib->verify < 2) l.name += " (mine)";
        edTitle.setText ("EDIT PROFILE  -  " + EnochSoloProcessor::partNames()[part], juce::dontSendNotification);
        showEditor (true);
        loadIntoEditor (l, inst, part);
        edStatus.setText ("Set the key of each articulation (or press Learn and play it). Test / Test all send on MIDI ch " + juce::String (rt.channel) + ".", juce::dontSendNotification);
    }
    void newProfile()
    {
        const int slot = proc.previewSlot();
        const int inst = juce::jmin (slot, (int) enochsolo::I_Count - 1);
        enochsolo::Library l; l.inst = inst; l.verify = 2;
        l.name = "My " + std::string (slot == 5 ? "bass" : enochsolo::instName (inst)) + " instrument";
        loadIntoEditor (l, inst, editPart);
        edName.grabKeyboardFocus();
        edStatus.setText ("New profile: name it, then set each key (Learn, or Paste list...). Add the instrument's other keys with + Custom key.", juce::dontSendNotification);
    }
    /** a note name in Kontakt numbering (C3 = 60: "G0", "F#0", "D#1", "A-1", "Bb2") or a number, -1 if it is neither */
    static int parseNote (juce::String t)
    {
        t = t.trim().removeCharacters ("()");
        if (t.isEmpty()) return -1;
        if (t.containsOnly ("0123456789")) { const int v = t.getIntValue(); return v >= 0 && v <= 127 ? v : -1; }
        static const char* names = "C D EF G A B";
        const juce::juce_wchar c0 = juce::CharacterFunctions::toUpperCase (t[0]);
        const int base = juce::String (names).indexOfChar (c0);
        if (base < 0 || c0 == ' ') return -1;
        int i = 1, pc = base;
        if (t[i] == '#') { ++pc; ++i; } else if (t[i] == 'b' && t.length() > 2) { --pc; ++i; }
        const auto oct = t.substring (i);
        if (oct.isEmpty() || ! oct.containsOnly ("-0123456789")) return -1;
        const int n = (oct.getIntValue() + 2) * 12 + pc;
        return n >= 0 && n <= 127 ? n : -1;
    }
    std::vector<enochsolo::KsPress> parseModeKeys (const juce::String& text, juce::String& bad) const
    {
        std::vector<enochsolo::KsPress> out;
        for (auto tok : juce::StringArray::fromTokens (text, ",;", ""))
        {
            tok = tok.trim(); if (tok.isEmpty()) continue;
            const int n = parseNote (tok.upToFirstOccurrenceOf ("@", false, false));
            const int v = tok.containsChar ('@') ? juce::jlimit (1, 127, tok.fromFirstOccurrenceOf ("@", false, false).getIntValue()) : 100;
            if (n < 0) { bad << (bad.isEmpty() ? "" : ", ") << tok; continue; }
            out.push_back ({ n, v });
        }
        return out;
    }
    /** v3.8: turn a pasted key-switch list into the profile: Enoch's articulations where the name says which, custom keys for the rest */
    void importList (const juce::String& text)
    {
        using namespace enochsolo;
        const bool bass = proc.previewSlot() == 5;
        auto rowFor = [&] (int art) -> ArtRow* { for (auto* a : arts) if (! a->custom && a->art == art) return a; return nullptr; };
        std::set<int> filled;
        int nArt = 0, nCustom = 0, skipped = 0;
        auto artOf = [&] (juce::String nm) -> int
        {
            nm = nm.toLowerCase();
            if (nm.contains ("select string") || nm.contains ("fret position") || nm.contains ("chord mode") || nm.contains ("open string") || nm.contains ("non vibrato")) return -1;
            struct K { const char* w; int a; };
            static const K ks[] = { { "hammer", A_HammerOn }, { "pull", A_PullOff }, { "gliss", A_Gliss }, { "slide", A_Slide }, { "flageolet", A_Harmonic }, { "harmonic", A_Harmonic },
                                    { "flag.", A_Harmonic }, { "x-note", A_Ghost }, { "dead", A_Ghost }, { "ghost", A_Ghost }, { "palm", A_PalmMute }, { "mute", A_PalmMute },
                                    { "staccato", A_Staccato }, { "legato", A_Legato }, { "vibrato", A_Vibrato }, { "trill", A_Trill }, { "tremolo", A_Tremolo }, { "pizz", A_Pizz },
                                    { "fall", A_Fall }, { "doit", A_Doit }, { "growl", A_Growl }, { "flutter", A_Flutter }, { "bend", A_Bend }, { "sustain", A_Sustain },
                                    { "marcato", A_Marcato }, { "accent", A_Marcato }, { "scoop", A_Scoop }, { "shake", A_Shake }, { "strum", A_Strum } };
            for (auto& k : ks) if (nm.contains (k.w)) return k.a;
            if (bass && nm.contains ("pop") && ! nm.contains ("polyphony")) { const int a = enochsolo::articulationFromName ("pop"); if (a >= 0) return a; }
            if (bass && nm.contains ("slap")) return enochsolo::articulationFromName ("slap");
            return -1;
        };
        auto momentary = [] (int a) { return a == A_Slide || a == A_HammerOn || a == A_PullOff || a == A_Harmonic || a == A_PalmMute || a == A_Ghost || a == A_Gliss || a == A_Fall || a == A_Doit || a == A_Scoop; };
        auto place = [&] (const juce::String& name, int note, int vel, bool forceCustom)
        {
            const int a = forceCustom ? -1 : artOf (name);
            ArtRow* r = a >= 0 ? rowFor (a) : nullptr;
            if (r != nullptr && ! filled.count (a))
            {
                filled.insert (a); r->shown = true;
                r->key.setSelectedId (note + 2, juce::dontSendNotification); r->vel.setValue (vel, juce::dontSendNotification); r->hold.setToggleState (momentary (a), juce::dontSendNotification);
                ++nArt; return;
            }
            if (addCustomRow (name, note, vel, false) != nullptr) ++nCustom;
        };
        for (auto line : juce::StringArray::fromLines (text))
        {
            line = line.trim();
            if (line.isEmpty()) continue;
            // the key: "(32)" in brackets, else the last word if it is a note name ("G#0")
            int note = -1; juce::String nameText = line;
            const int ob = line.lastIndexOfChar ('('), cb = line.lastIndexOfChar (')');
            if (ob >= 0 && cb > ob && line.substring (ob + 1, cb).trim().containsOnly ("0123456789"))
            {
                note = line.substring (ob + 1, cb).trim().getIntValue();
                nameText = line.substring (0, ob).trim();
                auto words = juce::StringArray::fromTokens (nameText, " \t", ""); words.removeEmptyStrings();
                if (! words.isEmpty() && parseNote (words[words.size() - 1]) >= 0) { words.remove (words.size() - 1); nameText = words.joinIntoString (" "); }
            }
            else
            {
                auto words = juce::StringArray::fromTokens (line, " \t", ""); words.removeEmptyStrings();
                if (! words.isEmpty()) { note = parseNote (words[words.size() - 1]); if (note >= 0) { words.remove (words.size() - 1); nameText = words.joinIntoString (" "); } }
            }
            if (note < 0 || note > 127) { ++skipped; continue; }
            const bool velo = nameText.containsIgnoreCase ("(velo)");
            nameText = nameText.replace ("(velo)", "", true).replace ("(VELO)", "", true).trim();
            if (nameText.isEmpty() || nameText.equalsIgnoreCase ("nothing")) { ++skipped; continue; }
            if (nameText.containsIgnoreCase ("on/off"))
            {
                // a mode key: velocity 101-127 switches it on, 1-100 off
                const auto nm = nameText.replace ("on/off", "", true).trim();
                if (nm.containsIgnoreCase ("legato") && rowFor (A_Legato) != nullptr && ! filled.count (A_Legato)) place ("legato", note, 120, false);
                else { if (addCustomRow (nm + " on", note, 120, false)) ++nCustom; }
                if (addCustomRow (nm + " off", note, 40, false)) ++nCustom;
                continue;
            }
            if (velo && nameText.containsChar ('/'))
            {
                // "A / B (velo)": low velocity = A, high velocity = B (e.g. Mute / X-Note, Pull off / Hammer on, Slide slow / fast)
                auto a = nameText.upToFirstOccurrenceOf ("/", false, false).trim(), b = nameText.fromFirstOccurrenceOf ("/", false, false).trim();
                const bool speed = b.containsIgnoreCase ("fast");
                if (speed) { const auto stem = a.upToLastOccurrenceOf (" ", false, false).trim(); place (a, note, 30, false); place (stem + " fast", note, 110, true); }
                else { place (a, note, 30, false); place (b, note, 110, false); }
                continue;
            }
            place (nameText, note, 100, false);
        }
        layoutArts();
        edStatus.setText ("Imported " + juce::String (nArt + nCustom) + " keys: " + juce::String (nArt) + " Enoch articulations, " + juce::String (nCustom) + " custom keys"
                          + (skipped > 0 ? " (" + juce::String (skipped) + (skipped == 1 ? " line" : " lines") + " skipped)" : juce::String()) + ". Check them with Test all, then Save profile.", juce::dontSendNotification);
    }
    /** a note the profile plays as a note (never one of its key-switches), in the middle of its range */
    int testNote() const
    {
        enochsolo::Library l = base;
        int lo = 0, hi = 127; enochsolo::libSafeRange (l, lo, hi);
        int n = juce::jlimit (lo, hi, (lo + hi) / 2);
        const auto keys = enochsolo::libKeyMask (l);
        for (int d = 0; d < 64 && keys[(size_t) n]; ++d) n = juce::jlimit (0, 127, n + (d % 2 ? -12 : 12));
        if (lo == 0 && hi == 127) n = 60;
        return n;
    }
    void testArt (ArtRow& a)
    {
        // v3.12: each key is tested the way its technique works, so what you hear is what Enoch's MIDI will do
        const int ch = proc.getRoute (proc.previewSlot(), editPart).channel;
        const int tn = testNote();
        const int key = a.key.getSelectedId() > 1 ? a.key.getSelectedId() - 2 : -1;
        const auto vel = (juce::uint8) a.vel.getValue();
        const int when = a.when.getSelectedId() - 1;
        auto on = [&] (int n, juce::uint8 v) { return juce::MidiMessage::noteOn (ch, n, v); };
        auto off = [&] (int n) { return juce::MidiMessage::noteOff (ch, n); };
        const bool joined = ! a.custom && (a.art == enochsolo::A_Legato || a.art == enochsolo::A_HammerOn || a.art == enochsolo::A_PullOff
                                           || a.art == enochsolo::A_Slide || a.art == enochsolo::A_Gliss);
        if (key >= 0 && when == enochsolo::KW_Replace) { sendLater ({ on (key, vel) }, 0); sendLater ({ off (key) }, 150); return; }       // the key is the sound
        if (key >= 0 && when == enochsolo::KW_During)                                                                                        // pressed while the note is held
        {
            sendLater ({ on (tn, 96) }, 0); sendLater ({ on (key, vel) }, 250); sendLater ({ off (key) }, 290); sendLater ({ off (tn) }, 800);
            return;
        }
        if (joined)                                                                                                                          // a note, then a joined note
        {
            const int up = a.art == enochsolo::A_PullOff ? -2 : (a.art == enochsolo::A_Slide || a.art == enochsolo::A_Gliss ? 5 : 2);
            sendLater ({ on (tn, 96) }, 0);
            if (key >= 0) sendLater ({ on (key, vel) }, 230);
            sendLater ({ on (tn + up, 96) }, 250); sendLater ({ off (tn) }, 275);
            std::vector<juce::MidiMessage> end { off (tn + up) }; if (key >= 0) end.push_back (off (key));
            sendLater (end, 800);
            return;
        }
        std::vector<juce::MidiMessage> o, f;
        if (key >= 0) o.push_back (on (key, vel));
        sendLater (o, 0); sendLater ({ on (tn, 96) }, 20);
        if (key >= 0) f.push_back (off (key));
        f.push_back (off (tn));
        sendLater (f, 520);
    }
    // ---- v3.8 Test all: every key of the profile in turn, a test note after each
    std::vector<ArtRow*> testQueue; int testPos = -1; juce::uint32 testToken = 0;
    void startTestAll()
    {
        stopTestAll();
        for (auto* a : arts) if ((a->shown || a->custom) && a->key.getSelectedId() > 1) testQueue.push_back (a);
        if (testQueue.empty()) { edStatus.setText ("Test all: no keys are set yet.", juce::dontSendNotification); return; }
        testPos = 0; ++testToken; testAllBtn.setButtonText ("Stop");
        testAllBtn.onClick = [this] { stopTestAll(); edStatus.setText ("Test all stopped.", juce::dontSendNotification); };
        testStep (testToken);
    }
    juce::uint32 nextTestAt = 0;
    void testStep (juce::uint32 token)
    {
        if (token != testToken || testPos < 0) return;
        if (testPos >= (int) testQueue.size()) { const int done = (int) testQueue.size(); stopTestAll(); edStatus.setText ("Test all: done (" + juce::String (done) + " keys).", juce::dontSendNotification); return; }
        auto* a = testQueue[(size_t) testPos];
        edStatus.setText ("Test all " + juce::String (testPos + 1) + " of " + juce::String ((int) testQueue.size()) + ":  " + a->label() + "  (" + noteName (a->key.getSelectedId() - 2) + ")", juce::dontSendNotification);
        testArt (*a);
        ++testPos;
        nextTestAt = juce::Time::getMillisecondCounter() + 800;          // the next key comes from timerCallback
        if (! isTimerRunning() || getTimerInterval() > 40) startTimer (25);
    }
    void stopTestAll()
    {
        ++testToken; testPos = -1; testQueue.clear();
        testAllBtn.setButtonText ("Test all");
        testAllBtn.onClick = [this] { startTestAll(); };
    }
    void saveEditor()
    {
        enochsolo::Library l = base;
        l.name = edName.getText().trim().toStdString();
        if (l.name.empty()) l.name = "My profile";
        l.verify = 2; l.standard = false;
        l.transpose = edTr.getSelectedId() - 25;
        l.overlapMs = edOverlap.getValue();
        l.preRollMs = edPre.getValue(); l.overlapLegato = edOvLeg.getToggleState(); l.detachKey = edDetach.getSelectedId() > 1 ? edDetach.getSelectedId() - 2 : -1;
        l.repKey = edRep.getSelectedId() > 1 ? edRep.getSelectedId() - 2 : -1;
        juce::String bad;
        l.init = parseModeKeys (edMode.getText(), bad);
        if (bad.isNotEmpty()) { edStatus.setText ("Mode keys: could not read '" + bad + "'. Use names like G0@120 or numbers like 31@120.", juce::dontSendNotification); return; }
        std::vector<enochsolo::LibArt> kept;
        for (auto& x : l.arts) { bool edited = false; for (auto* a : arts) if ((a->shown || a->custom) && a->art == x.art) edited = true; if (! edited && ! enochsolo::isCustomArt (x.art)) kept.push_back (x); }
        for (auto* a : arts)
        {
            if (! (a->shown || a->custom) || a->key.getSelectedId() <= 1) continue;
            enochsolo::LibArt la; la.art = a->art; la.hold = a->hold.getToggleState(); la.oneShot = la.hold;
            if (a->custom) la.name = a->label().toStdString();
            // v3.12: when the key goes down; timing details kept from the original profile, else sensible defaults
            la.when = juce::jlimit (0, 2, a->when.getSelectedId() - 1);
            if (la.when != enochsolo::KW_Before) la.oneShot = true;
            if (const auto* orig = enochsolo::libFind (base, a->art)) { la.atFrac = orig->atFrac; la.pressMs = orig->pressMs; }
            if (la.pressMs <= 0 && la.when == enochsolo::KW_During) la.pressMs = 40;
            if (la.pressMs <= 0 && la.when == enochsolo::KW_Replace) la.pressMs = 60;
            la.press.push_back ({ a->key.getSelectedId() - 2, (int) a->vel.getValue() });
            // keep the extra presses of the original (e.g. Ilya Efimov's X-note off key) after the first one
            if (! a->custom) if (const auto* orig = enochsolo::libFind (base, a->art)) for (size_t k = 1; k < orig->press.size(); ++k) la.press.push_back (orig->press[k]);
            kept.push_back (la);
        }
        l.arts = kept;
        l.noKeySwitches = l.arts.empty() && l.init.empty();
        // the played range keeps clear of every key (a played note on a key-switch would only switch, not sound)
        const auto problems = enochsolo::libCheck (l);
        juce::String msg;
        if (proc.saveProfile (l, msg))
        {
            const int slot = proc.previewSlot();
            auto rt = proc.getRoute (slot, editPart); rt.profile = juce::String (l.name); proc.setRoute (slot, editPart, rt);
            if (editPart == 0) { const auto& ll = proc.getLibraries (juce::jmin (slot, (int) enochsolo::I_Count - 1)); for (size_t k = 0; k < ll.size(); ++k) if (ll[k]->name == l.name) proc.setParamFromUI ("library", (float) k); }
            showEditor (false);
            status.setText (msg + "  -  now used by " + EnochSoloProcessor::partNames()[editPart] + "."
                            + (problems.empty() ? juce::String() : "  Note: some keys sit inside the played range; notes there are moved an octave so they still sound."), juce::dontSendNotification);
        }
        else edStatus.setText (msg, juce::dontSendNotification);
    }

    // ---- rig files
    std::unique_ptr<juce::FileChooser> chooser;
    void saveRigFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save routing rig", enochsolo::lastFolder ("rig", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)).getChildFile ("My rig.enochrig"), "*.enochrig");
        juce::Component::SafePointer<RoutingPanel> self (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [self] (const juce::FileChooser& fc)
        {
            if (self == nullptr || fc.getResult() == juce::File()) return;
            auto f = fc.getResult().withFileExtension ("enochrig");
            enochsolo::rememberFolder ("rig", f);
            if (auto x = self->proc.routesToTree().createXml()) { x->writeTo (f); self->status.setText ("Rig saved: " + f.getFileName(), juce::dontSendNotification); }
        });
    }
    void loadRigFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Load routing rig", enochsolo::lastFolder ("rig", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)), "*.enochrig");
        juce::Component::SafePointer<RoutingPanel> self (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self] (const juce::FileChooser& fc)
        {
            if (self == nullptr || fc.getResult() == juce::File()) return;
            enochsolo::rememberFolder ("rig", fc.getResult());
            if (auto x = juce::XmlDocument::parse (fc.getResult()))
            {
                self->proc.routesFromTree (juce::ValueTree::fromXml (*x));
                for (int p = 0; p < EnochSoloProcessor::kParts; ++p) self->proc.setRoute (self->proc.previewSlot(), p, self->proc.getRoute (self->proc.previewSlot(), p));
                self->refresh();
                self->status.setText ("Rig loaded: " + fc.getResult().getFileName(), juce::dontSendNotification);
            }
        });
    }

    void timerCallback() override
    {
        flushPending (false);
        if (testPos >= 0 && (juce::int32) (juce::Time::getMillisecondCounter() - nextTestAt) >= 0) testStep (testToken);
        if (pending.empty() && testPos < 0 && getTimerInterval() < 100) startTimerHz (8);
        if (! editing && proc.previewSlot() != shownSlot) refresh();
        if (editing && armed != nullptr && proc.inNoteCount.load() != armCount)
        {
            const int n = proc.lastInNote.load(), v = proc.lastInVel.load();
            armed->key.setSelectedId (n + 2, juce::dontSendNotification);
            armed->vel.setValue (juce::jlimit (1, 127, v), juce::dontSendNotification);
            edStatus.setText ("Learned " + noteName (n) + " (" + juce::String (n) + ", velocity " + juce::String (v) + ") for " + armed->label(), juce::dontSendNotification);
            armed->learn.setToggleState (false, juce::dontSendNotification);
            armed = nullptr;
        }
    }

    EnochSoloProcessor& proc;
    juce::Label title, hint, status, hPart, hDest, hCh, hProf, hProg, hTr, hPort;
    juce::OwnedArray<Row> rows;
    juce::TextButton saveRig { "Save rig..." }, loadRig { "Load rig..." };
    StepCombo quickBox;
    juce::Label delayLbl, delayVal; juce::Slider delaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAtt;
    juce::Label internalLbl, internalVal; juce::Slider internalSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> internalAtt;
    // ---- v3.2 quick setups: one click for the usual rigs (the rows stay editable)
    struct QuickPart { int dest; int ch; const char* profile; };
    struct Quick { juce::String label; QuickPart parts[3]; int mode; };     // mode: Output to set (-1 = leave it)
    std::vector<Quick> quicks;
    void fillQuick (int slot)
    {
        using R = EnochSoloProcessor::RouteDest;
        quicks.clear();
        const int B = R::RD_Builtin, E = R::RD_External, O = R::RD_Off;
        const auto cur = proc.getRoute (slot, 0);
        const int ch = cur.channel;
        quicks.push_back ({ "All parts on the built-in sound", { { B, ch, "" }, { B, proc.getRoute (slot, 1).channel, "" }, { B, proc.getRoute (slot, 2).channel, "" } }, -1 });
        if (slot == 5)
        {
            quicks.push_back ({ "EZbass (ch 4)", { { E, 4, "Toontrack EZbass" }, { B, 5, "" }, { B, 12, "" } }, -1 });
            quicks.push_back ({ "EZbass (ch 4) + log drum VST (ch 5)", { { E, 4, "Toontrack EZbass" }, { E, 5, "Log drum VST: DX10 style (one octave up)" }, { B, 12, "" } }, -1 });
            quicks.push_back ({ "Ilya Efimov Modern Bass (ch 4) + log drum (ch 5)", { { E, 4, "Ilya Efimov Modern Bass" }, { E, 5, "Log drum VST: DX10 style (one octave up)" }, { B, 12, "" } }, -1 });
            quicks.push_back ({ "Bass line to my VST (ch 4, instrument of choice)", { { E, 4, "" }, { B, 5, "" }, { B, 12, "" } }, -1 });
        }
        else
        {
            if (slot == 3)
            {
                quicks.push_back ({ "EZkeys 2 - Full: both hands on ch 1", { { E, 1, "Toontrack EZkeys 2" }, { E, 1, "Toontrack EZkeys 2" }, { O, 12, "" } }, 2 });
                quicks.push_back ({ "EZkeys 2 - Comping only (ch 1)", { { E, 1, "Toontrack EZkeys 2" }, { E, 1, "Toontrack EZkeys 2" }, { O, 12, "" } }, 1 });
                quicks.push_back ({ "EZkeys 2 - Fills (ch 1)", { { E, 1, "Toontrack EZkeys 2" }, { E, 1, "Toontrack EZkeys 2" }, { O, 12, "" } }, 3 });
                quicks.push_back ({ "Pianoteq 9 - Full: both hands on ch 1", { { E, 1, "Modartt Pianoteq 9" }, { E, 1, "Modartt Pianoteq 9" }, { O, 12, "" } }, 2 });
            }
            quicks.push_back ({ "Solo line to my VST (ch " + juce::String (ch) + ", instrument of choice)", { { E, ch, "" }, { B, 11, "" }, { B, 12, "" } }, -1 });
            quicks.push_back ({ "Solo + accompaniment to my VST (ch " + juce::String (ch) + ")", { { E, ch, "" }, { E, ch, "" }, { B, 12, "" } }, 2 });
        }
        quickBox.clear (juce::dontSendNotification);
        for (size_t i = 0; i < quicks.size(); ++i) quickBox.addItem (quicks[i].label, (int) i + 1);
    }
    void applyQuick (int id)
    {
        if (id < 1 || id > (int) quicks.size()) return;
        const auto& q = quicks[(size_t) (id - 1)];
        const int slot = proc.previewSlot();
        for (int part = 0; part < EnochSoloProcessor::kParts; ++part)
        {
            auto rt = proc.getRoute (slot, part);
            rt.dest = q.parts[part].dest; rt.channel = q.parts[part].ch; rt.profile = q.parts[part].profile;
            proc.setRoute (slot, part, rt);
        }
        if (q.mode >= 0) proc.setParamFromUI ("mode", (float) q.mode);
        refresh();
        status.setText ("Quick setup: " + q.label + ". Set each instrument track's Input to Enoch and the channel shown.", juce::dontSendNotification);
    }
    juce::Array<juce::MidiDeviceInfo> portIds;
    bool updating = false, editing = false; int shownSlot = -1;
    // editor
    enochsolo::Library base;
    juce::Label edTitle, edStatus, lTr, lOv, lMode;
    juce::TextEditor edName, edMode, pasteBox;
    juce::TextButton addKeyBtn { "+ Custom key" }, pasteBtn { "Paste list..." }, testAllBtn { "Test all" }, newBtn { "New profile" }, pasteOk { "Import these keys" }, pasteCancel { "Cancel" };
    juce::Viewport artView; juce::Component artHolder;
    bool pasting = false;
    juce::ComboBox edTr, edRep;
    juce::Slider edOverlap;
    juce::Slider edPre; juce::ToggleButton edOvLeg { "Overlapping notes play legato" }; juce::ComboBox edDetach; juce::Label lPre;   // v3.12
    juce::OwnedArray<ArtRow> arts;
    juce::TextButton edSave { "Save profile" }, edBack { "Back" };
};
