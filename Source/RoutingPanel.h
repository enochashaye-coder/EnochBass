#pragma once
#include "StepCombo.h"   // v3.4: routing dropdowns step with the arrows, keys and wheel
// v3.0 ROUTING drawer (identical in Enoch Solo and Enoch Bass): where every part plays, on which MIDI channel, with which mapping profile,
// plus the profile editor (articulation keys with Learn and Test, transpose, legato overlap, repetition key, program) and rig templates.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "PluginProcessor.h"
#include "EnochLook.h"
#include "FolderMemory.h"

class RoutingPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> paintPlate;

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
        delaySlider.setRange (0.0, 200.0, 0.1); delaySlider.setSliderStyle (juce::Slider::LinearHorizontal);
        delaySlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        delayVal.setFont (EnochLookAndFeel::fnt (12.f, "SemiBold")); delayVal.setJustificationType (juce::Justification::centredRight);
        delayVal.setColour (juce::Label::textColourId, juce::Colours::white); addAndMakeVisible (delayVal);
        delaySlider.onValueChange = [this] { delayVal.setText (juce::String::formatted ("%.1f ms", delaySlider.getValue()), juce::dontSendNotification); };
        delaySlider.setTooltip ("Sends your external instrument's MIDI this many ms late, to line it up with the built-in sounds. 0 = off. Steps of 0.1 ms; the 'MIDI Delay (samples)' parameter adds whole samples.");
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
        for (auto* l : { &lTr, &lOv }) { l->setFont (EnochLookAndFeel::fnt (10.5f, "SemiBold", 0.06f)); l->setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addChildComponent (*l); }
        lTr.setText ("TRANSPOSE", juce::dontSendNotification); lOv.setText ("LEGATO OVERLAP", juce::dontSendNotification);
        for (auto a : editArts())
        {
            auto* r = arts.add (new ArtRow());
            r->art = a;
            r->name.setText (enochsolo::artName (a), juce::dontSendNotification); r->name.setFont (EnochLookAndFeel::fnt (11.5f));
            r->key.addItem ("-", 1); for (int n = 0; n < 128; ++n) r->key.addItem (noteName (n) + "  (" + juce::String (n) + ")", n + 2);
            r->vel.setRange (1, 127, 1); r->vel.setValue (100); r->vel.setSliderStyle (juce::Slider::LinearBar); r->vel.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 18);
            r->vel.setTooltip ("Key-switch velocity (Ilya Efimov mode keys: 101-127 = on, 1-100 = off)");
            r->hold.setButtonText ("hold"); r->hold.setTooltip ("Held while the note starts (slide, harmonic, mute keys)");
            r->learn.setTooltip ("Press, then play the key-switch on your MIDI keyboard");
            r->test.setTooltip ("Send this key-switch and a test note on the part's channel");
            ArtRow* rp = r;
            r->learn.onClick = [this, rp] { armed = rp; armCount = proc.inNoteCount.load(); for (auto* x : arts) x->learn.setToggleState (x == rp, juce::dontSendNotification); edStatus.setText ("Learn: play the key-switch for " + juce::String (enochsolo::artName (rp->art)) + " on your keyboard", juce::dontSendNotification); };
            r->learn.setClickingTogglesState (false);
            r->test.onClick = [this, rp] { testArt (*rp); };
            for (juce::Component* c : { (juce::Component*) &r->name, (juce::Component*) &r->key, (juce::Component*) &r->vel, (juce::Component*) &r->hold, (juce::Component*) &r->learn, (juce::Component*) &r->test })
                addChildComponent (c);
        }
        edSave.onClick = [this] { saveEditor(); };
        edBack.onClick = [this] { showEditor (false); };
        edSave.setTooltip ("Save as a profile file (in the Libraries folder). It appears in every MAPPING PROFILE list for this instrument.");
        addChildComponent (edSave); addChildComponent (edBack);
        edStatus.setFont (EnochLookAndFeel::fnt (11.5f)); edStatus.setColour (juce::Label::textColourId, EnochLookAndFeel::dim()); addChildComponent (edStatus);
        refresh();
        startTimerHz (8);
    }
    ~RoutingPanel() override { stopTimer(); }
    /** picks a quick setup by its menu number (also used by the tests) */
    void chooseQuick (int id) { fillQuick (proc.previewSlot()); applyQuick (id); }
    juce::StringArray quickNames() { fillQuick (proc.previewSlot()); juce::StringArray a; for (auto& q : quicks) a.add (q.label); return a; }

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
            er.removeFromTop (6);
            juce::Array<ArtRow*> vis; for (auto* a : arts) if (a->shown) vis.add (a);
            const int perCol = (vis.size() + 1) / 2;
            auto left = er.removeFromLeft (er.getWidth() / 2), right = er;
            for (int i = 0; i < vis.size(); ++i)
            {
                auto& col = i < perCol ? left : right;
                auto line = col.removeFromTop (juce::jmin (26, col.getHeight() / juce::jmax (1, perCol - (i < perCol ? i : i - perCol)))).reduced (4, 1);
                auto* a = vis[i];
                a->name.setBounds (line.removeFromLeft (120));
                a->key.setBounds (line.removeFromLeft (130).reduced (1, 0));
                a->vel.setBounds (line.removeFromLeft (76).reduced (1, 1));
                a->hold.setBounds (line.removeFromLeft (56));
                a->learn.setBounds (line.removeFromLeft (58).reduced (1, 0));
                a->test.setBounds (line.removeFromLeft (52).reduced (1, 0));
            }
        }
    }

private:
    struct Row { juce::Label name; StepCombo dest, ch, prof, prog, tr, port; juce::TextButton edit { "Edit" }, test { "Test" }; };
    struct ArtRow { int art = 0; bool shown = true; juce::Label name; juce::ComboBox key; juce::Slider vel; juce::ToggleButton hold; juce::TextButton learn { "Learn" }, test { "Test" }; };

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
    void sendLater (std::vector<juce::MidiMessage> m, int ms)
    {
        juce::Component::SafePointer<RoutingPanel> self (this);
        juce::Timer::callAfterDelay (ms, [self, m] { if (self != nullptr) self->proc.sendTest (m); });
    }

    // ---- editor
    int editPart = 0;
    ArtRow* armed = nullptr; uint32_t armCount = 0;
    void showEditor (bool on)
    {
        editing = on;
        for (auto* r : rows) for (juce::Component* c : { (juce::Component*) &r->name, (juce::Component*) &r->dest, (juce::Component*) &r->ch, (juce::Component*) &r->prof, (juce::Component*) &r->prog,
                                                         (juce::Component*) &r->tr, (juce::Component*) &r->port, (juce::Component*) &r->edit, (juce::Component*) &r->test }) c->setVisible (! on);
        for (auto* h : { &hPart, &hDest, &hCh, &hProf, &hProg, &hTr, &hPort, &hint, &status }) h->setVisible (! on);
        saveRig.setVisible (! on); loadRig.setVisible (! on); quickBox.setVisible (! on);
        for (auto* a : arts) for (juce::Component* c : { (juce::Component*) &a->name, (juce::Component*) &a->key, (juce::Component*) &a->vel, (juce::Component*) &a->hold, (juce::Component*) &a->learn, (juce::Component*) &a->test }) c->setVisible (on && a->shown);
        for (juce::Component* c : { (juce::Component*) &edTitle, (juce::Component*) &edName, (juce::Component*) &edTr, (juce::Component*) &edOverlap, (juce::Component*) &edRep,
                                    (juce::Component*) &lTr, (juce::Component*) &lOv, (juce::Component*) &edSave, (juce::Component*) &edBack, (juce::Component*) &edStatus }) c->setVisible (on);
        armed = nullptr;
        if (! on) refresh();
    }
    void openEditor (int part)
    {
        editPart = part;
        const int slot = proc.previewSlot();
        const int inst = juce::jmin (slot, (int) enochsolo::I_Count - 1);
        const auto rt = proc.getRoute (slot, part);
        std::shared_ptr<const enochsolo::Library> lib = rt.profile.isNotEmpty() ? proc.libraryByName (inst, rt.profile) : (part == 0 ? proc.currentLibrary() : nullptr);
        base = lib ? *lib : enochsolo::Library();
        if (! lib) { base.inst = inst; base.name = "My " + std::string (slot == 5 ? "bass" : enochsolo::instName (inst)) + " profile"; base.verify = 2; }
        if (base.standard) { base = enochsolo::Library(); base.inst = inst; base.name = "My profile"; }
        edTitle.setText ("EDIT PROFILE  -  " + EnochSoloProcessor::partNames()[part], juce::dontSendNotification);
        edName.setText (juce::String (base.name) + (lib && lib->verify < 2 ? " (mine)" : ""), false);
        edTr.setSelectedId (juce::jlimit (-24, 24, base.transpose) / 12 * 12 + 25, juce::dontSendNotification);
        edOverlap.setValue (base.overlapMs, juce::dontSendNotification);
        edRep.setSelectedId (base.repKey < 0 ? 1 : base.repKey + 2, juce::dontSendNotification);
        {
            // only the articulations this instrument plays (the chord pad: piano's), plus any the profile already maps
            const auto& own = enochsolo::artList (part == 2 ? (int) enochsolo::I_Piano : inst);
            for (auto* a : arts)
            {
                bool mapped = false; for (auto& x : base.arts) if (x.art == a->art && ! x.press.empty()) mapped = true;
                a->shown = mapped || std::find (own.begin(), own.end(), a->art) != own.end();
            }
            resized();
        }
        for (auto* a : arts)
        {
            const enochsolo::LibArt* la = enochsolo::libFind (base, a->art);
            a->key.setSelectedId (la && ! la->press.empty() ? la->press[0].note + 2 : 1, juce::dontSendNotification);
            a->vel.setValue (la && ! la->press.empty() ? la->press[0].vel : 100, juce::dontSendNotification);
            a->hold.setToggleState (la && la->hold, juce::dontSendNotification);
        }
        edStatus.setText ("Set the key of each articulation (or press Learn and play it). Test sends it on MIDI ch " + juce::String (rt.channel) + ".", juce::dontSendNotification);
        showEditor (true);
    }
    void testArt (ArtRow& a)
    {
        const int ch = proc.getRoute (proc.previewSlot(), editPart).channel;
        std::vector<juce::MidiMessage> on, off;
        if (a.key.getSelectedId() > 1) on.push_back (juce::MidiMessage::noteOn (ch, a.key.getSelectedId() - 2, (juce::uint8) a.vel.getValue()));
        on.push_back (juce::MidiMessage::noteOn (ch, 52, (juce::uint8) 96));
        if (a.key.getSelectedId() > 1) off.push_back (juce::MidiMessage::noteOff (ch, a.key.getSelectedId() - 2));
        off.push_back (juce::MidiMessage::noteOff (ch, 52));
        sendLater (on, 0); sendLater (off, 500);
    }
    void saveEditor()
    {
        enochsolo::Library l = base;
        l.name = edName.getText().trim().toStdString();
        if (l.name.empty()) l.name = "My profile";
        l.verify = 2; l.standard = false;
        l.transpose = edTr.getSelectedId() - 25;
        l.overlapMs = edOverlap.getValue();
        l.repKey = edRep.getSelectedId() > 1 ? edRep.getSelectedId() - 2 : -1;
        std::vector<enochsolo::LibArt> kept;
        for (auto& x : l.arts) { bool edited = false; for (auto* a : arts) if (a->shown && a->art == x.art) edited = true; if (! edited) kept.push_back (x); }
        for (auto* a : arts)
        {
            if (! a->shown || a->key.getSelectedId() <= 1) continue;
            enochsolo::LibArt la; la.art = a->art; la.hold = a->hold.getToggleState(); la.oneShot = la.hold;
            la.press.push_back ({ a->key.getSelectedId() - 2, (int) a->vel.getValue() });
            // keep the extra presses of the original (e.g. Ilya Efimov's X-note off key) after the first one
            if (const auto* orig = enochsolo::libFind (base, a->art)) for (size_t k = 1; k < orig->press.size(); ++k) la.press.push_back (orig->press[k]);
            kept.push_back (la);
        }
        l.arts = kept;
        l.noKeySwitches = l.arts.empty();
        juce::String msg;
        if (proc.saveProfile (l, msg))
        {
            const int slot = proc.previewSlot();
            auto rt = proc.getRoute (slot, editPart); rt.profile = juce::String (l.name); proc.setRoute (slot, editPart, rt);
            if (editPart == 0) { const auto& ll = proc.getLibraries (juce::jmin (slot, (int) enochsolo::I_Count - 1)); for (size_t k = 0; k < ll.size(); ++k) if (ll[k]->name == l.name) proc.setParamFromUI ("library", (float) k); }
            showEditor (false);
            status.setText (msg + "  -  now used by " + EnochSoloProcessor::partNames()[editPart] + ".", juce::dontSendNotification);
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
        if (! editing && proc.previewSlot() != shownSlot) refresh();
        if (editing && armed != nullptr && proc.inNoteCount.load() != armCount)
        {
            const int n = proc.lastInNote.load(), v = proc.lastInVel.load();
            armed->key.setSelectedId (n + 2, juce::dontSendNotification);
            armed->vel.setValue (juce::jlimit (1, 127, v), juce::dontSendNotification);
            edStatus.setText ("Learned " + noteName (n) + " (" + juce::String (n) + ", velocity " + juce::String (v) + ") for " + juce::String (enochsolo::artName (armed->art)), juce::dontSendNotification);
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
    juce::Label edTitle, edStatus, lTr, lOv;
    juce::TextEditor edName;
    juce::ComboBox edTr, edRep;
    juce::Slider edOverlap;
    juce::OwnedArray<ArtRow> arts;
    juce::TextButton edSave { "Save profile" }, edBack { "Back" };
};
