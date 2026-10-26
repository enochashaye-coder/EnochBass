#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <set>
#include "PluginProcessor.h"

//==============================================================================
/** Look and feel with one colour theme per instrument (guitar = amber, sax = rose, violin = violet, piano = ice blue, trumpet = emerald). */
struct EnochLookAndFeel : public juce::LookAndFeel_V4
{
    struct Theme { juce::Colour accent, bg, panel, panel2, border; };

    static juce::Colour text()  { return juce::Colour (0xfff3ead9); }
    static juce::Colour dim()   { return juce::Colour (0xff9a8f7d); }
    static juce::Colour green() { return juce::Colour (0xff3fae6a); }
    static juce::Colour red()   { return juce::Colour (0xffe0533d); }

    static Theme makeTheme (int inst)
    {
        switch (inst)
        {
            case 1:  return { juce::Colour (0xfff0577a), juce::Colour (0xff170f12), juce::Colour (0xff22171b), juce::Colour (0xff2e2026), juce::Colour (0xff47303a) };   // sax: rose
            case 2:  return { juce::Colour (0xffb07cff), juce::Colour (0xff120f1a), juce::Colour (0xff1b1727), juce::Colour (0xff262036), juce::Colour (0xff3a3355) };   // violin: violet
            case 3:  return { juce::Colour (0xff4db8ff), juce::Colour (0xff0e141a), juce::Colour (0xff151e27), juce::Colour (0xff1d2a36), juce::Colour (0xff2f4356) };   // piano: ice blue
            case 4:  return { juce::Colour (0xff3ddc97), juce::Colour (0xff0e1713), juce::Colour (0xff15231d), juce::Colour (0xff1d3128), juce::Colour (0xff2e4a3e) };   // trumpet: emerald
            case 5:  return { juce::Colour (0xffff7a2f), juce::Colour (0xff130e0b), juce::Colour (0xff1e1511), juce::Colour (0xff2b1d16), juce::Colour (0xff45301f) };   // bass: deep orange
            default: return { juce::Colour (0xfff2b134), juce::Colour (0xff14110f), juce::Colour (0xff1f1a16), juce::Colour (0xff2a231d), juce::Colour (0xff3d342b) };   // guitar: amber
        }
    }
    static const Theme& of (const juce::Component& c)
    {
        if (auto* l = dynamic_cast<EnochLookAndFeel*> (&c.getLookAndFeel())) return l->theme;
        static const Theme d = makeTheme (5);
        return d;
    }

    Theme theme = makeTheme (5);

    EnochLookAndFeel() { applyColours(); }
    void setInstrument (int inst) { theme = makeTheme (inst); applyColours(); }

    void applyColours()
    {
        setColour (juce::ResizableWindow::backgroundColourId, theme.bg);
        setColour (juce::ComboBox::backgroundColourId, theme.panel2);
        setColour (juce::ComboBox::textColourId, text());
        setColour (juce::ComboBox::outlineColourId, theme.border);
        setColour (juce::ComboBox::arrowColourId, theme.accent);
        setColour (juce::PopupMenu::backgroundColourId, theme.panel);
        setColour (juce::PopupMenu::textColourId, text());
        setColour (juce::PopupMenu::headerTextColourId, theme.accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::TextButton::buttonColourId, theme.panel2);
        setColour (juce::TextButton::buttonOnColourId, theme.accent);
        setColour (juce::TextButton::textColourOffId, text());
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Slider::thumbColourId, theme.accent);
        setColour (juce::Slider::trackColourId, theme.accent);
        setColour (juce::Slider::rotarySliderFillColourId, theme.accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, theme.border);
        setColour (juce::Slider::backgroundColourId, theme.border);
        setColour (juce::Slider::textBoxTextColourId, text());
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, text());
        setColour (juce::ToggleButton::textColourId, text());
        setColour (juce::ToggleButton::tickColourId, theme.accent);
        setColour (juce::ScrollBar::thumbColourId, theme.accent.withAlpha (0.55f));
        setColour (juce::TooltipWindow::backgroundColourId, theme.panel2);
        setColour (juce::TooltipWindow::textColourId, text());
        setColour (juce::TextEditor::backgroundColourId, theme.panel2);
        setColour (juce::TextEditor::textColourId, text());
        setColour (juce::TextEditor::outlineColourId, theme.border);
        setColour (juce::TextEditor::focusedOutlineColourId, theme.accent);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto base = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
        if (! b.isEnabled()) base = base.withMultipliedAlpha (0.45f);
        if (over) base = base.brighter (0.12f);
        if (down) base = base.darker (0.15f);
        g.setColour (base); g.fillRoundedRectangle (r, 7.f);
        g.setColour (theme.border); g.drawRoundedRectangle (r, 7.f, 1.f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions ((float) juce::jmin (16, h * 6 / 10), juce::Font::bold)); }
};

//==============================================================================
/** Overview bar under the piano roll: drag the middle to scroll, drag either END to zoom in or out (left-right arrows show at the ends). */
class ZoomBar : public juce::Component
{
public:
    std::function<void (double start, double len)> onChange;
    std::function<void()> onFit;
    void setView (double total, double start, double len) { tot = std::max (1e-6, total); st = start; ln = std::max (1e-6, len); repaint(); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { mode = 0; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onFit) onFit(); }
    void mouseExit (const juce::MouseEvent&) override { hoverZone = 0; repaint(); }
private:
    int zoneAt (float x) const;                 // 1 = left end, 2 = right end, 3 = middle of the thumb, 0 = outside
    float xOf (double beat) const;
    double minLen() const { return std::min (tot, 2.0); }
    double tot = 1, st = 0, ln = 1;
    int mode = 0, hoverZone = 0;
    double o0 = 0, o1 = 0; float downX = 0;
};

//==============================================================================
/** Piano roll: chord cell above every bar (click to select bars + choose the chord), note tools (select / draw / erase / split / mute),
    zoom + scrollbars, and an editable lane below (velocity, pitch bend, modulation, aftertouch, expression, sustain pedal). */
class RollView : public juce::Component, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    enum class Tool { Select = 0, Draw, Erase, Split, Mute };

    explicit RollView (EnochSoloProcessor& p);
    ~RollView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    juce::MouseCursor getMouseCursor() override;

    // tools / lanes
    void setTool (Tool t);
    Tool getTool() const { return tool; }
    void setLane (int lane);                 // 0 velocity, 1 pitch bend, 2 modulation, 3 aftertouch, 4 expression, 5 sustain pedal, 6 hidden
    int getLane() const { return laneSel; }

    // view
    double getZoomH() const;  void setZoomH (double z01);     // 0 = whole solo, 1 = closest
    double getZoomV() const;  void setZoomV (double z01);     // 0 = every pitch, 1 = closest
    void zoomFit();
    void zoomAround (double factor, double anchorBeat);

    // bar selection (click the chord names)
    int barSelFirst() const { return barA < 0 ? -1 : std::min (barA, barB); }
    int barSelLast() const  { return barA < 0 ? -1 : std::max (barA, barB); }
    juce::String barSelText() const;
    void clearBarSelection();
    void rerollSelectedBars();
    void copyChord();
    void pasteChord();
    bool hasChordClipboard() const { return chordClip.has; }
    void applyBusyToSelection (float busy, uint64_t seed);
    float busyOfSelection() const;           // value shown on the slider (explicit value, or the global one)
    bool selectionHasExplicitBusy() const;

    // note commands (toolbar / menus / keys)
    void showEditMenu (juce::Component& target);
    void selectAll();
    void deleteSelected();
    void duplicateSelected();
    void quantizeSelected();
    void copySelected();
    void cutSelected();
    void pasteClipboard();
    void doUndo();
    void doRedo();

    std::function<void()> onBarSelection, onViewChanged, onToolChanged, onPhrasesChanged;
    void lockAllPhrases (bool lock) { proc.setAllPhrasesLocked (lock); repaint(); if (onPhrasesChanged) onPhrasesChanged(); }

private:
    struct Geo
    {
        juce::Rectangle<float> chord, ruler, phrase, notes, lane, ks, gutter, hbar, vbar;
        int lo = 48, hi = 84;
        double viewStart = 0, viewLen = 4, len = 4;
        float ppb = 1.f, rowH = 8.f;
    };
    enum class Drag { None, Move, Resize, Rubber, Velocity, Seek, BarSel, DrawLen, EraseNotes, MuteNotes, LaneDraw, LaneErase };
    struct ChordClip { bool has = false; int rootPc = 0; enochsolo::Quality q = enochsolo::Quality::Min7; };

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newStart) override;
    void updateScrollBars();
    void notifyView() { if (onViewChanged) onViewChanged(); }
    Geo geo() const;
    float xOf (const Geo&, double beat) const;
    double beatAt (const Geo&, float x) const;
    float yOf (const Geo&, int pitch) const;
    int pitchAt (const Geo&, float y) const;
    juce::Rectangle<float> noteRect (const Geo&, const enochsolo::Note&) const;
    int hitTest (const Geo&, juce::Point<float> p, bool& resizeZone) const;
    double gridBeats() const;
    double snapBeat (double b, bool floorIt = false) const;
    bool snapIsOn() const;
    int snapPitch (int pitch, double beat) const;
    double metricWeight (double beat) const;
    double totalBeats() const;
    void refresh (bool force = false);
    void recomputeRange();
    void clampView();
    void commit (std::vector<enochsolo::Note> v);
    int barAtX (const Geo&, float x) const;
    juce::Rectangle<int> cellRect (const Geo&, int bar) const;
    void notifyBars() { if (onBarSelection) onBarSelection(); }
    void eraseAt (const Geo&, juce::Point<float> p);
    void muteAt (const Geo&, juce::Point<float> p);
    void showChordMenu (int anchorBar, juce::Rectangle<int> cell);
    void showNoteMenu (juce::Point<int> screenPos, bool onNote, double beat, int pitch);
    void showLaneMenu (juce::Point<int> screenPos, double beat);
    void showPhraseMenu (int index);
    int phraseAtX (const Geo&, float x) const;
    void doOp (int id);
    void addEditItems (juce::PopupMenu& m, bool withSelectionItems);
    void addNoteAt (double beat, int pitch);
    void nudge (double dBeat, int dPitch);
    void setArtOnSelected (int art);
    void setVelOnSelected (int vel);
    void transposeSelected (int semis);
    void applyChordToSelection (int rootPc, enochsolo::Quality q);
    void setVelFromY (const Geo&, float x0, float x1, float y);
    void drawLane (juce::Graphics&, const Geo&, const EnochLookAndFeel::Theme&);
    int ctrlLane() const { return laneSel >= 1 && laneSel <= 5 ? laneSel - 1 : -1; }
    int valueAtY (const Geo&, float y, int lane) const;
    float yOfValue (const Geo&, int value, int lane) const;
    double ctrlStep (const Geo&) const;
    void drawShape (int shape, int lane, double t0, double t1);
    static juce::Colour colourFor (int art, const juce::Colour& accent);
    static const char* laneName (int lane);

    EnochSoloProcessor& proc;
    std::shared_ptr<const EnochSoloProcessor::Snapshot> snap;
    std::vector<enochsolo::Note> raw;
    std::set<int> selected;
    std::vector<enochsolo::Note> clipboard;
    ChordClip chordClip;
    Tool tool = Tool::Select;
    int laneSel = 0;
    int lastNumber = -1; double lastBeat = -2;
    double viewStart = 0, viewLen = 0;          // viewLen 0 = show everything
    int visRows = 36, topPitch = 90; bool vAuto = true;
    int hoverBar = -1, hoverPhrase = -1;
    std::vector<enochsolo::Phrase> phrases;
    juce::String hover;
    bool focusChords = false;

    int barA = -1, barB = -1, barDragStart = -1; bool openMenuOnUp = false;
    Drag drag = Drag::None;
    std::vector<enochsolo::Note> dragOrig;
    juce::Point<float> downPos; double downBeat = 0; int downPitch = 60;
    int anchorId = -1; bool duplicated = false; bool moved = false; bool muteTarget = true;
    juce::Rectangle<float> rubber;
    std::set<int> selBeforeRubber;
    double pasteAt = 0;
    int lastAuditioned = -1;
    bool overResize = false;
    float lastLaneX = 0;
    std::vector<enochsolo::CtrlPoint> stroke; double strokeT0 = 0, strokeT1 = 0; bool strokeLine = false; juce::Point<float> strokeStart;
    bool rightDownInLane = false;

    ZoomBar hBar;
    juce::ScrollBar vBar { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollView)
};

//==============================================================================
/** The "drag the MIDI into your track" pad. */
class DragPad : public juce::Component
{
public:
    explicit DragPad (EnochSoloProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { file = proc.writeMidiToTemp(); dragging = false; }
    void mouseDrag (const juce::MouseEvent& e) override;
private:
    EnochSoloProcessor& proc;
    juce::File file; bool dragging = false;
};

//==============================================================================
/** Invisible frame around the whole plugin: drag any edge or corner to resize (arrows show the direction). */
class EdgeResizer : public juce::Component
{
public:
    explicit EdgeResizer (juce::Component& t) : target (t) {}
    int minW = 760, minH = 560, maxW = 3400, maxH = 2400;
    bool hitTest (int x, int y) override { return zoneAt (x, y) != 0; }
    juce::MouseCursor getMouseCursor() override
    {
        const auto p = getMouseXYRelative();
        return cursorFor (dragging ? zone : zoneAt (p.x, p.y));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        zone = zoneAt ((int) e.position.x, (int) e.position.y); dragging = zone != 0;
        startW = target.getWidth(); startH = target.getHeight(); startScreen = e.getScreenPosition();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto d = e.getScreenPosition() - startScreen;
        int w = startW, h = startH;
        if (zone & 2) w = startW + d.x; else if (zone & 1) w = startW - d.x;
        if (zone & 8) h = startH + d.y; else if (zone & 4) h = startH - d.y;
        target.setSize (juce::jlimit (minW, maxW, w), juce::jlimit (minH, maxH, h));
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    // bits: 1 = left, 2 = right, 4 = top, 8 = bottom
    int zoneAt (int x, int y) const
    {
        const int w = getWidth(), h = getHeight(), t = 7, c = 15;
        if (x < c && y < c) return 1 | 4;
        if (x >= w - c && y < c) return 2 | 4;
        if (x < c && y >= h - c) return 1 | 8;
        if (x >= w - c && y >= h - c) return 2 | 8;
        int z = 0;
        if (x < t) z |= 1;
        if (x >= w - t) z |= 2;
        if (y < t) z |= 4;
        if (y >= h - t) z |= 8;
        return z;
    }
    static juce::MouseCursor cursorFor (int z)
    {
        using MC = juce::MouseCursor;
        switch (z)
        {
            case 1: case 2:  return MC (MC::LeftRightResizeCursor);
            case 4: case 8:  return MC (MC::UpDownResizeCursor);
            case 1 | 4: return MC (MC::TopLeftCornerResizeCursor);
            case 2 | 4: return MC (MC::TopRightCornerResizeCursor);
            case 1 | 8: return MC (MC::BottomLeftCornerResizeCursor);
            case 2 | 8: return MC (MC::BottomRightCornerResizeCursor);
            default:    return MC (MC::NormalCursor);
        }
    }
    juce::Component& target;
    int zone = 0, startW = 0, startH = 0; bool dragging = false;
    juce::Point<int> startScreen;
};

//==============================================================================
class EnochSoloEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit EnochSoloEditor (EnochSoloProcessor&);
    ~EnochSoloEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // drop an audio recording or a MIDI file anywhere on the plugin to use it as a style reference
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }

private:
    // every control lives inside 'body', which is scaled to fit the window (so the whole interface grows and shrinks)
    struct Body : juce::Component
    {
        std::function<void (juce::Graphics&)> paintFn;
        void paint (juce::Graphics& g) override { if (paintFn) paintFn (g); }
    };
    void addAndMakeVisible (juce::Component& c) { body.addAndMakeVisible (c); }
    void addChildComponent (juce::Component& c) { body.addChildComponent (c); }
    void paintBody (juce::Graphics&);
    void layoutBody();
    void loadRefFile (const juce::File& f);
    void chooseRefFile();
    void showSongMenu();
    void refillLibraries();
    void chooseSoundFont();
    void chooseBassSample();
    juce::TextButton smpLoadBtn { "Load bass one-shot..." }, smpClearBtn { "Remove" };
    juce::ToggleButton smpOnT { "Use bass sample" };
    juce::ComboBox smpRootBox;
    juce::Label smpLabel;
    juce::String smpShown;
    juce::Label sfLabel;
    void runSongFile (bool chords);
    void chooseTranscribeFile();
    void runTranscribe (const juce::File& f);
    void timerCallback() override;
    void setChoiceParam (const char* id, int index);
    void refreshArtists();
    void refreshKeySwitchText();
    void refreshBarPanel();
    void applyTheme (int inst);
    void saveMidi();
    void refreshMyStyles (const juce::String& select = juce::String());
    void askStyleName();
    void setBoolParam (const char* id, bool v);

    EnochSoloProcessor& proc;
    EnochLookAndFeel laf;
    juce::Image photo;
    Body body;

    juce::OwnedArray<juce::TextButton> instButtons;
    juce::ComboBox artist, key, scale, genre, timeSig, bars, mode, harmony, presetBox, gridBox, laneBox, exportBox, refTempoBox, refShiftBox, myStyleBox, startBarBox, voice2Box, claveBox, libBox;
    juce::TextEditor chordBox;
    juce::Label lArtist, lKey, lScale, lGenre, lTs, lBars, lMode, lIntensity, lBusy, lSpace, lRepeat, lOct, lHarmony, lPreset, lGrid, lZoom, lLane, lExport, lRef, lRefAmt, lBarBusy, lZoomT, lZoomP, lMyStyles, lStart, lVoice2, lClave, lGroove, songInfo, lLib, libInfo;
    juce::Slider intensity, busyG, space, repeat, octave, hTiming, hVel, hLength, hFeel, zoomH, zoomV, barBusy, refAmt, grooveAmt;
    juce::TextButton tryAnother { "TRY ANOTHER" }, back { "<" }, fwd { ">" }, humanize { "HUMANIZE" }, reHumanize { "Re-roll feel" },
                     play { "PLAY" }, saveBtn { "Save .mid" }, advBtn { "Advanced" },
                     undoBtn { "Undo" }, redoBtn { "Redo" }, editBtn { "Edit  v" }, quantizeBtn { "Quantize" }, deleteBtn { "Delete" },
                     zoomFitBtn { "Fit" }, resetChords { "Reset chords" }, ksToggle { "KEY-SWITCHES: ON" },
                     toolSelect { "Select" }, toolDraw { "Draw" }, toolErase { "Erase" }, toolSplit { "Split" }, toolMute { "Mute" },
                     refLoad { "Load..." }, refClear { "Clear" }, barAuto { "Auto" }, barReroll { "Re-roll bars" }, barCopy { "Copy chord" }, barPaste { "Paste chord" }, barClear { "Deselect" },
                     phraseBtn { "Lock all phrases" }, liveBtn { "LIVE MIDI OUT: OFF" }, saveStyleBtn { "Save as my style" }, delStyleBtn { "Delete" }, learnBtn { "LEARN FROM THIS BASS LINE" }, songBtn { "FROM YOUR SONG  v" },
                     sfLoadBtn { "Load SoundFont (.sf2)..." }, sfClearBtn { "Remove" };
    juce::ToggleButton playerOn { "Test player" }, midiOut { "Send MIDI to host" }, loopT { "Loop" }, hostSync { "Follow host transport" },
                       emitKS { "Key-switches" }, emitBends { "Pitch-bends" }, emitCC { "Expression CCs" }, realize { "Trills as notes" },
                       snapScale { "Snap scale" }, snapGrid { "Snap grid" }, chordPad { "Chord pad" }, showScaleT { "Show scale" }, sfOnT { "Use SoundFont" };
    juce::Slider ksBase, bendRange, volume;
    juce::Label lKs, lBend, lVol, ksText, status, hint, barInfo, barBusyText;

    RollView roll;
    DragPad drag;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> photoRect, cardRect, titleRect, barPanelRect, refRect;
    EdgeResizer edgeResizer { *this };
    bool dropHover = false, refBusy = false;
    juce::String refMsg, refShown, styleMsg, songMsg, songShown, sfShown;
    int shownLocks = -1;
    bool advanced = false;
    int shownInstrument = -1;
    int shownStyleKey = -1;
    bool updatingUi = false;
    uint64_t busySeed = 1;

    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAtt;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> comboAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnochSoloEditor)
};
