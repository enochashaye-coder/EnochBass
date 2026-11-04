#pragma once
#include "StepCombo.h"
#include "EnochLog.h"   // v3.6: open / close / removal log
#include "OutputPanel.h"  // v3.7
#include "TapPanel.h"     // v3.7   // v3.4: dropdowns that step with the arrows, keys and wheel
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <set>
#include "PluginProcessor.h"

#define ENOCH_DEFAULT_THEME 5
#include "EnochLook.h"
#include "SourceBar.h"
#include "FeelPanel.h"
#include "RoutingPanel.h"
#include "LogDrumPanel.h"

//==============================================================================
/** Overview bar under the piano roll: drag the middle to scroll, drag either END to zoom in or out (left-right arrows show at the ends). */
// v3.6: the panel textures live while any Enoch window is open (every editor holds this through a SharedResourcePointer).
// They used to sit in static variables until Windows unloaded the plugin, after JUCE had already shut down its Direct2D
// graphics, and freeing them then could hang the DAW when the plugin was removed.
struct EnochTextureCache
{
    juce::Image sheet = EnochTextures::brushed (1024, 1024, juce::Colour (0xff141922), 0.55f, 3);   // v2.7: midnight charcoal
    juce::Image plate = EnochTextures::brushed (1024, 1024, juce::Colour (0xff252c39), 0.5f, 5);
    juce::Image black = EnochTextures::brushed (1024, 1024, juce::Colour (0xff0f131a), 0.8f, 9);
    juce::Image badge = EnochTextures::brushed (512, 256, juce::Colour (0xffb9bcc2), 0.6f, 13);
    juce::Image wood  = EnochTextures::walnut (48, 1024, 17);
    ~EnochTextureCache() { enochlog::write ("window textures freed (last Enoch window closed)"); }
};

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

/** thin handle on the top edge of the piano roll: drag it up to give the roll more height (the rows above slide out of the way), down to bring them back */
class RollGrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    RollGrip() { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); setRepaintsOnMouseActivity (true); }
    std::function<int()> getGrow; std::function<void (int)> setGrow;
    void paint (juce::Graphics& g) override
    {
        const bool on = isMouseOverOrDragging();
        const auto c = getLocalBounds().toFloat().getCentre();
        g.setColour (EnochLookAndFeel::of (*this).accent.withAlpha (on ? 0.95f : 0.35f));
        for (int i = -2; i <= 2; ++i) g.fillEllipse (c.x + (float) i * 9.f - 1.5f, c.y - 1.5f, 3.f, 3.f);
        if (on) g.fillRect (getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth() - 4.f, 1.f));
    }
    void mouseDown (const juce::MouseEvent& e) override { startY = e.getScreenY(); startGrow = getGrow ? getGrow() : 0; }
    void mouseDrag (const juce::MouseEvent& e) override { if (setGrow) setGrow (startGrow + (startY - e.getScreenY())); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (setGrow) setGrow (0); }
private:
    int startY = 0, startGrow = 0;
};

class RollView : public juce::Component, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    /** height of the velocity / controller lane in pixels: drag the thin line above it (up = taller lane, down = more room for notes) */
    void setLaneHeight (float h) { laneHpx = juce::jlimit (28.f, 900.f, h); repaint(); }
    float getLaneHeight() const { return laneHpx; }
    std::function<void (float)> onLaneHeightChanged;
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
    void clearAllNotes();            // v3.1: blank piano roll (one undo step)
    void selectBars (int a, int b) { barA = a; barB = b; notifyBars(); repaint(); }
    void clearSelectedBarNotes();    // v3.1: empty the selected bars (one undo step)
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
    // ---- v3.8
    void humanizeSelected (int amount);                // 1 light, 2 medium, 3 strong (H = medium)
    void toggleLoopSelection();                        // PLAY loops the selected notes / bars (Ctrl+L)
    void setShowShortcuts (bool on) { showKeys = on; repaint(); }
    // ---- v3.9: the view (zoom, scroll, lane, tool) saved with the project
    juce::String getViewState() const;
    void setViewState (const juce::String&);
    // v3.9 hooks for the automatic tests
    void testAddNote (double beat, int pitch) { addNoteAt (beat, pitch); }
    juce::Rectangle<float> testNoteRect (int id) const { const auto G = geo(); for (auto& n : raw) if (n.id == id) return noteRect (G, n); return {}; }
    juce::Rectangle<float> testRulerRect() const { return geo().ruler; }
    bool testKsOn() const { return ksView().on; }
    int testKsShift() const { return ksView().shift; }
    int testKsCount() const { return (int) ksEvents().size(); }
    bool shortcutsShown() const { return showKeys; }
    std::function<void (const juce::String&)> onMessage;   // a short line for the status bar
    void lockAllPhrases (bool lock) { proc.setAllPhrasesLocked (lock); repaint(); if (onPhrasesChanged) onPhrasesChanged(); }

private:
    struct Geo
    {
        juce::Rectangle<float> chord, ruler, phrase, notes, lane, ks, gutter, hbar, vbar;
        int lo = 48, hi = 84;
        double viewStart = 0, viewLen = 4, len = 4;
        float ppb = 1.f, rowH = 8.f;
    };
    double wheelAccum = 0.0;
    enum class Drag { None, Move, Resize, Rubber, Velocity, Seek, BarSel, DrawLen, EraseNotes, MuteNotes, LaneDraw, LaneErase, Key, Divider, ResizeStart, KsMove };
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
    int pitchFor (int pitch, double beat, int dir) const;   // v3.4: scale-locked bars step inside their scale (dir +1 up, -1 down, 0 nearest)
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
    float laneHpx = 96.f, laneHAtDown = 96.f; bool overDivider = false;     // height of the velocity / controller lane (drag the line above it)
    bool nearDivider (const Geo& g, juce::Point<float> p) const;
    // ---- v3.8
    void autoScrollFor (juce::Point<float> p);
    void selectNext (int dir, bool extend);
    void stepSelected (int dir, bool octave);
    int stepPitch (int pitch, double beat, int dir) const;
    void ensureVisible (const enochsolo::Note& n);
    const enochsolo::Library* artLib() const;
    void drawShortcuts (juce::Graphics&, const Geo&);
    double rubberBeat = 0; float rubberPitchF = 0;
    // ---- v3.9 key-switch notes: written for an external instrument's profile, shown on their keys in the roll
    struct KsView { bool on = false; const enochsolo::Library* lib = nullptr; int shift = 0; std::array<bool, 128> row {}; };
    struct KsEvent { int key = 0, vel = 0, noteId = -1; double t0 = 0, t1 = 0; };
    KsView ksView() const;
    const std::vector<KsEvent>& ksEvents() const;
    juce::String ksName (const enochsolo::Library& l, int key, int vel) const;
    int ksHit (const Geo&, juce::Point<float>) const;
    juce::Rectangle<float> ksRect (const Geo&, const KsView&, const KsEvent&) const;
    void showKsMenu (juce::Point<int> screenPos, int noteId);
    void setArtOnNote (int noteId, int art);
    void markKsNotes (std::vector<enochsolo::Note>& v, const std::set<int>& ids) const;
    mutable std::vector<KsEvent> ksCache; mutable int ksCacheNumber = -1;
    int ksDragNote = -1, ksDragRow = -1;
    mutable bool hitLeft = false;
    bool showKeys = false;
    enochx::GhostSet ghosts; juce::Time ghostStamp; juce::uint32 ghostCheckAt = 0;
    std::vector<enochsolo::CtrlPoint> stroke; double strokeT0 = 0, strokeT1 = 0; bool strokeLine = false; juce::Point<float> strokeStart;
    bool rightDownInLane = false;

    ZoomBar hBar;
    juce::ScrollBar vBar { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollView)
};

//==============================================================================
/** The "drag the MIDI into your track" pad. */
/** analogue VU meter for the preview player: backlit paper face, needle with real ballistics, peak lamp */
class VUMeter : public juce::Component, private juce::Timer
{
public:
    VUMeter() { startTimerHz (40); setInterceptsMouseClicks (false, false); }
    ~VUMeter() override { stopTimer(); }
    std::atomic<float>* src = nullptr;
    void paint (juce::Graphics& g) override;
private:
    void timerCallback() override;
    float needle = 0.f, peakHold = 0.f; int peakLamp = 0;
};

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
class EnochSoloEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    enochlog::Mark partsMark { "window: every part destroyed" };   // v3.6: first member = destroyed last
    explicit EnochSoloEditor (EnochSoloProcessor&);
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
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
    juce::SharedResourcePointer<EnochFontHost> fontHost;
    juce::SharedResourcePointer<EnochTextureCache> textures;   // v3.6: freed when the last Enoch window closes     // embedded Inter typeface for every font in the plugin
    EnochLookAndFeel laf;
    StepCombo soundBox; juce::Label lSound;                  // built-in preview sound for the current instrument
    void refreshSounds();
    juce::Rectangle<int> headRect, deckRect, toolRect, footRect, sideRect;
    VUMeter vu;
    juce::Image chassis; juce::String chassisKey;                // the rendered hardware chassis (rebuilt when the size or colour changes)
    void renderChassis();
    void makeHardwareKnob (juce::Slider& s, const char* paramId, const char* lo, const char* hi, int fmt);
    juce::Image photo;
    Body body;
    Body advBg;                                                  // the Advanced drawer that slides over the lower part of the roll

    juce::OwnedArray<juce::TextButton> instButtons;
    StepCombo artist, key, scale, genre, timeSig, bars, mode, harmony, presetBox, gridBox, laneBox, exportBox, refTempoBox, refShiftBox, myStyleBox, startBarBox, voice2Box, claveBox, libBox;
    juce::TextEditor chordBox;
    juce::Label lArtist, lKey, lScale, lGenre, lTs, lBars, lMode, lIntensity, lBusy, lSpace, lRepeat, lOct, lHarmony, lPreset, lGrid, lZoom, lLane, lExport, lRef, lRefAmt, lBarBusy, lZoomT, lZoomP, lMyStyles, lStart, lVoice2, lClave, lGroove, songInfo, lLib, libInfo;
    juce::Slider intensity, busyG, space, repeat, octave, hTiming, hVel, hLength, hFeel, zoomH, zoomV, barBusy, refAmt, grooveAmt, trClean;
    juce::Label lClean; juce::ToggleButton trGhostsT { "Ghosts" }; int trSeen = 0;
    juce::TextButton tryAnother { "TRY ANOTHER" }, back { "<" }, fwd { ">" }, humanize { "HUMANIZE" }, reHumanize { "Re-roll feel" },
                     play { "PLAY" }, saveBtn { "Save .mid" }, advBtn { "Advanced" },
                     undoBtn { "Undo" }, redoBtn { "Redo" }, editBtn { "Edit  v" }, quantizeBtn { "Quantize" }, deleteBtn { "Delete" },
                     zoomFitBtn { "Fit" }, resetChords { "Reset chords" }, ksToggle { "KEY-SWITCHES: ON" },
                     toolSelect { "Select" }, toolDraw { "Draw" }, toolErase { "Erase" }, toolSplit { "Split" }, toolMute { "Mute" },
                     refLoad { "Load..." }, refClear { "Clear" }, barAuto { "Auto" }, barReroll { "Re-roll bars" }, barClearNotes { "Clear bar" }, clearAllBtn { "Clear all" }, panicBtn { "Panic" }, barCopy { "Copy chord" }, barPaste { "Paste chord" }, barClear { "Deselect" },
                     phraseBtn { "Lock all phrases" }, liveBtn { "LIVE MIDI OUT: OFF" }, saveStyleBtn { "Save as my style" }, delStyleBtn { "Delete" }, learnBtn { "LEARN FROM THIS BASS LINE" }, songBtn { "FROM YOUR SONG  v" },
                     sfLoadBtn { "Load SoundFont (.sf2)..." }, sfClearBtn { "Remove" };
    juce::ToggleButton playerOn { "Test player" }, midiOut { "Send MIDI to host" }, loopT { "Loop" }, hostSync { "Follow host transport" },
                       emitKS { "Key-switches" }, emitBends { "Pitch-bends" }, emitCC { "Expression CCs" }, realize { "Trills as notes" },
                       snapScale { "Snap scale" }, snapGrid { "Snap grid" }, chordPad { "Chord pad" }, showScaleT { "Show scale" }, sfOnT { "Use SoundFont" };
    juce::Slider ksBase, bendRange, volume;
    juce::Label lKs, lBend, lVol, ksText, status, hint, barInfo, barBusyText;

    RollView roll;
    LogDrumPanel ldPanel { proc };                                // v2.7: Amapiano log drum drawer
    juce::TextButton ldBtn { "LOG DRUM" };
    bool logOpen = false;
    RoutingPanel routePanel { proc };                             // v3.0: ROUTING drawer
    juce::TextButton routeBtn { "ROUTING" };
    bool routeOpen = false;
    FeelPanel feelPanel { proc };                                 // v2.7 batch 2: SOUND & FEEL drawer
    juce::TextButton feelBtn { "SOUND & FEEL" };
    // v3.7: OUTPUT and TAP RHYTHM drawers, TRY ANOTHER options, remembered window size, clip light on the OUTPUT button
    OutputPanel outPanel { proc };
    TapPanel tapPanel { proc };
    juce::TextButton outBtn { "OUTPUT" }, tapBtn { "TAP" }, tryOpts { "v" };
    // v3.8: Follow the playhead, ghost notes of the other Enoch plugin, loop the selection, the shortcut list, favourite takes
    juce::TextButton followBtn { "Follow" }, ghostBtn { "Ghost" }, loopSelBtn { "Loop sel." }, keysBtn { "?" };
    void askFavouriteName();
    // v3.9: the view and the open drawer come back when the window opens again
    juce::String uiNow() const;
    void restoreUi();
    int uiTicks = 0; juce::String uiSaved; juce::uint32 uiSaveAt = 0;
    bool outOpen = false, tapOpen = false, clipShown = false;
    int savedW = 0, savedH = 0; juce::uint32 sizeSaveAt = 0;
    bool feelOpen = false;
    // v3.4: bar locks, the project-wide scale lock, grid visibility
    juce::TextButton barLockBtn { "Lock bar" }, barScaleLockBtn { "Lock scale" };
    juce::ToggleButton lockScaleAll { "Lock all" };
    juce::Slider gridVisSlider; juce::Label lGridVis;
    StepCombo barSpeedBox;                                   // v2.7: per-bar speed
    SourceBar srcBar { proc };
    juce::ScrollBar vScroll { true };                             // v2.7: always-visible scroll bar on the right
    float scrollY = 0.f, scaleNow = 1.f; int shownTheme = -1;
    void scrollBarMoved (juce::ScrollBar*, double) override;                                    // v2.7: SOUND SOURCE row + NOW PLAYING (main UI)
    RollGrip rollGrip;
    int rollGrow = 0;
    void applyRollGrow (int v);
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
