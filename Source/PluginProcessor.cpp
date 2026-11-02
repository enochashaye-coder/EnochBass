#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginEditor.h"
#include "Sounds.h"
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
const char* kGenIds[] = { "inst", "artist", "genre", "key", "scale", "ts", "bars", "intensity", "mode", "octave", "harmony", "space", "repeat", "chordTrack", "busy", "refAmount", "refTempo", "refShift", "voice2", "clave", "grooveAmount", "grooveTempo", "grooveShift", "speed", "subdiv" };
const char* kFinIds[] = { "library", "humanize", "hTiming", "hVel", "hLength", "hFeel", "feel", "ksBase", "bendRange", "emitKS", "emitBends", "emitCC", "realize", "exportMode" };
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
    flt ("busy", "Note Density (sparse to dense)", 0.f, 1.f, 0.5f);
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
    l.add (std::make_unique<juce::AudioParameterFloat> (P { "trClean", 1 }, "Transcription Cleanup", juce::NormalisableRange<float> (0.f, 1.f), 0.5f,
                                                        juce::AudioParameterFloatAttributes().withAutomatable (false)));
    l.add (std::make_unique<juce::AudioParameterBool> (P { "trGhosts", 1 }, "Transcription: Keep Ghost Notes", false,
                                                       juce::AudioParameterBoolAttributes().withAutomatable (false)));
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
    // built-in preview sound, one choice per instrument (Concert grand, Rhodes, Prophet-5, Stratocaster, Tenor sax, P-Bass ...)
    // ---- v2.7 log drum control panel (relative controls: 100% = the selected log drum type's own value)
    {
        auto f = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit)
        {
            l.add (std::make_unique<juce::AudioParameterFloat> (P { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
                                                                juce::AudioParameterFloatAttributes().withLabel (unit)));
        };
        l.add (std::make_unique<juce::AudioParameterInt> (P { "ldTune", 1 }, "Log Drum Tune", -12, 12, 0));
        f ("ldFine", "Log Drum Fine Tune", -50.f, 50.f, 0.f, "ct");
        f ("ldDrop", "Log Drum Pitch Drop", 0.f, 200.f, 100.f, "%");
        f ("ldDropT", "Log Drum Drop Time", 25.f, 300.f, 100.f, "%");
        f ("ldAttack", "Log Drum Attack", 0.f, 20.f, 0.5f, "ms");
        f ("ldPunch", "Log Drum Punch", 0.f, 200.f, 100.f, "%");
        f ("ldDecay", "Log Drum Decay", 25.f, 300.f, 100.f, "%");
        f ("ldSub", "Log Drum Sub Level", -24.f, 6.f, 0.f, "dB");
        f ("ldBody", "Log Drum Body Resonance", 0.f, 200.f, 100.f, "%");
        f ("ldTone", "Log Drum Tone", -100.f, 100.f, 0.f, "%");
        f ("ldDrive", "Log Drum Drive", 0.f, 100.f, 0.f, "%");
        f ("ldWidth", "Log Drum Stereo Width", 0.f, 100.f, 20.f, "%");
        f ("ldVel", "Log Drum Velocity Response", 0.f, 100.f, 60.f, "%");
        f ("ldGlide", "Log Drum Glide", 0.f, 500.f, 60.f, "ms");
        f ("ldHuman", "Log Drum Humanize", 0.f, 100.f, 30.f, "%");
        f ("ldRoom", "Log Drum Room", 0.f, 100.f, 10.f, "%");
        f ("ldOut", "Log Drum Output", -24.f, 6.f, 0.f, "dB");
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "ldMono", 1 }, "Log Drum Mono Maker", juce::StringArray { "Off", "80 Hz", "120 Hz", "160 Hz" }, 2));
        l.add (std::make_unique<juce::AudioParameterBool> (P { "ldSC", 1 }, "Log Drum Sidechain-Friendly Output", true));
    }
    for (int i : { 5 })
    {
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "src" + juce::String (i), 1 }, juce::String ("Sound Source (") + instName (i) + ")",
                                                             juce::StringArray { "Built-in engine", "One-shot sample", "SoundFont", "External instrument (MIDI out)" }, 0,
                                                             juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    }
    for (int i : { 5 })
    {
        juce::StringArray names; for (auto& pt : enochsolo::snd::patchList (i)) names.add (pt.name);
        const juce::String id = "sound" + juce::String (i);
        l.add (std::make_unique<juce::AudioParameterChoice> (P { id, 1 }, juce::String ("Preview Sound (") + instName (i) + ")", names, 0,
                                                             juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    }
    // ---- v2.7 batch 2: global sound controls (both plugins), glide, speed and subdivisions
    {
        auto pct = [&] (const char* id, const char* name, float lo, float hi, float def)
        {
            l.add (std::make_unique<juce::AudioParameterFloat> (P { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
                                                                juce::AudioParameterFloatAttributes().withLabel ("%")));
        };
        pct ("realism", "Realism", 0.f, 100.f, 60.f);
        pct ("sndHuman", "Sound Humanization", 0.f, 100.f, 30.f);
        pct ("dynamics", "Dynamics", 0.f, 100.f, 50.f);
        pct ("exprResp", "Expression Response", 0.f, 100.f, 50.f);
        pct ("bright", "Brightness", -100.f, 100.f, 0.f);
        pct ("mechNoise", "Mechanical Noise", 0.f, 100.f, 50.f);
        pct ("micPos", "Mic Position (close - far)", 0.f, 100.f, 35.f);
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "roomType", 1 }, "Room", juce::StringArray { "Classic (as before)", "Studio", "Small room", "Club", "Hall", "Plate", "Church" }, 0));
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "outMode", 1 }, "Preview Output", juce::StringArray { "Stereo", "Mono" }, 0));
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "glideMode", 1 }, "Glide Mode", juce::StringArray { "Off", "Legato glide", "Always glide", "Overlap glide", "Portamento" }, 1));
        pct ("glideTime", "Glide Time (fast - slow)", 0.f, 100.f, 20.f);
        l.add (std::make_unique<juce::AudioParameterBool> (P { "humanPlay", 1 }, "Human Play (phrasing engine)", true));
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "speed", 1 }, "Speed", juce::StringArray { "Normal", "Double time (2x)" }, 0,
                                                             juce::AudioParameterChoiceAttributes().withAutomatable (false)));
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "subdiv", 1 }, "Subdivision", juce::StringArray { "Auto (style)", "1/16", "1/16 triplets", "1/32", "1/32 triplets", "1/4", "1/8", "1/8 triplets" }, 0,
                                                             juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    }
    // ---- v2.7 batch 3: modulation matrix (4 slots) + creative effect
    {
        const juce::StringArray srcs { "Off", "LFO 1", "LFO 2", "Velocity", "Mod wheel", "Aftertouch", "Expression", "Key (pitch)", "Note envelope", "Random per note" };
        const juce::StringArray dsts { "Pitch", "Brightness", "Level", "Pan", "Vibrato", "Drive" };
        for (int k = 1; k <= 4; ++k)
        {
            const juce::String n (k);
            l.add (std::make_unique<juce::AudioParameterChoice> (P { "mm" + n + "Src", 1 }, "Mod " + n + " Source", srcs, 0));
            l.add (std::make_unique<juce::AudioParameterChoice> (P { "mm" + n + "Dst", 1 }, "Mod " + n + " Destination", dsts, 0));
            l.add (std::make_unique<juce::AudioParameterFloat> (P { "mm" + n + "Amt", 1 }, "Mod " + n + " Amount", juce::NormalisableRange<float> (-100.f, 100.f), 0.f,
                                                                juce::AudioParameterFloatAttributes().withLabel ("%")));
        }
        l.add (std::make_unique<juce::AudioParameterFloat> (P { "lfo1Rate", 1 }, "LFO 1 Rate", juce::NormalisableRange<float> (0.1f, 12.f, 0.f, 0.5f), 5.f, juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        l.add (std::make_unique<juce::AudioParameterFloat> (P { "lfo2Rate", 1 }, "LFO 2 Rate", juce::NormalisableRange<float> (0.02f, 4.f, 0.f, 0.5f), 0.3f, juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        l.add (std::make_unique<juce::AudioParameterChoice> (P { "fxMode", 1 }, "Creative Effect", juce::StringArray { "Off", "Tape wobble", "Lo-fi", "Wide", "Telephone / radio", "Warm saturation" }, 0));
        l.add (std::make_unique<juce::AudioParameterFloat> (P { "fxAmt", 1 }, "Creative Effect Amount", juce::NormalisableRange<float> (0.f, 100.f), 50.f, juce::AudioParameterFloatAttributes().withLabel ("%")));
    }
    // ---- v3.1: timing-feel preset (humanize)
    l.add (std::make_unique<juce::AudioParameterChoice> (P { "feel", 1 }, "Timing Feel", juce::StringArray { "Style's own feel", "Tight studio grid", "Laid-back pocket", "Pushed / ahead of the beat",
                                                         "Afrobeats pocket", "Amapiano swing", "Gospel - behind the beat", "Neo-soul drunk", "Jazz swing (8ths)", "Live and loose" }, 0,
                                                         juce::AudioParameterChoiceAttributes().withAutomatable (false)));
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
    defaultRoutes();
    for (int i = 0; i < TestPlayer::kSlots; ++i)
    {
        sfPresets[(size_t) i] = -1;
        soundParams[(size_t) i] = apvts.getRawParameterValue ("sound" + juce::String (i));     // null for instruments this plugin does not have
        srcParams[(size_t) i] = apvts.getRawParameterValue ("src" + juce::String (i));
    }
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
    s.speed = juce::jlimit (0, 1, getI (apvts, "speed")); s.subdiv = juce::jlimit (0, 7, getI (apvts, "subdiv")); s.barSpeed = barSpeed;
    fillRoutes (s);
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
    s.feel = getI (apvts, "feel");
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
        const int ch = e.ch >= 0 ? e.ch + 1 : e.track + 1;     // v3.0: a routed part plays on its own channel
        juce::MidiMessage m;
        switch (e.type)
        {
            case EV_ON:   m = juce::MidiMessage::noteOn (ch, e.a, (juce::uint8) e.b); break;
            case EV_OFF:  m = juce::MidiMessage::noteOff (ch, e.a); break;
            case EV_CC:   m = juce::MidiMessage::controllerEvent (ch, e.a, e.b); break;
            case EV_AT:   m = juce::MidiMessage::channelPressureChange (ch, e.a); break;
            case EV_PC:   m = juce::MidiMessage::programChange (ch, e.a); break;
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
    s.speed = m.speed; s.subdiv = m.subdiv; s.barSpeed = barSpeed;
    fillRoutes (s);
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

void EnochSoloProcessor::applyBarSpeed (int bar0, int bar1, int speed, uint64_t sd)
{
    const int bars = master.bars;
    if (bars <= 0) return;
    bar0 = std::max (0, bar0); bar1 = std::min (bars - 1, bar1);
    if (bar1 < bar0) return;
    pushUndo();
    if ((int) barSpeed.size() < bars) barSpeed.resize ((size_t) bars, -1);
    for (int b = bar0; b <= bar1; ++b) barSpeed[(size_t) b] = juce::jlimit (-1, 1, speed);
    Settings s = editSettings(); s.barChords = barChords;
    Generator::rerollRegion (master, s, bar0, bar1 + 1, sd);
    Generator::finalize (master, s);
    publishMaster();
}
int EnochSoloProcessor::barSpeedOf (int bar) const { return bar >= 0 && bar < (int) barSpeed.size() ? barSpeed[(size_t) bar] : -1; }

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
    barBusy.clear(); barSpeed.clear();
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
struct Preset { const char* name; int inst; const char* artist; int genre; int key; int scale; int harmony; float intensity; int bars;
                int mode = 0; int feel = 0; float busy = 0.5f; };      // v3.2: output mode, timing feel and note density (older presets keep the defaults)
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
    // ---- v3.2 session presets (EZbass-style player profiles per genre; feel / density per genre)
    { "Session - Funk",            I_Bass, "Funk Session Bass",               G_Funk,        4, SC_Dorian,     1, 0.80f, 3, 0, 1, 0.75f },
    { "Session - Gospel",          I_Bass, "Gospel Session Bass",             G_Gospel,     10, SC_Major,      1, 0.70f, 3, 0, 6, 0.65f },
    { "Session - Neo-soul",        I_Bass, "Neo-Soul & R&B Session Bass",     G_SmoothJazz,  3, SC_Dorian,     1, 0.50f, 3, 0, 7, 0.45f },
    { "Session - R&B",             I_Bass, "Neo-Soul & R&B Session Bass",     G_SmoothJazz, 10, SC_Major,      1, 0.45f, 3, 0, 2, 0.40f },
    { "Session - Afrobeats",       I_Bass, "Afrobeats Session Bass",          G_AfroFusion,  6, SC_Minor,      1, 0.55f, 3, 0, 4, 0.50f },
    { "Session - Amapiano",        I_Bass, "Amapiano Session Bass",           G_Amapiano,    1, SC_Minor,      1, 0.55f, 3, 0, 5, 0.35f },
    { "Session - Reggae",          I_Bass, "Reggae & Dancehall Session Bass", G_Reggae,      7, SC_Major,      1, 0.55f, 3, 0, 2, 0.40f },
    { "Session - Dancehall",       I_Bass, "Reggae & Dancehall Session Bass", G_Reggae,      9, SC_Minor,      1, 0.70f, 3, 0, 1, 0.55f },
    { "Session - Rock",            I_Bass, "Pop & Rock Session Bass",         G_Rock,        4, SC_MinPent,    1, 0.80f, 3, 0, 1, 0.65f },
    { "Session - Pop",             I_Bass, "Pop & Rock Session Bass",         G_Pop,         0, SC_Major,      1, 0.60f, 3, 0, 1, 0.55f },
    { "Session - Jazz",            I_Bass, "Jazz Walking Session Bass",       G_Swing,       5, SC_Major,      1, 0.60f, 3, 0, 8, 0.60f },
    { "Session - Kizomba",         I_Bass, "Kizomba & Zouk Session Bass",     G_Kizomba,     9, SC_Minor,      1, 0.50f, 3, 0, 2, 0.45f },
    { "Session - Zouk",            I_Bass, "Kizomba & Zouk Session Bass",     G_Zouk,        2, SC_Minor,      1, 0.50f, 3, 0, 2, 0.45f },
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
    setParam ("mode", (float) pr.mode); setParam ("space", 0.5f); setParam ("repeat", 0.5f); setParam ("busy", pr.busy); setParam ("feel", (float) pr.feel);
    barBusy.clear(); barSpeed.clear();
    loadingState = false;
    chordText = juce::String();
    needRegen = true; timerCallback();
    tryAnother();
}

void EnochSoloProcessor::timerCallback()
{
    if (lastTr && (std::abs (getF (apvts, "trClean") - lastTrClean) > 1.0e-4f || getB (apvts, "trGhosts") != lastTrGhosts)) { juce::String m; installTranscription (m); if (m.isNotEmpty()) lastTrMessage = m, ++trVersion; lastTrClean = getF (apvts, "trClean"); lastTrGhosts = getB (apvts, "trGhosts"); }
    if (auditionNote >= 0 && ! auditionHeld && (juce::int32) (juce::Time::getMillisecondCounter() - auditionOffAt) >= 0)
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

void EnochSoloProcessor::audition (int pitch, int velocity, bool hold)
{
    if (auditionNote >= 0) auditionState.noteOff (1, auditionNote, 0.f);
    auditionNote = juce::jlimit (0, 127, pitch);
    auditionState.noteOn (1, auditionNote, juce::jlimit (1, 127, velocity) / 127.0f);
    auditionStart = juce::Time::getMillisecondCounter();
    auditionHeld = hold;
    auditionOffAt = auditionStart + (hold ? 6000u : 380u);        // a held key never hangs: it is released at the latest after 6 s
}

void EnochSoloProcessor::auditionRelease()
{
    if (! auditionHeld) return;
    auditionHeld = false;
    // let the note be heard at least briefly even on a quick click
    auditionOffAt = std::max (juce::Time::getMillisecondCounter(), auditionStart + 260u);
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
    {
        // the player switches instrument, sound and source in one step on the audio thread (fade, full flush, then the new sound)
        TestPlayer::Config cfg;
        const int slot = juce::jlimit (0, TestPlayer::kSlots - 1, inst);
        cfg.inst = inst; cfg.sax = st.variant;
        if (auto* sp = soundParams[(size_t) slot]) cfg.sound = (int) std::lround (sp->load());
        if (auto* sp = srcParams[(size_t) slot]) cfg.src = (int) std::lround (sp->load());
        { const auto rd = getRoute (slot, 0).dest; if (rd == RD_External || rd == RD_Hardware) cfg.src = TestPlayer::S_External; }   // v3.0: the routed instrument plays instead
        cfg.sfPreset = sfPresets[(size_t) slot].load();
        cfg.smpGen = smpGen[(size_t) slot].load(); cfg.sfGen = sfGen[(size_t) slot].load();
        player.setDesired (cfg);
        player.update();
    }
    player.setKeySwitches (getI (apvts, "ksBase"), getI (apvts, "bendRange"));
    {
        const auto& ll = getLibraries (inst);
        const int li = getI (apvts, "library");
        const enochsolo::Library* lp = (li > 0 && li < (int) ll.size()) ? ll[(size_t) li].get() : nullptr;
        if (lp != lastLibPtr || inst != lastLibInst || getI (apvts, "ksBase") != lastLibBase)
        { player.configureKeySwitches (lp, inst, getI (apvts, "ksBase")); lastLibPtr = lp; lastLibInst = inst; lastLibBase = getI (apvts, "ksBase"); }
    }
    {
        enochsolo::snd::Engine::LogCtl lc;
        lc.tune = (float) getI (apvts, "ldTune") + getF (apvts, "ldFine") / 100.f;
        lc.dropMul = getF (apvts, "ldDrop") / 100.f; lc.dropTimeMul = getF (apvts, "ldDropT") / 100.f;
        lc.attack = getF (apvts, "ldAttack") / 1000.f; lc.punchMul = getF (apvts, "ldPunch") / 100.f;
        lc.decayMul = getF (apvts, "ldDecay") / 100.f; lc.subDb = getF (apvts, "ldSub"); lc.bodyMul = getF (apvts, "ldBody") / 100.f;
        lc.tone = getF (apvts, "ldTone") / 100.f; lc.drive = getF (apvts, "ldDrive") / 100.f; lc.width = getF (apvts, "ldWidth") / 100.f;
        lc.velResp = getF (apvts, "ldVel") / 100.f; lc.glide = getF (apvts, "ldGlide") / 1000.f; lc.human = getF (apvts, "ldHuman") / 100.f;
        lc.outDb = getF (apvts, "ldOut");
        static const float monoHz[] = { 0.f, 80.f, 120.f, 160.f };
        lc.monoHz = monoHz[juce::jlimit (0, 3, getI (apvts, "ldMono"))]; lc.scSafe = getB (apvts, "ldSC");
        player.eng.setLogControls (lc);
        // v3.0: a log drum one-shot plays with the same glide, drop and tune as the built-in log drum
        {
            enochsolo::BassSampler::LogMode lm;
            const auto& pl = enochsolo::snd::patchList (5);
            const int si = juce::jlimit (0, (int) pl.size() - 1, getI (apvts, "sound5"));
            lm.on = enochsolo::snd::isLogKind (pl[(size_t) si].kind);
            lm.glide = lc.glide; lm.tune = lc.tune; lm.drop = std::max (0.f, lc.dropMul - 1.f) * 3.f; lm.dropT = 0.03f * lc.dropTimeMul;
            player.smp.setLogMode (lm);
        }
    }
    // a log drum uses its own room amount (the panel's ROOM knob)
    {
        enochsolo::snd::Engine::GlobalCtl g;
        g.realism = getF (apvts, "realism") / 100.f; g.human = getF (apvts, "sndHuman") / 100.f; g.dynamics = getF (apvts, "dynamics") / 100.f;
        g.exprResp = getF (apvts, "exprResp") / 100.f; g.bright = getF (apvts, "bright") / 100.f; g.mech = getF (apvts, "mechNoise") / 100.f;
        g.mic = getF (apvts, "micPos") / 100.f; g.glideMode = juce::jlimit (0, 4, getI (apvts, "glideMode")); g.glideTime = getF (apvts, "glideTime") / 100.f;
        g.humanPlay = getB (apvts, "humanPlay"); g.seed = (uint32_t) (master.s.seed & 0xffffffffu) | 1u;
        player.eng.setGlobal (g);
        enochsolo::snd::Engine::ModCtl mc;
        for (int k = 0; k < 4; ++k)
        {
            const juce::String n (k + 1);
            mc.slot[(size_t) k].src = getI (apvts, ("mm" + n + "Src").toRawUTF8()); mc.slot[(size_t) k].dst = getI (apvts, ("mm" + n + "Dst").toRawUTF8());
            mc.slot[(size_t) k].amt = getF (apvts, ("mm" + n + "Amt").toRawUTF8()) / 100.f;
        }
        mc.lfo1Hz = getF (apvts, "lfo1Rate"); mc.lfo2Hz = getF (apvts, "lfo2Rate");
        mc.fx = getI (apvts, "fxMode"); mc.fxAmt = getF (apvts, "fxAmt") / 100.f;
        player.eng.setMod (mc);
        player.setRoom (getI (apvts, "roomType"));
        player.setMonoOut (getI (apvts, "outMode") == 1);
    }
    // a far microphone hears more of the room
    player.setLevel (getF (apvts, "volume"), juce::jlimit (0.f, 1.f, (player.eng.logDrumActive() ? getF (apvts, "ldRoom") / 100.f : getF (apvts, "reverb")) + 0.3f * juce::jmax (0.f, (getF (apvts, "micPos") / 100.f - 0.35f) / 0.65f)));
    { const int ri = getI (apvts, "smpRoot"); player.smp.setRootOverride (ri > 0 ? 11 + ri : 0); }

    // notes auditioned from the piano roll join the incoming MIDI
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { lastInNote = m.getNoteNumber(); lastInVel = (int) m.getVelocity(); inNoteCount.fetch_add (1); }
    }
    juce::MidiBuffer auditionBuf;
    auditionState.processNextMidiBuffer (auditionBuf, 0, n, true);
    midi.addEvents (auditionBuf, 0, n, 0);

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
    // v3.0: which channels go to the DAW and which to a hardware port
    bool liveCh[16] = {}, hwCh[16] = {}; bool anyLive = false, anyHw = false;
    const bool routedNow = anyRouted (curSlot());
    if (routedNow)
        for (int part = 0; part < kParts; ++part)
        {
            const auto r = getRoute (curSlot(), part);
            const int c = juce::jlimit (0, 15, r.channel - 1);
            if (r.dest == RD_External || r.dest == RD_Both) { liveCh[c] = true; anyLive = true; }
            if (r.dest == RD_Hardware) { hwCh[c] = true; anyHw = true; }
        }
    const bool sendMidiOut = getB (apvts, "midiOut") || player.effectiveSource() == TestPlayer::S_External || anyLive || anyHw;     // External: your own instrument plays
    if (sendMidiOut) genBuffer.addEvents (auditionBuf, 0, n, 0);        // clicked keys / notes also reach your instrument track
    auto flushHeld = [&]
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int nt = 0; nt < 128; ++nt)
                if (heldOut[ch][nt]) { genBuffer.addEvent (juce::MidiMessage::noteOff (ch + 1, nt), 0); heldOut[ch][nt] = false; }
    };
    // a new pattern arrived (a knob was moved, notes edited, regenerated): notes and the sustain pedal of the old pattern would
    // otherwise never get their note-off / pedal-up, which is what left piano notes ringing on
    // a route changed: every note, pedal and bend on every channel is released before the new routing plays
    if (routesChanged.exchange (false))
    {
        player.allNotesOff();
        flushHeld();
        for (int ch = 1; ch <= 16; ++ch)
        {
            genBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), 0);
            genBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 123, 0), 0);
            genBuffer.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), 0);
        }
    }
    if (audioSnap.get() != (const void*) lastPlayedSnap)
    {
        if (lastPlayedSnap != nullptr && wasRunning)
        {
            player.allNotesOff();
            if (sendMidiOut || wasSendingOut)
            {
                flushHeld();
                for (int ch = 1; ch <= 16; ++ch) genBuffer.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), 0);
            }
        }
        lastPlayedSnap = audioSnap.get();
    }

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

    {
        float lv = 0.f;
        if (getB (apvts, "playerOn")) for (int c = 0; c < juce::jmin (2, buffer.getNumChannels()); ++c) lv = juce::jmax (lv, buffer.getRMSLevel (c, 0, n));
        outLevel.store (lv);
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
        if (routedNow && (anyHw || ! getB (apvts, "midiOut")))
        {
            // split by channel: DAW parts stay on the plugin output, hardware parts go to the port (Built-in-only parts are not sent)
            juce::MidiBuffer daw, hw;
            for (const auto meta : genBuffer)
            {
                const auto m = meta.getMessage();
                const int c = m.getChannel() - 1;
                if (c < 0) continue;
                if (hwCh[c]) hw.addEvent (m, meta.samplePosition);
                if (liveCh[c]) daw.addEvent (m, meta.samplePosition);
            }
            midi.addEvents (daw, 0, n, 0);
            if (anyHw && ! hw.isEmpty())
            {
                const juce::SpinLock::ScopedTryLockType lk (hwLock);
                if (lk.isLocked() && hwOut) hwOut->sendBlockOfMessages (hw, juce::Time::getMillisecondCounterHiRes() + 2.0, getSampleRate());
            }
        }
        else midi.addEvents (genBuffer, 0, n, 0);
    }
    {
        // profile editor tests go out at once, on the part's own channel
        const juce::SpinLock::ScopedTryLockType lk (testLock);
        if (lk.isLocked() && ! testBuf.isEmpty())
        {
            midi.addEvents (testBuf, 0, n, 0);
            const juce::SpinLock::ScopedTryLockType lh (hwLock);
            if (lh.isLocked() && hwOut) hwOut->sendBlockOfMessages (testBuf, juce::Time::getMillisecondCounterHiRes() + 2.0, getSampleRate());
            testBuf.clear();
        }
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
    { auto old = state.getChildWithName ("ROUTES"); if (old.isValid()) state.removeChild (old, nullptr); state.appendChild (routesToTree(), nullptr); }
    if (auto lib = currentLibrary()) state.setProperty ("libraryName", juce::String (lib->name), nullptr);
    if (auto snap = getSnapshot())
    {
        // the notes themselves (so piano-roll edits survive saving the project) and the per-bar chords
        juce::String notes;
        for (auto& n : snap->pattern.raw)
            notes << n.id << ',' << juce::String (n.start, 5) << ',' << juce::String (n.length, 5) << ',' << n.pitch << ',' << n.vel << ',' << n.art << ','
                  << n.track << ',' << n.voice << ',' << juce::String (n.weight, 3) << ',' << n.bend << ',' << (n.vib ? 1 : 0) << ',' << (n.mute ? 1 : 0) << ',' << n.role << ';';
        state.setProperty ("notes", notes, nullptr);
        state.setProperty ("notesNextId", snap->pattern.nextId, nullptr);
        state.setProperty ("notesCleared", snap->pattern.raw.empty(), nullptr);
        state.setProperty ("notesBars", snap->pattern.bars, nullptr);
        state.setProperty ("notesInst", snap->pattern.s.instrument, nullptr);
        juce::String cp, cr;
        for (auto& c : snap->pattern.userPts) cp << c.lane << ':' << juce::String (c.t, 5) << ':' << c.value << ';';
        for (auto& r : snap->pattern.userRegions) cr << r.lane << ':' << juce::String (r.t0, 5) << ':' << juce::String (r.t1, 5) << ';';
        state.setProperty ("ctrlPts", cp, nullptr);
        state.setProperty ("ctrlRegions", cr, nullptr);
    }
    state.setProperty ("smpPath", smpPaths[(size_t) curSlot()], nullptr);         // older versions read this one
    state.setProperty ("uiRollGrow", uiRollGrow, nullptr);
    state.setProperty ("uiLaneH", uiLaneH, nullptr);
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
        juce::String bs; for (int v : barSpeed) bs << v << ',';
        state.setProperty ("barSpeed", bs, nullptr);
        juce::String bc;
        for (auto& b : barChords) bc << (b.set ? 1 : 0) << ':' << b.degree << ':' << (int) b.q << ':' << b.mode << ',';
        state.setProperty ("barChords", bc, nullptr);
        for (int i = 0; i < TestPlayer::kSlots; ++i)
        {
            state.setProperty ("sfPath_" + juce::String (i), sfPaths[(size_t) i], nullptr);
            state.setProperty ("sfPreset_" + juce::String (i), sfPresets[(size_t) i].load(), nullptr);
            state.setProperty ("smpPath_" + juce::String (i), smpPaths[(size_t) i], nullptr);
        }
        state.setProperty ("srcVer", 1, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void EnochSoloProcessor::restoreSoundSlots (const juce::ValueTree& vt)
{
    for (int i = 0; i < TestPlayer::kSlots; ++i)
    {
        player.setSlotSample (i, nullptr); player.slotSF (i).unload();
        smpPaths[(size_t) i] = sfPaths[(size_t) i] = juce::String(); sfPresets[(size_t) i] = -1;
    }
    auto loadSmp = [&] (int i, const juce::String& path)
    {
        if (path.isEmpty() || ! juce::File (path).existsAsFile()) return;
        juce::String m; auto smp = loadBassSample_ (juce::File (path), m);
        if (smp) { player.setSlotSample (i, smp); smpPaths[(size_t) i] = path; }
    };
    auto loadSf = [&] (int i, const juce::String& path)
    {
        if (path.isEmpty() || ! juce::File (path).existsAsFile()) return;
        juce::String m; loadSoundFontInto (i, juce::File (path), m);
    };
    if ((int) vt.getProperty ("srcVer", 0) >= 1)
    {
        for (int i = 0; i < TestPlayer::kSlots; ++i)
        {
            loadSmp (i, vt.getProperty ("smpPath_" + juce::String (i)).toString());
            loadSf (i, vt.getProperty ("sfPath_" + juce::String (i)).toString());
            sfPresets[(size_t) i] = (int) vt.getProperty ("sfPreset_" + juce::String (i), -1);
        }
    }
    else
    {
        // a project saved before per-instrument sounds: the one-shot belongs to the instrument that was selected, the SoundFont to every
        // instrument (as before); the Sound Source of each instrument is set to match what used to play
        const int inst = juce::jlimit (0, TestPlayer::kSlots - 1, getI (apvts, "inst"));
        const juce::String sp = vt.getProperty ("smpPath").toString(), fp = vt.getProperty ("sfPath").toString();
        loadSmp (inst, sp);
        if (fp.isNotEmpty()) for (int i : { 5 }) loadSf (i, fp);
        for (int i : { 5 })
        {
            int src = TestPlayer::S_Builtin;
            if (getB (apvts, "smpOn") && i == inst && smpPaths[(size_t) i].isNotEmpty()) src = TestPlayer::S_OneShot;
            else if (getB (apvts, "sfOn") && sfPaths[(size_t) i].isNotEmpty()) src = TestPlayer::S_SoundFont;
            setParam (("src" + juce::String (i)).toRawUTF8(), (float) src);
        }
    }
    for (int i = 0; i < TestPlayer::kSlots; ++i) { smpGen[(size_t) i].fetch_add (1); sfGen[(size_t) i].fetch_add (1); }
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
    uiRollGrow = juce::jlimit (0, 400, (int) vt.getProperty ("uiRollGrow", 0));
    uiLaneH = juce::jlimit (28.f, 900.f, (float) (double) vt.getProperty ("uiLaneH", 96.0));
    restoreSoundSlots (vt);
    // v3.0: routes (older projects have none: every part stays on the built-in player, as before)
    defaultRoutes();
    routesFromTree (vt.getChildWithName ("ROUTES"));
    // the instrument of choice by name (the list position can move when libraries are added)
    {
        const juce::String ln = vt.getProperty ("libraryName").toString();
        if (ln.isNotEmpty())
        {
            const auto& ll = getLibraries (getI (apvts, "inst"));
            for (size_t i = 0; i < ll.size(); ++i) if (juce::String (ll[i]->name) == ln) { setParam ("library", (float) i); break; }
        }
    }
    openHardwareFor (curSlot());
    barBusy.clear(); barSpeed.clear();
    for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("barBusy").toString(), ",", "")) barBusy.push_back (tok.getFloatValue());
    barSpeed.clear();
    for (auto& tok : juce::StringArray::fromTokens (vt.getProperty ("barSpeed").toString(), ",", "")) if (tok.isNotEmpty()) barSpeed.push_back (tok.getIntValue());
    undoStack.clear(); redoStack.clear();

    // restore the exact notes (including edits) when they were saved
    const juce::String notes = vt.getProperty ("notes").toString();
    needRegen = true;
    if (notes.isNotEmpty() || (bool) vt.getProperty ("notesCleared", false))
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
        const bool cleared = (bool) vt.getProperty ("notesCleared", false);     // v3.1: a roll you cleared on purpose stays blank
        if ((! raw.empty() || cleared) && (int) vt.getProperty ("notesBars", -1) == p.bars && (int) vt.getProperty ("notesInst", -1) == s.instrument)
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

int EnochSoloProcessor::curSlot() const { return juce::jlimit (0, TestPlayer::kSlots - 1, getI (apvts, "inst")); }

bool EnochSoloProcessor::loadSoundFontInto (int slot, const juce::File& f, juce::String& message)
{
    slot = juce::jlimit (0, TestPlayer::kSlots - 1, slot);
    auto& dst = player.slotSF (slot);
    bool ok = false;
    // the same file already loaded for another instrument: share its samples instead of loading a second copy
    for (int o = 0; o < TestPlayer::kSlots && ! ok; ++o)
        if (o != slot && player.slotSF (o).isLoaded() && player.slotSF (o).path == f.getFullPathName())
            ok = dst.loadCopyOf (player.slotSF (o), sr);
    if (! ok) ok = dst.load (f, sr, message);
    if (! ok) return false;
    sfPaths[(size_t) slot] = f.getFullPathName();
    sfPresets[(size_t) slot] = -1;
    sfGen[(size_t) slot].fetch_add (1);
    return true;
}

bool EnochSoloProcessor::loadSoundFont (const juce::File& f, juce::String& message)
{
    const int slot = curSlot();
    if (! loadSoundFontInto (slot, f, message)) return false;
    setParam (("src" + juce::String (slot)).toRawUTF8(), (float) TestPlayer::S_SoundFont);
    setParam ("sfOn", 1.f);
    return true;
}

void EnochSoloProcessor::clearSoundFont()
{
    const int slot = curSlot();
    player.slotSF (slot).unload();
    sfPaths[(size_t) slot] = juce::String(); sfPresets[(size_t) slot] = -1;
    sfGen[(size_t) slot].fetch_add (1);
}
bool EnochSoloProcessor::hasSoundFont() const { return const_cast<TestPlayer&> (player).slotSF (curSlot()).isLoaded(); }
juce::String EnochSoloProcessor::soundFontName() const { return const_cast<TestPlayer&> (player).slotSF (curSlot()).getName(); }
juce::StringArray EnochSoloProcessor::soundFontPresets() { return player.slotSF (curSlot()).presetNames(); }
int EnochSoloProcessor::soundFontPreset() const { return sfPresets[(size_t) curSlot()].load(); }
void EnochSoloProcessor::setSoundFontPreset (int index) { const int slot = curSlot(); sfPresets[(size_t) slot] = index; sfGen[(size_t) slot].fetch_add (1); }

bool EnochSoloProcessor::loadBassSample (const juce::File& f, juce::String& message)
{
    auto s = loadBassSample_ (f, message);
    if (! s) return false;
    const int slot = curSlot();
    player.setSlotSample (slot, s);
    smpPaths[(size_t) slot] = f.getFullPathName();
    smpGen[(size_t) slot].fetch_add (1);
    setParam ("smpRoot", 0.f);
    setParam (("src" + juce::String (slot)).toRawUTF8(), (float) TestPlayer::S_OneShot);
    setParam ("smpOn", 1.f);
    return true;
}

void EnochSoloProcessor::clearBassSample()
{
    const int slot = curSlot();
    player.setSlotSample (slot, nullptr);
    smpPaths[(size_t) slot] = juce::String();
    smpGen[(size_t) slot].fetch_add (1);
}
bool EnochSoloProcessor::hasBassSample() const { return player.slotSample (curSlot()) != nullptr; }
juce::String EnochSoloProcessor::bassSampleInfo() const
{
    auto s = player.slotSample (curSlot());
    if (! s) return juce::String();
    const int ri = (int) std::lround (apvts.getRawParameterValue ("smpRoot")->load());
    const int root = ri > 0 ? 11 + ri : s->rootNote;
    return s->name + "  (root " + juce::String (noteName (root)) + (ri > 0 ? ", set by you" : ", found automatically") + ")";
}

juce::String EnochSoloProcessor::nowPlayingText (bool& matches, bool& warning) const
{
    const int inst = getI (apvts, "inst"), slot = curSlot();
    auto& pl = const_cast<TestPlayer&> (player);
    const int ni = pl.nowInst.load(), ns = pl.nowSound.load(), req = pl.nowReqSrc.load(), eff = pl.nowEffSrc.load(), w = pl.nowWarn.load();
    const int wantSound = soundParams[(size_t) slot] ? (int) std::lround (soundParams[(size_t) slot]->load()) : 0;
    const int wantSrc = srcParams[(size_t) slot] ? (int) std::lround (srcParams[(size_t) slot]->load()) : 0;
    matches = (ni == inst && ns == wantSound && req == wantSrc);
    warning = (w != TestPlayer::W_None);
    if (ni < 0) { matches = true; warning = true; return "waiting for the audio engine  !! the preview starts when your DAW is processing audio"; }
    juce::String t = juce::String (instName (ni)).toUpperCase() + "  |  ";
    const auto& pl2 = enochsolo::snd::patchList (ni);
    switch (eff)
    {
        case TestPlayer::S_OneShot:  { auto smp = pl.slotSample (ni); t << "One-shot: " << (smp ? smp->name : juce::String ("?")); break; }
        case TestPlayer::S_SoundFont:
        {
            const int pr = sfPresets[(size_t) juce::jlimit (0, TestPlayer::kSlots - 1, ni)].load();
            t << "SoundFont: " << pl.slotSF (ni).getName();
            if (pr >= 0) { auto names = pl.slotSF (ni).presetNames(); if (pr < names.size()) t << " > " << names[pr]; }
            break;
        }
        case TestPlayer::S_External: { const auto rt = routeText (ni); t << (rt.isNotEmpty() ? "External: " + rt : juce::String ("External instrument (MIDI out to your track)")); break; }
        default:
            t << "Built-in: " << (ns >= 0 && ns < (int) pl2.size() ? juce::String (pl2[(size_t) ns].name) : juce::String ("?"));
            if (ni == enochsolo::I_Sax && ns == 0) t << "  ->  " << (pl.nowAlto.load() ? "alto" : "tenor") << " sax for this artist";
            break;
    }
    if (w == TestPlayer::W_NoOneShot)   t << "   !! no one-shot loaded for " << instName (ni) << " - built-in engine plays";
    if (w == TestPlayer::W_NoSoundFont) t << "   !! no SoundFont loaded for " << instName (ni) << " - built-in engine plays";
    if (w == TestPlayer::W_NoProgram)   t << "   !! this SoundFont has no " << instName (ni) << " program - built-in engine plays (pick a preset)";
    return t;
}

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
    TranscribeOptions o;
    o.bpm = hostBpm.load() > 20.0 ? hostBpm.load() : 120.0;
    o.detectTempo = trTempoMode == 1;
    o.tidy = juce::jlimit (0, 3, trTidy);
    o.firstNoteIsBarOne = trAnchor == 1;
    TranscribeResult r = transcribeBass (mono, sr, o);
    if (! r.ok || r.notes.empty()) { message = juce::String (r.error.empty() ? std::string ("No notes found in that recording.") : r.error); return false; }
    lastTr = std::make_shared<TranscribeResult> (std::move (r));
    pushUndo();
    return installTranscription (message);
}

// the notes of the last transcription that survive the cleanup slider
static std::vector<TNote> filterTranscription (const TranscribeResult& r, float clean, bool keepGhosts, int& dropped)
{
    std::vector<int> vs; for (auto& n : r.notes) if (n.art != A_Ghost) vs.push_back (n.vel);
    std::sort (vs.begin(), vs.end());
    const double medVel = vs.empty() ? 90.0 : (double) vs[vs.size() / 2];
    const double minLen = 0.040 + 0.090 * clean;                    // seconds
    const double minVel = medVel * (0.30 + 0.50 * clean);
    const double minConf = 0.30 + 0.50 * clean;
    std::vector<TNote> out; dropped = 0;
    for (auto& n : r.notes)
    {
        const double len = n.t1 - n.t0;
        if (n.art == A_Ghost) { if (keepGhosts && n.vel >= 0.15 * medVel) out.push_back (n); else ++dropped; continue; }
        if (len < minLen || n.vel < minVel || n.conf < minConf) { ++dropped; continue; }
        // two notes almost on top of each other: keep the stronger one
        if (! out.empty() && out.back().art != A_Ghost && n.t0 - out.back().t0 < 0.045 + 0.05 * clean)
        {
            if (n.vel > out.back().vel) out.back() = n;
            ++dropped; continue;
        }
        out.push_back (n);
    }
    return out;
}

bool EnochSoloProcessor::installTranscription (juce::String& message)
{
    if (! lastTr) return false;
    const TranscribeResult& r = *lastTr;
    const Settings cur = currentSettings();
    const double barBeats = cur.tsNum * 4.0 / std::max (1, cur.tsDen);
    const float clean = getF (apvts, "trClean");
    const bool ghosts = getB (apvts, "trGhosts");
    int dropped = 0;
    const std::vector<TNote> notes = filterTranscription (r, clean, ghosts, dropped);
    if (notes.empty()) { message = "Nothing is left at this cleanup setting. Move the CLEANUP slider down."; return false; }
    lastTrClean = clean; lastTrGhosts = ghosts;

    // chords from what was played
    std::vector<ChordNote> cn;
    for (auto& n : notes) if (n.art != A_Ghost) cn.push_back ({ n.beat, n.lenBeats, n.pitch, juce::jlimit (0.1f, 1.f, (float) n.vel / 127.f) });
    DetectResult dr = detectChordsFromNotes (cn, barBeats, 256);
    const int needBars = std::max (1, (int) std::ceil (r.lengthBeats / barBeats - 0.02));
    int lenIdx = kNumBarValues - 1;
    for (int i = 0; i < kNumBarValues; ++i) if (kBarValues[i] >= needBars) { lenIdx = i; break; }
    const int bars = kBarValues[lenIdx];
    const bool truncated = needBars > bars;

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

    Settings s = currentSettings();
    Pattern p = Generator::makeShell (s);
    const int chordCount = (int) p.chords.size();
    for (auto& t : notes)
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

    int ghostCount = 0; for (auto& n : notes) if (n.art == A_Ghost) ++ghostCount;
    message = juce::String ((int) notes.size()) + " notes kept (" + juce::String (dropped) + " weak or doubtful ones left out" + (ghostCount ? ", " + juce::String (ghostCount) + " ghost notes kept" : juce::String()) + ") at "
              + juce::String (juce::roundToInt (r.bpm)) + " BPM. ";
    if (dr.ok) message += "Chords: " + juce::String (dr.text).substring (0, 140) + (dr.text.size() > 140 ? " ..." : "") + "  (key " + kNoteNames[dr.keyRoot] + (dr.minor ? " minor" : " major") + ")";
    if (truncated) message += "   Only the first " + juce::String (bars) + " bars were kept.";
    lastTrMessage = message; ++trVersion;
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EnochSoloProcessor(); }

//==============================================================================
// v3.0 external instrument driver: routes
juce::StringArray EnochSoloProcessor::partNames() { return { "Bass line", "Log drum (own pattern)", "Chord pad" }; }

void EnochSoloProcessor::defaultRoutes()
{
    const juce::SpinLock::ScopedLockType lk (routeLock);
    // your rig from the plan: Piano 1, Guitar 2, Sax 3, Bass 4, Log drum 5; Violin 6, Trumpet 7; accompaniment 11, chord pad 12
    static const int leadCh[TestPlayer::kSlots] = { 2, 3, 6, 1, 7, 4 };
    for (int sl = 0; sl < TestPlayer::kSlots; ++sl)
        for (int p = 0; p < kParts; ++p)
        {
            PartRoute r; r.dest = RD_Builtin;
            r.channel = p == 0 ? leadCh[sl] : (p == 1 ? (sl == 5 ? 5 : 11) : 12);
            if (sl == 5 && p == 1) r.dest = RD_Off;              // Enoch Bass: the log drum part is off until you route it
            routes[(size_t) sl][(size_t) p] = r;
        }
}

EnochSoloProcessor::PartRoute EnochSoloProcessor::getRoute (int slot, int part) const
{
    const juce::SpinLock::ScopedLockType lk (routeLock);
    return routes[(size_t) juce::jlimit (0, TestPlayer::kSlots - 1, slot)][(size_t) juce::jlimit (0, kParts - 1, part)];
}

void EnochSoloProcessor::setRoute (int slot, int part, const PartRoute& r)
{
    {
        const juce::SpinLock::ScopedLockType lk (routeLock);
        routes[(size_t) juce::jlimit (0, TestPlayer::kSlots - 1, slot)][(size_t) juce::jlimit (0, kParts - 1, part)] = r;
    }
    openHardwareFor (slot);
    routesChanged = true;
    refinalize();
}

bool EnochSoloProcessor::anyRouted (int slot) const
{
    const juce::SpinLock::ScopedLockType lk (routeLock);
    // v3.2: routing is on when a part leaves Enoch (external VST, both, hardware) or a part was switched Off. The Enoch Bass log drum
    // row starts Off (there is no log drum until you route one), so on its own it does not switch routing on.
    const int sl = juce::jlimit (0, TestPlayer::kSlots - 1, slot);
    for (int part = 0; part < kParts; ++part)
    {
        const auto& r = routes[(size_t) sl][(size_t) part];
        if (r.dest == RD_External || r.dest == RD_Both || r.dest == RD_Hardware) return true;
        if (r.dest == RD_Off && ! (sl == 5 && part == 1)) return true;
    }
    return false;
}

std::shared_ptr<const enochsolo::Library> EnochSoloProcessor::libraryByName (int inst, const juce::String& name) const
{
    if (name.isEmpty()) return nullptr;
    for (int i = 0; i < (int) enochsolo::I_Count; ++i)
    {
        if (inst >= 0 && i != inst) continue;
        for (auto& l : libLists[(size_t) i]) if (juce::String (l->name) == name) return l;
    }
    return nullptr;
}

void EnochSoloProcessor::fillRoutes (enochsolo::Settings& s) const
{
    const int slot = curSlot();
    s.routed = anyRouted (slot);
    if (! s.routed) return;
    for (int p = 0; p < kParts; ++p)
    {
        const auto r = getRoute (slot, p);
        auto& rc = s.routes[p];
        rc.on = r.dest != RD_Off;
        rc.channel = juce::jlimit (0, 15, r.channel - 1);
        rc.lib = r.profile.isNotEmpty() ? libraryByName (p == 2 ? -1 : s.instrument, r.profile) : (p == 0 ? s.lib : nullptr);
        rc.transpose = r.transpose;
        rc.program = r.program;
    }
    s.dupToTrack1 = (slot == 5) && s.routes[1].on;
}

void EnochSoloProcessor::openHardwareFor (int slot)
{
    juce::String want;
    for (int p = 0; p < kParts; ++p) { const auto r = getRoute (slot, p); if (r.dest == RD_Hardware && r.port.isNotEmpty()) { want = r.port; break; } }
    if (want == hwOpenId) return;
    std::shared_ptr<juce::MidiOutput> dev;
    if (want.isNotEmpty())
        if (auto d = juce::MidiOutput::openDevice (want)) { d->startBackgroundThread(); dev = std::move (d); }
    const juce::SpinLock::ScopedLockType lk (hwLock);
    hwOut = dev; hwOpenId = dev ? want : juce::String();
}

void EnochSoloProcessor::sendTest (const std::vector<juce::MidiMessage>& msgs)
{
    const juce::SpinLock::ScopedLockType lk (testLock);
    int pos = 0;
    for (auto& m : msgs) testBuf.addEvent (m, pos++);
}

juce::String EnochSoloProcessor::routeText (int slot) const
{
    const auto r = getRoute (slot, 0);
    if (r.dest == RD_Builtin) return {};
    static const char* dn[] = { "built-in", "external VST", "external VST + built-in", "hardware", "off" };
    juce::String prof = r.profile;
    if (prof.isEmpty()) if (auto l = currentLibrary()) prof = l->name;
    juce::String t; t << dn[juce::jlimit (0, 4, r.dest)] << " on MIDI ch " << r.channel;
    if (prof.isNotEmpty()) t << " (" << prof << ")";
    if (r.dest == RD_Hardware) t << (hwOpenId.isNotEmpty() ? juce::String (", port open") : juce::String (", no port open"));
    return t;
}

juce::ValueTree EnochSoloProcessor::routesToTree() const
{
    juce::ValueTree t ("ROUTES");
    const juce::SpinLock::ScopedLockType lk (routeLock);
    for (int sl = 0; sl < TestPlayer::kSlots; ++sl)
        for (int p = 0; p < kParts; ++p)
        {
            const auto& r = routes[(size_t) sl][(size_t) p];
            juce::ValueTree c ("R");
            c.setProperty ("slot", sl, nullptr); c.setProperty ("part", p, nullptr); c.setProperty ("dest", r.dest, nullptr); c.setProperty ("ch", r.channel, nullptr);
            c.setProperty ("profile", r.profile, nullptr); c.setProperty ("program", r.program, nullptr); c.setProperty ("transpose", r.transpose, nullptr); c.setProperty ("port", r.port, nullptr);
            t.appendChild (c, nullptr);
        }
    return t;
}

void EnochSoloProcessor::routesFromTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    {
        const juce::SpinLock::ScopedLockType lk (routeLock);
        for (auto c : t)
        {
            const int sl = (int) c.getProperty ("slot", -1), p = (int) c.getProperty ("part", -1);
            if (sl < 0 || sl >= TestPlayer::kSlots || p < 0 || p >= kParts) continue;
            auto& r = routes[(size_t) sl][(size_t) p];
            r.dest = juce::jlimit (0, 4, (int) c.getProperty ("dest", 0)); r.channel = juce::jlimit (1, 16, (int) c.getProperty ("ch", 1));
            r.profile = c.getProperty ("profile").toString(); r.program = juce::jlimit (-1, 127, (int) c.getProperty ("program", -1));
            r.transpose = juce::jlimit (-48, 48, (int) c.getProperty ("transpose", 0)); r.port = c.getProperty ("port").toString();
        }
    }
    routesChanged = true;
}

bool EnochSoloProcessor::saveProfile (const enochsolo::Library& l, juce::String& message)
{
    const auto dir = librariesFolder();
    if (! dir.createDirectory()) { message = "Could not create " + dir.getFullPathName(); return false; }
    juce::String fn = juce::File::createLegalFileName (juce::String (l.name)).trim();
    if (fn.isEmpty()) fn = "My profile";
    const auto file = dir.getChildFile (fn + ".enochlib");
    if (! file.replaceWithText (juce::String (enochsolo::libraryToText (l)))) { message = "Could not write " + file.getFullPathName(); return false; }
    loadLibraries();
    message = "Saved " + file.getFileName();
    return true;
}
