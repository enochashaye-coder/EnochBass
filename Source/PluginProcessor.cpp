#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginEditor.h"
#include "Engine/ChordDetect.h"
#include "Engine/Transcribe.h"

static std::shared_ptr<enochsolo::BassSample> loadBassSample_ (const juce::File& f, juce::String& m) { return enochsolo::loadBassSample (f, m); }
#include <tuple>

using namespace enochsolo;

namespace
{
const juce::StringArray kTimeSigs { "Auto (project)", "4/4", "3/4", "2/4", "6/8", "12/8", "9/8", "5/4", "7/8", "7/4", "5/8" };
const juce::StringArray kBars { "1", "2", "4", "8", "12", "16", "24", "32", "48", "64", "96", "128", "192", "256" };
const int kBarValues[] = { 1, 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256 };
const int kNumBarValues = 14;
const juce::StringArray kKeys { "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B" };

juce::StringArray choices (int n, const char* (*f) (int))
{
    juce::StringArray a; for (int i = 0; i < n; ++i) a.add (f (i)); return a;
}

float getF (const juce::AudioProcessorValueTreeState& a, const char* id) { return a.getRawParameterValue (id)->load(); }
int getI (const juce::AudioProcessorValueTreeState& a, const char* id) { return (int) std::lround (getF (a, id)); }
bool getB (const juce::AudioProcessorValueTreeState& a, const char* id) { return getF (a, id) > 0.5f; }

// parameters that change the notes (need a full regenerate with the same seed) / only the expression pass
const char* kGenIds[] = { "inst", "artist", "genre", "key", "scale", "ts", "bars", "intensity", "mode", "octave", "harmony", "space", "repeat", "chordTrack", "busy", "refAmount", "refTempo", "refShift", "voice2", "clave", "grooveAmount", "grooveTempo", "grooveShift" };
const char* kFinIds[] = { "library", "humanize", "hTiming", "hVel", "hLength", "hFeel", "ksBase", "bendRange", "emitKS", "emitBends", "emitCC", "realize", "exportMode" };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EnochSoloProcessor::createLayout()
{
    using P = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto choice = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { l.add (std::make_unique<juce::AudioParameterChoice> (P { id, 1 }, name, items, def)); };
    auto flt = [&] (const char* id, const char* name, float lo, float hi, float def)
    { l.add (std::make_unique<juce::AudioParameterFloat> (P { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def)); };
    auto boolean = [&] (const char* id, const char* name, bool def)
    { l.add (std::make_unique<juce::AudioParameterBool> (P { id, 1 }, name, def)); };

    juce::StringArray insts; for (int i = 0; i < I_Count; ++i) insts.add (instName (i));
    choice ("inst", "Instrument", insts, I_Bass);
    l.add (std::make_unique<juce::AudioParameterInt> (P { "artist", 1 }, "Bass Style", 0, std::max (1, maxArtists() - 1), 0));
    choice ("genre", "Genre", choices (G_Count, [] (int i) { return genreInfo (i).name; }), G_Afrobeat);
    choice ("key", "Key", kKeys, 9);
    choice ("scale", "Scale", choices (SC_Count, [] (int i) { return scaleName (i); }), SC_Minor);
    choice ("ts", "Time Signature", kTimeSigs, 0);
    choice ("bars", "Bass Length (bars)", kBars, 3);
    flt ("intensity", "Intensity", 0.f, 1.f, 0.6f);
    choice ("mode", "Output", juce::StringArray { "Bass", "Accompaniment", "Bass + Accompaniment" }, 0);
    l.add (std::make_unique<juce::AudioParameterInt> (P { "octave", 1 }, "Octave", -2, 2, 0));
    choice ("harmony", "Chords", juce::StringArray { "Key vamp (safest)", "Auto progression", "My chords" }, 0);
    flt ("space", "Space (busy to spacious)", 0.f, 1.f, 0.5f);
    flt ("repeat", "Repeat (new ideas to riffs)", 0.f, 1.f, 0.5f);
    flt ("busy", "Busy (lazy to rapid)", 0.f, 1.f, 0.5f);
    boolean ("chordTrack", "Write Chord Pad Track", false);
    flt ("refAmount", "Reference Influence", 0.f, 1.f, 0.7f);
    choice ("refTempo", "Reference Tempo", juce::StringArray { "As detected", "Twice as fast", "Half as fast" }, 0);
    choice ("refShift", "Reference Bar Line", juce::StringArray { "Auto", "+1/16", "+1/8", "+3/16", "+1 beat", "+2 beats", "+3 beats" }, 0);
    choice ("clave", "Clave (Salsa / Afro-Cuban)", juce::StringArray { "Off", "3-2 clave", "2-3 clave" }, 1);
    choice ("voice2", "Second Voice", juce::StringArray { "Off", "Harmony: third below", "Harmony: third above", "Horn section (3 parts)", "Call and response" }, 0);
    flt ("grooveAmount", "Groove Lock", 0.f, 1.f, 0.7f);
    choice ("grooveTempo", "Groove Tempo", juce::StringArray { "As detected", "Twice as fast", "Half as fast" }, 0);
    choice ("grooveShift", "Groove Bar Line", juce::StringArray { "Auto", "+1/16", "+1/8", "+3/16", "+1 beat", "+2 beats", "+3 beats" }, 0);
    l.add (std::make_unique<juce::AudioParameterInt> (P { "library", 1 }, "Instrument Of Choice", 0, 63, 1));
    l.add (std::make_unique<juce::AudioParameterBool> (P { "smpOn", 1 }, "Preview: Use Bass Sample", true,
                                                       juce::AudioParameterBoolAttributes().withAutomatable (false)));
    {
        juce::StringArray rn; rn.add ("Auto");
        for (int n = 12; n <= 72; ++n) rn.add (juce::String (noteName (n)));
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "smpRoot", 1 }, "Bass Sample Root Note", rn, 0,
                                                             juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    }
    l.add (std::make_unique<juce::AudioParameterBool> (P { "sfOn", 1 }, "Preview: Use SoundFont", true,
                                                       juce::AudioParameterBoolAttributes().withAutomatable (false)));
    l.add (std::make_unique<juce::AudioParameterBool> (P { "showScale", 1 }, "Roll: Show Scale", true,
                                                       juce::AudioParameterBoolAttributes().withAutomatable (false)));
    // editor-only helpers (not offered to host automation)
    l.add (std::make_unique<juce::AudioParameterChoice> (P { "grid", 1 }, "Edit Grid", juce::StringArray { "1/4", "1/4 triplet", "1/8", "1/8 triplet", "1/16", "1/16 triplet", "1/32", "1/32 triplet", "Off" }, 4,
                                                         juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    l.add (std::make_unique<juce::AudioParameterBool> (P { "snapScale", 1 }, "Edit: Snap To Scale", false,
                                                       juce::AudioParameterBoolAttributes().withAutomatable (false)));
    l.add (std::make_unique<juce::AudioParameterBool> (P { "snapGrid", 1 }, "Edit: Snap To Grid", true,
                                                       juce::AudioParameterBoolAttributes().withAutomatable (false)));

    boolean ("humanize", "Humanize", true);
    flt ("hTiming", "Humanize Timing", 0.f, 1.f, 0.5f);
    flt ("hVel", "Humanize Velocity", 0.f, 1.f, 0.5f);
    flt ("hLength", "Humanize Length", 0.f, 1.f, 0.5f);
    flt ("hFeel", "Humanize Feel (laid-back)", 0.f, 1.f, 0.5f);

    l.add (std::make_unique<juce::AudioParameterInt> (P { "ksBase", 1 }, "Key-switch Base Note", 0, 60, 12));
    l.add (std::make_unique<juce::AudioParameterInt> (P { "bendRange", 1 }, "Pitch-bend Range", 1, 24, 2));
    choice ("exportMode", "MIDI Export Mode", juce::StringArray { "Full: key-switches + expression", "No key-switches (articulations baked into notes)", "Plain notes only", "One track per articulation" }, 0);
    boolean ("emitKS", "Write Key-switches", true);
    boolean ("emitBends", "Write Pitch-bends", true);
    boolean ("emitCC", "Write Expression CCs", true);
    boolean ("realize", "Write Trills as Notes", false);

    boolean ("playerOn", "Test Player", true);
    boolean ("midiOut", "Send MIDI to Host", false);
    boolean ("loop", "Loop Playback", true);
    boolean ("hostSync", "Follow Host Transport", true);
    {
        juce::StringArray sb; for (int i = 1; i <= 128; ++i) sb.add (juce::String (i));
        choice ("startBar", "Bass Starts At Bar", sb, 0);
    }
    flt ("volume", "Player Volume", 0.f, 1.f, 0.75f);
    flt ("reverb", "Player Reverb", 0.f, 1.f, 0.25f);
    return l;
}

int EnochSoloProcessor::tsNumFor (int c, int hostN)
{
    static const int n[] = { 0, 4, 3, 2, 6, 12, 9, 5, 7, 7, 5 };
    return c <= 0 ? std::max (1, hostN) : n[std::min (c, 10)];
}
int EnochSoloProcessor::tsDenFor (int c, int hostD)
{
    static const int d[] = { 0, 4, 4, 4, 8, 8, 8, 4, 8, 4, 8 };
    return c <= 0 ? std::max (1, hostD) : d[std::min (c, 10)];
}

//==============================================================================
EnochSoloProcessor::EnochSoloProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ENOCHBASS", createLayout())
{
    loadLibraries();
    for (auto* id : kGenIds) apvts.addParameterListener (id, this);
    for (auto* id : kFinIds) apvts.addParameterListener (id, this);
    seed = (uint64_t) juce::Random::getSystemRandom().nextInt64();
    hSeed = (uint64_t) juce::Random::getSystemRandom().nextInt64();
    regenerate (false, true);
    startTimerHz (20);
}

EnochSoloProcessor::~EnochSoloProcessor()
{
    stopTimer();
    for (auto* id : kGenIds) apvts.removeParameterListener (id, this);
    for (auto* id : kFinIds) apvts.removeParameterListener (id, this);
}

void EnochSoloProcessor::parameterChanged (const juce::String& id, float)
{
    if (loadingState) return;
    const bool onMessageThread = juce::MessageManager::getInstance()->isThisTheMessageThread();
    if (id == "harmony") barChords.clear();
    if (id == "inst" && onMessageThread) setParam ("library", 1.f);          // libraries belong to one instrument
    for (auto* g : kGenIds) if (id == g) { needRegen = true; if (onMessageThread) timerCallback(); return; }
    needFinalize = true;
    if (onMessageThread) timerCallback();
}

//==============================================================================
Settings EnochSoloProcessor::currentSettings() const
{
    Settings s;
    s.instrument = getI (apvts, "inst");
    const int nArtists = (int) stylesFor (s.instrument).size();
    s.artist = juce::jlimit (0, std::max (0, nArtists - 1), getI (apvts, "artist"));
    s.genre = getI (apvts, "genre");
    s.keyRoot = getI (apvts, "key");
    s.scale = getI (apvts, "scale");
    const int ts = getI (apvts, "ts");
    s.tsNum = tsNumFor (ts, hostNum.load());
    s.tsDen = tsDenFor (ts, hostDen.load());
    s.bars = kBarValues[juce::jlimit (0, kNumBarValues - 1, getI (apvts, "bars"))];
    s.intensity = getF (apvts, "intensity");
    s.mode = getI (apvts, "mode");
    s.octaveShift = getI (apvts, "octave");
    s.harmony = getI (apvts, "harmony");
    s.chordText = chordText.toStdString();
    s.barChords = barChords;
    s.space = getF (apvts, "space");
    s.repeat = getF (apvts, "repeat");
    s.chordTrack = getB (apvts, "chordTrack");
    s.busy = getF (apvts, "busy");
    s.barBusy = barBusy;
    s.refProfile = refProfile;
    s.refAmount = getF (apvts, "refAmount");
    s.refTempo = juce::jlimit (0, 2, getI (apvts, "refTempo"));
    {
        static const double kShift[] = { 0.0, 0.25, 0.5, 0.75, 1.0, 2.0, 3.0 };
        s.refShift = kShift[juce::jlimit (0, 6, getI (apvts, "refShift"))];
    }
    s.voice2 = juce::jlimit (0, 4, getI (apvts, "voice2"));
    s.clave = juce::jlimit (0, 2, getI (apvts, "clave"));
    if (auto lib = currentLibrary()) if (! lib->standard) s.lib = lib;
    s.grooveProfile = grooveProfile;
    s.grooveAmount = getF (apvts, "grooveAmount");
    s.grooveTempo = juce::jlimit (0, 2, getI (apvts, "grooveTempo"));
    {
        static const double kShift[] = { 0.0, 0.25, 0.5, 0.75, 1.0, 2.0, 3.0 };
        s.grooveShift = kShift[juce::jlimit (0, 6, getI (apvts, "grooveShift"))];
    }
    s.exportMode = getI (apvts, "exportMode");
    s.ksBase = getI (apvts, "ksBase");
    s.bendRange = getI (apvts, "bendRange");
    s.emitKS = getB (apvts, "emitKS");
    s.emitBends = getB (apvts, "emitBends");
    s.emitCC = getB (apvts, "emitCC");
    s.realizeOrnaments = getB (apvts, "realize");
    s.humanize = getB (apvts, "humanize");
    s.hTiming = getF (apvts, "hTiming");
    s.hVel = getF (apvts, "hVel");
    s.hLength = getF (apvts, "hLength");
    s.hFeel = getF (apvts, "hFeel");
    s.bpm = hostBpm.load();
    s.seed = seed;
    s.hSeed = hSeed;
    return s;
}

std::shared_ptr<EnochSoloProcessor::Snapshot> EnochSoloProcessor::makeSnapshot (Pattern&& p)
{
    auto snap = std::make_shared<Snapshot>();
    snap->lengthBeats = p.lengthBeats;
    snap->seed = p.s.seed;
    for (auto& e : p.events)
    {
        const int ch = e.track + 1;
        juce::MidiMessage m;
        switch (e.type)
        {
            case EV_ON:   m = juce::MidiMessage::noteOn (ch, e.a, (juce::uint8) e.b); break;
            case EV_OFF:  m = juce::MidiMessage::noteOff (ch, e.a); break;
            case EV_CC:   m = juce::MidiMessage::controllerEvent (ch, e.a, e.b); break;
            case EV_AT:   m = juce::MidiMessage::channelPressureChange (ch, e.a); break;
            default:      m = juce::MidiMessage::pitchWheel (ch, e.a); break;
        }
        snap->events.push_back ({ e.t, m });
    }
    for (auto& e : p.playEvents)
    {
        const int ch = e.track + 1;
        juce::MidiMessage m;
        switch (e.type)
        {
            case EV_ON:   m = juce::MidiMessage::noteOn (ch, e.a, (juce::uint8) e.b); break;
            case EV_OFF:  m = juce::MidiMessage::noteOff (ch, e.a); break;
            case EV_CC:   m = juce::MidiMessage::controllerEvent (ch, e.a, e.b); break;
            case EV_AT:   m = juce::MidiMessage::channelPressureChange (ch, e.a); break;
            default:      m = juce::MidiMessage::pitchWheel (ch, e.a); break;
        }
        snap->testEvents.push_back ({ e.t, m });
    }
    snap->pattern = std::move (p);
    return snap;
}

void EnochSoloProcessor::publishMaster()
{
    Pattern copy = master;
    auto snap = makeSnapshot (std::move (copy));
    snap->number = ++snapCounter;
    lastBpmUsed = snap->pattern.s.bpm;
    lastTsNum = snap->pattern.s.tsNum; lastTsDen = snap->pattern.s.tsDen;
    const juce::SpinLock::ScopedLockType l (snapLock);
    snapshot = snap;
}

void EnochSoloProcessor::regenerate (bool newSeed, bool pushHistory)
{
    if (newSeed)
    {
        seed = (uint64_t) juce::Random::getSystemRandom().nextInt64();
        hSeed = (uint64_t) juce::Random::getSystemRandom().nextInt64();
    }
    if (pushHistory)
    {
        if (histIndex + 1 < (int) history.size()) history.resize ((size_t) histIndex + 1);   // new branch
        history.push_back ({ seed, hSeed });
        if (history.size() > 100) history.erase (history.begin());
        histIndex = (int) history.size() - 1;
    }
    Settings s = currentSettings();
    if (! phraseLocks.empty())
    {
        // locked phrases survive: same chords, new notes everywhere else, then the locked notes are put back
        const Pattern& o = master;
        const bool same = ! o.raw.empty() && o.s.instrument == s.instrument && o.s.keyRoot == s.keyRoot && o.s.scale == s.scale && o.s.genre == s.genre
                          && o.s.bars == s.bars && o.s.tsNum == s.tsNum && o.s.tsDen == s.tsDen;
        if (same)
        {
            const Pattern old = master;
            s.chordSeed = old.s.chordSeed ? old.s.chordSeed : old.s.seed;
            master = Generator::generate (s);
            mergeLocked (master, old, phraseLocks);
            Generator::finalize (master, s);
            publishMaster();
            return;
        }
        phraseLocks.clear();
    }
    master = Generator::generate (s);
    publishMaster();
}

Settings EnochSoloProcessor::editSettings() const
{
    Settings s = currentSettings();
    const Settings& m = master.s;
    // structural settings belong to the pattern on screen; only humanise / output options follow the live parameters
    s.seed = m.seed; s.chordSeed = m.chordSeed;
    s.instrument = m.instrument; s.artist = m.artist; s.genre = m.genre; s.keyRoot = m.keyRoot; s.scale = m.scale;
    s.tsNum = m.tsNum; s.tsDen = m.tsDen; s.bars = m.bars; s.mode = m.mode; s.octaveShift = m.octaveShift; s.intensity = m.intensity;
    s.harmony = m.harmony; s.chordText = m.chordText; s.space = m.space; s.repeat = m.repeat; s.chordTrack = m.chordTrack;
    s.busy = m.busy; s.barBusy = barBusy;
    s.refProfile = m.refProfile; s.refAmount = m.refAmount; s.refTempo = m.refTempo; s.refShift = m.refShift;
    s.grooveProfile = m.grooveProfile; s.grooveAmount = m.grooveAmount; s.grooveTempo = m.grooveTempo; s.grooveShift = m.grooveShift; s.voice2 = m.voice2; s.clave = m.clave;
    s.hSeed = hSeed;
    return s;
}

void EnochSoloProcessor::refinalize()
{
    if (master.raw.empty() && master.chords.empty()) { regenerate (false, false); return; }
    Settings s = editSettings();
    s.barChords = master.s.barChords;
    Generator::finalize (master, s);
    publishMaster();
}

//==============================================================================
// editing
void EnochSoloProcessor::pushUndo()
{
    undoStack.push_back ({ master, barChords, barBusy });
    if (undoStack.size() > 60) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void EnochSoloProcessor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back ({ master, barChords, barBusy });
    master = std::move (undoStack.back().p); barChords = std::move (undoStack.back().bc); barBusy = std::move (undoStack.back().busy);
    undoStack.pop_back();
    publishMaster();
}

void EnochSoloProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back ({ master, barChords, barBusy });
    master = std::move (redoStack.back().p); barChords = std::move (redoStack.back().bc); barBusy = std::move (redoStack.back().busy);
    redoStack.pop_back();
    publishMaster();
}

void EnochSoloProcessor::commitRaw (std::vector<Note> raw)
{
    std::stable_sort (raw.begin(), raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    master.raw = std::move (raw);
    Settings s = editSettings(); s.barChords = master.s.barChords;
    Generator::finalize (master, s);
    publishMaster();
}

void EnochSoloProcessor::setBarChordRange (int bar0, int bar1, bool set, int rootPc, Quality q)
{
    pushUndo();
    const Settings base = editSettings();
    if ((int) barChords.size() < base.bars) barChords.resize ((size_t) base.bars);
    const int degree = ((rootPc - base.keyRoot) % 12 + 12) % 12;
    for (int b = std::max (0, bar0); b <= bar1 && b < base.bars; ++b) barChords[(size_t) b] = { set, degree, q };
    Settings s = base; s.barChords = barChords;
    std::string pn;
    const auto newChords = Generator::chordsFor (s, pn);
    Generator::reharmonize (master, master.chords, newChords, s);
    master.progName = pn;
    s.barChords = barChords; master.s.barChords = barChords;
    Generator::finalize (master, s);
    publishMaster();
}

void EnochSoloProcessor::setBarChord (int bar, bool set, int rootPc, Quality q) { setBarChordRange (bar, bar, set, rootPc, q); }

bool EnochSoloProcessor::hasBarChords() const
{
    for (auto& b : barChords) if (b.set) return true;
    return false;
}

void EnochSoloProcessor::clearBarChords()
{
    if (! hasBarChords()) return;
    pushUndo();
    barChords.clear();
    Settings s = editSettings(); s.barChords.clear();
    std::string pn;
    const auto newChords = Generator::chordsFor (s, pn);
    Generator::reharmonize (master, master.chords, newChords, s);
    master.progName = pn; master.s.barChords.clear();
    Generator::finalize (master, s);
    publishMaster();
}

void EnochSoloProcessor::rerollBars (int bar0, int bar1)
{
    pushUndo();
    Settings s = editSettings(); s.barChords = barChords;
    Generator::rerollRegion (master, s, std::max (0, bar0), std::min (master.bars, bar1 + 1), (uint64_t) juce::Random::getSystemRandom().nextInt64());
    Generator::finalize (master, s);
    publishMaster();
}

void EnochSoloProcessor::applyBusy (int bar0, int bar1, float busy, uint64_t sd)
{
    const int bars = master.bars;
    if (bars <= 0) return;
    bar0 = std::max (0, bar0); bar1 = std::min (bars - 1, bar1);
    if (bar1 < bar0) return;
    if ((int) barBusy.size() < bars) barBusy.resize ((size_t) bars, -1.f);
    for (int b = bar0; b <= bar1; ++b) barBusy[(size_t) b] = busy < 0.f ? -1.f : juce::jlimit (0.f, 1.f, busy);
    Settings s = editSettings(); s.barChords = barChords;
    Generator::rerollRegion (master, s, bar0, bar1 + 1, sd);
    Generator::finalize (master, s);
    publishMaster();
}

void EnochSoloProcessor::commitCtrl (int lane, double t0, double t1, std::vector<CtrlPoint> pts)
{
    if (lane < 0 || lane >= L_Count) return;
    if (t1 < t0) std::swap (t0, t1);
    pushUndo();
    auto& u = master.userPts;
    u.erase (std::remove_if (u.begin(), u.end(), [&] (const CtrlPoint& c) { return c.lane == lane && c.t >= t0 - 1e-9 && c.t <= t1 + 1e-9; }), u.end());
    for (auto& c : pts) { c.lane = lane; u.push_back (c); }
    std::stable_sort (u.begin(), u.end(), [] (const CtrlPoint& a, const CtrlPoint& b) { return a.t < b.t; });
    // merge the drawn range into the regions of this lane
    auto& rg = master.userRegions;
    CtrlRegion nr { lane, t0, t1 };
    for (size_t i = 0; i < rg.size();)
        if (rg[i].lane == lane && rg[i].t0 <= nr.t1 + 1e-9 && rg[i].t1 >= nr.t0 - 1e-9) { nr.t0 = std::min (nr.t0, rg[i].t0); nr.t1 = std::max (nr.t1, rg[i].t1); rg.erase (rg.begin() + (long) i); }
        else ++i;
    rg.push_back (nr);
    Settings s = editSettings(); s.barChords = master.s.barChords;
    Generator::finalize (master, s);
    publishMaster();
}

void EnochSoloProcessor::clearCtrl (int lane, double t0, double t1)
{
    bool any = false;
    for (auto& r : master.userRegions) if ((lane < 0 || r.lane == lane) && r.t1 >= t0 && r.t0 <= t1) any = true;
    for (auto& c : master.userPts) if ((lane < 0 || c.lane == lane) && c.t >= t0 && c.t <= t1) any = true;
    if (! any) return;
    pushUndo();
    auto& u = master.userPts;
    u.erase (std::remove_if (u.begin(), u.end(), [&] (const CtrlPoint& c) { return (lane < 0 || c.lane == lane) && c.t >= t0 - 1e-9 && c.t <= t1 + 1e-9; }), u.end());
    std::vector<CtrlRegion> out;
    for (auto& r : master.userRegions)
    {
        if ((lane >= 0 && r.lane != lane) || r.t1 < t0 || r.t0 > t1) { out.push_back (r); continue; }
        if (r.t0 < t0) out.push_back ({ r.lane, r.t0, t0 - 1e-6 });
        if (r.t1 > t1) out.push_back ({ r.lane, t1 + 1e-6, r.t1 });
    }
    master.userRegions = std::move (out);
    Settings s = editSettings(); s.barChords = master.s.barChords;
    Generator::finalize (master, s);
    publishMaster();
}

//==============================================================================
// phrase locking
void EnochSoloProcessor::setPhraseLocked (const Phrase& ph, bool lock) { enochsolo::setPhraseLocked (phraseLocks, ph, lock); }

void EnochSoloProcessor::setAllPhrasesLocked (bool lock)
{
    phraseLocks.clear();
    if (! lock) return;
    for (auto& ph : findPhrases (master)) enochsolo::setPhraseLocked (phraseLocks, ph, true);
}

int EnochSoloProcessor::numLockedPhrases() const
{
    if (phraseLocks.empty()) return 0;
    int c = 0;
    for (auto& ph : findPhrases (master)) if (phraseLocked (phraseLocks, ph)) ++c;
    return c;
}

void EnochSoloProcessor::rerollPhrase (const Phrase& ph)
{
    pushUndo();
    Settings s = editSettings(); s.barChords = barChords;
    Settings q = s;
    q.chordSeed = s.chordSeed ? s.chordSeed : s.seed;
    q.seed = (uint64_t) juce::Random::getSystemRandom().nextInt64(); q.chordTrack = false; q.humanize = false;
    Pattern np = Generator::generate (q);
    std::vector<Note> pad;
    for (auto& n : master.raw) if (n.track == 2) pad.push_back (n);
    np.nextId = std::max (np.nextId, master.nextId);
    LockRanges keep { { 0.0, ph.r0 }, { ph.r1, 1e9 } };
    mergeLocked (np, master, keep);
    for (auto& n : pad) np.raw.push_back (n);
    std::stable_sort (np.raw.begin(), np.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    master.raw = std::move (np.raw);
    master.nextId = np.nextId;
    Generator::finalize (master, s);
    publishMaster();
}

//==============================================================================
// my styles
juce::File EnochSoloProcessor::stylesFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Enoch Bass").getChildFile ("My Styles");
    d.createDirectory();
    return d;
}

juce::StringArray EnochSoloProcessor::listMyStyles()
{
    juce::StringArray names;
    for (auto& f : stylesFolder().findChildFiles (juce::File::findFiles, false, "*.enochstyle")) names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool EnochSoloProcessor::saveMyStyle (const juce::String& nameIn, juce::String& message)
{
    const juce::String name = nameIn.removeCharacters ("\\/:*?\"<>|").trim().substring (0, 48);
    if (name.isEmpty()) { message = "Type a name for the style first."; return false; }
    juce::ValueTree t ("EnochStyle");
    t.setProperty ("name", name, nullptr);
    for (auto* id : { "inst", "artist", "genre", "mode", "octave", "harmony", "refTempo", "refShift" })
        t.setProperty (id, getI (apvts, id), nullptr);
    for (auto* id : { "intensity", "space", "repeat", "busy", "refAmount", "hTiming", "hVel", "hLength", "hFeel" })
        t.setProperty (id, (double) getF (apvts, id), nullptr);
    t.setProperty ("humanize", getB (apvts, "humanize") ? 1 : 0, nullptr);
    if (refProfile && refProfile->valid())
    {
        t.setProperty ("refName", juce::String (refProfile->name), nullptr);
        t.setProperty ("refBpm", refProfile->bpm, nullptr);
        t.setProperty ("refPhase", refProfile->phase, nullptr);
        t.setProperty ("refLen", refProfile->lengthSec, nullptr);
        t.setProperty ("refMidi", refProfile->fromMidi ? 1 : 0, nullptr);
        juce::String rn;
        for (auto& n : refProfile->notes) rn << juce::String (n.t, 4) << ':' << juce::String (n.dur, 4) << ':' << juce::String (n.loud, 3) << ':' << n.pitch << ';';
        t.setProperty ("refNotes", rn, nullptr);
    }
    auto xml = t.createXml();
    if (xml == nullptr) { message = "Could not save the style."; return false; }
    const auto f = stylesFolder().getChildFile (name + ".enochstyle");
    if (! xml->writeTo (f)) { message = "Could not write " + f.getFullPathName(); return false; }
    message = "Saved \"" + name + "\".";
    return true;
}

bool EnochSoloProcessor::loadMyStyle (const juce::String& name, juce::String& message)
{
    const auto f = stylesFolder().getChildFile (name + ".enochstyle");
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
    if (xml == nullptr || ! xml->hasTagName ("EnochStyle")) { message = "Could not read that style."; return false; }
    const auto t = juce::ValueTree::fromXml (*xml);
    loadingState = true;
    for (auto* id : { "inst", "artist", "genre", "mode", "octave", "harmony", "refTempo", "refShift" })
        if (t.hasProperty (id)) setParam (id, (float) (int) t.getProperty (id));
    for (auto* id : { "intensity", "space", "repeat", "busy", "refAmount", "hTiming", "hVel", "hLength", "hFeel" })
        if (t.hasProperty (id)) setParam (id, (float) (double) t.getProperty (id));
    if (t.hasProperty ("humanize")) setParam ("humanize", (int) t.getProperty ("humanize") != 0 ? 1.f : 0.f);
    refProfile.reset();
    if (t.hasProperty ("refNotes"))
    {
        auto rp = std::make_shared<RefProfile>();
        rp->name = t.getProperty ("refName").toString().toStdString();
        rp->bpm = (double) t.getProperty ("refBpm", 0.0);
        rp->phase = (double) t.getProperty ("refPhase", 0.0);
        rp->lengthSec = (double) t.getProperty ("refLen", 0.0);
        rp->fromMidi = (int) t.getProperty ("refMidi", 0) != 0;
        for (auto& tok : juce::StringArray::fromTokens (t.getProperty ("refNotes").toString(), ";", ""))
        {
            auto fl = juce::StringArray::fromTokens (tok, ":", "");
            if (fl.size() != 4) continue;
            RefRaw n; n.t = fl[0].getDoubleValue(); n.dur = fl[1].getDoubleValue(); n.loud = fl[2].getFloatValue(); n.pitch = fl[3].getIntValue();
            rp->notes.push_back (n);
        }
        if (rp->valid()) refProfile = rp;
    }
    barBusy.clear();
    loadingState = false;
    needRegen = true; timerCallback();
    tryAnother();
    message = "Loaded \"" + name + "\".";
    return true;
}

bool EnochSoloProcessor::deleteMyStyle (const juce::String& name)
{
    return stylesFolder().getChildFile (name + ".enochstyle").deleteFile();
}

bool EnochSoloProcessor::learnFromSolo (juce::String& message)
{
    auto snap = getSnapshot();
    if (! snap) { message = "Nothing to learn from yet."; return false; }
    const auto& P = snap->pattern;
    const double spb = 60.0 / std::max (20.0, P.s.bpm);
    std::vector<RefRaw> notes;
    for (auto& n : P.raw)
    {
        if (n.track != 0 || n.voice != 0 || n.mute) continue;
        RefRaw r; r.t = n.start * spb; r.dur = std::max (0.03, n.length * spb); r.pitch = n.pitch; r.loud = n.vel / 127.0f;
        notes.push_back (r);
    }
    if (notes.size() < 4) { message = "This bass line has too few notes to learn from."; return false; }
    std::vector<double> lv; for (auto& n : notes) lv.push_back (n.loud);
    std::sort (lv.begin(), lv.end());
    const double lo = lv[(size_t) (lv.size() * 0.1)], hi = lv[std::min (lv.size() - 1, (size_t) (lv.size() * 0.9))];
    for (auto& n : notes) n.loud = (float) juce::jlimit (0.0, 1.0, (n.loud - lo) / std::max (0.1, hi - lo));
    auto prof = std::make_shared<RefProfile>();
    prof->name = "My bass line - " + P.styleName;
    prof->fromMidi = true; prof->bpm = P.s.bpm; prof->phase = 0.0; prof->lengthSec = P.lengthBeats * spb; prof->notes = std::move (notes);
    if (! prof->valid()) { message = "This bass line has too few notes to learn from."; return false; }
    refProfile = prof;
    message = juce::String();
    tryAnother();
    return true;
}

//==============================================================================
// presets
namespace
{
struct Preset { const char* name; int inst; const char* artist; int genre; int key; int scale; int harmony; float intensity; int bars; };
const Preset kPresets[] = {
    { "Afrobeat - Classic ostinato",              I_Bass, "Afrobeat Classic",             G_Afrobeat,  9, SC_Minor,       0, 0.75f, 3 },
    { "Afrobeat - Slap funk (Larry Graham style)",I_Bass, "Larry Graham Style",           G_Afrobeat,  4, SC_Dorian,      0, 0.80f, 3 },
    { "Afrobeats - Laid-back pocket",             I_Bass, "Afrobeats Laid-back",          G_AfroFusion,6, SC_Minor,       1, 0.50f, 3 },
    { "Amapiano - Log-drum bass",                 I_Bass, "Amapiano Log-Drum Bass",       G_Amapiano,  6, SC_Minor,       1, 0.60f, 3 },
    { "Amapiano - Private-school glide",          I_Bass, "Amapiano Private-School Glide",G_Amapiano,  1, SC_Minor,       1, 0.50f, 3 },
    { "Highlife - Bounce",                        I_Bass, "Highlife Bounce",              G_Highlife,  7, SC_Major,       1, 0.65f, 3 },
    { "Juju - Interlock",                         I_Bass, "Juju Interlock",               G_Juju,      2, SC_Major,       1, 0.65f, 3 },
    { "Soukous - Runner",                         I_Bass, "Soukous Runner",               G_Soukous,   0, SC_Major,       1, 0.80f, 3 },
    { "Ethio-Jazz - Modal pulse",                 I_Bass, "Ethio-Jazz Modal Pulse",       G_EthioJazz, 9, SC_Dorian,      0, 0.60f, 3 },
    { "Township Jive - Walk",                     I_Bass, "Township Jive Walk",           G_Township,  5, SC_Major,       1, 0.65f, 3 },
    { "Afro-Fusion - Bakithi Kumalo style",       I_Bass, "Bakithi Kumalo Style",         G_AfroFusion,7, SC_Major,       1, 0.60f, 3 },
    { "Neo-soul - Pino Palladino style",          I_Bass, "Pino Palladino Style",         G_AfroFusion,3, SC_Minor,       1, 0.45f, 3 },
    { "Gospel - Walk-ups",                        I_Bass, "Gospel Walk-ups",              G_Gospel,   10, SC_Major,       1, 0.60f, 3 },
    { "Pop - Driving eighths",                    I_Bass, "Pop Driving Eighths",          G_Pop,       9, SC_Minor,       1, 0.70f, 3 },
    { "Rock - Palm-muted eighths",                I_Bass, "Rock Eighths (Palm-muted)",    G_Rock,      4, SC_MinPent,     1, 0.80f, 3 },
    { "Swing - Walking bass",                     I_Bass, "Ron Carter Style Walking",     G_Swing,     5, SC_Major,       1, 0.60f, 3 },
    { "Bossa Nova - Root-fifth",                  I_Bass, "Bossa Root-Fifth",             G_Bossa,     2, SC_Minor,       1, 0.45f, 3 },
    { "Samba - Partido",                          I_Bass, "Samba Partido",                G_Samba,     7, SC_Major,       1, 0.70f, 3 },
    { "Latin - Tumbao",                           I_Bass, "Latin Tumbao",                 G_Salsa,     9, SC_Minor,       1, 0.60f, 3 },
    { "Salsa - Tumbao (clave-aware)", I_Bass, "Salsa Tumbao (Clave-Aware)", G_Salsa, 2, SC_Minor, 1, 0.60f, 3 },
    { "Salsa - Dura (busy New York)", I_Bass, "Salsa Dura Busy", G_Salsa, 7, SC_Minor, 1, 0.85f, 3 },
    { "Son Montuno - Anticipated bass", I_Bass, "Son Montuno Anticipated Bass", G_AfroCuban, 0, SC_Major, 1, 0.55f, 3 },
    { "Mambo - Driving pulse", I_Bass, "Mambo Driving Pulse", G_AfroCuban, 9, SC_Minor, 1, 0.85f, 3 },
    { "Rumba - Guaguanco pulse", I_Bass, "Rumba Guaguanco Pulse", G_AfroCuban, 4, SC_Minor, 1, 0.75f, 3 },
    { "Cha-Cha-Cha - Walk", I_Bass, "Cha-Cha-Cha Walk", G_AfroCuban, 5, SC_Major, 1, 0.55f, 3 },
    { "Bolero - Slow pulse", I_Bass, "Bolero Slow Pulse", G_AfroCuban, 0, SC_Major, 1, 0.40f, 3 },
    { "Timba - Funk-Latin slap", I_Bass, "Timba / Songo Funk-Latin", G_AfroCuban, 2, SC_Minor, 1, 0.90f, 3 },
    { "Latin Jazz - Bajo anticipado", I_Bass, "Latin Jazz Bajo Anticipado", G_AfroCuban, 9, SC_Dorian, 1, 0.70f, 3 },
    { "Baiao - Northeast Brazil", I_Bass, "Baiao Pulse", G_AfroBrazilian, 7, SC_Mixolydian, 1, 0.65f, 3 },
    { "Maracatu - Alfaia drive", I_Bass, "Maracatu Alfaia Drive", G_AfroBrazilian, 4, SC_Minor, 1, 0.85f, 3 },
    { "Afoxe / Ijexa - Flow", I_Bass, "Afoxe / Ijexa Flow", G_AfroBrazilian, 2, SC_Minor, 1, 0.65f, 3 },
    { "Samba-Reggae - Olodum punch", I_Bass, "Samba-Reggae Olodum Punch", G_AfroBrazilian, 9, SC_Minor, 1, 0.85f, 3 },
    { "Axe - Bahia bounce", I_Bass, "Axe Bahia Bounce", G_AfroBrazilian, 0, SC_Major, 1, 0.85f, 3 },
    { "Forro - Oom-pah", I_Bass, "Forro Oom-Pah", G_AfroBrazilian, 7, SC_Major, 1, 0.65f, 3 },
    { "Cumbia - Roll", I_Bass, "Cumbia Colombiana Roll", G_Caribbean, 9, SC_Minor, 1, 0.60f, 3 },
    { "Merengue - Tambora drive", I_Bass, "Merengue Tambora Drive", G_Caribbean, 7, SC_Major, 1, 0.90f, 3 },
    { "Bachata - Bounce", I_Bass, "Bachata Bounce", G_Caribbean, 9, SC_Major, 1, 0.65f, 3 },
    { "Zouk - Smooth glide", I_Bass, "Zouk Smooth Glide", G_Caribbean, 2, SC_Minor, 1, 0.50f, 3 },
    { "Reggae - One-drop roots", I_Bass, "Reggae One-Drop Roots", G_Reggae, 7, SC_Major, 1, 0.55f, 3 },
    { "Dub - Heavy sub", I_Bass, "Dub Heavy Sub", G_Reggae, 9, SC_Minor, 1, 0.35f, 3 },
    { "Dancehall - Riddim", I_Bass, "Dancehall Riddim", G_Reggae, 9, SC_Minor, 1, 0.55f, 3 },
    { "Funk - Pocket sixteenths", I_Bass, "Funk Pocket Sixteenths", G_Funk, 4, SC_Dorian, 1, 0.85f, 3 },
    { "Disco - Octaves", I_Bass, "Disco Octaves", G_Funk, 9, SC_Major, 1, 0.85f, 3 },
    { "Hip-Hop - Boom-bap", I_Bass, "Boom-Bap Hip-Hop", G_HipHop, 0, SC_Minor, 1, 0.45f, 3 },
    { "Trap - 808 slides", I_Bass, "Trap 808 Slides", G_HipHop, 6, SC_Minor, 1, 0.60f, 3 },
    { "Afro-House - Rolling offbeat", I_Bass, "Afro-House Rolling Offbeat", G_AfroHouse, 9, SC_Minor, 1, 0.75f, 3 },
    { "Gqom - Dark bounce", I_Bass, "Gqom Dark Bounce", G_AfroHouse, 6, SC_Minor, 1, 0.45f, 3 },
};
}

int EnochSoloProcessor::numPresets() { return (int) (sizeof (kPresets) / sizeof (kPresets[0])); }
juce::String EnochSoloProcessor::presetName (int i) { return (i >= 0 && i < numPresets()) ? juce::String (kPresets[i].name) : juce::String(); }

void EnochSoloProcessor::setParam (const char* id, float value)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

void EnochSoloProcessor::applyPreset (int i)
{
    if (i < 0 || i >= numPresets()) return;
    currentPreset = i;
    const auto& pr = kPresets[i];
    int artistIdx = 0;
    const auto& idx = stylesFor (pr.inst);
    for (size_t k = 0; k < idx.size(); ++k) if (allStyles()[(size_t) idx[k]].name == pr.artist) artistIdx = (int) k;
    loadingState = true;                       // set everything first, then regenerate once
    barChords.clear();
    setParam ("inst", (float) pr.inst); setParam ("artist", (float) artistIdx); setParam ("genre", (float) pr.genre);
    setParam ("key", (float) pr.key); setParam ("scale", (float) pr.scale); setParam ("harmony", (float) pr.harmony);
    setParam ("intensity", pr.intensity); setParam ("bars", (float) pr.bars);
    setParam ("mode", 0.f); setParam ("space", 0.5f); setParam ("repeat", 0.5f); setParam ("busy", 0.5f);
    barBusy.clear();
    loadingState = false;
    chordText = juce::String();
    needRegen = true; timerCallback();
    tryAnother();
}

void EnochSoloProcessor::timerCallback()
{
    if (auditionNote >= 0 && (juce::int32) (juce::Time::getMillisecondCounter() - auditionOffAt) >= 0)
    {
        auditionState.noteOff (1, auditionNote, 0.f);
        auditionNote = -1;
    }
    // host tempo / time signature changes
    const int ts = getI (apvts, "ts");
    if (ts == 0 && (hostNum.load() != lastTsNum || hostDen.load() != lastTsDen)) needRegen = true;
    if (std::abs (hostBpm.load() - lastBpmUsed) > 0.5) needFinalize = true;

    if (needRegen.exchange (false))
    {
        needFinalize = false;
        regenerate (false, false);
        if (! history.empty() && histIndex >= 0) history[(size_t) histIndex] = { seed, hSeed };
    }
    else if (needFinalize.exchange (false))
        refinalize();
}

void EnochSoloProcessor::setChordText (const juce::String& t)
{
    chordText = t.trim();
    std::vector<ParsedChord> pc; std::string err;
    const bool ok = chordText.isNotEmpty() && parseChordText (chordText.toStdString(), pc, err);
    if (chordText.isEmpty()) chordStatus = juce::String();
    else if (! ok)           chordStatus = juce::String (err);
    else                     chordStatus = err.empty() ? juce::String() : juce::String (err) + " (that one was skipped)";
    if (ok)
        if (auto* prm = apvts.getParameter ("harmony"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (2.0f));
            prm->endChangeGesture();
        }
    needRegen = true;
    timerCallback();
}

void EnochSoloProcessor::tryAnother()
{
    needRegen = false; needFinalize = false;
    pushUndo();
    regenerate (true, true);
}

void EnochSoloProcessor::historyBack()
{
    if (! canGoBack()) return;
    pushUndo();
    --histIndex; seed = history[(size_t) histIndex].first; hSeed = history[(size_t) histIndex].second;
    regenerate (false, false);
}
void EnochSoloProcessor::historyForward()
{
    if (! canGoForward()) return;
    pushUndo();
    ++histIndex; seed = history[(size_t) histIndex].first; hSeed = history[(size_t) histIndex].second;
    regenerate (false, false);
}
void EnochSoloProcessor::rehumanize()
{
    hSeed = (uint64_t) juce::Random::getSystemRandom().nextInt64();
    if (histIndex >= 0) history[(size_t) histIndex].second = hSeed;
    refinalize();
}

//==============================================================================
juce::String EnochSoloProcessor::suggestedFileName() const
{
    auto snap = getSnapshot();
    if (! snap) return "Enoch Bass.mid";
    const auto& s = snap->pattern.s;
    juce::String style = snap->pattern.styleName;
    juce::String name = juce::String ("Enoch Bass - ") + instName (s.instrument) + " - " + style + " - "
                      + kNoteNames[s.keyRoot] + (scaleFamily (s.scale) == 1 ? "m" : "") + " - " + juce::String (s.bars) + " bars - "
                      + juce::String (s.tsNum) + "-" + juce::String (s.tsDen) + " - " + juce::String ((int) std::lround (s.bpm)) + "bpm";
    return name.removeCharacters ("\\/:*?\"<>|") + ".mid";
}

bool EnochSoloProcessor::saveMidiTo (const juce::File& f)
{
    auto snap = getSnapshot();
    if (! snap) return false;
    const auto bytes = MidiWriter::build (snap->pattern);
    f.deleteFile();
    return f.replaceWithData (bytes.data(), bytes.size());
}

juce::File EnochSoloProcessor::writeMidiToTemp()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Enoch Bass");
    dir.createDirectory();
    // tidy old drags
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.mid"))
        if (f.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::hours (6)) f.deleteFile();
    auto f = dir.getChildFile (suggestedFileName());
    saveMidiTo (f);
    return f;
}

//==============================================================================
void EnochSoloProcessor::setInternalPlaying (bool p)
{
    internalPlaying = p;
}

void EnochSoloProcessor::audition (int pitch, int velocity)
{
    if (auditionNote >= 0) auditionState.noteOff (1, auditionNote, 0.f);
    auditionNote = juce::jlimit (0, 127, pitch);
    auditionState.noteOn (1, auditionNote, juce::jlimit (1, 127, velocity) / 127.0f);
    auditionOffAt = juce::Time::getMillisecondCounter() + 380;
}

void EnochSoloProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    player.prepare (sampleRate);
    internalBeat = 0; expectedNext = -1; wasRunning = false;
    genBuffer.ensureSize (8192); playBuffer.ensureSize (8192);
}

bool EnochSoloProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void EnochSoloProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // ---------------------------------------------------------------- pick up latest pattern (never block)
    {
        const juce::SpinLock::ScopedTryLockType l (snapLock);
        if (l.isLocked()) audioSnap = snapshot;
    }

    // ---------------------------------------------------------------- host info
    bool playing = false; double ppq = 0, bpm = hostBpm.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = *b; hostBpm = bpm; }
            if (auto ts = pos->getTimeSignature()) { hostNum = ts->numerator; hostDen = ts->denominator; }
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    hostPlaying = playing;
    bpm = juce::jlimit (20.0, 400.0, bpm);

    // ---------------------------------------------------------------- player configuration
    const int inst = getI (apvts, "inst");
    const auto& st = styleFor (inst, juce::jlimit (0, std::max (0, maxArtists() - 1), getI (apvts, "artist")));
    if (inst != lastInstrument) { player.reset(); lastInstrument = inst; }
    player.setInstrument (inst, st.variant);
    player.setKeySwitches (getI (apvts, "ksBase"), getI (apvts, "bendRange"));
    {
        const auto& ll = getLibraries (inst);
        const int li = getI (apvts, "library");
        const enochsolo::Library* lp = (li > 0 && li < (int) ll.size()) ? ll[(size_t) li].get() : nullptr;
        if (lp != lastLibPtr || inst != lastLibInst || getI (apvts, "ksBase") != lastLibBase)
        { player.configureKeySwitches (lp, inst, getI (apvts, "ksBase")); lastLibPtr = lp; lastLibInst = inst; lastLibBase = getI (apvts, "ksBase"); }
    }
    player.setLevel (getF (apvts, "volume"), getF (apvts, "reverb"));
    player.useSoundFont = getB (apvts, "sfOn");
    player.useSample = getB (apvts, "smpOn");
    { const int ri = getI (apvts, "smpRoot"); player.smp.setRootOverride (ri > 0 ? 11 + ri : 0); }

    // notes auditioned from the piano roll join the incoming MIDI
    auditionState.processNextMidiBuffer (midi, 0, n, true);

    // ---------------------------------------------------------------- transport
    {
        const double sk = seekBeat.exchange (-1.0);
        if (sk >= 0) { internalBeat = sk; expectedNext = -1.0; }
    }
    const bool followHost = getB (apvts, "hostSync");
    const bool loop = getB (apvts, "loop");
    // the solo can sit anywhere in the project: "starts at bar N" moves where it begins on the host timeline
    const double barBeatsNow = audioSnap ? std::max (0.5, audioSnap->pattern.barBeats) : 4.0;
    const double startOffset = (double) getI (apvts, "startBar") * barBeatsNow;
    const double blockBeats0 = (double) n * bpm / (60.0 * sr);
    const bool hostActive = followHost && playing && (ppq - startOffset) > -blockBeats0;      // before the start bar the solo stays silent
    const bool running = hostActive || internalPlaying.load();
    const bool useHost = hostActive;
    genBuffer.clear(); playBuffer.clear();
    const bool sendMidiOut = getB (apvts, "midiOut");
    auto flushHeld = [&]
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int nt = 0; nt < 128; ++nt)
                if (heldOut[ch][nt]) { genBuffer.addEvent (juce::MidiMessage::noteOff (ch + 1, nt), 0); heldOut[ch][nt] = false; }
    };

    if (audioSnap && running)
    {
        const double len = audioSnap->lengthBeats;
        const double blockBeats = (double) n * bpm / (60.0 * sr);
        double b0 = useHost ? ppq - startOffset : internalBeat;
        if (loop && len > 0 && b0 >= 0.0) b0 = std::fmod (b0, len);
        const bool discontinuity = ! wasRunning || expectedNext < 0 || std::abs (b0 - expectedNext) > std::max (0.02, blockBeats * 0.75);
        if (discontinuity && wasRunning) { player.allNotesOff(); if (sendMidiOut) flushHeld(); }
        auto emit = [&] (double from, double to, double sampleBase)
        {
            auto run = [&] (const std::vector<Playback>& evs, juce::MidiBuffer& dest)
            {
                auto it = std::lower_bound (evs.begin(), evs.end(), from, [] (const Playback& e, double v) { return e.beat < v; });
                for (; it != evs.end() && it->beat < to; ++it)
                {
                    const int pos = (int) std::lround (sampleBase + (it->beat - from) * 60.0 * sr / bpm);
                    dest.addEvent (it->msg, juce::jlimit (0, n - 1, pos));
                }
            };
            run (audioSnap->testEvents, playBuffer);
            if (sendMidiOut) run (audioSnap->events, genBuffer);
        };
        double endBeat = b0 + blockBeats;
        if (loop && len > 0 && endBeat > len)
        {
            emit (b0, len, 0.0);
            const double wrapSample = (len - b0) * 60.0 * sr / bpm;
            emit (0.0, endBeat - len, wrapSample);
            expectedNext = endBeat - len;
        }
        else
        {
            if (! loop && b0 >= len) { if (wasRunning) { player.allNotesOff(); flushHeld(); } }
            else emit (b0, endBeat, 0.0);
            expectedNext = endBeat;
        }
        playBeat = b0;
        if (! useHost) { internalBeat += blockBeats; if (loop && len > 0) internalBeat = std::fmod (internalBeat, len); else if (internalBeat > len + 1.0) internalPlaying = false; }
    }
    else
    {
        if (wasRunning) { player.allNotesOff(); flushHeld(); }
        playBeat = -1.0; expectedNext = -1.0;
        if (! internalPlaying.load()) internalBeat = 0.0;
    }
    wasRunning = running;

    // ---------------------------------------------------------------- feed player (incoming MIDI + generated MIDI), sample accurate
    if (getB (apvts, "playerOn"))
    {
        juce::MidiBuffer all;
        all.addEvents (midi, 0, n, 0);
        all.addEvents (playBuffer, 0, n, 0);
        int pos = 0;
        for (const auto meta : all)
        {
            const int t = juce::jlimit (0, n, meta.samplePosition);
            if (t > pos) { player.render (buffer, pos, t - pos); pos = t; }
            player.handle (meta.getMessage());
        }
        if (pos < n) player.render (buffer, pos, n - pos);
    }

    // ---------------------------------------------------------------- MIDI out
    if (! sendMidiOut && wasSendingOut) flushHeld();               // switched off while notes were sounding
    midi.clear();
    if (sendMidiOut || wasSendingOut)
    {
        for (const auto meta : genBuffer)
        {
            const auto m = meta.getMessage();
            if (m.getChannel() < 1) continue;
            if (m.isNoteOn()) heldOut[m.getChannel() - 1][m.getNoteNumber()] = true;
            else if (m.isNoteOff()) heldOut[m.getChannel() - 1][m.getNoteNumber()] = false;
        }
        midi.addEvents (genBuffer, 0, n, 0);
    }
    wasSendingOut = sendMidiOut;
}

//==============================================================================
juce::AudioProcessorEditor* EnochSoloProcessor::createEditor() { return new EnochSoloEditor (*this); }

void EnochSoloProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("seed", juce::String ((juce::int64) seed), nullptr);
    state.setProperty ("hSeed", juce::String ((juce::int64) hSeed), nullptr);
    juce::StringArray hist;
    for (auto& h : history) hist.add (juce::String ((juce::int64) h.first) + ":" + juce::String ((juce::int64) h.second));
    state.setProperty ("history", hist.joinIntoString (","), nullptr);
    state.setProperty ("histIndex", histIndex, nullptr);
    state.setProperty ("chords", chordText, nullptr);
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto snap = getSnapshot())
    {
        // the notes themselves (so piano-roll edits survive saving the project) and the per-bar chords
        juce::String notes;
        for (auto& n : snap->pattern.raw)
            notes << n.id << ',' << juce::String (n.start, 5) << ',' << juce::String (n.length, 5) << ',' << n.pitch << ',' << n.vel << ',' << n.art << ','
                  << n.track << ',' << n.voice << ',' << juce::String (n.weight, 3) << ',' << n.bend << ',' << (n.vib ? 1 : 0) << ',' << (n.mute ? 1 : 0) << ',' << n.role << ';';
        state.setProperty ("notes", notes, nullptr);
        state.setProperty ("notesNextId", snap->pattern.nextId, nullptr);
        state.setProperty ("notesBars", snap->pattern.bars, nullptr);
        state.setProperty ("notesInst", snap->pattern.s.instrument, nullptr);
        juce::String cp, cr;
        for (auto& c : snap->pattern.userPts) cp << c.lane << ':' << juce::String (c.t, 5) << ':' << c.value << ';';
        for (auto& r : snap->pattern.userRegions) cr << r.lane << ':' << juce::String (r.t0, 5) << ':' << juce::String (r.t1, 5) << ';';
        state.setProperty ("ctrlPts", cp, nullptr);
        state.setProperty ("ctrlRegions", cr, nullptr);
    }
    state.setProperty ("smpPath", bassSamplePath, nullptr);
    if (grooveProfile && grooveProfile->valid())
    {
        state.setProperty ("grvName", juce::String (grooveProfile->name), nullptr);
        state.setProperty ("grvBpm", grooveProfile->bpm, nullptr);
        state.setProperty ("grvPhase", grooveProfile->phase, nullptr);
        state.setProperty ("grvLen", grooveProfile->lengthSec, nullptr);
        state.setProperty ("grvMidi", grooveProfile->fromMidi ? 1 : 0, nullptr);
        juce::String gn;
        for (auto& n : grooveProfile->notes) gn << juce::String (n.t, 4) << ':' << juce::String (n.dur, 4) << ':' << juce::String (n.loud, 3) << ':' << n.pitch << ';';
        state.setProperty ("grvNotes", gn, nullptr);
    }
    if (refProfile && refProfile->valid())
    {
        state.setProperty ("refName", juce::String (refProfile->name), nullptr);
        state.setProperty ("refBpm", refProfile->bpm, nullptr);
        state.setProperty ("refPhase", refProfile->phase, nullptr);
        state.setProperty ("refLen", refProfile->lengthSec, nullptr);
        state.setProperty ("refMidi", refProfile->fromMidi ? 1 : 0, nullptr);
        juce::String rn;
        for (auto& n : refProfile->notes) rn << juce::String (n.t, 4) << ':' << juce::String (n.dur, 4) << ':' << juce::String (n.loud, 3) << ':' << n.pitch << ';';
        state.setProperty ("refNotes", rn, nullptr);
    }
    {
        juce::String bb; for (float v : barBusy) bb << juce::String (v, 3) << ',';
        state.setProperty ("barBusy", bb, nullptr);
        juce::String bc;
        for (auto& b : barChords) bc << (b.set ? 1 : 0) << ':' << b.degree << ':' << (int) b.q << ':' << b.mode << ',';
        state.setProperty ("barChords", bc, nullptr);
        state.setProperty ("sfPath", soundFontPath, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void EnochSoloProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = juce::ValueTree::fromXml (*xml);
    loadingState = true;
    apvts.replaceState (vt);
    loadingState = false;
    seed = (uint64_t) vt.getProperty ("seed", juce::String ((juce::int64) seed)).toString().getLargeIntValue();
    hSeed = (uint64_t) vt.getProperty ("hSeed", juce::String ((juce::int64) hSeed)).toString().getLargeIntValue();
    history.clear();
    for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("history").toString(), ",", ""))
    {
        auto parts = juce::StringArray::fromTokens (tok, ":", "");
        if (parts.size() == 2) history.push_back ({ (uint64_t) parts[0].getLargeIntValue(), (uint64_t) parts[1].getLargeIntValue() });
    }
    chordText = vt.getProperty ("chords").toString();
    chordStatus = juce::String();
    grooveProfile.reset();
    if (vt.hasProperty ("grvNotes"))
    {
        auto gp = std::make_shared<RefProfile>();
        gp->name = vt.getProperty ("grvName").toString().toStdString();
        gp->bpm = (double) vt.getProperty ("grvBpm", 0.0);
        gp->phase = (double) vt.getProperty ("grvPhase", 0.0);
        gp->lengthSec = (double) vt.getProperty ("grvLen", 0.0);
        gp->fromMidi = (int) vt.getProperty ("grvMidi", 0) != 0;
        for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("grvNotes").toString(), ";", ""))
        {
            auto f = juce::StringArray::fromTokens (tok, ":", "");
            if (f.size() != 4) continue;
            RefRaw n; n.t = f[0].getDoubleValue(); n.dur = f[1].getDoubleValue(); n.loud = f[2].getFloatValue(); n.pitch = f[3].getIntValue();
            gp->notes.push_back (n);
        }
        if (gp->valid()) grooveProfile = gp;
    }
    refProfile.reset();
    if (vt.hasProperty ("refNotes"))
    {
        auto rp = std::make_shared<RefProfile>();
        rp->name = vt.getProperty ("refName").toString().toStdString();
        rp->bpm = (double) vt.getProperty ("refBpm", 0.0);
        rp->phase = (double) vt.getProperty ("refPhase", 0.0);
        rp->lengthSec = (double) vt.getProperty ("refLen", 0.0);
        rp->fromMidi = (int) vt.getProperty ("refMidi", 0) != 0;
        for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("refNotes").toString(), ";", ""))
        {
            auto f = juce::StringArray::fromTokens (tok, ":", "");
            if (f.size() != 4) continue;
            RefRaw n; n.t = f[0].getDoubleValue(); n.dur = f[1].getDoubleValue(); n.loud = f[2].getFloatValue(); n.pitch = f[3].getIntValue();
            rp->notes.push_back (n);
        }
        if (rp->valid()) refProfile = rp;
    }
    histIndex = juce::jlimit (-1, (int) history.size() - 1, (int) vt.getProperty ("histIndex", (int) history.size() - 1));
    if (history.empty()) { history.push_back ({ seed, hSeed }); histIndex = 0; }
    currentPreset = juce::jlimit (0, std::max (0, numPresets() - 1), (int) vt.getProperty ("preset", 0));

    barChords.clear();
    for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("barChords").toString(), ",", ""))
    {
        auto parts = juce::StringArray::fromTokens (tok, ":", "");
        if (parts.size() == 3 || parts.size() == 4)
        {
            BarChord b; b.set = parts[0].getIntValue() != 0; b.degree = parts[1].getIntValue(); b.q = (Quality) juce::jlimit (0, (int) Quality::Count - 1, parts[2].getIntValue());
            if (parts.size() == 4) b.mode = juce::jlimit (-1, (int) M_Count - 1, parts[3].getIntValue());
            barChords.push_back (b);
        }
    }
    bassSamplePath = vt.getProperty ("smpPath").toString();
    if (bassSamplePath.isNotEmpty() && juce::File (bassSamplePath).existsAsFile()) { juce::String m; auto s = loadBassSample_ (juce::File (bassSamplePath), m); player.smp.setSample (s); if (! s) bassSamplePath = juce::String(); }
    else { player.smp.setSample (nullptr); bassSamplePath = juce::String(); }
    soundFontPath = vt.getProperty ("sfPath").toString();
    if (soundFontPath.isNotEmpty() && juce::File (soundFontPath).existsAsFile()) { juce::String m; player.sf.load (juce::File (soundFontPath), sr, m); }
    else { player.sf.unload(); if (soundFontPath.isNotEmpty()) soundFontPath = juce::String(); }
    barBusy.clear();
    for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("barBusy").toString(), ",", "")) barBusy.push_back (tok.getFloatValue());
    undoStack.clear(); redoStack.clear();

    // restore the exact notes (including edits) when they were saved
    const juce::String notes = vt.getProperty ("notes").toString();
    needRegen = true;
    if (notes.isNotEmpty())
    {
        Settings s = currentSettings();
        Pattern p = Generator::makeShell (s);
        std::vector<Note> raw;
        for (auto& tok : juce::StringArray::fromTokens (notes, ";", ""))
        {
            auto f = juce::StringArray::fromTokens (tok, ",", "");
            if (f.size() < 11) continue;
            Note n; n.id = f[0].getIntValue(); n.start = f[1].getDoubleValue(); n.length = f[2].getDoubleValue();
            n.pitch = juce::jlimit (0, 127, f[3].getIntValue()); n.vel = juce::jlimit (1, 127, f[4].getIntValue());
            n.art = juce::jlimit (0, (int) A_Count - 1, f[5].getIntValue()); n.track = juce::jlimit (0, 2, f[6].getIntValue()); n.voice = f[7].getIntValue();
            n.weight = f[8].getDoubleValue(); n.bend = f[9].getIntValue(); n.vib = f[10].getIntValue() != 0;
            n.mute = f.size() > 11 && f[11].getIntValue() != 0;
            n.role = f.size() > 12 ? f[12].getIntValue() : 0;
            raw.push_back (n);
        }
        if (! raw.empty() && (int) vt.getProperty ("notesBars", -1) == p.bars && (int) vt.getProperty ("notesInst", -1) == s.instrument)
        {
            p.raw = std::move (raw);
            p.nextId = std::max ((int) vt.getProperty ("notesNextId", 1), 1);
            for (auto& n : p.raw) p.nextId = std::max (p.nextId, n.id + 1);
            for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("ctrlPts").toString(), ";", ""))
            {
                auto f = juce::StringArray::fromTokens (tok, ":", "");
                if (f.size() == 3) { CtrlPoint c; c.lane = juce::jlimit (0, (int) L_Count - 1, f[0].getIntValue()); c.t = f[1].getDoubleValue(); c.value = f[2].getIntValue(); p.userPts.push_back (c); }
            }
            for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("ctrlRegions").toString(), ";", ""))
            {
                auto f = juce::StringArray::fromTokens (tok, ":", "");
                if (f.size() == 3) p.userRegions.push_back ({ juce::jlimit (0, (int) L_Count - 1, f[0].getIntValue()), f[1].getDoubleValue(), f[2].getDoubleValue() });
            }
            std::stable_sort (p.raw.begin(), p.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
            Generator::finalize (p, s);
            master = std::move (p);
            publishMaster();
            needRegen = false;
        }
    }
}

//==============================================================================
// reference recording / MIDI
bool EnochSoloProcessor::loadReference (const juce::File& f, juce::String& message)
{
    const auto ext = f.getFileExtension().toLowerCase();
    std::shared_ptr<RefProfile> prof;
    if (ext == ".mid" || ext == ".midi")
    {
        juce::FileInputStream in (f);
        juce::MidiFile mf;
        if (! in.openedOk() || ! mf.readFrom (in)) { message = "Could not read that MIDI file."; return false; }
        double secPerQuarter = 0.5;
        for (int t = 0; t < mf.getNumTracks(); ++t)
            for (auto* ev : *mf.getTrack (t)) if (ev->message.isTempoMetaEvent()) { secPerQuarter = ev->message.getTempoSecondsPerQuarterNote(); t = 1000; break; }
        mf.convertTimestampTicksToSeconds();
        int bestTrack = -1; int bestCount = 0;
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            int c = 0;
            for (auto* ev : *mf.getTrack (t)) if (ev->message.isNoteOn() && ev->message.getChannel() != 10) ++c;
            if (c > bestCount) { bestCount = c; bestTrack = t; }
        }
        if (bestTrack < 0 || bestCount < 3) { message = "That MIDI file has no melody (need at least 3 notes)."; return false; }
        juce::MidiMessageSequence seq (*mf.getTrack (bestTrack));
        seq.updateMatchedPairs();
        std::vector<RefRaw> notes;
        double endT = 0;
        for (auto* ev : seq)
        {
            if (! ev->message.isNoteOn() || ev->message.getChannel() == 10) continue;
            RefRaw n; n.t = ev->message.getTimeStamp();
            const double off = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() : n.t + 0.25;
            n.dur = std::max (0.03, off - n.t); n.pitch = ev->message.getNoteNumber(); n.loud = ev->message.getVelocity() / 127.0f;
            endT = std::max (endT, off);
            // a chord counts as one melody note: keep the highest
            if (! notes.empty() && std::abs (notes.back().t - n.t) < 0.02) { if (n.pitch > notes.back().pitch) notes.back() = n; continue; }
            notes.push_back (n);
        }
        std::vector<double> lv; for (auto& n : notes) lv.push_back (n.loud);
        std::sort (lv.begin(), lv.end());
        const double lo = lv[(size_t) (lv.size() * 0.1)], hi = lv[std::min (lv.size() - 1, (size_t) (lv.size() * 0.9))];
        for (auto& n : notes) n.loud = (float) juce::jlimit (0.0, 1.0, (n.loud - lo) / std::max (0.1, hi - lo));
        prof = std::make_shared<RefProfile>();
        prof->name = f.getFileName().toStdString(); prof->fromMidi = true; prof->bpm = 60.0 / std::max (0.05, secPerQuarter);
        prof->phase = 0.0; prof->lengthSec = endT; prof->notes = std::move (notes);
    }
    else
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        if (rd == nullptr) { message = "Could not open that audio file (use WAV, AIFF, FLAC, MP3 or MIDI)."; return false; }
        const double sr = rd->sampleRate;
        const juce::int64 maxN = (juce::int64) (sr * 60.0);                       // the first 60 seconds are enough to learn the style
        const int n = (int) std::min<juce::int64> (rd->lengthInSamples, maxN);
        if (n < (int) (sr * 1.5)) { message = "That recording is too short (need at least 2 seconds)."; return false; }
        const int chans = (int) std::min<unsigned> (2u, rd->numChannels);
        juce::AudioBuffer<float> buf (chans, n);
        rd->read (&buf, 0, n, 0, true, chans > 1);
        std::vector<float> mono ((size_t) n, 0.f);
        for (int c = 0; c < chans; ++c) { auto* p = buf.getReadPointer (c); for (int i = 0; i < n; ++i) mono[(size_t) i] += p[i] / (float) chans; }
        prof = std::make_shared<RefProfile> (analyseAudio (mono, sr, f.getFileName().toStdString()));
    }
    if (! prof || ! prof->valid())
    {
        message = "I could not find a clear melody or rhythm in that file. A recording of one instrument playing a bass line works best.";
        return false;
    }
    refProfile = prof;
    message = juce::String();
    tryAnother();
    return true;
}

void EnochSoloProcessor::clearReference()
{
    refProfile.reset();
    needRegen = true;
    timerCallback();
}

juce::String EnochSoloProcessor::referenceName() const { return refProfile ? juce::String (refProfile->name) : juce::String(); }

juce::String EnochSoloProcessor::referenceSummary() const
{
    if (! refProfile) return juce::String();
    const Settings s = currentSettings();
    return juce::String (describeRef (*refProfile, s.refTempo, s.refShift));
}

//==============================================================================
// groove loop (drums / bass / keys of the project) and chords from your song
namespace
{
/** every note of a MIDI file in beats (quarter notes); drums (channel 10) are optional */
bool readMidiNotes (const juce::File& f, bool withDrums, std::vector<std::tuple<double, double, int, float>>& out, juce::String& message, double* bpmOut = nullptr)
{
    juce::FileInputStream in (f);
    juce::MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) { message = "Could not read that MIDI file."; return false; }
    const int tf = mf.getTimeFormat();
    if (tf <= 0) { message = "That MIDI file uses SMPTE timing, which I cannot read."; return false; }
    if (bpmOut)
    {
        double spq = 0.5;
        for (int t = 0; t < mf.getNumTracks(); ++t)
            for (auto* ev : *mf.getTrack (t)) if (ev->message.isTempoMetaEvent()) { spq = ev->message.getTempoSecondsPerQuarterNote(); t = 1000; break; }
        *bpmOut = 60.0 / std::max (0.05, spq);
    }
    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq (*mf.getTrack (t));
        seq.updateMatchedPairs();
        for (auto* ev : seq)
        {
            if (! ev->message.isNoteOn()) continue;
            if (! withDrums && ev->message.getChannel() == 10) continue;
            const double a = ev->message.getTimeStamp() / tf;
            const double b = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() / tf : a + 0.5;
            out.emplace_back (a, std::max (0.05, b - a), ev->message.getNoteNumber(), ev->message.getVelocity() / 127.0f);
        }
    }
    return true;
}

bool readAudioMono (const juce::File& f, double maxSeconds, std::vector<float>& mono, double& sr, juce::String& message)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (rd == nullptr) { message = "Could not open that audio file (use WAV, AIFF, FLAC, MP3 or MIDI)."; return false; }
    sr = rd->sampleRate;
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (sr * maxSeconds));
    if (n < (int) (sr * 1.5)) { message = "That recording is too short (need at least 2 seconds)."; return false; }
    const int chans = (int) std::min<unsigned> (2u, rd->numChannels);
    juce::AudioBuffer<float> buf (chans, n);
    rd->read (&buf, 0, n, 0, true, chans > 1);
    mono.assign ((size_t) n, 0.f);
    for (int c = 0; c < chans; ++c) { auto* p = buf.getReadPointer (c); for (int i = 0; i < n; ++i) mono[(size_t) i] += p[i] / (float) chans; }
    return true;
}
}

bool EnochSoloProcessor::loadGroove (const juce::File& f, juce::String& message)
{
    const auto ext = f.getFileExtension().toLowerCase();
    std::shared_ptr<RefProfile> prof;
    if (ext == ".mid" || ext == ".midi")
    {
        std::vector<std::tuple<double, double, int, float>> raw; double bpm = 120.0;
        if (! readMidiNotes (f, true, raw, message, &bpm)) return false;
        if (raw.size() < 4) { message = "That MIDI file has too few notes to learn a groove from."; return false; }
        std::sort (raw.begin(), raw.end());
        const double spb = 60.0 / bpm;
        std::vector<RefRaw> notes; double endT = 0;
        for (auto& r : raw)
        {
            RefRaw n; n.t = std::get<0> (r) * spb; n.dur = std::get<1> (r) * spb; n.pitch = std::get<2> (r); n.loud = std::get<3> (r);
            if (! notes.empty() && std::abs (notes.back().t - n.t) < 0.015) { notes.back().loud = std::max (notes.back().loud, n.loud); continue; }   // simultaneous hits are one hit
            endT = std::max (endT, n.t + n.dur);
            notes.push_back (n);
        }
        std::vector<double> lv; for (auto& n : notes) lv.push_back (n.loud);
        std::sort (lv.begin(), lv.end());
        const double lo = lv[(size_t) (lv.size() * 0.1)], hi = lv[std::min (lv.size() - 1, (size_t) (lv.size() * 0.9))];
        for (auto& n : notes) n.loud = (float) juce::jlimit (0.0, 1.0, (n.loud - lo) / std::max (0.1, hi - lo));
        prof = std::make_shared<RefProfile>();
        prof->name = f.getFileName().toStdString(); prof->fromMidi = true; prof->bpm = bpm; prof->phase = 0.0; prof->lengthSec = endT; prof->notes = std::move (notes);
    }
    else
    {
        std::vector<float> mono; double sr = 0;
        if (! readAudioMono (f, 60.0, mono, sr, message)) return false;
        prof = std::make_shared<RefProfile> (analyseAudio (mono, sr, f.getFileName().toStdString()));
    }
    if (! prof || ! prof->valid()) { message = "I could not find a clear beat in that file. A drum, percussion or bass loop works best."; return false; }
    grooveProfile = prof;
    message = juce::String();
    tryAnother();
    return true;
}

void EnochSoloProcessor::clearGroove()
{
    grooveProfile.reset();
    needRegen = true;
    timerCallback();
}

juce::File EnochSoloProcessor::librariesFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Enoch Bass").getChildFile ("Instruments");
}

void EnochSoloProcessor::loadLibraries()
{
    for (int i = 0; i < I_Count; ++i) libLists[i] = builtinLibraries (i);
    libReport = juce::String();
    const auto dir = librariesFolder();
    if (! dir.isDirectory()) return;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.enochlib"))
    {
        Library l; std::string err;
        if (parseLibraryText (f.loadFileAsString().toStdString(), l, err))
        {
            if (l.name.empty()) continue;
            if (libLists[l.inst].size() < 64) libLists[l.inst].push_back (std::make_shared<const Library> (std::move (l)));
        }
        else libReport << f.getFileName() << ": " << juce::String (err) << "\n";
    }
}

std::shared_ptr<const enochsolo::Library> EnochSoloProcessor::currentLibrary() const
{
    const auto& ll = getLibraries (getI (apvts, "inst"));
    const int li = getI (apvts, "library");
    return (li > 0 && li < (int) ll.size()) ? ll[(size_t) li] : nullptr;
}

bool EnochSoloProcessor::loadSoundFont (const juce::File& f, juce::String& message)
{
    if (! player.sf.load (f, sr, message)) return false;
    soundFontPath = f.getFullPathName();
    setParam ("sfOn", 1.f);
    return true;
}

void EnochSoloProcessor::clearSoundFont()
{
    player.sf.unload();
    soundFontPath = juce::String();
}

bool EnochSoloProcessor::loadBassSample (const juce::File& f, juce::String& message)
{
    auto s = loadBassSample_ (f, message);
    if (! s) return false;
    player.smp.setSample (s);
    bassSamplePath = f.getFullPathName();
    setParam ("smpRoot", 0.f);
    setParam ("smpOn", 1.f);
    return true;
}

void EnochSoloProcessor::clearBassSample() { player.smp.setSample (nullptr); player.smp.allNotesOff(); bassSamplePath = juce::String(); }
bool EnochSoloProcessor::hasBassSample() const { return player.smp.isLoaded(); }
juce::String EnochSoloProcessor::bassSampleInfo() const
{
    auto s = player.smp.getSample();
    if (! s) return juce::String();
    const int ri = (int) std::lround (apvts.getRawParameterValue ("smpRoot")->load());
    const int root = ri > 0 ? 11 + ri : s->rootNote;
    return s->name + "  (root " + juce::String (noteName (root)) + (ri > 0 ? ", set by you" : ", found automatically") + ")";
}

juce::String EnochSoloProcessor::soundFontName() const { return player.sf.getName(); }

void EnochSoloProcessor::setBarMode (int bar0, int bar1, int mode)
{
    pushUndo();
    const Settings base = editSettings();
    if ((int) barChords.size() < base.bars) barChords.resize ((size_t) base.bars);
    const double bb = std::max (0.5, master.barBeats);
    for (int b = std::max (0, bar0); b <= bar1 && b < base.bars; ++b)
    {
        auto& bc = barChords[(size_t) b];
        if (! bc.set) { const auto& c = master.chordAt (b * bb + 0.01); bc.set = true; bc.degree = c.degree; bc.q = c.q; }
        bc.mode = mode;
    }
    Settings s = base; s.barChords = barChords;
    std::string pn;
    const auto newChords = Generator::chordsFor (s, pn);
    Generator::reharmonize (master, master.chords, newChords, s);
    master.progName = pn;
    s.barChords = barChords; master.s.barChords = barChords;
    Generator::finalize (master, s);
    publishMaster();
}

juce::String EnochSoloProcessor::grooveName() const { return grooveProfile ? juce::String (grooveProfile->name) : juce::String(); }

juce::String EnochSoloProcessor::grooveSummary() const
{
    if (! grooveProfile) return juce::String();
    const Settings s = currentSettings();
    const double bb = s.tsNum * 4.0 / std::max (1, s.tsDen);
    return juce::String (describeGroove (*grooveProfile, s.grooveTempo, s.grooveShift, bb));
}

bool EnochSoloProcessor::detectChords (const juce::File& f, juce::String& message)
{
    const Settings cur = currentSettings();
    const double barBeats = cur.tsNum * 4.0 / std::max (1, cur.tsDen);
    DetectResult r;
    const auto ext = f.getFileExtension().toLowerCase();
    if (ext == ".mid" || ext == ".midi")
    {
        std::vector<std::tuple<double, double, int, float>> raw;
        if (! readMidiNotes (f, false, raw, message)) return false;
        std::vector<ChordNote> notes;
        for (auto& t : raw) notes.push_back ({ std::get<0> (t), std::get<1> (t), std::get<2> (t), std::get<3> (t) });
        r = detectChordsFromNotes (notes, barBeats, 32);
    }
    else
    {
        std::vector<float> mono; double sr = 0;
        const double bpm = hostBpm.load() > 20.0 ? hostBpm.load() : 120.0;
        if (! readAudioMono (f, 33.0 * barBeats * 60.0 / bpm, mono, sr, message)) return false;
        r = detectChordsFromAudio (mono, sr, bpm, barBeats, 32);
    }
    if (! r.ok) { message = juce::String (r.error); return false; }

    // length: the smallest solo length that holds the detected bars (the chords repeat to fill it)
    int lenIdx = kNumBarValues - 1;
    for (int i = 0; i < kNumBarValues; ++i) if (kBarValues[i] >= r.bars) { lenIdx = i; break; }
    const int bars = kBarValues[lenIdx];
    loadingState = true;
    setParam ("key", (float) r.keyRoot);
    setParam ("scale", (float) (r.minor ? SC_Minor : SC_Major));
    setParam ("bars", (float) lenIdx);
    setParam ("harmony", 2.f);
    loadingState = false;
    barChords.assign ((size_t) bars, BarChord());
    for (int b = 0; b < bars; ++b) barChords[(size_t) b] = r.chords[(size_t) (b % r.bars)];
    phraseLocks.clear();
    chordStatus = juce::String();
    needRegen = false; needFinalize = false;
    pushUndo();
    regenerate (true, true);
    message = juce::String (r.text) + "   (key " + kNoteNames[r.keyRoot] + (r.minor ? " minor" : " major") + ", " + juce::String (r.bars) + " bars"
              + (r.bars < bars ? ", repeated to fill " + juce::String (bars) : juce::String()) + ")";
    return true;
}

bool EnochSoloProcessor::transcribeAudio (const juce::File& f, juce::String& message)
{
    std::vector<float> mono; double sr = 0;
    if (! readAudioMono (f, 900.0, mono, sr, message)) return false;
    const Settings cur = currentSettings();
    const double barBeats = cur.tsNum * 4.0 / std::max (1, cur.tsDen);
    TranscribeOptions o;
    o.bpm = hostBpm.load() > 20.0 ? hostBpm.load() : 120.0;
    o.detectTempo = trTempoMode == 1;
    o.tidy = juce::jlimit (0, 3, trTidy);
    o.firstNoteIsBarOne = trAnchor == 1;
    const TranscribeResult r = transcribeBass (mono, sr, o);
    if (! r.ok || r.notes.empty()) { message = juce::String (r.error.empty() ? std::string ("No notes found in that recording.") : r.error); return false; }

    // chords from what was played
    std::vector<ChordNote> cn;
    for (auto& n : r.notes) if (n.art != A_Ghost) cn.push_back ({ n.beat, n.lenBeats, n.pitch, juce::jlimit (0.1f, 1.f, (float) n.vel / 127.f) });
    DetectResult dr = detectChordsFromNotes (cn, barBeats, 256);
    const int needBars = std::max (1, (int) std::ceil (r.lengthBeats / barBeats - 0.02));
    int lenIdx = kNumBarValues - 1;
    for (int i = 0; i < kNumBarValues; ++i) if (kBarValues[i] >= needBars) { lenIdx = i; break; }
    const int bars = kBarValues[lenIdx];
    const bool truncated = needBars > bars;   // only if the recording is longer than 256 bars

    loadingState = true;
    if (dr.ok)
    {
        setParam ("key", (float) dr.keyRoot);
        setParam ("scale", (float) (dr.minor ? SC_Minor : SC_Major));
    }
    setParam ("bars", (float) lenIdx);
    setParam ("harmony", 2.f);
    loadingState = false;
    barChords.assign ((size_t) bars, BarChord());
    if (dr.ok) for (int b = 0; b < bars; ++b) barChords[(size_t) b] = dr.chords[(size_t) std::min (b, dr.bars - 1)];
    phraseLocks.clear();
    chordStatus = juce::String();
    pushUndo();

    Settings s = currentSettings();
    Pattern p = Generator::makeShell (s);
    const int chordCount = (int) p.chords.size();
    for (auto& t : r.notes)
    {
        if (t.beat >= p.lengthBeats - 1e-6) continue;
        Note n; n.id = p.nextId++;
        n.start = std::max (0.0, t.beat);
        n.length = std::min (std::max (0.06, t.lenBeats), p.lengthBeats - n.start);
        n.pitch = juce::jlimit (0, 127, t.pitch); n.vel = juce::jlimit (1, 127, t.vel); n.art = t.art; n.track = 0; n.voice = 0;
        const double inBar = std::fmod (n.start, std::max (0.5, p.barBeats));
        const double fr = inBar - std::floor (inBar + 1e-6);
        n.weight = (inBar < 0.05) ? 1.0 : (std::abs (fr) < 0.05 ? 0.8 : (std::abs (fr - 0.5) < 0.05 ? 0.5 : 0.3));
        int role = BR_Other;
        if (t.art == A_Ghost) role = BR_Ghost;
        else if (chordCount > 0)
        {
            const ChordSpan& c = p.chordAt (n.start);
            const int rel = ((n.pitch - c.rootPc) % 12 + 12) % 12;
            role = rel == 0 ? BR_Root : rel == 7 ? BR_Fifth : (rel == 3 || rel == 4) ? BR_Third : (rel == 10 || rel == 11) ? BR_Seventh : BR_Other;
        }
        n.role = role;
        p.raw.push_back (n);
    }
    std::stable_sort (p.raw.begin(), p.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    needRegen = false; needFinalize = false;
    Generator::finalize (p, s);
    master = std::move (p);
    publishMaster();

    message = juce::String (describeTranscription (r));
    if (dr.ok) message += "   Chords: " + juce::String (dr.text).substring (0, 140) + (dr.text.size() > 140 ? " ..." : "") + "  (key " + kNoteNames[dr.keyRoot] + (dr.minor ? " minor" : " major") + ")";
    if (truncated) message += "   Only the first " + juce::String (bars) + " bars were kept.";
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EnochSoloProcessor(); }
