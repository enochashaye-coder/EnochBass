#include "PluginEditor.h"
#include "FolderMemory.h"
#include "PhotoData.h"

using namespace enochsolo;
using LF = EnochLookAndFeel;

//==============================================================================
// DragPad
//==============================================================================
void DragPad::paint (juce::Graphics& g)
{
    const auto& t = LF::of (*this);
    auto b = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (t.accent.withAlpha (0.14f)); g.fillRoundedRectangle (b, 9.f);
    juce::Path dash; dash.addRoundedRectangle (b, 9.f);
    juce::Path stroked; const float dl[] = { 6.f, 4.f };
    juce::PathStrokeType (1.6f).createDashedStroke (stroked, dash, dl, 2);
    g.setColour (t.accent); g.fillPath (stroked);
    g.setColour (LF::text()); g.setFont (juce::Font (juce::FontOptions (15.f, juce::Font::bold)));
    g.drawText ("DRAG MIDI", b.withTrimmedBottom (b.getHeight() * 0.45f), juce::Justification::centredBottom);
    g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText ("into your instrument track", b.withTrimmedTop (b.getHeight() * 0.58f), juce::Justification::centredTop);
}

void DragPad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    if (! file.existsAsFile()) file = proc.writeMidiToTemp();
    if (! file.existsAsFile()) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, [this] { dragging = false; });
}

//==============================================================================
// Editor
//==============================================================================
namespace
{
void styleLabel (juce::Label& l, const juce::String& t, juce::Component& parent)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    l.setColour (juce::Label::textColourId, LF::dim());
    l.setJustificationType (juce::Justification::centredLeft);
    parent.addAndMakeVisible (l);
}
void styleKnob (juce::Slider& s, juce::Component& parent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    parent.addAndMakeVisible (s);
}
}

EnochSoloEditor::EnochSoloEditor (EnochSoloProcessor& p)
    : AudioProcessorEditor (&p), proc (p), roll (p), drag (p)
{
    setLookAndFeel (&laf);
    juce::Component::addAndMakeVisible (body);
    body.paintFn = [this] (juce::Graphics& g) { paintBody (g); };
    auto& A = proc.apvts;
    photo = juce::ImageFileFormat::loadFrom (EnochPhoto::data, (size_t) EnochPhoto::size);

    // ---- instrument buttons
    for (int i = 0; i < I_Count; ++i)       // Enoch Bass writes bass only: the other instrument buttons stay hidden
    {
        auto* b = instButtons.add (new juce::TextButton (i == I_Bass ? "BASS GUITAR" : instName (i)));
        b->setClickingTogglesState (false);
        b->setTooltip ("Enoch Bass writes bass guitar lines");
        addAndMakeVisible (*b);
        b->setVisible (i == I_Bass);
    }

    // ---- combos bound to parameters
    auto setupCombo = [&] (juce::ComboBox& c, const char* id)
    {
        auto* pc = dynamic_cast<juce::AudioParameterChoice*> (A.getParameter (id));
        jassert (pc != nullptr);
        c.addItemList (pc->choices, 1);
        c.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (c);
        comboAtt.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (*pc, c));
    };
    setupCombo (key, "key"); setupCombo (scale, "scale"); setupCombo (genre, "genre");
    setupCombo (timeSig, "ts"); setupCombo (bars, "bars"); setupCombo (mode, "mode"); setupCombo (harmony, "harmony");
    setupCombo (gridBox, "grid"); setupCombo (exportBox, "exportMode");
    setupCombo (refTempoBox, "refTempo"); setupCombo (refShiftBox, "refShift");
    styleLabel (lLib, "INSTRUMENT OF CHOICE", body);
    libBox.setJustificationType (juce::Justification::centredLeft);
    libBox.setTooltip ("Pick the sample library or virtual instrument you will play this bass line with. Enoch Bass writes the key-switches and controllers that library expects, so you can drag the MIDI straight in.");
    libBox.onChange = [this] { if (! updatingUi && libBox.getSelectedItemIndex() >= 0) setChoiceParam ("library", libBox.getSelectedItemIndex()); };
    addAndMakeVisible (libBox);
    libInfo.setFont (juce::FontOptions (12.f)); libInfo.setMinimumHorizontalScale (0.7f); libInfo.setColour (juce::Label::textColourId, LF::dim());
    libInfo.setJustificationType (juce::Justification::centredLeft); addAndMakeVisible (libInfo);
    setupCombo (voice2Box, "voice2"); setupCombo (claveBox, "clave");
    claveBox.setTooltip ("Salsa and Afro-Cuban genres: which way the clave runs. The bass answers the three-hit side and the two-hit side of the clave in alternate bars. Off = ignore the clave.");
    styleLabel (lClave, "CLAVE", body);
    voice2Box.setTooltip ("A second line written under / beside the bass line: parallel harmony a third below or above, a three-part horn section, or a call-and-response line that answers the bass line in its rests. It takes the place of the accompaniment track.");
    styleLabel (lVoice2, "2ND VOICE", body); styleLabel (lGroove, "GROOVE LOCK", body);
    grooveAmt.setSliderStyle (juce::Slider::LinearHorizontal);
    grooveAmt.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    grooveAmt.setTooltip ("How tightly the bass line locks to the groove loop you loaded (FROM YOUR SONG > Load groove): 0 = ignore it, 1 = land on its hits, leave room where it is busy, swing like it.");
    addAndMakeVisible (grooveAmt);
    sliderAtt.push_back (std::make_unique<juce::SliderParameterAttachment> (*A.getParameter ("grooveAmount"), grooveAmt));

    styleLabel (lClean, "CLEANUP", body);
    trClean.setSliderStyle (juce::Slider::LinearHorizontal);
    trClean.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    trClean.setTooltip ("After you drop in a recording: how strict the note filter is. Low = keep everything the analysis heard. High = only definite, clear notes (short, quiet or doubtful notes are dropped). Moves live, no need to transcribe again.");
    addAndMakeVisible (trClean);
    sliderAtt.push_back (std::make_unique<juce::SliderParameterAttachment> (*A.getParameter ("trClean"), trClean));
    trGhostsT.setTooltip ("Keep the ghost (muted, dead) notes found in the recording. Off = only the clear notes.");
    addAndMakeVisible (trGhostsT);
    buttonAtt.push_back (std::make_unique<juce::ButtonParameterAttachment> (*A.getParameter ("trGhosts"), trGhostsT));
    songBtn.setTooltip ("Detect the chords / key / length from a MIDI clip or audio loop of your song, or load a drum / bass / keys loop for the bass line to groove with.");
    songBtn.onClick = [this] { showSongMenu(); };
    addAndMakeVisible (songBtn);
    songInfo.setFont (juce::FontOptions (12.f)); songInfo.setMinimumHorizontalScale (0.7f);
    songInfo.setColour (juce::Label::textColourId, LF::dim());
    addAndMakeVisible (songInfo);
    refTempoBox.setTooltip ("If the detected tempo of your reference is half or double what you hear, fix it here");
    refShiftBox.setTooltip ("If the reference's bar line is detected in the wrong place, move it later here (rhythm lands on different beats)");
    harmony.setTooltip ("Which chords the bass line is played over. You can also click any chord above the piano roll to change it bar by bar.");
    key.setTooltip ("Key of your project");
    scale.setTooltip ("Scale / mode used for the bass line");
    genre.setTooltip ("Groove and phrasing flavour: Afrobeat, Amapiano, Highlife, Juju, Soukous, Pop, Rock, Bossa, Samba, Flamenco...");
    timeSig.setTooltip ("Auto follows the project time signature");
    bars.setTooltip ("Length of the bass line in bars");
    mode.setTooltip ("Bass line, accompaniment (comping / stabs) or both on two tracks");
    gridBox.setTooltip ("Snap grid for editing notes in the piano roll (straight and triplet values)");
    exportBox.setTooltip ("What goes into the MIDI you drag out or save:\n"
                          "Full = notes + key-switches + expression\n"
                          "No key-switches = articulations are baked into note length / velocity, nothing for the instrument to misread\n"
                          "Plain notes = only notes, no CC and no pitch-bend\n"
                          "One track per articulation = notes split onto one MIDI track per articulation, so you can route each to its own patch");

    artist.setJustificationType (juce::Justification::centredLeft);
    artist.onChange = [this] { if (! updatingUi) setChoiceParam ("artist", artist.getSelectedItemIndex()); };
    artist.setTooltip ("Play in the style of this player");
    addAndMakeVisible (artist);

    for (int i = 0; i < EnochSoloProcessor::numPresets(); ++i) presetBox.addItem (EnochSoloProcessor::presetName (i), i + 1);
    presetBox.setSelectedItemIndex (juce::jmax (0, proc.getCurrentProgram()), juce::dontSendNotification);
    presetBox.onChange = [this] { if (! updatingUi) proc.applyPreset (presetBox.getSelectedItemIndex()); };
    presetBox.setTooltip ("One-click starting points: instrument + style + genre + feel");
    addAndMakeVisible (presetBox);

    // reference (drop audio / MIDI)
    styleLabel (lRef, "STYLE REFERENCE", body); styleLabel (lRefAmt, "INFLUENCE", body);
    refAmt.setSliderStyle (juce::Slider::LinearHorizontal);
    refAmt.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    refAmt.setTooltip ("How closely the new bass line follows the rhythm, note lengths, accents and shape of your reference. 0 = ignore, 1 = follow closely. The notes themselves are always new.");
    addAndMakeVisible (refAmt);
    sliderAtt.push_back (std::make_unique<juce::SliderParameterAttachment> (*A.getParameter ("refAmount"), refAmt));
    refLoad.setTooltip ("Choose an audio recording (WAV, AIFF, FLAC, MP3) of one instrument playing, or a MIDI file. You can also drag it onto the plugin.");
    refLoad.onClick = [this] { chooseRefFile(); };
    refClear.setTooltip ("Forget the reference and go back to the normal generator");
    refClear.onClick = [this] { refMsg = juce::String(); proc.clearReference(); };
    addAndMakeVisible (refLoad); addAndMakeVisible (refClear);

    // my styles (saved settings + the style reference) and "learn from this bass line"
    styleLabel (lMyStyles, "MY STYLES", body);
    myStyleBox.setTextWhenNothingSelected ("choose a saved style...");
    myStyleBox.setTooltip ("Your own saved styles. Picking one loads its instrument, artist, genre, feel sliders, humanise settings and style reference, then writes a new bass line in it.");
    myStyleBox.onChange = [this]
    {
        if (updatingUi || myStyleBox.getSelectedId() <= 0) return;
        juce::String m; proc.loadMyStyle (myStyleBox.getText(), m); styleMsg = m;
    };
    addAndMakeVisible (myStyleBox);
    saveStyleBtn.setTooltip ("Save the current instrument, artist, genre, feel sliders, humanise settings and style reference under a name of your own");
    saveStyleBtn.onClick = [this] { askStyleName(); };
    delStyleBtn.setTooltip ("Delete the style chosen above");
    delStyleBtn.onClick = [this]
    {
        if (myStyleBox.getSelectedId() <= 0) return;
        proc.deleteMyStyle (myStyleBox.getText()); styleMsg = "Deleted.";
        refreshMyStyles();
    };
    learnBtn.setTooltip ("Use the bass line on screen (including your edits) as the style reference: new bass lines keep its rhythm, note lengths, accents and shape. Then use 'Save as my style' to keep it.");
    learnBtn.onClick = [this] { juce::String m; if (! proc.learnFromSolo (m)) styleMsg = m; else styleMsg = "Learned this bass line - new bass lines follow its feel."; };
    for (auto* c : std::initializer_list<juce::Component*> { &myStyleBox, &saveStyleBtn, &delStyleBtn, &learnBtn }) addAndMakeVisible (*c);
    refreshMyStyles();

    // live output (the DAW's play button drives it) and where the solo starts in the project
    liveBtn.setTooltip ("ON: the bass line is sent as live MIDI to whatever you route this plugin's MIDI output to (your own sax / guitar / piano track). "
                        "Press play in your DAW and it plays along from the start bar below. The built-in test sound is switched off while this is on.");
    liveBtn.onClick = [this]
    {
        const bool on = proc.apvts.getRawParameterValue ("midiOut")->load() < 0.5f;
        setBoolParam ("midiOut", on); setBoolParam ("playerOn", ! on);
    };
    addAndMakeVisible (liveBtn);
    styleLabel (lStart, "bass line starts at bar", body);
    for (int i = 1; i <= 128; ++i) startBarBox.addItem (juce::String (i), i);
    startBarBox.setTooltip ("Where the bass line begins on your DAW timeline. Bar 1 = the very start of the project; choose 17 to have it enter at bar 17.");
    addAndMakeVisible (startBarBox);
    comboAtt.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (*A.getParameter ("startBar"), startBarBox));
    phraseBtn.setTooltip ("Lock every phrase (or unlock them all). Locked phrases keep their notes when you press Try another. Click a phrase block above the piano roll to lock just that one.");
    phraseBtn.onClick = [this] { roll.lockAllPhrases (proc.numLockedPhrases() == 0); };
    addAndMakeVisible (phraseBtn);
    roll.onPhrasesChanged = [this] { shownLocks = -1; };

    // lane selector
    laneBox.addItemList ({ "Velocity", "Pitch bend", "Modulation (CC1)", "Aftertouch", "Expression (CC11)", "Sustain pedal (CC64)", "Hidden" }, 1);
    laneBox.setSelectedItemIndex (0, juce::dontSendNotification);
    laneBox.onChange = [this] { roll.setLane (laneBox.getSelectedItemIndex()); };
    laneBox.setTooltip ("What the lane under the piano roll edits. Velocity: drag bars. Others: draw curves freehand (Shift = straight line, right-drag or Alt-drag = erase, right-click = shapes).");
    addAndMakeVisible (laneBox);

    styleLabel (lArtist, "STYLE OF", body); styleLabel (lKey, "KEY", body); styleLabel (lScale, "SCALE", body);
    styleLabel (lGenre, "GENRE", body); styleLabel (lTs, "TIME SIG", body); styleLabel (lBars, "BARS", body); styleLabel (lMode, "OUTPUT", body);
    styleLabel (lHarmony, "CHORDS", body); styleLabel (lIntensity, "INTENSITY", body); styleLabel (lOct, "OCTAVE", body);
    styleLabel (lBusy, "BUSY (lazy - rapid)", body);
    styleLabel (lSpace, "SPACE (busy - airy)", body); styleLabel (lRepeat, "REPEAT (new - riffs)", body);
    styleLabel (lPreset, "PRESET", body); styleLabel (lGrid, "GRID", body); styleLabel (lZoom, "ZOOM", body); styleLabel (lLane, "LANE", body);
    styleLabel (lZoomT, "time", body); styleLabel (lZoomP, "pitch", body);

    // ---- sliders
    auto linear = [&] (juce::Slider& s, const char* id, bool text = false)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (text ? juce::Slider::TextBoxRight : juce::Slider::NoTextBox, false, 44, 20);
        addAndMakeVisible (s);
        sliderAtt.push_back (std::make_unique<juce::SliderParameterAttachment> (*A.getParameter (id), s));
    };
    linear (intensity, "intensity");
    intensity.setTooltip ("Calm and spacious  <->  big and fiery (dynamics and range)");
    linear (busyG, "busy");
    busyG.setTooltip ("How many notes get played: low = laid back, high = rapid runs. Every bar follows this unless you set a bar's own Busy below.");
    linear (space, "space");
    space.setTooltip ("How much silence and breathing room the bass line leaves");
    linear (repeat, "repeat");
    repeat.setTooltip ("Low = always new ideas, high = memorable riffs that come back");
    linear (octave, "octave", true);
    octave.setTooltip ("Shift the whole output up or down in octaves");
    auto knob = [&] (juce::Slider& s, const char* id, const char* tip)
    {
        styleKnob (s, body); s.setTooltip (tip);
        sliderAtt.push_back (std::make_unique<juce::SliderParameterAttachment> (*A.getParameter (id), s));
    };
    knob (hTiming, "hTiming", "Timing looseness");
    knob (hVel, "hVel", "Velocity variation");
    knob (hLength, "hLength", "Note-length variation");
    knob (hFeel, "hFeel", "Laid-back feel of the artist");

    // ---- buttons
    tryAnother.setTooltip ("Generate a brand-new bass line with the same settings (endless variations)");
    tryAnother.onClick = [this] { proc.tryAnother(); };
    addAndMakeVisible (tryAnother);
    back.setTooltip ("Previous bass line"); fwd.setTooltip ("Next bass line");
    back.onClick = [this] { proc.historyBack(); };
    fwd.onClick  = [this] { proc.historyForward(); };
    addAndMakeVisible (back); addAndMakeVisible (fwd);

    humanize.setClickingTogglesState (true);
    humanize.setTooltip ("Humanize velocity, timing, note length and feel");
    buttonAtt.push_back (std::make_unique<juce::ButtonParameterAttachment> (*A.getParameter ("humanize"), humanize));
    addAndMakeVisible (humanize);
    reHumanize.setTooltip ("Roll the humanisation dice again - same notes, new feel");
    reHumanize.onClick = [this] { proc.rehumanize(); };
    addAndMakeVisible (reHumanize);

    play.setTooltip ("Audition the bass line with the built-in test player (or press play in your DAW)");
    play.onClick = [this] { proc.setInternalPlaying (! proc.isInternalPlaying()); };
    addAndMakeVisible (play);
    saveBtn.onClick = [this] { saveMidi(); };
    saveBtn.setTooltip ("Save the MIDI file anywhere (you can also drag it)");
    addAndMakeVisible (saveBtn);
    advBtn.setClickingTogglesState (true);
    advBtn.onClick = [this] { advanced = advBtn.getToggleState(); resized(); };
    addAndMakeVisible (advBtn);

    ksToggle.setTooltip ("ON: the MIDI contains articulation key-switches and expression.\n"
                         "OFF: key-switches are removed and the articulations are baked into the notes themselves (length, velocity, real ornament notes),\n"
                         "so any instrument plays it correctly. The built-in test player still sounds the full articulations.");
    ksToggle.onClick = [this]
    {
        const int cur = juce::roundToInt (proc.apvts.getRawParameterValue ("exportMode")->load());
        setChoiceParam ("exportMode", cur == 0 ? 1 : 0);
    };
    addAndMakeVisible (ksToggle);

    // ---- note tools (radio group)
    struct TB { juce::TextButton* b; RollView::Tool t; const char* tip; };
    for (auto& tb : { TB { &toolSelect, RollView::Tool::Select, "Select tool (1): click / drag a box to select, drag to move, drag the right edge to resize, Alt-drag to copy" },
                      TB { &toolDraw,   RollView::Tool::Draw,   "Draw tool (2): click or drag to draw notes (length follows the grid)" },
                      TB { &toolErase,  RollView::Tool::Erase,  "Erase tool (3): click or drag over notes to delete them" },
                      TB { &toolSplit,  RollView::Tool::Split,  "Split tool (4): click a note to cut it in two at the grid" },
                      TB { &toolMute,   RollView::Tool::Mute,   "Mute tool (5): click or drag over notes to mute / unmute them (muted notes stay in the roll but are not played or exported)" } })
    {
        tb.b->setClickingTogglesState (true);
        tb.b->setRadioGroupId (4711);
        tb.b->setTooltip (tb.tip);
        const auto tool = tb.t;
        tb.b->onClick = [this, tool] { roll.setTool (tool); };
        addAndMakeVisible (*tb.b);
    }
    toolSelect.setToggleState (true, juce::dontSendNotification);
    roll.onToolChanged = [this]
    {
        const int t = (int) roll.getTool();
        juce::TextButton* bs[] = { &toolSelect, &toolDraw, &toolErase, &toolSplit, &toolMute };
        for (int i = 0; i < 5; ++i) bs[i]->setToggleState (i == t, juce::dontSendNotification);
    };

    undoBtn.setTooltip ("Undo the last edit   (Ctrl+Z)");  undoBtn.onClick = [this] { roll.doUndo(); };
    redoBtn.setTooltip ("Redo   (Ctrl+Y)");                  redoBtn.onClick = [this] { roll.doRedo(); };
    editBtn.setTooltip ("Cut, copy, paste, duplicate, mute, legato, transpose, velocity, articulation, quantize...");
    editBtn.onClick = [this] { roll.showEditMenu (editBtn); };
    quantizeBtn.setTooltip ("Snap selected notes (or all notes) to the grid   (Q)"); quantizeBtn.onClick = [this] { roll.quantizeSelected(); };
    deleteBtn.setTooltip ("Delete selected notes   (Del)");  deleteBtn.onClick = [this] { roll.deleteSelected(); };
    zoomFitBtn.setTooltip ("Show the whole bass line");           zoomFitBtn.onClick = [this] { roll.zoomFit(); };
    resetChords.setTooltip ("Put every bar back to the song's own chords"); resetChords.onClick = [this] { proc.clearBarChords(); };
    for (auto* b : { &undoBtn, &redoBtn, &editBtn, &quantizeBtn, &deleteBtn, &zoomFitBtn, &resetChords }) addAndMakeVisible (*b);

    // zoom sliders
    for (auto* s : { &zoomH, &zoomV })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRange (0.0, 1.0, 0.0);
        addAndMakeVisible (*s);
    }
    zoomH.setTooltip ("Time zoom. Also: scroll the mouse wheel over the bar numbers (just under the chord names).");
    zoomV.setTooltip ("Pitch zoom (roll height). Also: Ctrl + mouse wheel, or scroll over the piano keys on the left.");
    zoomH.onValueChange = [this] { if (! updatingUi) roll.setZoomH (zoomH.getValue()); };
    zoomV.onValueChange = [this] { if (! updatingUi) roll.setZoomV (zoomV.getValue()); };
    roll.onViewChanged = [this]
    {
        updatingUi = true;
        zoomH.setValue (roll.getZoomH(), juce::dontSendNotification);
        zoomV.setValue (roll.getZoomV(), juce::dontSendNotification);
        updatingUi = false;
    };

    // ---- bar panel (select bars by clicking the chord names)
    barInfo.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
    barInfo.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (barInfo);
    styleLabel (lBarBusy, "BUSY OF SELECTED BARS  (lazy <-> rapid)", body);
    lBarBusy.setTooltip ("Drag the slider to make the selected bar(s) lazier or busier. Click chord names above the roll to select bars.");
    barBusy.setSliderStyle (juce::Slider::LinearHorizontal);
    barBusy.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    barBusy.setRange (0.0, 1.0, 0.01);
    barBusy.setTooltip ("Make the SELECTED bars lazier or busier. The notes are rewritten over each bar's own chord. Drag right for rapid playing.");
    barBusy.onDragStart = [this] { proc.pushUndo(); busySeed = (uint64_t) juce::Random::getSystemRandom().nextInt64(); };
    barBusy.onValueChange = [this]
    {
        if (updatingUi) return;
        roll.applyBusyToSelection ((float) barBusy.getValue(), busySeed);
        barBusyText.setText (juce::String (juce::roundToInt ((float) barBusy.getValue() * 100.f)) + "%  (own value)", juce::dontSendNotification);
    };
    addAndMakeVisible (barBusy);
    barBusyText.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    barBusyText.setColour (juce::Label::textColourId, LF::dim());
    addAndMakeVisible (barBusyText);
    barAuto.setTooltip ("Selected bars follow the global BUSY slider again");
    barAuto.onClick = [this] { proc.pushUndo(); roll.applyBusyToSelection (-1.f, busySeed + 1); };
    barReroll.setTooltip ("Write new notes for the selected bars only");
    barReroll.onClick = [this] { roll.rerollSelectedBars(); };
    barCopy.setTooltip ("Copy the chord of the selected bar");
    barCopy.onClick = [this] { roll.copyChord(); refreshBarPanel(); };
    barPaste.setTooltip ("Paste the copied chord onto the selected bar(s). Tip: or click a chord, then choose 'Same as bar...'");
    barPaste.onClick = [this] { roll.pasteChord(); };
    barClear.setTooltip ("Clear the bar selection");
    barClear.onClick = [this] { roll.clearBarSelection(); };
    for (auto* b : { &barAuto, &barReroll, &barCopy, &barPaste, &barClear }) addAndMakeVisible (*b);
    roll.onBarSelection = [this] { refreshBarPanel(); };

    auto toggleP = [&] (juce::ToggleButton& t, const char* id, const char* tip, bool visible)
    {
        t.setTooltip (tip);
        if (visible) addAndMakeVisible (t); else addChildComponent (t);
        buttonAtt.push_back (std::make_unique<juce::ButtonParameterAttachment> (*A.getParameter (id), t));
    };
    toggleP (snapGrid, "snapGrid", "ON: notes you move, draw or resize snap to the grid. OFF: free editing, notes land exactly where you drop them.", true);
    toggleP (snapScale, "snapScale", "While editing, notes you move or draw jump to the nearest note of the bar's scale", true);
    toggleP (chordPad, "chordTrack", "Also write a block-chord pad track you can drag out with the bass line", true);
    toggleP (showScaleT, "showScale", "Dim the piano-roll rows that are outside each bar's scale, and mark the scale on the piano keys (select a bar to see its scale)", true);
    toggleP (sfOnT, "sfOn", "Play the preview through your SoundFont instead of the built-in sounds", false);
    sfLoadBtn.setTooltip ("Load a General MIDI SoundFont (.sf2) so the built-in preview sounds like a real sax, guitar, violin, piano or trumpet. Only the preview changes - the MIDI you export is the same.");
    sfLoadBtn.onClick = [this] { chooseSoundFont(); };
    sfClearBtn.onClick = [this] { proc.clearSoundFont(); };
    smpLoadBtn.setTooltip ("Load a single bass note recording (WAV, AIFF, FLAC or MP3) and the built-in preview plays your bass lines with that sound, at every pitch. The note's pitch is found automatically. Only the preview changes - the MIDI you export is the same.");
    smpLoadBtn.onClick = [this] { chooseBassSample(); };
    smpClearBtn.onClick = [this] { proc.clearBassSample(); };
    toggleP (smpOnT, "smpOn", "Play the preview through your bass one-shot instead of the built-in bass", true);
    setupCombo (smpRootBox, "smpRoot");
    smpRootBox.setTooltip ("The note your one-shot was recorded at. Auto finds it; set it by hand if the preview sounds out of tune with the key.");
    smpLabel.setFont (juce::FontOptions (12.f)); smpLabel.setColour (juce::Label::textColourId, LF::dim());
    addChildComponent (smpLoadBtn); addChildComponent (smpClearBtn); addChildComponent (smpLabel);
    addChildComponent (sfLoadBtn); addChildComponent (sfClearBtn);
    sfLabel.setFont (juce::FontOptions (12.f)); sfLabel.setColour (juce::Label::textColourId, LF::dim()); addChildComponent (sfLabel);
    toggleP (playerOn, "playerOn", "Turn the built-in test sound off when you route MIDI to another instrument", false);
    toggleP (midiOut, "midiOut", "Output the generated MIDI to the host (hosts that support VST3 MIDI out)", false);
    toggleP (loopT, "loop", "Loop the bass line during playback", false);
    toggleP (hostSync, "hostSync", "Play the bass line in time with the DAW transport (bar 1 = bass line start)", false);
    toggleP (emitKS, "emitKS", "Write articulation key-switch notes into the MIDI (only used when MIDI export is 'Full')", false);
    toggleP (emitBends, "emitBends", "Write pitch-bend gestures (falls, scoops, bends, slides)", false);
    toggleP (emitCC, "emitCC", "Write expression (CC11), vibrato (CC1) and pedal (CC64)", false);
    toggleP (realize, "realize", "Write trills / shakes / tremolo as real notes instead of key-switch only", false);

    auto adv = [&] (juce::Slider& s, const char* id, const char* tip)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20); s.setTooltip (tip);
        addChildComponent (s); sliderAtt.push_back (std::make_unique<juce::SliderParameterAttachment> (*A.getParameter (id), s));
    };
    adv (ksBase, "ksBase", "MIDI note of the first articulation key-switch (24 = C0). Match your sample library.");
    adv (bendRange, "bendRange", "Pitch-bend range (semitones) set in your target instrument");
    adv (volume, "volume", "Test player volume");
    styleLabel (lKs, "KEY-SWITCH BASE NOTE", body); styleLabel (lBend, "PITCH-BEND RANGE", body); styleLabel (lVol, "PLAYER VOLUME", body);
    for (auto* l : { &lKs, &lBend, &lVol }) l->setVisible (false);
    ksText.setFont (juce::Font (juce::FontOptions (12.f))); ksText.setColour (juce::Label::textColourId, LF::dim());
    ksText.setJustificationType (juce::Justification::topLeft); addChildComponent (ksText);

    status.setFont (juce::Font (juce::FontOptions (12.f))); status.setColour (juce::Label::textColourId, LF::dim());
    status.setJustificationType (juce::Justification::topLeft); addAndMakeVisible (status);
    hint.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::italic))); hint.setColour (juce::Label::textColourId, LF::dim());
    addAndMakeVisible (hint);

    chordBox.setMultiLine (false); chordBox.setReturnKeyStartsNewLine (false);
    chordBox.setFont (juce::Font (juce::FontOptions (15.f)));
    chordBox.setTextToShowWhenEmpty ("Type your song's chords, e.g.  Am7 | D7 | Gmaj7 | Cmaj7   (one chord per bar; Am7:2 = two bars)  then press Enter", LF::dim());
    chordBox.setText (proc.getChordText(), false);
    chordBox.setTooltip ("Tell Enoch Bass the chords of your song so the bass line plays over them. One chord per bar, e.g. Am7 | D7 | Gmaj7 | Cmaj7");
    chordBox.onReturnKey = [this] { proc.setChordText (chordBox.getText()); };
    chordBox.onFocusLost = [this] { if (chordBox.getText().trim() != proc.getChordText()) proc.setChordText (chordBox.getText()); };
    addAndMakeVisible (chordBox);

    addAndMakeVisible (roll); addAndMakeVisible (drag);
    rollGrow = proc.uiRollGrow;
    rollGrip.getGrow = [this] { return rollGrow; };
    rollGrip.setGrow = [this] (int v) { applyRollGrow (v); };
    rollGrip.setTooltip ("Drag up to make the piano roll taller (the rows above it slide out of the way), drag down to bring them back. Double-click to reset.");
    addAndMakeVisible (rollGrip);
    refreshArtists();
    refreshBarPanel();
    applyTheme (juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load()));
    setResizable (true, false);                       // our own edge / corner handles do the resizing (all four edges and corners)
    setResizeLimits (720, 560, 3400, 2400);
    juce::Component::addAndMakeVisible (edgeResizer);
    setSize (1260, 940);      // last: this triggers resized(), which needs every child to exist
    startTimerHz (10);
}

EnochSoloEditor::~EnochSoloEditor()
{
    stopTimer();
    roll.onBarSelection = nullptr; roll.onViewChanged = nullptr; roll.onToolChanged = nullptr;
    body.paintFn = nullptr;
    setLookAndFeel (nullptr);
}

void EnochSoloEditor::setBoolParam (const char* id, bool v)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (v ? 1.f : 0.f);
        prm->endChangeGesture();
    }
}

void EnochSoloEditor::refreshMyStyles (const juce::String& select)
{
    updatingUi = true;
    myStyleBox.clear (juce::dontSendNotification);
    const auto names = EnochSoloProcessor::listMyStyles();
    for (int i = 0; i < names.size(); ++i) myStyleBox.addItem (names[i], i + 1);
    if (select.isNotEmpty()) myStyleBox.setSelectedId (names.indexOf (select) + 1, juce::dontSendNotification);
    updatingUi = false;
}

void EnochSoloEditor::askStyleName()
{
    auto* aw = new juce::AlertWindow ("Save as my style", "Name for this style (instrument, artist, genre, feel and style reference are kept):", juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", proc.hasReference() ? juce::String (proc.referenceName()).upToLastOccurrenceOf (".", false, false) : juce::String ("My style"), juce::String());
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<EnochSoloEditor> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int r)
    {
        if (r != 1 || safe == nullptr) return;
        const auto name = aw->getTextEditorContents ("name");
        juce::String m;
        if (safe->proc.saveMyStyle (name, m))
            safe->refreshMyStyles (name.removeCharacters ("\\/:*?\"<>|").trim().substring (0, 48));
        safe->styleMsg = m;
    }), true);
}

void EnochSoloEditor::setChoiceParam (const char* id, int index)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
        prm->endChangeGesture();
    }
}

void EnochSoloEditor::applyTheme (int inst)
{
    laf.setInstrument (inst);
    tryAnother.setColour (juce::TextButton::buttonColourId, laf.theme.accent);
    tryAnother.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    refLoad.setColour (juce::TextButton::buttonColourId, laf.theme.accent);
    refLoad.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    learnBtn.setColour (juce::TextButton::buttonColourId, laf.theme.accent.withAlpha (0.85f));
    learnBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    songBtn.setColour (juce::TextButton::buttonColourId, laf.theme.accent.withAlpha (0.85f));
    songBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    liveBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.accent);
    liveBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    sendLookAndFeelChange();
    repaint();
}

void EnochSoloEditor::refreshArtists()
{
    updatingUi = true;
    const int inst = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
    artist.clear (juce::dontSendNotification);
    const auto idx = stylesFor (inst);
    std::string lastCat;
    for (size_t i = 0; i < idx.size(); ++i)
    {
        const auto& st = allStyles()[(size_t) idx[i]];
        if (st.cat != lastCat) { artist.addSectionHeading (st.cat); lastCat = st.cat; }
        artist.addItem (st.name, (int) i + 1);
    }
    const int a = juce::jlimit (0, (int) idx.size() - 1, juce::roundToInt (proc.apvts.getRawParameterValue ("artist")->load()));
    artist.setSelectedItemIndex (a, juce::dontSendNotification);
    shownInstrument = inst;
    updatingUi = false;
    refillLibraries();
    refreshKeySwitchText();
}

void EnochSoloEditor::refillLibraries()
{
    updatingUi = true;
    const int inst = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
    libBox.clear (juce::dontSendNotification);
    const auto& ll = proc.getLibraries (inst);
    for (size_t i = 0; i < ll.size(); ++i)
    {
        juce::String t = juce::String (ll[i]->name);
        if (! ll[i]->standard) t << "   [" << verifyText (ll[i]->verify) << "]";
        libBox.addItem (t, (int) i + 1);
    }
    const int cur = juce::jlimit (0, std::max (0, (int) ll.size() - 1), juce::roundToInt (proc.apvts.getRawParameterValue ("library")->load()));
    libBox.setSelectedItemIndex (cur, juce::dontSendNotification);
    updatingUi = false;
}

void EnochSoloEditor::refreshKeySwitchText()
{
    {
        const int inst2 = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
        const auto& ll = proc.getLibraries (inst2);
        const int cur = juce::jlimit (0, std::max (0, (int) ll.size() - 1), juce::roundToInt (proc.apvts.getRawParameterValue ("library")->load()));
        if (libBox.getNumItems() == (int) ll.size() && libBox.getSelectedItemIndex() != cur) { updatingUi = true; libBox.setSelectedItemIndex (cur, juce::dontSendNotification); updatingUi = false; }
        if (cur < (int) ll.size()) { const auto txt = juce::String (ll[(size_t) cur]->note); if (libInfo.getText() != txt) { libInfo.setText (txt, juce::dontSendNotification); libInfo.setTooltip (txt + "\nSource: " + juce::String (ll[(size_t) cur]->source)); } }
        ksBase.setEnabled (cur == 0);
    }
    if (const auto lib = proc.currentLibrary())
    {
        juce::String t = juce::String (lib->name) + ":  ";
        if (lib->noKeySwitches) t << "no key-switch notes are written (controllers only).";
        else for (size_t i = 0; i < lib->arts.size(); ++i)
        {
            t << artName (lib->arts[i].art) << " = ";
            for (size_t k = 0; k < lib->arts[i].press.size(); ++k) t << juce::String (noteName (lib->arts[i].press[k].note)) << (k + 1 < lib->arts[i].press.size() ? "+" : "");
            t << (i + 1 < lib->arts.size() ? "   |   " : "");
        }
        ksText.setText (t, juce::dontSendNotification);
        return;
    }
    const int inst = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
    const int base = juce::roundToInt (proc.apvts.getRawParameterValue ("ksBase")->load());
    const auto& l = artList (inst);
    juce::String t = juce::String (instName (inst)) + " key-switches (base " + juce::String (noteName (base)) + "):  ";
    for (size_t i = 0; i < l.size(); ++i) t << juce::String (noteName (base + (int) i)) << " = " << artName (l[i]) << (i + 1 < l.size() ? "   |   " : "");
    ksText.setText (t, juce::dontSendNotification);
}

void EnochSoloEditor::refreshBarPanel()
{
    const bool has = roll.barSelFirst() >= 0;
    updatingUi = true;
    barInfo.setText (roll.barSelText(), juce::dontSendNotification);
    barInfo.setColour (juce::Label::textColourId, has ? LF::of (*this).accent : LF::dim());
    barBusy.setValue ((double) roll.busyOfSelection(), juce::dontSendNotification);
    updatingUi = false;
    barBusyText.setText (! has ? juce::String ("select a bar first")
                               : juce::String (juce::roundToInt (roll.busyOfSelection() * 100.f)) + (roll.selectionHasExplicitBusy() ? "%  (own value)" : "%  (follows global)"),
                         juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &barBusy, &barAuto, &barReroll, &barCopy, &barClear })
        c->setEnabled (has);
    lBarBusy.setEnabled (has); lBarBusy.setAlpha (has ? 1.f : 0.5f);
    barAuto.setEnabled (has && roll.selectionHasExplicitBusy());
    barPaste.setEnabled (has && roll.hasChordClipboard());
}

void EnochSoloEditor::timerCallback()
{
    const int inst = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
    const int a = juce::roundToInt (proc.apvts.getRawParameterValue ("artist")->load());
    if (inst != shownInstrument) { refreshArtists(); applyTheme (inst); }
    else if (artist.getNumItems() > 0 && artist.getSelectedItemIndex() != juce::jlimit (0, artist.getNumItems() - 1, a))
    { updatingUi = true; artist.setSelectedItemIndex (juce::jlimit (0, artist.getNumItems() - 1, a), juce::dontSendNotification); updatingUi = false; }
    for (int i = 0; i < instButtons.size(); ++i) instButtons[i]->setToggleState (i == inst, juce::dontSendNotification);
    refreshKeySwitchText();

    if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram())
    { updatingUi = true; presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification); updatingUi = false; }

    back.setEnabled (proc.canGoBack()); fwd.setEnabled (proc.canGoForward());
    undoBtn.setEnabled (proc.canUndo()); redoBtn.setEnabled (proc.canRedo());
    resetChords.setEnabled (proc.hasBarChords());
    const bool hostPlay = proc.hostIsPlaying() && proc.apvts.getRawParameterValue ("hostSync")->load() > 0.5f;
    play.setButtonText (hostPlay ? "HOST PLAYING" : (proc.isInternalPlaying() ? "STOP" : "PLAY"));
    play.setToggleState (proc.isInternalPlaying() || hostPlay, juce::dontSendNotification);

    {
        const bool live = proc.apvts.getRawParameterValue ("midiOut")->load() > 0.5f;
        liveBtn.setButtonText (live ? "LIVE MIDI OUT: ON" : "LIVE MIDI OUT: OFF");
        liveBtn.setToggleState (live, juce::dontSendNotification);
        const int locks = proc.numLockedPhrases();
        if (locks != shownLocks)
        {
            shownLocks = locks;
            phraseBtn.setButtonText (locks > 0 ? "Unlock all (" + juce::String (locks) + ")" : "Lock all phrases");
            tryAnother.setButtonText (locks > 0 ? "TRY ANOTHER  (" + juce::String (locks) + " kept)" : "TRY ANOTHER");
        }
    }
    const int em = juce::roundToInt (proc.apvts.getRawParameterValue ("exportMode")->load());
    ksToggle.setButtonText (em == 0 ? "KEY-SWITCHES: ON" : "KEY-SWITCHES: OFF");
    ksToggle.setToggleState (em == 0, juce::dontSendNotification);

    const bool autoTs = proc.apvts.getRawParameterValue ("ts")->load() < 0.5f;
    juce::String st; st << juce::String (proc.getHostBpm(), 1) << " BPM  |  "
                        << (autoTs ? juce::String (proc.getHostNum()) + "/" + juce::String (proc.getHostDen()) + " (project)" : juce::String ("manual time signature"))
                        << "\nsolo " << proc.historyPosition() << " of " << proc.historyCount();
    if (styleMsg.isNotEmpty()) st << "\n" << styleMsg;
    status.setText (st, juce::dontSendNotification);

    const auto chordMsg = proc.getChordStatus();
    hint.setColour (juce::Label::textColourId, chordMsg.isNotEmpty() ? LF::red() : LF::dim());
    hint.setText (chordMsg.isNotEmpty() ? chordMsg
                  : juce::String ("Tip: left-click a chord = select the bar (then use BUSY)  |  right-click a chord = chord menu  |  wheel over the bar numbers = zoom  |  keys 1-5 = tools"),
                  juce::dontSendNotification);

    {
        const juce::String now = proc.hasReference() ? proc.referenceName() + "\n" + proc.referenceSummary() : juce::String();
        const bool has = proc.hasReference();
        refClear.setEnabled (has); refAmt.setEnabled (has); refTempoBox.setEnabled (has); refShiftBox.setEnabled (has);
        if (now != refShown) { refShown = now; body.repaint (refRect); }
    }
    {
        const juce::String now = songMsg.isNotEmpty() ? songMsg : (proc.hasGroove() ? "Groove: " + proc.grooveName() + "  |  " + proc.grooveSummary() : juce::String ("No groove loop loaded"));
        if (now != songShown) { songShown = now; songInfo.setText (now, juce::dontSendNotification); songInfo.setTooltip (now); }
        grooveAmt.setEnabled (proc.hasGroove());
        trClean.setEnabled (proc.hasTranscription()); trGhostsT.setEnabled (proc.hasTranscription());
        if (proc.trVersion != trSeen) { trSeen = proc.trVersion; songMsg = "Transcribed: " + proc.lastTrMessage; songShown = "<changed>"; }
        const juce::String sfNow = proc.hasSoundFont() ? "SoundFont: " + proc.soundFontName() : juce::String ("No SoundFont: built-in preview sounds");
        if (sfNow != sfShown) { sfShown = sfNow; sfLabel.setText (sfNow, juce::dontSendNotification); }
        sfClearBtn.setEnabled (proc.hasSoundFont()); sfOnT.setEnabled (proc.hasSoundFont());
        const juce::String smNow = proc.hasBassSample() ? "Bass sample: " + proc.bassSampleInfo() : juce::String ("No bass one-shot loaded");
        if (smNow != smpShown) { smpShown = smNow; smpLabel.setText (smNow, juce::dontSendNotification); }
        smpClearBtn.setEnabled (proc.hasBassSample()); smpOnT.setEnabled (proc.hasBassSample()); smpRootBox.setEnabled (proc.hasBassSample());
    }
    const int sk = inst * 1000 + a;
    if (sk != shownStyleKey) { shownStyleKey = sk; body.repaint (cardRect); }
    barPaste.setEnabled (roll.barSelFirst() >= 0 && roll.hasChordClipboard());
}

static bool isRefExt (const juce::File& f)
{
    const auto e = f.getFileExtension().toLowerCase();
    return e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac" || e == ".mp3" || e == ".ogg" || e == ".m4a" || e == ".wma" || e == ".mid" || e == ".midi";
}

bool EnochSoloEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isRefExt (juce::File (f))) return true;
    return false;
}

void EnochSoloEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dropHover = false;
    for (auto& f : files)
    {
        const juce::File file (f);
        if (! isRefExt (file)) continue;
        const auto e = file.getFileExtension().toLowerCase();
        if (e == ".mid" || e == ".midi") { loadRefFile (file); return; }
        juce::PopupMenu m;
        m.addSectionHeader (file.getFileName());
        m.addItem (1, "Turn this bass into MIDI (transcribe)");
        m.addItem (2, "Use as a rhythm reference (learn its feel)");
        juce::Component::SafePointer<EnochSoloEditor> self (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [self, file] (int r)
        {
            if (self == nullptr) return;
            if (r == 1) self->runTranscribe (file);
            else if (r == 2) self->loadRefFile (file);
        });
        return;
    }
    repaint();
}

void EnochSoloEditor::chooseRefFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a reference recording or MIDI file",
                                                   enochsolo::lastFolder ("reference", juce::File::getSpecialLocation (juce::File::userMusicDirectory)),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.wma;*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) { enochsolo::rememberFolder ("reference", f); loadRefFile (f); } });
}

void EnochSoloEditor::loadRefFile (const juce::File& f)
{
    if (refBusy) return;
    refBusy = true;
    refMsg = "Analysing " + f.getFileName() + " ...";
    body.repaint (refRect);
    juce::Component::SafePointer<EnochSoloEditor> self (this);
    juce::MessageManager::callAsync ([self, f]
    {
        if (self == nullptr) return;
        juce::String msg;
        const bool ok = self->proc.loadReference (f, msg);
        self->refMsg = ok ? juce::String() : msg;
        self->refBusy = false;
        self->refShown = "<changed>";
        self->repaint();
    });
}

void EnochSoloEditor::chooseBassSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a single bass note recording", enochsolo::lastFolder ("oneshot", juce::File::getSpecialLocation (juce::File::userMusicDirectory)), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
    juce::Component::SafePointer<EnochSoloEditor> self (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File() || self == nullptr) return;
        enochsolo::rememberFolder ("oneshot", f);
        self->smpLabel.setText ("Loading " + f.getFileName() + " ...", juce::dontSendNotification);
        juce::MessageManager::callAsync ([self, f]
        {
            if (self == nullptr) return;
            juce::String msg;
            if (! self->proc.loadBassSample (f, msg)) self->smpLabel.setText (msg, juce::dontSendNotification);
            self->smpShown = "<changed>";
            self->repaint();
        });
    });
}

void EnochSoloEditor::chooseSoundFont()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a SoundFont (General MIDI .sf2)", enochsolo::lastFolder ("soundfont", juce::File::getSpecialLocation (juce::File::userMusicDirectory)), "*.sf2");
    juce::Component::SafePointer<EnochSoloEditor> self (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File() || self == nullptr) return;
        enochsolo::rememberFolder ("soundfont", f);
        self->sfLabel.setText ("Loading " + f.getFileName() + " ...", juce::dontSendNotification);
        juce::MessageManager::callAsync ([self, f]
        {
            if (self == nullptr) return;
            juce::String msg;
            if (! self->proc.loadSoundFont (f, msg)) self->sfLabel.setText (msg, juce::dontSendNotification);
            self->repaint();
        });
    });
}

void EnochSoloEditor::showSongMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("FROM YOUR SONG");
    m.addItem (1, "Detect chords from a MIDI clip or audio loop...");
    m.addSeparator();
    m.addItem (4, "Bass audio to MIDI (transcribe a bass recording)...");
    {
        juce::PopupMenu tm; tm.addItem (40, "Project tempo", true, proc.trTempoMode == 0); tm.addItem (41, "Detect from the recording", true, proc.trTempoMode == 1);
        m.addSubMenu ("   Transcribe: tempo", tm);
        juce::PopupMenu ti; const char* tn[] = { "As played (exact timing)", "Tidy (75% towards the 1/16 grid)", "Snap to 1/16", "Snap to 1/8" };
        for (int i = 0; i < 4; ++i) ti.addItem (50 + i, tn[i], true, proc.trTidy == i);
        m.addSubMenu ("   Transcribe: timing", ti);
        juce::PopupMenu ta; ta.addItem (60, "Start of the audio is bar 1", true, proc.trAnchor == 0); ta.addItem (61, "First note is bar 1", true, proc.trAnchor == 1);
        m.addSubMenu ("   Transcribe: bar 1", ta);
    }
    m.addSeparator();
    m.addItem (2, "Load groove loop (drums / bass / keys)...");
    m.addItem (3, "Clear groove loop", proc.hasGroove());
    juce::PopupMenu tp; const int gt = (int) std::lround (proc.apvts.getRawParameterValue ("grooveTempo")->load());
    tp.addItem (10, "As detected", true, gt == 0); tp.addItem (11, "Twice as fast", true, gt == 1); tp.addItem (12, "Half as fast", true, gt == 2);
    m.addSubMenu ("Groove tempo", tp, proc.hasGroove());
    juce::PopupMenu sp; const int gs = (int) std::lround (proc.apvts.getRawParameterValue ("grooveShift")->load());
    const char* sn[] = { "Auto", "+1/16", "+1/8", "+3/16", "+1 beat", "+2 beats", "+3 beats" };
    for (int i = 0; i < 7; ++i) sp.addItem (20 + i, sn[i], true, gs == i);
    m.addSubMenu ("Groove bar line", sp, proc.hasGroove());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&songBtn), [this] (int r)
    {
        if (r == 1) runSongFile (true);
        else if (r == 4) chooseTranscribeFile();
        else if (r == 40 || r == 41) proc.trTempoMode = r - 40;
        else if (r >= 50 && r <= 53) proc.trTidy = r - 50;
        else if (r == 60 || r == 61) proc.trAnchor = r - 60;
        else if (r == 2) runSongFile (false);
        else if (r == 3) { songMsg = juce::String(); proc.clearGroove(); }
        else if (r >= 10 && r <= 12) setChoiceParam ("grooveTempo", r - 10);
        else if (r >= 20 && r <= 26) setChoiceParam ("grooveShift", r - 20);
    });
}

void EnochSoloEditor::chooseTranscribeFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a recording of a bass (one note at a time works best)",
                                                   enochsolo::lastFolder ("transcribe", juce::File::getSpecialLocation (juce::File::userMusicDirectory)),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.wma");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) { enochsolo::rememberFolder ("transcribe", f); runTranscribe (f); } });
}

void EnochSoloEditor::runTranscribe (const juce::File& f)
{
    songMsg = "Listening to " + f.getFileName() + " ...";
    songShown = "<changed>";
    repaint();
    juce::Component::SafePointer<EnochSoloEditor> self (this);
    juce::MessageManager::callAsync ([self, f]
    {
        if (self == nullptr) return;
        juce::String msg;
        const bool ok = self->proc.transcribeAudio (f, msg);
        self->songMsg = ok ? "Transcribed: " + msg : msg;
        self->songShown = "<changed>";
        self->refShown = "<changed>";
        self->repaint();
    });
}

void EnochSoloEditor::runSongFile (bool chords)
{
    chooser = std::make_unique<juce::FileChooser> (chords ? "Choose a MIDI clip or audio loop with your chords" : "Choose a drum / bass / keys loop (audio or MIDI)",
                                                   enochsolo::lastFolder ("song", juce::File::getSpecialLocation (juce::File::userMusicDirectory)),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.wma;*.mid;*.midi");
    juce::Component::SafePointer<EnochSoloEditor> self (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self, chords] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File() || self == nullptr) return;
        enochsolo::rememberFolder ("song", f);
        self->songMsg = (chords ? "Listening to " : "Analysing ") + f.getFileName() + " ...";
        self->songShown = "<changed>";
        juce::MessageManager::callAsync ([self, f, chords]
        {
            if (self == nullptr) return;
            juce::String msg;
            const bool ok = chords ? self->proc.detectChords (f, msg) : self->proc.loadGroove (f, msg);
            self->songMsg = ok ? (chords ? "Chords: " + msg : juce::String()) : msg;
            self->songShown = "<changed>";
            self->repaint();
        });
    });
}

void EnochSoloEditor::applyRollGrow (int v)
{
    rollGrow = juce::jlimit (0, 210, v);
    proc.uiRollGrow = rollGrow;
    resized();
    repaint();
}

void EnochSoloEditor::saveMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI file", enochsolo::lastFolder ("midiExport", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)).getChildFile (proc.suggestedFileName()), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != juce::File()) { enochsolo::rememberFolder ("midiExport", f); proc.saveMidiTo (f.withFileExtension ("mid")); }
                          });
}

void EnochSoloEditor::paint (juce::Graphics& g)
{
    g.fillAll (LF::of (*this).bg);
}

void EnochSoloEditor::paintBody (juce::Graphics& g)
{
    const auto& T = LF::of (*this);
    g.fillAll (T.bg);

    // sidebar card
    auto side = juce::Rectangle<float> (10.f, 8.f, 274.f, (float) body.getHeight() - 16.f);
    g.setColour (T.panel); g.fillRoundedRectangle (side, 14.f);

    // logo (square portrait) + title
    if (photo.isValid())
    {
        juce::Path rp; rp.addRoundedRectangle (photoRect.toFloat(), 14.f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (rp);
            g.drawImage (photo, photoRect.toFloat(), juce::RectanglePlacement::fillDestination | juce::RectanglePlacement::xMid | juce::RectanglePlacement::yTop);
        }
        g.setColour (T.accent); g.strokePath (rp, juce::PathStrokeType (2.f));
    }
    {
        auto tr = titleRect;
        g.setColour (T.accent); g.setFont (juce::Font (juce::FontOptions (23.f, juce::Font::bold)));
        g.drawText ("ENOCH BASS", tr.removeFromTop (30), juce::Justification::centredLeft);
        g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (11.f)));
        g.drawFittedText ("smart bass-line generator", tr.removeFromTop (30), juce::Justification::topLeft, 2, 0.9f);
        g.setColour (T.accent); g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
        g.drawText ("@enochsings", tr.removeFromTop (18), juce::Justification::centredLeft);
        const juce::Colour cols[3] = { LF::green(), T.accent, LF::red() };
        for (int i = 0; i < 3; ++i) { g.setColour (cols[i]); g.fillRect (titleRect.getX() + i * 26, titleRect.getBottom() - 3, 24, 3); }
    }

    // artist card
    {
        const int inst = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
        const int a = juce::roundToInt (proc.apvts.getRawParameterValue ("artist")->load());
        const auto& style = styleFor (inst, a);
        auto c = cardRect.toFloat();
        g.setColour (T.panel2); g.fillRoundedRectangle (c, 10.f);
        auto in = cardRect.reduced (12, 8);
        g.setColour (T.accent); g.setFont (juce::Font (juce::FontOptions (17.f, juce::Font::bold)));
        g.drawText (juce::String (style.name), in.removeFromTop (22), juce::Justification::centredLeft);
        g.setColour (LF::green()); g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (juce::String (style.cat).toUpperCase(), in.removeFromTop (16), juce::Justification::centredLeft);
        in.removeFromTop (3);
        g.setColour (LF::text()); g.setFont (juce::Font (juce::FontOptions (12.5f)));
        auto best = in.removeFromBottom (style.fav.empty() ? 0 : 32);
        g.drawFittedText (juce::String (style.desc), in, juce::Justification::topLeft, 4, 0.9f);
        if (! style.fav.empty())
        {
            g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (11.5f)));
            g.drawFittedText ("Best with: " + juce::String (style.fav), best, juce::Justification::bottomLeft, 2, 0.9f);
        }
    }

    // captions under the humanize knobs
    g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    auto cap = [&] (juce::Slider& sl, const char* t) { auto b = sl.getBounds(); g.drawText (t, b.getX() - 10, b.getBottom() + 1, b.getWidth() + 20, 14, juce::Justification::centred); };
    cap (hTiming, "TIMING"); cap (hVel, "VELOCITY"); cap (hLength, "LENGTH"); cap (hFeel, "FEEL");

    // style reference drop box
    {
        auto b = refRect.toFloat().reduced (1.f);
        g.setColour (dropHover ? T.accent.withAlpha (0.28f) : T.panel2); g.fillRoundedRectangle (b, 9.f);
        juce::Path dash; dash.addRoundedRectangle (b, 9.f);
        juce::Path stroked; const float dl[] = { 5.f, 4.f };
        juce::PathStrokeType (1.3f).createDashedStroke (stroked, dash, dl, 2);
        g.setColour (dropHover ? T.accent : T.border.brighter (0.35f)); g.fillPath (stroked);
        auto in = refRect.reduced (10, 4);
        if (refMsg.isNotEmpty())
        {
            g.setColour (refMsg.startsWith ("Analysing") ? T.accent : LF::red()); g.setFont (juce::Font (juce::FontOptions (11.5f)));
            g.drawFittedText (refMsg, in, juce::Justification::centredLeft, 3, 0.9f);
        }
        else if (proc.hasReference())
        {
            g.setColour (T.accent); g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
            g.drawText (proc.referenceName(), in.removeFromTop (18), juce::Justification::centredLeft, true);
            g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (11.f)));
            g.drawFittedText (proc.referenceSummary(), in, juce::Justification::topLeft, 2, 0.9f);
        }
        else
        {
            g.setColour (LF::text()); g.setFont (juce::Font (juce::FontOptions (13.5f, juce::Font::bold)));
            g.drawText ("DROP AUDIO / MIDI HERE", in.removeFromTop (20), juce::Justification::centred);
            g.setColour (LF::dim()); g.setFont (juce::Font (juce::FontOptions (11.f)));
            g.drawFittedText ("a bass line recording sets the rhythm and feel", in, juce::Justification::centredTop, 2, 0.9f);
        }
    }

    // thin accent frame around the bar panel
    g.setColour (T.panel2); g.fillRoundedRectangle (barPanelRect.toFloat(), 8.f);
    g.setColour (T.border); g.drawRoundedRectangle (barPanelRect.toFloat().reduced (0.5f), 8.f, 1.f);
}

namespace
{
constexpr float kBaseW = 1180.f, kBaseH = 860.f;
constexpr int kEdge = 8;

/** lay a row of components left to right; width 0 + null = gap. Shrinks everything a little when the row is too narrow. */
void rowLayout (juce::Rectangle<int> r, std::initializer_list<std::pair<juce::Component*, int>> items, int vInset = 0)
{
    int total = 0; for (auto& it : items) total += it.second;
    const double k = total > r.getWidth() ? (double) r.getWidth() / total : 1.0;
    int x = r.getX();
    for (auto& it : items)
    {
        const int w = (int) std::lround (it.second * k);
        if (it.first != nullptr) it.first->setBounds (juce::Rectangle<int> (x, r.getY() + vInset, std::max (4, w - 4), r.getHeight() - 2 * vInset));
        x += w;
    }
}
}

void EnochSoloEditor::resized()
{
    if (instButtons.size() < I_Count) return;     // children not built yet
    edgeResizer.setBounds (getLocalBounds());
    const float aw = (float) (getWidth() - 2 * kEdge), ah = (float) (getHeight() - 2 * kEdge);
    const float s = juce::jlimit (0.55f, 2.6f, std::min (aw / kBaseW, ah / kBaseH));
    body.setBounds (0, 0, (int) std::lround (aw / s), (int) std::lround (ah / s));
    body.setTransform (juce::AffineTransform::scale (s).translated ((float) kEdge, (float) kEdge));
    layoutBody();
    edgeResizer.toFront (false);
    repaint();
}

void EnochSoloEditor::layoutBody()
{
    auto area = body.getLocalBounds().reduced (6, 4);
    const int H = body.getHeight();

    // ---- sidebar
    auto s = area.removeFromLeft (266).reduced (4, 4);
    area.removeFromLeft (14);
    {
        auto top = s.removeFromTop (92);
        photoRect = top.removeFromLeft (92);                  // the portrait is a square logo
        top.removeFromLeft (12);
        titleRect = top;
    }
    s.removeFromTop (14);
    lPreset.setBounds (s.removeFromTop (16)); presetBox.setBounds (s.removeFromTop (28));
    s.removeFromTop (8);
    cardRect = s.removeFromTop (juce::jlimit (104, 160, H / 7));
    s.removeFromTop (12);
    auto hrow = s.removeFromTop (34);
    humanize.setBounds (hrow.removeFromLeft (124)); hrow.removeFromLeft (6); reHumanize.setBounds (hrow);
    s.removeFromTop (8);
    {
        auto kr = s.removeFromTop (50);
        const int kw = kr.getWidth() / 4;
        for (auto* k : { &hTiming, &hVel, &hLength, &hFeel })
            k->setBounds (kr.removeFromLeft (kw).withSizeKeepingCentre (46, 46));
    }
    s.removeFromTop (20);
    // style reference: the Load button sits right under the humanize controls
    lRef.setVisible (false);
    refLoad.setButtonText ("LOAD AUDIO / MIDI REFERENCE");
    refLoad.setBounds (s.removeFromTop (32));
    s.removeFromTop (6);
    refRect = s.removeFromTop (52);
    s.removeFromTop (6);
    refClear.setBounds (s.removeFromTop (24));
    s.removeFromTop (6);
    {
        auto ar = s.removeFromTop (22);
        lRefAmt.setBounds (ar.removeFromLeft (64)); refAmt.setBounds (ar);
    }
    s.removeFromTop (4);
    {
        auto cr2 = s.removeFromTop (24);
        refTempoBox.setBounds (cr2.removeFromLeft (cr2.getWidth() / 2).withTrimmedRight (3));
        refShiftBox.setBounds (cr2.withTrimmedLeft (3));
    }
    s.removeFromTop (10);
    lMyStyles.setBounds (s.removeFromTop (16));
    myStyleBox.setBounds (s.removeFromTop (26));
    s.removeFromTop (4);
    {
        auto sr = s.removeFromTop (26);
        delStyleBtn.setBounds (sr.removeFromRight (62).reduced (1, 0));
        saveStyleBtn.setBounds (sr.reduced (1, 0));
    }
    s.removeFromTop (4);
    learnBtn.setBounds (s.removeFromTop (28));
    s.removeFromTop (6);
    status.setBounds (s.removeFromTop (50));

    // ---- main column
    auto r = area;
    auto head = r.removeFromTop (44);
    fwd.setBounds (head.removeFromRight (34).withSizeKeepingCentre (34, 30));
    head.removeFromRight (4);
    back.setBounds (head.removeFromRight (34).withSizeKeepingCentre (34, 30));
    head.removeFromRight (8);
    for (int i = 0; i < I_Count; ++i) instButtons[i]->setBounds (i == I_Bass ? head.reduced (3, 0) : juce::Rectangle<int>());
    r.removeFromTop (6);
    const int upperTopY = r.getY();
    const int grow = juce::jlimit (0, 210, rollGrow);
    r.setY (r.getY() - grow); r.setHeight (r.getHeight() + grow);          // the rows above the roll slide up behind the header

    auto cr = r.removeFromTop (46);
    {
        const int total = cr.getWidth();
        const int wArtist = total * 26 / 100;
        const int wRest = (total - wArtist) / 5;
        auto place = [&] (juce::Label& l, juce::ComboBox& c, int w)
        {
            auto col = cr.removeFromLeft (w).reduced (3, 0);
            l.setBounds (col.removeFromTop (16)); c.setBounds (col.removeFromTop (26));
        };
        place (lArtist, artist, wArtist);
        place (lKey, key, wRest); place (lScale, scale, wRest); place (lGenre, genre, wRest);
        place (lTs, timeSig, wRest); place (lBars, bars, wRest);
        lMode.setBounds (0, 0, 0, 0); mode.setBounds (0, 0, 0, 0); lMode.setVisible (false); mode.setVisible (false);
    }
    r.removeFromTop (4);

    // sliders row
    {
        auto tr = r.removeFromTop (46);
        tryAnother.setBounds (tr.removeFromRight (150).reduced (2, 2));
        tr.removeFromRight (6);
        auto hm = tr.removeFromRight (120);
        lHarmony.setBounds (hm.removeFromTop (16)); harmony.setBounds (hm.removeFromTop (26).reduced (3, 0));
        auto oct = tr.removeFromRight (104);
        lOct.setBounds (oct.removeFromTop (16)); octave.setBounds (oct.removeFromTop (26));
        const int sw = tr.getWidth() / 4;
        auto c1 = tr.removeFromLeft (sw).reduced (3, 0); auto c2 = tr.removeFromLeft (sw).reduced (3, 0);
        auto c3 = tr.removeFromLeft (sw).reduced (3, 0); auto c4 = tr.reduced (3, 0);
        lIntensity.setBounds (c1.removeFromTop (16)); intensity.setBounds (c1.removeFromTop (26));
        lBusy.setBounds (c2.removeFromTop (16));      busyG.setBounds (c2.removeFromTop (26));
        lSpace.setBounds (c3.removeFromTop (16));     space.setBounds (c3.removeFromTop (26));
        lRepeat.setBounds (c4.removeFromTop (16));    repeat.setBounds (c4.removeFromTop (26));
    }
    r.removeFromTop (2);
    chordBox.setBounds (r.removeFromTop (28).reduced (3, 0));
    hint.setBounds (r.removeFromTop (20));
    r.removeFromTop (2);
    {
        auto lr2 = r.removeFromTop (28);
        lLib.setBounds (lr2.removeFromLeft (150));
        libBox.setBounds (lr2.removeFromLeft (juce::jmin (330, lr2.getWidth() / 2)).reduced (2, 0)); lr2.removeFromLeft (6);
        libInfo.setBounds (lr2);
        r.removeFromTop (3);
    }
    {
        auto sr = r.removeFromTop (28);
        songBtn.setBounds (sr.removeFromLeft (150).reduced (2, 0)); sr.removeFromLeft (6);
        lGroove.setBounds (sr.removeFromLeft (84)); grooveAmt.setBounds (sr.removeFromLeft (96)); sr.removeFromLeft (8);
        lClean.setBounds (sr.removeFromLeft (52)); trClean.setBounds (sr.removeFromLeft (90)); trGhostsT.setBounds (sr.removeFromLeft (70)); sr.removeFromLeft (8);
        lVoice2.setBounds (0, 0, 0, 0); voice2Box.setBounds (0, 0, 0, 0); lVoice2.setVisible (false); voice2Box.setVisible (false);
        lClave.setBounds (sr.removeFromLeft (46)); claveBox.setBounds (sr.removeFromLeft (110).reduced (2, 0)); sr.removeFromLeft (6);
        songInfo.setBounds (sr);
        r.removeFromTop (3);
    }

    // toolbar row 1: tools / undo / edit / grid / snaps
    {
        auto tb = r.removeFromTop (28);
        lGrid.setBounds (0, 0, 0, 0);
        rowLayout (tb, { { &toolSelect, 58 }, { &toolDraw, 52 }, { &toolErase, 56 }, { &toolSplit, 52 }, { &toolMute, 54 }, { nullptr, 8 },
                         { &undoBtn, 52 }, { &redoBtn, 52 }, { &editBtn, 64 }, { nullptr, 8 },
                         { &gridBox, 92 }, { nullptr, 4 }, { &snapGrid, 100 }, { &snapScale, 104 }, { &quantizeBtn, 74 }, { &deleteBtn, 58 } });
        // the toggles are drawn smaller, centred in the row
        for (auto* t : { &snapGrid, &snapScale }) t->setBounds (t->getBounds().reduced (0, 4));
        gridBox.setTooltip ("Grid used for the grid lines, snapping, quantize and new-note length");
        lGrid.setVisible (false);
    }
    r.removeFromTop (3);
    // toolbar row 2: zoom / lane / chord helpers
    {
        auto tb = r.removeFromTop (26);
        lZoom.setBounds (tb.removeFromLeft (42));
        const int fixed = 38 + 36 + 150 + 104 + 104 + 24 + 64 + 110;
        const int zw = juce::jmax (80, (tb.getWidth() - fixed) / 2);
        const int zv = juce::jmax (60, zw * 3 / 4);
        lZoomT.setBounds (tb.removeFromLeft (30)); zoomH.setBounds (tb.removeFromLeft (zw).reduced (2, 0));
        lZoomP.setBounds (tb.removeFromLeft (34)); zoomV.setBounds (tb.removeFromLeft (zv).reduced (2, 0));
        zoomFitBtn.setBounds (tb.removeFromLeft (38).reduced (2, 0)); tb.removeFromLeft (8);
        auto rest = tb;
        lLane.setBounds (rest.removeFromLeft (36)); laneBox.setBounds (rest.removeFromLeft (150).reduced (2, 0));
        rest.removeFromLeft (8);
        resetChords.setBounds (rest.removeFromLeft (104).reduced (2, 0)); chordPad.setBounds (rest.removeFromLeft (104).reduced (2, 0));
        showScaleT.setBounds (rest.removeFromLeft (110).reduced (2, 0));
    }
    r.removeFromTop (3);
    // bar panel (two lines: which bars, then the labelled BUSY slider)
    {
        barPanelRect = r.removeFromTop (58);
        auto bp = barPanelRect.reduced (8, 4);
        auto l1 = bp.removeFromTop (24);
        phraseBtn.setBounds (l1.removeFromRight (124).reduced (2, 0));
        barClear.setBounds (l1.removeFromRight (76).reduced (2, 0));
        barPaste.setBounds (l1.removeFromRight (90).reduced (2, 0));
        barCopy.setBounds (l1.removeFromRight (86).reduced (2, 0));
        barInfo.setBounds (l1);
        bp.removeFromTop (2);
        auto l2 = bp.removeFromTop (24);
        barReroll.setBounds (l2.removeFromRight (98).reduced (2, 0));
        barAuto.setBounds (l2.removeFromRight (54).reduced (2, 0));
        barBusyText.setBounds (l2.removeFromRight (118));
        lBarBusy.setBounds (l2.removeFromLeft (252));
        barBusy.setBounds (l2);
    }
    r.removeFromTop (4);

    auto bottom = r.removeFromBottom (62);
    r.removeFromBottom (6);
    if (advanced)
    {
        auto ap = r.removeFromBottom (176); r.removeFromBottom (6);
        {
            auto smr = ap.removeFromBottom (30);
            smpLoadBtn.setBounds (smr.removeFromLeft (190).reduced (2, 2)); smpClearBtn.setBounds (smr.removeFromLeft (80).reduced (2, 2));
            smpOnT.setBounds (smr.removeFromLeft (130)); smpRootBox.setBounds (smr.removeFromLeft (84).reduced (2, 3)); smpLabel.setBounds (smr);
            ap.removeFromBottom (4);
        }
        {
            auto sfr = ap.removeFromBottom (30);
            sfLoadBtn.setBounds (sfr.removeFromLeft (190).reduced (2, 2)); sfClearBtn.setBounds (sfr.removeFromLeft (80).reduced (2, 2));
            sfOnT.setBounds (sfr.removeFromLeft (130)); sfLabel.setBounds (sfr);
            ap.removeFromBottom (4);
        }
        auto left = ap.removeFromLeft (ap.getWidth() / 2);
        auto col = [&] (juce::Rectangle<int>& a, juce::Component& c, int h) { c.setBounds (a.removeFromTop (h)); };
        col (left, playerOn, 24); col (left, hostSync, 24); col (left, loopT, 24); col (left, midiOut, 24);
        auto mid = ap.removeFromLeft (ap.getWidth() / 2);
        col (mid, emitKS, 24); col (mid, emitBends, 24); col (mid, emitCC, 24); col (mid, realize, 24);
        lKs.setBounds (ap.removeFromTop (14)); ksBase.setBounds (ap.removeFromTop (22));
        lBend.setBounds (ap.removeFromTop (14)); bendRange.setBounds (ap.removeFromTop (22));
        lVol.setBounds (ap.removeFromTop (14)); volume.setBounds (ap.removeFromTop (22));
        ksText.setBounds (juce::Rectangle<int> (r.getX(), r.getBottom() - 34, r.getWidth(), 34));
        r.removeFromBottom (36);
    }
    for (auto* c : std::initializer_list<juce::Component*> { &playerOn, &hostSync, &loopT, &midiOut, &emitKS, &emitBends, &emitCC, &realize, &ksBase, &bendRange, &volume, &lKs, &lBend, &lVol, &ksText, &sfLoadBtn, &sfClearBtn, &sfOnT, &sfLabel, &smpLoadBtn, &smpClearBtn, &smpOnT, &smpRootBox, &smpLabel })
        c->setVisible (advanced);
    roll.setBounds (r);
    rollGrip.setBounds (r.getX(), r.getY() - 6, r.getWidth(), 12);
    rollGrip.toFront (false);
    {
        // rows that slid above the top are hidden; the others stay as they were
        std::initializer_list<juce::Component*> rows = { &lArtist, &artist, &lKey, &key, &lScale, &scale, &lGenre, &genre, &lTs, &timeSig, &lBars, &bars,
            &tryAnother, &lHarmony, &harmony, &lOct, &octave, &lIntensity, &intensity, &lBusy, &busyG, &lSpace, &space, &lRepeat, &repeat,
            &chordBox, &hint, &lLib, &libBox, &libInfo, &songBtn, &lGroove, &grooveAmt, &lClean, &trClean, &trGhostsT, &lClave, &claveBox, &songInfo };
        for (auto* c : rows) if (! c->getBounds().isEmpty()) c->setVisible (c->getY() >= upperTopY - 1);
    }

    drag.setBounds (bottom.removeFromRight (juce::jmin (260, juce::jmax (150, bottom.getWidth() - 690))).reduced (2, 2));
    bottom.removeFromRight (8);
    play.setBounds (bottom.removeFromLeft (130).reduced (2, 8));
    bottom.removeFromLeft (8);
    auto sb = bottom.removeFromLeft (108);
    saveBtn.setBounds (sb.removeFromTop (28).reduced (2, 0)); sb.removeFromTop (4);
    advBtn.setBounds (sb.removeFromTop (28).reduced (2, 0));
    bottom.removeFromLeft (10);
    auto kc = bottom.removeFromLeft (230);
    ksToggle.setBounds (kc.removeFromTop (28).reduced (2, 0)); kc.removeFromTop (4);
    exportBox.setBounds (kc.removeFromTop (28).reduced (2, 0));
    bottom.removeFromLeft (10);
    auto lc = bottom.removeFromLeft (juce::jmax (150, bottom.getWidth() - 8));
    liveBtn.setBounds (lc.removeFromTop (28).reduced (2, 0)); lc.removeFromTop (4);
    {
        auto lr = lc.removeFromTop (28);
        lStart.setBounds (lr.removeFromLeft (juce::jmax (70, lr.getWidth() - 62)));
        startBarBox.setBounds (lr.reduced (2, 0));
    }
}
