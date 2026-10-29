#include "PluginEditor.h"
#include "FolderMemory.h"
#include "Sounds.h"
#include "PhotoData.h"

using namespace enochsolo;
using LF = EnochLookAndFeel;

namespace
{
constexpr float kBaseW = 1250.f, kBaseH = 970.f;
constexpr int kEdge = 2, kWood = 16;
// surface textures, made once
const juce::Image& texSheet()  { static const juce::Image i = EnochTextures::brushed (1024, 1024, juce::Colour (0xff2b2d31), 1.0f, 3);  return i; }
const juce::Image& texPlate()  { static const juce::Image i = EnochTextures::brushed (1024, 1024, juce::Colour (0xff4a4d54), 0.9f, 5);  return i; }
const juce::Image& texBlack()  { static const juce::Image i = EnochTextures::brushed (1024, 1024, juce::Colour (0xff1f2023), 1.3f, 9);  return i; }
const juce::Image& texBadge()  { static const juce::Image i = EnochTextures::brushed (512, 256, juce::Colour (0xffb9bcc2), 0.6f, 13);  return i; }
const juce::Image& texWood()   { static const juce::Image i = EnochTextures::walnut (48, 1024, 17);                                       return i; }
}


//==============================================================================
// DragPad
//==============================================================================
void DragPad::paint (juce::Graphics& g)
{
    const auto& t = LF::of (*this);
    auto b = getLocalBounds().toFloat().reduced (2.f);
    const bool over = isMouseOver (true);
    LF::recess (g, b, 7.f, juce::Colour (0xff0a0b0c).interpolatedWith (t.accent, over ? 0.12f : 0.04f));
    juce::Path shape; shape.addRoundedRectangle (b.reduced (4.f), 5.f);
    juce::Path stroked; const float dl[] = { 5.f, 4.f };
    juce::PathStrokeType (1.2f).createDashedStroke (stroked, shape, dl, 2);
    g.setColour (t.accent.withAlpha (over ? 0.95f : 0.55f)); g.fillPath (stroked);
    const float ic = juce::jmin (20.f, b.getHeight() * 0.40f);
    auto ir = juce::Rectangle<float> (b.getX() + 16.f, b.getCentreY() - ic * 0.5f, ic, ic);
    juce::Path ar;
    ar.startNewSubPath (ir.getCentreX(), ir.getY()); ar.lineTo (ir.getCentreX(), ir.getY() + ic * 0.62f);
    ar.startNewSubPath (ir.getCentreX() - ic * 0.26f, ir.getY() + ic * 0.36f); ar.lineTo (ir.getCentreX(), ir.getY() + ic * 0.62f); ar.lineTo (ir.getCentreX() + ic * 0.26f, ir.getY() + ic * 0.36f);
    ar.startNewSubPath (ir.getX(), ir.getY() + ic * 0.62f); ar.lineTo (ir.getX(), ir.getBottom()); ar.lineTo (ir.getRight(), ir.getBottom()); ar.lineTo (ir.getRight(), ir.getY() + ic * 0.62f);
    juce::Path as; juce::PathStrokeType (2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (as, ar);
    juce::DropShadow (t.accent.withAlpha (0.7f), 6, {}).drawForPath (g, as);
    g.setColour (t.accent.interpolatedWith (juce::Colours::white, 0.2f)); g.fillPath (as);
    auto tx = b.withTrimmedLeft (ic + 26.f).withTrimmedRight (8.f);
    LF::ledText (g, "DRAG MIDI", tx.withTrimmedBottom (tx.getHeight() * 0.48f), LF::fnt (14.5f, "Bold", 0.08f), juce::Justification::bottomLeft, t.accent.interpolatedWith (juce::Colours::white, 0.15f));
    g.setColour (LF::text().withAlpha (0.65f)); g.setFont (LF::fnt (11.f));
    g.drawFittedText ("into your instrument track", tx.withTrimmedTop (tx.getHeight() * 0.56f).toNearestInt(), juce::Justification::topLeft, 1, 0.8f);
}

//==============================================================================
// VU meter
//==============================================================================
void VUMeter::timerCallback()
{
    const float rms = src != nullptr ? src->load() : 0.f;
    const float vuDb = juce::Decibels::gainToDecibels (rms, -80.f) + 18.f;            // -18 dBFS = 0 VU
    const float v = std::pow (10.f, juce::jlimit (-25.f, 6.f, vuDb) / 20.f);
    const float target = juce::jlimit (-0.02f, 1.08f, (v - 0.1f) / (1.4125f - 0.1f));
    needle += (target - needle) * (1.f - std::exp (-1.f / (40.f * 0.075f)));          // ~300 ms VU ballistics
    if (vuDb > 3.f) peakLamp = 12; else if (peakLamp > 0) --peakLamp;
    repaint();
}

void VUMeter::paint (juce::Graphics& g)
{
    const auto& T = LF::of (*this);
    auto b = getLocalBounds().toFloat().reduced (2.f);
    // bezel with four screws
    LF::plate (g, b, texBlack(), 8.f, false, 0.03f);
    for (auto p : { b.getTopLeft() + juce::Point<float> (7.f, 7.f), b.getTopRight() + juce::Point<float> (-7.f, 7.f), b.getBottomLeft() + juce::Point<float> (7.f, -7.f), b.getBottomRight() + juce::Point<float> (-7.f, -7.f) })
        LF::screw (g, p, 3.f, p.x * 0.013f);
    auto face = b.reduced (16.f, 13.f);
    juce::Path fp; fp.addRoundedRectangle (face, 4.f);
    // backlit paper
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6edd2).interpolatedWith (T.accent, 0.10f), face.getCentreX(), face.getBottom(),
                                             juce::Colour (0xffb9ad8c).interpolatedWith (T.accent, 0.06f), face.getCentreX(), face.getY() - face.getHeight() * 0.3f, true));
    g.fillPath (fp);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (fp);
    const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.35f);
    const float R = face.getHeight() * 1.31f;
    const float aMin = -0.70f, aMax = 0.70f;
    auto angOf = [&] (float vu) { const float v = std::pow (10.f, vu / 20.f); return aMin + (aMax - aMin) * (v - 0.1f) / (1.4125f - 0.1f); };
    // scale arcs
    juce::Path arc; arc.addCentredArc (pivot.x, pivot.y, R * 0.80f, R * 0.80f, 0.f, aMin, aMax, true);
    g.setColour (juce::Colour (0xff1d1a14)); g.strokePath (arc, juce::PathStrokeType (1.1f));
    juce::Path red; red.addCentredArc (pivot.x, pivot.y, R * 0.815f, R * 0.815f, 0.f, angOf (0.f), aMax, true);
    g.setColour (juce::Colour (0xffc8241c)); g.strokePath (red, juce::PathStrokeType (3.2f));
    const float marks[] = { -20.f, -10.f, -7.f, -5.f, -3.f, -2.f, -1.f, 0.f, 1.f, 2.f, 3.f };
    for (float m : marks)
    {
        const float a = angOf (m);
        const bool lab = (m == -20.f || m == -10.f || m == -7.f || m == -5.f || m == -3.f || m == 0.f || m == 3.f);
        g.setColour (m > 0.f ? juce::Colour (0xffc8241c) : juce::Colour (0xff1d1a14));
        g.drawLine ({ pivot.getPointOnCircumference (R * 0.80f, a), pivot.getPointOnCircumference (R * (lab ? 0.89f : 0.86f), a) }, lab ? 1.4f : 1.f);
        if (lab)
        {
            const auto tp = pivot.getPointOnCircumference (R * 0.96f, a);
            g.setFont (LF::fnt (juce::jmin (10.f, face.getHeight() * 0.11f), "Bold"));
            g.drawText (m > 0 ? "+" + juce::String ((int) m) : juce::String ((int) m), juce::Rectangle<float> (tp.x - 14.f, tp.y - 6.f, 28.f, 12.f), juce::Justification::centred);
        }
    }
    g.setColour (juce::Colour (0xff1d1a14));
    g.setFont (LF::fnt (juce::jmin (20.f, face.getHeight() * 0.22f), "ExtraBold", 0.08f));
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.50f).withHeight (face.getHeight() * 0.24f), juce::Justification::centred);
    g.setFont (LF::fnt (juce::jmin (8.f, face.getHeight() * 0.085f), "Bold", 0.18f));
    g.setColour (juce::Colour (0xff4a4332));
    g.drawText ("ENOCH  PREVIEW OUTPUT", face.withTrimmedTop (face.getHeight() * 0.73f).withHeight (12.f), juce::Justification::centred);
    // needle with its shadow on the paper
    const float na = aMin + (aMax - aMin) * juce::jlimit (-0.03f, 1.08f, needle);
    const auto tip = pivot.getPointOnCircumference (R * 0.93f, na), root = pivot.getPointOnCircumference (R * 0.30f, na);
    g.setColour (juce::Colours::black.withAlpha (0.22f)); g.drawLine ({ root.translated (3.f, 4.f), tip.translated (3.f, 4.f) }, 1.6f);
    g.setColour (juce::Colour (0xff141210)); g.drawLine ({ root, tip }, 1.4f);
    g.setColour (juce::Colour (0xffc8241c)); g.drawLine ({ pivot.getPointOnCircumference (R * 0.82f, na), tip }, 1.4f);
    // needle hood
    juce::Path hood; hood.addCentredArc (pivot.x, pivot.y, R * 0.36f, R * 0.36f, 0.f, -1.6f, 1.6f, true); hood.lineTo (pivot); hood.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c41), pivot.x, pivot.y - R * 0.36f, juce::Colour (0xff111214), pivot.x, pivot.y, false));
    g.fillPath (hood);
    // peak lamp
    LF::led (g, { face.getRight() - 10.f, face.getY() + 10.f }, 3.5f, juce::Colour (0xffff3b30), peakLamp > 0);
    // glass
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.28f), face.getX(), face.getY(), juce::Colours::white.withAlpha (0.f), face.getX() + face.getWidth() * 0.55f, face.getY() + face.getHeight() * 0.6f, false));
    juce::Path gl; gl.startNewSubPath (face.getX(), face.getY()); gl.lineTo (face.getX() + face.getWidth() * 0.72f, face.getY()); gl.lineTo (face.getX(), face.getBottom() * 0.98f); gl.closeSubPath();
    g.fillPath (gl);
    // inner shadow of the window
    juce::Path ring; ring.addRectangle (face.expanded (10.f)); ring.addRoundedRectangle (face, 4.f); ring.setUsingNonZeroWinding (false);
    juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 7, { 1, 3 }).drawForPath (g, ring);
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
    l.setFont (LF::fnt (10.5f, "SemiBold", 0.09f));
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
    body.addChildComponent (advBg);
    advBg.paintFn = [this] (juce::Graphics& g)
    {
        auto b = advBg.getLocalBounds().toFloat().reduced (3.f);
        LF::plate (g, b, texPlate(), 7.f, true);
    };
    body.paintFn = [this] (juce::Graphics& g) { paintBody (g); };
    auto& A = proc.apvts;
    photo = juce::ImageFileFormat::loadFrom (EnochPhoto::data, (size_t) EnochPhoto::size);

    // ---- instrument buttons
    for (int i = 0; i < I_Count; ++i)       // Enoch Bass writes bass only: the other instrument buttons stay hidden
    {
        auto* b = instButtons.add (new juce::TextButton (i == I_Bass ? "BASS GUITAR" : instName (i)));
        b->setClickingTogglesState (false);
        b->getProperties().set ("seg", true);
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
    play.getProperties().set ("icon", "play");
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
    volume.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }; volume.updateText();
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

    // ---- built-in preview sound (modelled pianos, e-pianos, B3, Prophet-style synths, Strat / Tele / Les Paul, saxes, basses ...)
    styleLabel (lSound, "PREVIEW SOUND", body);
    soundBox.setJustificationType (juce::Justification::centredLeft);
    soundBox.setTooltip ("The sound the built-in test player uses for this instrument: concert / studio / upright / felt grands, Rhodes, Wurlitzer, FM e-piano, "
                         "clavinet, Hammond B3, Prophet-5 style synths, Stratocaster / Telecaster / Les Paul / hollow-body / acoustic guitars, tenor / alto / soprano / "
                         "baritone sax, trumpet / flugelhorn / muted trumpet, violin / viola / cello, and P / Jazz / slap / fretless / upright / synth / 808 / log drum basses.\n"
                         "They are modelled (not samples): great for auditioning, while your own instrument gets the MIDI. "
                         "A loaded one-shot or SoundFont (Advanced) takes priority when switched on. Only the preview changes - the MIDI is the same.");
    soundBox.onChange = [this]
    {
        if (updatingUi || soundBox.getSelectedId() <= 0) return;
        const int inst = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
        const juce::String id = "sound" + juce::String (inst);
        if (auto* pr = proc.apvts.getParameter (id))
            pr->setValueNotifyingHost (pr->convertTo0to1 ((float) (soundBox.getSelectedId() - 1)));
    };
    addAndMakeVisible (soundBox);

    // ---- hardware knobs: the main sound-shaping controls (drag up / down, Shift = fine, double-click = default, wheel = step)
    makeHardwareKnob (intensity, "intensity", "CALM", "FIERY", 0);
    makeHardwareKnob (busyG, "busy", "LAZY", "RAPID", 0);
    makeHardwareKnob (space, "space", "BUSY", "AIRY", 0);
    makeHardwareKnob (repeat, "repeat", "NEW", "RIFFS", 0);
    makeHardwareKnob (grooveAmt, "grooveAmount", "FREE", "LOCKED", 0);
    makeHardwareKnob (octave, "octave", "-2", "+2", 1);
    makeHardwareKnob (trClean, "trClean", "ALL", "CLEAN", 0);
    lClean.setText ("CLEANUP", juce::dontSendNotification);
    for (auto* k : { &hTiming, &hVel, &hLength, &hFeel }) makeHardwareKnob (*k, nullptr, "", "", 0);
    for (auto* l : { &lIntensity, &lBusy, &lSpace, &lRepeat, &lGroove, &lOct, &lClean }) { l->setJustificationType (juce::Justification::centred); l->setColour (juce::Label::textColourId, LF::text()); l->setFont (LF::fnt (10.5f, "Bold", 0.10f)); }
    lIntensity.setText ("INTENSITY", juce::dontSendNotification); lBusy.setText ("BUSY", juce::dontSendNotification);
    lSpace.setText ("SPACE", juce::dontSendNotification); lRepeat.setText ("REPEAT", juce::dontSendNotification);
    lGroove.setText ("GROOVE LOCK", juce::dontSendNotification); lOct.setText ("OCTAVE", juce::dontSendNotification);

    vu.src = &proc.outLevel;
    addAndMakeVisible (vu);
    addAndMakeVisible (roll); addAndMakeVisible (drag);
    rollGrow = proc.uiRollGrow;
    roll.setLaneHeight (proc.uiLaneH);
    roll.onLaneHeightChanged = [this] (float h) { proc.uiLaneH = h; };
    rollGrip.getGrow = [this] { return rollGrow; };
    rollGrip.setGrow = [this] (int v) { applyRollGrow (v); };
    rollGrip.setTooltip ("Drag up to make the piano roll taller (the rows above it slide out of the way), drag down to bring them back. Double-click to reset.");
    addAndMakeVisible (rollGrip);
    refreshArtists();
    refreshSounds();
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
    learnBtn.setColour (juce::TextButton::buttonColourId, laf.theme.panel2);
    learnBtn.setColour (juce::TextButton::textColourOffId, laf.theme.accent);
    songBtn.setColour (juce::TextButton::buttonColourId, laf.theme.panel2);
    songBtn.setColour (juce::TextButton::textColourOffId, laf.theme.accent);
    liveBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.accent);
    liveBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    sendLookAndFeelChange();
    repaint();
}

void EnochSoloEditor::makeHardwareKnob (juce::Slider& s, const char* paramId, const char* lo, const char* hi, int fmt)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setMouseDragSensitivity (230);                                                    // a full turn = 230 px of mouse travel
    s.setVelocityModeParameters (0.4, 1, 0.0, true, juce::ModifierKeys::shiftModifier); // hold Shift for fine adjustment
    s.getProperties().set ("lcd", true);
    s.getProperties().set ("lo", lo); s.getProperties().set ("hi", hi);
    juce::RangedAudioParameter* p = nullptr;
    if (paramId != nullptr) p = proc.apvts.getParameter (paramId);
    else for (auto* a : proc.getParameters())                                          // the humanize knobs are already attached: find their parameter by name
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (a))
            if ((&s == &hTiming && rp->getParameterID() == "hTiming") || (&s == &hVel && rp->getParameterID() == "hVel")
                || (&s == &hLength && rp->getParameterID() == "hLength") || (&s == &hFeel && rp->getParameterID() == "hFeel")) p = rp;
    if (p != nullptr) s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    if (fmt == 1) s.textFromValueFunction = [] (double v) { const int o = juce::roundToInt (v); return o > 0 ? "+" + juce::String (o) : juce::String (o); };
    else          s.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    s.updateText();
}

void EnochSoloEditor::refreshSounds()
{
    const int inst = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
    const auto& list = enochsolo::snd::patchList (inst);
    const auto* pr = proc.apvts.getRawParameterValue ("sound" + juce::String (inst));
    const int cur = pr != nullptr ? juce::jlimit (0, (int) list.size() - 1, juce::roundToInt (pr->load())) : 0;
    if (soundBox.getNumItems() != (int) list.size() || soundBox.getItemText (0) != juce::String (list[0].name))
    {
        updatingUi = true;
        soundBox.clear (juce::dontSendNotification);
        juce::String lastGroup;
        for (size_t i = 0; i < list.size(); ++i)
        {
            const juce::String grp (list[i].group);
            if (grp.isNotEmpty() && grp != lastGroup) { soundBox.addSectionHeading (grp); lastGroup = grp; }
            soundBox.addItem (list[i].name, (int) i + 1);
        }
        updatingUi = false;
    }
    if (soundBox.getSelectedId() != cur + 1) { updatingUi = true; soundBox.setSelectedId (cur + 1, juce::dontSendNotification); updatingUi = false; }
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
    refreshSounds();

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
    g.fillAll (juce::Colour (0xff0c0c0d));
}

void EnochSoloEditor::renderChassis()
{
    const auto& T = LF::of (*this);
    const float sc = juce::jlimit (1.f, 3.f, juce::Component::getApproximateScaleFactorForComponent (&body));
    const int W = body.getWidth(), H = body.getHeight();
    juce::String key; key << W << "x" << H << "@" << sc << T.accent.toString() << sideRect.toString() << headRect.toString() << deckRect.toString()
                         << toolRect.toString() << footRect.toString() << roll.getBounds().toString() << barPanelRect.toString() << photoRect.toString() << titleRect.toString();
    if (key == chassisKey && chassis.isValid()) return;
    chassisKey = key;
    chassis = juce::Image (juce::Image::ARGB, juce::jmax (1, (int) std::ceil ((float) W * sc)), juce::jmax (1, (int) std::ceil ((float) H * sc)), true);
    juce::Graphics g (chassis);
    g.addTransform (juce::AffineTransform::scale (sc));
    const auto full = juce::Rectangle<float> (0.f, 0.f, (float) W, (float) H);

    // the faceplate sheet
    g.drawImage (texSheet(), full, juce::RectanglePlacement::stretchToFit);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0.f, 0.f, juce::Colours::black.withAlpha (0.35f), full.getRight(), full.getBottom(), false));
    g.fillRect (full);
    // walnut end cheeks
    for (int side = 0; side < 2; ++side)
    {
        auto wr = side == 0 ? full.withWidth ((float) kWood) : full.withLeft (full.getRight() - (float) kWood);
        g.drawImage (texWood(), wr, juce::RectanglePlacement::stretchToFit);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), wr.getX(), 0.f, juce::Colours::black.withAlpha (0.45f), wr.getRight(), 0.f, false));
        g.fillRect (wr);
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.fillRect (side == 0 ? wr.getRight() - 1.f : wr.getX(), 0.f, 1.f, full.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRect (side == 0 ? wr.getRight() : wr.getX() - 1.f, 0.f, 1.f, full.getHeight());
    }
    // sidebar: black anodised module
    LF::plate (g, sideRect.toFloat(), texBlack(), 8.f, true);
    // nameplate badge + framed portrait
    {
        auto badge = titleRect.toFloat().expanded (4.f, 2.f);
        LF::plate (g, badge, texBadge(), 5.f, false, -0.02f);
        auto tr = titleRect.toFloat().reduced (6.f, 4.f);
        LF::engrave (g, "ENOCH", tr.removeFromTop (27.f), LF::fnt (25.f, "ExtraBold", 0.06f), juce::Justification::bottomLeft, juce::Colour (0xff2a2c31));
        {
            auto sr = tr.removeFromTop (28.f);
            LF::recess (g, sr.withWidth (juce::jmin (sr.getWidth(), 92.f)).expanded (2.f, 0.f), 3.f, juce::Colour (0xff0a0b0c), true);
            LF::ledText (g, "BASS", sr.withWidth (juce::jmin (sr.getWidth(), 92.f)), LF::fnt (21.f, "ExtraBold", 0.12f), juce::Justification::centred, T.accent);
        }
        tr.removeFromTop (3.f);
        LF::engrave (g, "SMART BASS GENERATOR", tr.removeFromTop (12.f), LF::fnt (7.6f, "Bold", 0.06f), juce::Justification::centredLeft, juce::Colour (0xff3b3e44));
        LF::engrave (g, "@enochsings", tr.removeFromTop (14.f), LF::fnt (10.5f, "Bold", 0.02f), juce::Justification::centredLeft, juce::Colour (0xff2a2c31));
    }
    if (photo.isValid())
    {
        auto pr = photoRect.toFloat();
        LF::recess (g, pr.expanded (4.f), 6.f, juce::Colour (0xff050506), false);
        juce::Path rp; rp.addRoundedRectangle (pr, 4.f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (rp);
            g.drawImage (photo, pr, juce::RectanglePlacement::fillDestination | juce::RectanglePlacement::xMid | juce::RectanglePlacement::yTop);
            // glass over the photo
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), pr.getX(), pr.getY(), juce::Colours::white.withAlpha (0.f), pr.getCentreX(), pr.getCentreY(), false));
            g.fillRect (pr);
        }
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.strokePath (rp, juce::PathStrokeType (1.f));
    }
    // instrument selector tray
    LF::recess (g, headRect.toFloat(), 7.f, juce::Colour (0xff111214), false);
    // main control plates
    if (! deckRect.isEmpty()) LF::plate (g, deckRect.toFloat(), texPlate(), 7.f, true);
    LF::plate (g, toolRect.toFloat(), texPlate(), 7.f, true);
    LF::plate (g, footRect.toFloat(), texPlate(), 7.f, true);
    // the piano-roll screen: a heavy bezel with screws
    {
        auto bz = roll.getBounds().toFloat().expanded (9.f);
        LF::plate (g, bz, texBlack(), 9.f, false, 0.02f);
        const float s = 4.5f;
        for (auto p : { bz.getTopLeft() + juce::Point<float> (s, s), bz.getTopRight() + juce::Point<float> (-s, s), bz.getBottomLeft() + juce::Point<float> (s, -s), bz.getBottomRight() + juce::Point<float> (-s, -s) })
            LF::screw (g, p, 2.6f, p.x * 0.01f);
        LF::recess (g, roll.getBounds().toFloat().expanded (2.f), 6.f, juce::Colour (0xff060708), false);
    }
    // bar panel: a recessed strip in the editor plate
    LF::recess (g, barPanelRect.toFloat(), 6.f, juce::Colour (0xff17181b), false);
}

void EnochSoloEditor::paintBody (juce::Graphics& g)
{
    const auto& T = LF::of (*this);
    renderChassis();
    if (chassis.isValid()) g.drawImage (chassis, body.getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // artist display
    {
        const int inst = juce::roundToInt (proc.apvts.getRawParameterValue ("inst")->load());
        const int a = juce::roundToInt (proc.apvts.getRawParameterValue ("artist")->load());
        const auto& style = styleFor (inst, a);
        auto c = cardRect.toFloat();
        LF::recess (g, c, 6.f, juce::Colour (0xff0a0b0c).interpolatedWith (T.accent, 0.04f));
        auto in = cardRect.reduced (12, 8).toFloat();
        LF::ledText (g, juce::String (style.name), in.removeFromTop (22.f), LF::fnt (16.5f, "Bold"), juce::Justification::centredLeft, laf.lcdText());
        LF::ledText (g, juce::String (style.cat).toUpperCase(), in.removeFromTop (14.f), LF::fnt (9.5f, "Bold", 0.12f), juce::Justification::centredLeft, T.accent.withAlpha (0.75f));
        in.removeFromTop (4.f);
        auto best = in.removeFromBottom (style.fav.empty() ? 0.f : 28.f);
        g.setColour (LF::text().withAlpha (0.86f)); g.setFont (LF::fnt (12.f));
        g.drawFittedText (juce::String (style.desc), in.toNearestInt(), juce::Justification::topLeft, 4, 0.9f);
        if (! style.fav.empty())
        {
            g.setColour (LF::dim()); g.setFont (LF::fnt (10.5f, "Medium"));
            g.drawFittedText ("Best with: " + juce::String (style.fav), best.toNearestInt(), juce::Justification::bottomLeft, 2, 0.9f);
        }
    }

    // captions under the humanize knobs
    auto cap = [&] (juce::Slider& sl, const char* t) { auto b = sl.getBounds().toFloat(); LF::ink (g, t, { b.getX() - 14.f, b.getBottom() + 1.f, b.getWidth() + 28.f, 13.f }, LF::fnt (9.f, "Bold", 0.10f), juce::Justification::centred); };
    cap (hTiming, "TIMING"); cap (hVel, "VELOCITY"); cap (hLength, "LENGTH"); cap (hFeel, "FEEL");

    // style reference drop window
    {
        auto b = refRect.toFloat().reduced (1.f);
        LF::recess (g, b, 6.f, juce::Colour (0xff0a0b0c).interpolatedWith (T.accent, dropHover ? 0.2f : 0.f));
        juce::Path shape; shape.addRoundedRectangle (b.reduced (3.f), 4.f);
        juce::Path stroked; const float dl[] = { 4.f, 4.f };
        juce::PathStrokeType (1.f).createDashedStroke (stroked, shape, dl, 2);
        g.setColour (T.accent.withAlpha (dropHover ? 0.9f : 0.35f)); g.fillPath (stroked);
        auto in = refRect.reduced (12, 6).toFloat();
        if (refMsg.isNotEmpty())
            LF::ledText (g, refMsg, in, LF::fnt (11.f, "Medium"), juce::Justification::centredLeft, refMsg.startsWith ("Analysing") ? T.accent : LF::red(), 3);
        else if (proc.hasReference())
        {
            LF::ledText (g, proc.referenceName(), in.removeFromTop (18.f), LF::fnt (12.5f, "Bold"), juce::Justification::centredLeft, laf.lcdText());
            g.setColour (LF::text().withAlpha (0.75f)); g.setFont (LF::fnt (10.5f));
            g.drawFittedText (proc.referenceSummary(), in.toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
        }
        else
        {
            LF::ledText (g, "DROP AUDIO / MIDI HERE", in.removeFromTop (19.f), LF::fnt (12.f, "Bold", 0.08f), juce::Justification::centred, laf.lcdText());
            g.setColour (LF::text().withAlpha (0.6f)); g.setFont (LF::fnt (10.5f));
            g.drawFittedText ("a bass line recording sets the rhythm and feel", in.toNearestInt(), juce::Justification::centredTop, 2, 0.9f);
        }
    }
}

namespace
{

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
    auto area = body.getLocalBounds().reduced (kWood + 8, 8);
    const int H = body.getHeight();

    // ---- sidebar
    sideRect = area.removeFromLeft (274).reduced (2, 2);
    auto s = sideRect.reduced (14, 14);
    area.removeFromLeft (12);
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
        auto kr = s.removeFromTop (68);
        const int kw = kr.getWidth() / 4;
        for (auto* k : { &hTiming, &hVel, &hLength, &hFeel })
            k->setBounds (kr.removeFromLeft (kw).withSizeKeepingCentre (juce::jmin (kw - 2, 60), 66));
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
    status.setBounds (s.removeFromTop (44));
    s.removeFromTop (4);
    vu.setVisible (s.getHeight() >= 80);
    vu.setBounds (s.removeFromTop (juce::jmin (s.getHeight(), 168)));

    // ---- main column
    auto r = area;
    auto head = r.removeFromTop (44);
    fwd.setBounds (head.removeFromRight (34).withSizeKeepingCentre (34, 30));
    head.removeFromRight (4);
    back.setBounds (head.removeFromRight (34).withSizeKeepingCentre (34, 30));
    head.removeFromRight (8);
    headRect = head;
    head = head.reduced (4, 4);
    for (int i = 0; i < I_Count; ++i) instButtons[i]->setBounds (i == I_Bass ? head.reduced (3, 0) : juce::Rectangle<int>());
    r.removeFromTop (10);
    const int deckTop = r.getY();
    r.removeFromTop (10);
    const int upperTopY = r.getY();
    const int grow = juce::jlimit (0, 210, rollGrow);
    r.setY (r.getY() - grow); r.setHeight (r.getHeight() + grow);          // the rows above the roll slide up behind the header
    const auto rFull = r;
    r = r.reduced (14, 0);

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

    // knob row: the main controls as hardware knobs, then CHORDS and TRY ANOTHER
    {
        auto tr = r.removeFromTop (100);
        auto right = tr.removeFromRight (juce::jmin (250, tr.getWidth() / 3));
        right.removeFromLeft (10);
        lHarmony.setBounds (right.removeFromTop (16)); harmony.setBounds (right.removeFromTop (28).reduced (2, 0));
        right.removeFromTop (10);
        tryAnother.setBounds (right.removeFromTop (40));
        juce::Slider* ks[] = { &intensity, &busyG, &space, &repeat, &trClean, &grooveAmt, &octave };
        juce::Label* ls[] = { &lIntensity, &lBusy, &lSpace, &lRepeat, &lClean, &lGroove, &lOct };
        const int kw = tr.getWidth() / 7;
        for (int i = 0; i < 7; ++i)
        {
            auto col = tr.removeFromLeft (kw);
            ls[i]->setBounds (col.removeFromTop (14));
            ks[i]->setBounds (col.withSizeKeepingCentre (juce::jmin (kw - 6, 88), col.getHeight()));
        }
    }
    r.removeFromTop (2);
    chordBox.setBounds (r.removeFromTop (28).reduced (3, 0));
    hint.setBounds (r.removeFromTop (20));
    r.removeFromTop (2);
    {
        auto lr2 = r.removeFromTop (28);
        lLib.setBounds (lr2.removeFromLeft (142));
        libBox.setBounds (lr2.removeFromLeft (juce::jmin (270, lr2.getWidth() * 2 / 5)).reduced (2, 0)); lr2.removeFromLeft (14);
        lSound.setBounds (lr2.removeFromLeft (102));
        soundBox.setBounds (lr2.removeFromLeft (juce::jmin (270, lr2.getWidth() * 3 / 5)).reduced (2, 0)); lr2.removeFromLeft (8);
        libInfo.setBounds (lr2);
        r.removeFromTop (3);
    }
    {
        auto sr = r.removeFromTop (28);
        songBtn.setBounds (sr.removeFromLeft (150).reduced (2, 0)); sr.removeFromLeft (6);
        trGhostsT.setBounds (sr.removeFromLeft (100)); sr.removeFromLeft (8);
        lVoice2.setBounds (0, 0, 0, 0); voice2Box.setBounds (0, 0, 0, 0); lVoice2.setVisible (false); voice2Box.setVisible (false);
        lClave.setBounds (sr.removeFromLeft (46)); claveBox.setBounds (sr.removeFromLeft (110).reduced (2, 0)); sr.removeFromLeft (6);
        songInfo.setBounds (sr);
        r.removeFromTop (3);
    }

    // deck card ends here; the toolbar card starts
    deckRect = juce::Rectangle<int> (rFull.getX(), deckTop, rFull.getWidth(), r.getY() + 6 - deckTop);
    if (deckRect.getBottom() - upperTopY < 24) deckRect = {};
    r = juce::Rectangle<int> (rFull.getX(), r.getY() + 6 + 10, rFull.getWidth(), r.getBottom() - (r.getY() + 16));
    const auto rFull2 = r;
    const int toolTop = r.getY();
    r.removeFromTop (9);
    r = r.reduced (12, 0);
    // toolbar row 1: tools / undo / edit / grid / snaps
    {
        auto tb = r.removeFromTop (28);
        lGrid.setBounds (0, 0, 0, 0);
        rowLayout (tb, { { &toolSelect, 58 }, { &toolDraw, 52 }, { &toolErase, 56 }, { &toolSplit, 52 }, { &toolMute, 54 }, { nullptr, 8 },
                         { &undoBtn, 52 }, { &redoBtn, 52 }, { &editBtn, 64 }, { nullptr, 8 },
                         { &gridBox, 92 }, { nullptr, 4 }, { &snapGrid, 110 }, { &snapScale, 118 }, { &quantizeBtn, 74 }, { &deleteBtn, 58 } });
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
        const int fixed = 38 + 36 + 150 + 116 + 24 + 64 + 124;
        const int zw = juce::jmax (80, (tb.getWidth() - fixed) / 2);
        const int zv = juce::jmax (60, zw * 3 / 4);
        lZoomT.setBounds (tb.removeFromLeft (30)); zoomH.setBounds (tb.removeFromLeft (zw).reduced (2, 0));
        lZoomP.setBounds (tb.removeFromLeft (34)); zoomV.setBounds (tb.removeFromLeft (zv).reduced (2, 0));
        zoomFitBtn.setBounds (tb.removeFromLeft (38).reduced (2, 0)); tb.removeFromLeft (8);
        auto rest = tb;
        lLane.setBounds (rest.removeFromLeft (36)); laneBox.setBounds (rest.removeFromLeft (150).reduced (2, 0));
        rest.removeFromLeft (8);
        chordPad.setBounds (rest.removeFromLeft (116).reduced (2, 0));
        showScaleT.setBounds (rest.removeFromLeft (124).reduced (2, 0));
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
        l1.removeFromRight (10);
        resetChords.setBounds (l1.removeFromRight (104).reduced (2, 0));
        barInfo.setBounds (l1);
        bp.removeFromTop (2);
        auto l2 = bp.removeFromTop (24);
        barReroll.setBounds (l2.removeFromRight (98).reduced (2, 0));
        barAuto.setBounds (l2.removeFromRight (54).reduced (2, 0));
        barBusyText.setBounds (l2.removeFromRight (118));
        lBarBusy.setBounds (l2.removeFromLeft (252));
        barBusy.setBounds (l2);
    }
    r.removeFromTop (2);
    toolRect = juce::Rectangle<int> (rFull2.getX(), toolTop, rFull2.getWidth(), r.getY() + 8 - toolTop);
    r = juce::Rectangle<int> (rFull2.getX(), r.getY() + 8 + 10, rFull2.getWidth(), r.getBottom() - (r.getY() + 18));

    auto bottom = r.removeFromBottom (72);
    footRect = bottom;
    bottom = bottom.reduced (12, 5);
    r.removeFromBottom (10);
    if (advanced)
    {
        auto drawer = r.withTrimmedTop (juce::jmax (0, r.getHeight() - 236));      // a drawer over the lower part of the roll: the roll keeps its size
        advBg.setBounds (drawer);
        auto ap = drawer.reduced (16, 10);
        ksText.setBounds (ap.removeFromTop (30));
        ap.removeFromTop (6);
        ap = ap.removeFromTop (176);
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
    }
    for (auto* c : std::initializer_list<juce::Component*> { &playerOn, &hostSync, &loopT, &midiOut, &emitKS, &emitBends, &emitCC, &realize, &ksBase, &bendRange, &volume, &lKs, &lBend, &lVol, &ksText, &sfLoadBtn, &sfClearBtn, &sfOnT, &sfLabel, &smpLoadBtn, &smpClearBtn, &smpOnT, &smpRootBox, &smpLabel })
        c->setVisible (advanced);
    roll.setBounds (r);
    advBg.setVisible (advanced);
    if (advanced)
    {
        advBg.toFront (false);
        for (auto* c : std::initializer_list<juce::Component*> { &playerOn, &hostSync, &loopT, &midiOut, &emitKS, &emitBends, &emitCC, &realize, &ksBase, &bendRange, &volume, &lKs, &lBend, &lVol, &ksText, &sfLoadBtn, &sfClearBtn, &sfOnT, &sfLabel, &smpLoadBtn, &smpClearBtn, &smpOnT, &smpRootBox, &smpLabel })
            c->toFront (false);
    }
    rollGrip.setBounds (r.getX(), r.getY() - 6, r.getWidth(), 12);
    rollGrip.toFront (false);
    {
        // rows that slid above the top are hidden; the others stay as they were
        std::initializer_list<juce::Component*> rows = { &lArtist, &artist, &lKey, &key, &lScale, &scale, &lGenre, &genre, &lTs, &timeSig, &lBars, &bars,
            &tryAnother, &lHarmony, &harmony, &lOct, &octave, &lIntensity, &intensity, &lBusy, &busyG, &lSpace, &space, &lRepeat, &repeat,
            &chordBox, &hint, &lLib, &libBox, &libInfo, &lSound, &soundBox, &songBtn, &lGroove, &grooveAmt, &lClean, &trClean, &trGhostsT, &lClave, &claveBox, &songInfo };
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
