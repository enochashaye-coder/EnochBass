#pragma once
// "Instrument of choice": how a particular sample library / virtual instrument wants its articulations and controllers (JUCE-free).
// Every library is a small table: for each articulation, which key-switch notes to press (and how), plus the expression / vibrato CC numbers.
// Built-in tables come only from official manuals; anything more is loaded from the user's own .enochlib text files (see README).
#include "Styles.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace enochsolo
{
struct KsPress { int note = 24; int vel = 100; };

struct LibArt
{
    int art = A_Sustain;
    std::vector<KsPress> press;
    bool hold = false;        // the key-switch must be held while the note starts (released a moment after it)
    bool oneShot = false;     // sent before every note with this articulation (it does not stay switched on)
};

struct Library
{
    std::string name, maker;
    int inst = I_Sax;
    int verify = 2;               // 0 = key-switches checked against the official manual, 1 = partly checked, 2 = Enoch's standard layout / user file
    bool standard = false;        // the original Enoch Bass layout (uses the Key-switch Base Note setting)
    bool noKeySwitches = false;   // controller-driven or plain instrument: no key-switch notes are written at all
    std::vector<LibArt> arts;
    int ccExpr = 11, ccVib = 1;
    std::string note;             // one line shown under the dropdown
    std::string source;           // where the data came from
    // ---- v3.0 external instrument driver (all optional; defaults change nothing)
    int transpose = 0;            // semitones added to every played note (e.g. +12 for basses mapped an octave up)
    int lo = 0, hi = 127;         // playable range after transposing: notes outside are folded in by octaves
    double overlapMs = 0;         // legato: how far the previous note must overlap the next one (libraries that detect legato by overlap)
    int repKey = -1;              // repetition key: played instead of every second fast repeat of the same pitch
    std::vector<KsPress> init;    // mode keys sent once at the start (e.g. auto legato on, X-note mode off)
    std::vector<int> tuning;      // open strings (sounding MIDI notes, low to high) for the string solver
    std::vector<int> stringKeys;  // key-switch per string (same order as tuning): forces the string the solver chose
    int maxFret = 22;
    int program = -1, bankMsb = -1, bankLsb = -1;
    int velMin = 1, velMax = 127; double velGamma = 1.0;   // velocity curve
    int pedalCC = 64; bool halfPedal = false;             // sustain pedal: continuous (Pianoteq) or on/off
};

inline const LibArt* libFind (const Library& l, int art)
{
    for (auto& a : l.arts) if (a.art == art) return &a;
    return nullptr;
}

/** the articulation of this library that stands in for `art` (what the library can actually do) */
inline int libResolve (const Library& l, int art)
{
    if (libFind (l, art)) return art;
    auto has = [&] (int a) { return libFind (l, a) != nullptr; };
    switch (art)
    {
        case A_Legato: case A_HammerOn: case A_PullOff: case A_Slide: case A_Gliss: case A_Bend: return has (A_Legato) ? A_Legato : A_Sustain;
        case A_PalmMute: case A_Ghost: case A_Marcato: return has (A_Staccato) ? A_Staccato : A_Sustain;
        default: return A_Sustain;
    }
}

/** the note that identifies an articulation in the key-switch lane / test player (first key of its first press) */
inline int libNoteOf (const LibArt& a) { return a.press.empty() ? -1 : a.press[0].note; }

inline const char* verifyText (int v) { return v == 0 ? "checked against the manual" : (v == 1 ? "partly checked" : "standard layout"); }

//------------------------------------------------------------------------------ built-in tables
namespace libdetail
{
inline LibArt la (int art, std::vector<KsPress> p, bool hold = false, bool one = false) { LibArt a; a.art = art; a.press = std::move (p); a.hold = hold; a.oneShot = one; return a; }

// Ilya Efimov guitars (Nylon, Acoustic, LP Electric, TC Electric): all four manuals share this key-switch layout.
// Kontakt numbering (middle C = C3 = 60): A-1 = 21, G0 = 31, D#1 = 39 ... C7 = 120.
inline Library ilyaGuitar (const char* name, const char* manual)
{
    Library l; l.name = name; l.maker = "Ilya Efimov"; l.inst = I_Guitar; l.verify = 0;
    l.arts = {
        la (A_Sustain,   { { 39, 30 } }),                       // D#1 low = legato mode off
        la (A_Staccato,  { { 39, 30 } }),
        la (A_Legato,    { { 39, 110 } }),                      // D#1 high = legato mode on (hammer-ons and pull-offs use it too)
        la (A_Slide,     { { 32, 30 } }, true, true),           // G#0 held: slide up one semitone into the note
        la (A_Gliss,     { { 37, 100 } }, true, true),          // C#1 held: glissando
        la (A_PalmMute,  { { 35, 30 } }, true, true),           // B0 low held: mute
        la (A_Ghost,     { { 35, 110 } }, true, true),          // B0 high held: X-note (dead note)
        la (A_Harmonic,  { { 30, 110 } }, true, true),          // F#0 high held: artificial flageolet
    };
    l.ccExpr = 11; l.ccVib = 1;
    l.note = "Key-switches: D#1 legato on/off, G#0 slide, C#1 gliss, B0 mute / X-note, F#0 harmonic. Vibrato on CC1.";
    l.source = manual;
    return l;
}
}

inline std::vector<std::shared_ptr<const Library>> builtinLibraries (int inst)
{
    using namespace libdetail;
    std::vector<std::shared_ptr<const Library>> v;
    auto add = [&] (Library l) { v.push_back (std::make_shared<const Library> (std::move (l))); };

    Library std_; std_.name = "Standard layout (Enoch default)"; std_.inst = inst; std_.verify = 2; std_.standard = true;
    std_.note = "Key-switches start at the Key-switch Base Note (Advanced) in Enoch Bass's own order.";
    add (std_);

    if (inst == I_Guitar)
    {
        add (ilyaGuitar ("Ilya Efimov Nylon Guitar", "Ilya Efimov Nylon Guitar manual v1.1"));
        add (ilyaGuitar ("Ilya Efimov Acoustic Guitar", "Ilya Efimov Acoustic Guitar manual"));
        add (ilyaGuitar ("Ilya Efimov LP Electric Guitar", "Ilya Efimov LP Electric Guitar manual"));
        add (ilyaGuitar ("Ilya Efimov TC Electric Guitar", "Ilya Efimov TC Electric Guitar manual"));
        Library m; m.name = "NI Session Guitarist - Electric Mint"; m.maker = "Native Instruments"; m.inst = I_Guitar; m.verify = 1;
        m.arts = { la (A_Slide, { { 47, 100 } }, true, true) };   // B1 held: slide modifier (Kontakt numbering: C1 = 36)
        m.note = "Only the slide key (B1) is checked against the manual; other articulations play as normal notes.";
        m.source = "NI Electric Mint manual (Using ELECTRIC MINT)";
        add (m);
    }
    else if (inst == I_Sax)
    {
        Library m; m.name = "SWAM Saxophones (Audio Modeling)"; m.maker = "Audio Modeling"; m.inst = I_Sax; m.verify = 1; m.noKeySwitches = true;
        m.ccExpr = 11; m.ccVib = 1;
        m.note = "Controller-driven: no key-switch notes are written. Expression CC11 (SWAM default), vibrato CC1 (map vibrato depth to the mod wheel).";
        m.source = "SWAM knowledge base: default expression controller CC11";
        add (m);
        Library h; h.name = "VHorns Saxophones (AcousticSamples, UVI Falcon / Workstation)"; h.maker = "AcousticSamples"; h.inst = I_Sax; h.verify = 1;
        // UVI numbering: middle C = C3 = 60, so C1 = 36. The guide names the notes; fall and rise are played together with the note.
        h.arts = { la (A_Fall, { { 37, 100 } }, true, true), la (A_Doit, { { 39, 100 } }, true, true) };   // C#1 fall, D#1 rise
        h.ccExpr = 1; h.ccVib = 20;
        h.note = "Fall = C#1, rise = D#1 (key noise C1, breath A1 are not used). Air flow is the mod wheel (CC1). Set the vibrato dial in the plugin.";
        h.source = "VHorns Saxophones user guide (key-switch list, air flow = mod wheel by default)";
        add (h);
    }
    else if (inst == I_Trumpet)
    {
        Library m; m.name = "SWAM Trumpet (Audio Modeling)"; m.maker = "Audio Modeling"; m.inst = I_Trumpet; m.verify = 1;
        m.arts = { la (A_Sustain, { { 27, 30 } }), la (A_Legato, { { 27, 110 } }) };    // D#1 (note 27) Legato Priority: low = off, high = on
        m.ccExpr = 11; m.ccVib = 1;
        m.note = "Legato Priority key (note 27) on / off, expression CC11, vibrato on the mod wheel. Fall (25) and Doit (26) are gestures: play them by hand.";
        m.source = "SWAM Solo Brass manual v3.5.0 (key-switch table)";
        add (m);
    }
    else if (inst == I_Violin)
    {
        Library m; m.name = "SWAM Violin (Audio Modeling)"; m.maker = "Audio Modeling"; m.inst = I_Violin; m.verify = 1; m.noKeySwitches = true;
        m.ccExpr = 11; m.ccVib = 1;
        m.note = "Controller-driven: no key-switch notes are written. Expression CC11, vibrato CC1 (SWAM defaults).";
        m.source = "SWAM Solo Strings manual: default controllers";
        add (m);
        Library sp; sp.name = "Spitfire Solo Violin"; sp.maker = "Spitfire Audio"; sp.inst = I_Violin; sp.verify = 1; sp.noKeySwitches = true;
        sp.ccExpr = 1; sp.ccVib = 21;
        sp.note = "No key-switches (articulations come from how you play). Dynamics on the mod wheel (CC1), vibrato CC21.";
        sp.source = "Spitfire Solo Violin user manual v2";
        add (sp);
    }
    else if (inst == I_Piano)
    {
        Library m; m.name = "Piano without key-switches (Pianoteq, Keyscape, EZkeys, Noire...)"; m.maker = ""; m.inst = I_Piano; m.verify = 1; m.noKeySwitches = true;
        m.note = "Plain piano: no key-switch notes are written, only notes and the sustain pedal (CC64).";
        m.source = "standard piano behaviour";
        add (m);
    }
    else if (inst == I_Bass)
    {
        // Toontrack EZbass. Toontrack names notes with middle C = C3 (= 60), so C0 = 24. The release notes give: palm mute C0,
        // Slide Down / Slide Up keys F0 and G0, and "the legato control key (32)" = G#0. The key-switches therefore sit at 24..32 and the
        // bass itself is played an octave above its sounding pitch (low E = E1 = 40). Ghost (E-1), harmonics (A#-1) and slap (B-1) come from help text only.
        Library m; m.name = "Toontrack EZbass"; m.maker = "Toontrack"; m.inst = I_Bass; m.verify = 1;
        m.arts = { la (A_Sustain, {}),
                   la (A_Legato,   { { 32, 100 } }, true, true),     // G#0 legato control key (hold)
                   la (A_HammerOn, { { 32, 100 } }, true, true),
                   la (A_PullOff,  { { 32, 100 } }, true, true),
                   la (A_Slide,    { { 31, 100 } }, true, true),     // G0 slide up (hold)
                   la (A_Gliss,    { { 29, 100 } }, true, true),     // F0 slide down (hold)
                   la (A_Ghost,    { { 16, 100 } }, true, true),     // E-1 ghost note
                   la (A_PalmMute, { { 24, 100 } }, true, true),     // C0 palm mute (EBX libraries with recorded palm mutes)
                   la (A_Harmonic, { { 22, 100 } }, true, true),     // A#-1 harmonics
                   la (A_Slap,     { { 23, 100 } }, true, true) };   // B-1 slap / pop
        m.ccExpr = 11; m.ccVib = 1;
        m.transpose = 12; m.lo = 35; m.hi = 81; m.overlapMs = 15;
        m.note = "Key-switches (Toontrack numbering, C3 = 60): legato G#0 (32), slide up G0, slide down F0, palm mute C0, ghost E-1, harmonic A#-1, slap B-1. Bass sent an octave up (low E = E1). Check one bar.";
        m.source = "Toontrack EZbass release notes (palm mute C0, slide keys F0/G0, legato control key 32); ghost / harmonic / slap from help text";
        add (m);
        Library pl; pl.name = "Plain bass (any bass sampler, no key-switches)"; pl.maker = ""; pl.inst = I_Bass; pl.verify = 1; pl.noKeySwitches = true;
        pl.note = "Notes only: no key-switch notes are written. Ghost, slide and mute are still shown in the piano roll and in the MIDI as note length and velocity.";
        pl.source = "plain MIDI";
        add (pl);
        // Ilya Efimov basses (Kontakt numbering, C3 = 60). Shared key-switch layout of the Modern, Classic and Fretless Bass manuals:
        //   strings A-1 (B), B-1 (E), C0 (A), D0 (D), E0 (G); mode keys C#0 chord, D#0 open strings, F0 X-note, F#0 easy legato, G0 auto legato
        //   (velocity 101-127 = on, 0-100 = off); G#0 FX gliss, A0 slap/pop hold, A#0 gliss key; E4 repetition, F4 repetition X-mute,
        //   F#4 fingered, G4 legato hold, G#4 slap, A4 flageolet hold, A#4 palm mute, B4 slide-up hold.
        auto ilyaBass = [&] (const char* name, const char* manual, bool fiveString, bool slap, int fingeredKey)
        {
            Library b; b.name = name; b.maker = "Ilya Efimov"; b.inst = I_Bass; b.verify = 1;
            const KsPress xOff { 29, 40 }, fing { fingeredKey, 100 };
            b.arts = { la (A_Sustain,  { fing, xOff }), la (A_Legato, { fing, xOff }), la (A_Staccato, { fing, xOff }), la (A_Marcato, { fing, xOff }),
                       la (A_Ghost,    { { 29, 120 } }),                    // X-note mode on (dead notes) until the next articulation
                       la (A_PalmMute, { { 82, 100 }, xOff }),
                       la (A_Harmonic, { { 81, 100 } }, true, true),        // A4 flageolet (hold)
                       la (A_Slide,    { { 83, 100 } }, true, true),        // B4 slide-up (hold)
                       la (A_Gliss,    { { 34, 100 } }, true, true) };      // A#0 gliss key (hold)
            if (slap) b.arts.push_back (la (A_Slap, { { 80, 100 }, xOff }));
            b.init = { { 31, 120 }, { 30, 40 }, { 29, 40 }, { 27, 40 }, { 25, 40 } };   // auto legato on, easy legato / X-note / open strings / chord mode off
            b.transpose = 12; b.hi = 75; b.overlapMs = 25; b.repKey = 76;
            b.tuning = fiveString ? std::vector<int> { 23, 28, 33, 38, 43 } : std::vector<int> { 28, 33, 38, 43 };
            b.stringKeys = fiveString ? std::vector<int> { 21, 23, 24, 26, 28 } : std::vector<int> { 23, 24, 26, 28 };
            b.lo = fiveString ? 35 : 40; b.maxFret = 20;
            b.ccExpr = 11; b.ccVib = 1;
            b.note = std::string ("Strings, auto legato by overlap, X-notes for dead notes, repetition key E4, slide B4, flageolet A4") + (slap ? ", slap G#4" : "") + ". Sent an octave up.";
            b.source = manual;
            add (b);
        };
        ilyaBass ("Ilya Efimov Modern Bass", "Ilya Efimov Modern Bass manual (key-switch table)", true, true, 78);
        ilyaBass ("Ilya Efimov Classic Bass", "Ilya Efimov Classic Bass manual (key-switch table)", false, true, 78);
        ilyaBass ("Ilya Efimov Fretless Bass", "Ilya Efimov Fretless Bass manual (key-switch table; F#4 normal right hand)", true, false, 78);
        ilyaBass ("Ilya Efimov Retro Bass", "assumed same layout as the Classic Bass manual (not checked)", false, false, 78);
        ilyaBass ("Ilya Efimov Rock Bass", "assumed same layout as the Modern Bass manual (not checked)", true, true, 78);
        // a DX10-style log drum in another plugin (FL Studio Fruity DX10, mda DX10 ...): it sounds an octave below the note played
        Library dx; dx.name = "Log drum VST: DX10 style (one octave up)"; dx.inst = I_Bass; dx.verify = 1; dx.noKeySwitches = true; dx.transpose = 12;
        dx.note = "Plain notes sent an octave up, because the DX10 'Log Drum' sounds an octave below the note it gets.";
        dx.source = "mda DX10 source code (carrier at half the note frequency)";
        add (dx);
    }
    return v;
}

//------------------------------------------------------------------------------ user files (.enochlib)
/** Text format, one setting per line (# starts a comment):
      name = My Sax Library          instrument = bass | sax | guitar | violin | piano | trumpet
      expression_cc = 11             vibrato_cc = 1           no_keyswitches = 0|1
      ks = Sustain: 24@100                       (press note 24 with velocity 100)
      ks = Legato: 25@110, 26@40                 (several presses)
      ks = Slide: 32@40 hold oneshot             (held while the note starts / sent before every such note)
    Articulation names are the ones shown in Enoch Bass (Sustain, Legato, Staccato, Marcato, Ghost, PalmMute, HammerOn, PullOff, Slide, Bend, Vibrato,
    Harmonic, Growl, Flutter, Fall, Doit, Scoop, Subtone, Shake, Trill, Tremolo, Pizzicato, Gliss, ColLegno, Arpeggio, Mute, Strum). */
inline int articulationFromName (std::string s)
{
    std::string k; for (char c : s) if (std::isalnum ((unsigned char) c)) k += (char) std::tolower ((unsigned char) c);
    struct E { const char* n; int a; };
    static const E t[] = { { "sustain", A_Sustain }, { "legato", A_Legato }, { "staccato", A_Staccato }, { "marcato", A_Marcato }, { "accent", A_Marcato }, { "ghost", A_Ghost },
        { "dead", A_Ghost }, { "palmmute", A_PalmMute }, { "hammeron", A_HammerOn }, { "pulloff", A_PullOff }, { "slide", A_Slide }, { "slap", A_Slap }, { "pop", A_Slap }, { "bend", A_Bend }, { "vibrato", A_Vibrato },
        { "harmonic", A_Harmonic }, { "growl", A_Growl }, { "flutter", A_Flutter }, { "flutter-tongue", A_Flutter }, { "fall", A_Fall }, { "doit", A_Doit }, { "scoop", A_Scoop },
        { "subtone", A_Subtone }, { "shake", A_Shake }, { "trill", A_Trill }, { "tremolo", A_Tremolo }, { "pizzicato", A_Pizz }, { "pizz", A_Pizz }, { "gliss", A_Gliss },
        { "glissando", A_Gliss }, { "collegno", A_ColLegno }, { "arpeggio", A_Arpeggio }, { "mute", A_Mute }, { "harmon", A_Mute }, { "strum", A_Strum } };
    for (auto& e : t) if (k == e.n) return e.a;
    for (int a = 0; a < (int) A_Count; ++a)                 // the full names Enoch shows ("Ghost / Dead Note", "Marcato / Accent" ...)
    {
        std::string n; for (const char* c = artName (a); *c; ++c) if (std::isalnum ((unsigned char) *c)) n += (char) std::tolower ((unsigned char) *c);
        if (k == n) return a;
    }
    return -1;
}

inline bool parseLibraryText (const std::string& text, Library& out, std::string& error)
{
    Library l; l.verify = 2; l.name.clear(); bool haveInst = false;
    std::istringstream in (text); std::string line; int lineNo = 0;
    auto trim = [] (std::string s) { size_t a = s.find_first_not_of (" \t\r"), b = s.find_last_not_of (" \t\r"); return a == std::string::npos ? std::string() : s.substr (a, b - a + 1); };
    while (std::getline (in, line))
    {
        ++lineNo;
        const size_t h = line.find ('#'); if (h != std::string::npos) line = line.substr (0, h);
        line = trim (line); if (line.empty()) continue;
        const size_t eq = line.find ('='); if (eq == std::string::npos) { error = "line " + std::to_string (lineNo) + ": expected key = value"; return false; }
        std::string key = trim (line.substr (0, eq)), val = trim (line.substr (eq + 1));
        for (auto& c : key) c = (char) std::tolower ((unsigned char) c);
        if (key == "name") l.name = val;
        else if (key == "maker") l.maker = val;
        else if (key == "note") l.note = val;
        else if (key == "instrument")
        {
            std::string v; for (char c : val) v += (char) std::tolower ((unsigned char) c);
            if (v == "guitar") l.inst = I_Guitar; else if (v == "sax" || v == "saxophone") l.inst = I_Sax; else if (v == "violin") l.inst = I_Violin;
            else if (v == "piano") l.inst = I_Piano; else if (v == "trumpet") l.inst = I_Trumpet; else if (v == "bass") l.inst = I_Bass;
            else { error = "line " + std::to_string (lineNo) + ": unknown instrument '" + val + "'"; return false; }
            haveInst = true;
        }
        else if (key == "expression_cc") l.ccExpr = std::clamp (std::atoi (val.c_str()), 0, 127);
        else if (key == "vibrato_cc") l.ccVib = std::clamp (std::atoi (val.c_str()), 0, 127);
        else if (key == "no_keyswitches") l.noKeySwitches = std::atoi (val.c_str()) != 0;
        // ---- v3.0 driver settings
        else if (key == "transpose") l.transpose = std::clamp (std::atoi (val.c_str()), -48, 48);
        else if (key == "range") { int a2 = 0, b2 = 127; if (std::sscanf (val.c_str(), "%d - %d", &a2, &b2) >= 1 || std::sscanf (val.c_str(), "%d-%d", &a2, &b2) >= 1) { l.lo = std::clamp (a2, 0, 127); l.hi = std::clamp (b2, 0, 127); } }
        else if (key == "overlap_ms") l.overlapMs = std::clamp (std::atof (val.c_str()), 0.0, 500.0);
        else if (key == "repetition_key") l.repKey = std::clamp (std::atoi (val.c_str()), -1, 127);
        else if (key == "program") l.program = std::clamp (std::atoi (val.c_str()), -1, 127);
        else if (key == "bank_msb") l.bankMsb = std::clamp (std::atoi (val.c_str()), -1, 127);
        else if (key == "bank_lsb") l.bankLsb = std::clamp (std::atoi (val.c_str()), -1, 127);
        else if (key == "vel_min") l.velMin = std::clamp (std::atoi (val.c_str()), 1, 127);
        else if (key == "vel_max") l.velMax = std::clamp (std::atoi (val.c_str()), 1, 127);
        else if (key == "vel_curve") l.velGamma = std::clamp (std::atof (val.c_str()), 0.2, 5.0);
        else if (key == "half_pedal") l.halfPedal = std::atoi (val.c_str()) != 0;
        else if (key == "max_fret") l.maxFret = std::clamp (std::atoi (val.c_str()), 5, 36);
        else if (key == "init" || key == "tuning" || key == "string_keys")
        {
            std::istringstream ps (val); std::string tok;
            while (std::getline (ps, tok, ','))
            {
                tok = trim (tok); if (tok.empty() || ! std::isdigit ((unsigned char) tok[0])) continue;
                const int nt = std::clamp (std::atoi (tok.c_str()), 0, 127);
                if (key == "init") { KsPress pr; pr.note = nt; pr.vel = 100; const size_t at = tok.find ('@'); if (at != std::string::npos) pr.vel = std::clamp (std::atoi (tok.c_str() + at + 1), 1, 127); l.init.push_back (pr); }
                else if (key == "tuning") l.tuning.push_back (nt);
                else l.stringKeys.push_back (nt);
            }
        }
        else if (key == "ks")
        {
            const size_t c = val.find (':'); if (c == std::string::npos) { error = "line " + std::to_string (lineNo) + ": ks needs 'Articulation: note@velocity'"; return false; }
            const int art = articulationFromName (trim (val.substr (0, c)));
            if (art < 0) { error = "line " + std::to_string (lineNo) + ": unknown articulation '" + trim (val.substr (0, c)) + "'"; return false; }
            LibArt a; a.art = art;
            std::string rest = val.substr (c + 1); std::string lower; for (char ch : rest) lower += (char) std::tolower ((unsigned char) ch);
            a.hold = lower.find ("hold") != std::string::npos; a.oneShot = lower.find ("oneshot") != std::string::npos || lower.find ("one-shot") != std::string::npos;
            std::istringstream ps (rest); std::string tok;
            while (std::getline (ps, tok, ','))
            {
                tok = trim (tok); if (tok.empty() || ! std::isdigit ((unsigned char) tok[0])) continue;
                KsPress p; p.note = std::clamp (std::atoi (tok.c_str()), 0, 127); p.vel = 100;
                const size_t at = tok.find ('@'); if (at != std::string::npos) p.vel = std::clamp (std::atoi (tok.c_str() + at + 1), 1, 127);
                a.press.push_back (p);
            }
            if (a.press.empty() && lower.find ("none") == std::string::npos) { error = "line " + std::to_string (lineNo) + ": no key-switch note found"; return false; }   // "none": played with no key press
            l.arts.push_back (a);
        }
        else { error = "line " + std::to_string (lineNo) + ": unknown setting '" + key + "'"; return false; }
    }
    if (l.name.empty()) { error = "missing name = ..."; return false; }
    if (! haveInst) { error = "missing instrument = ..."; return false; }
    l.source = "your file"; if (l.note.empty()) l.note = "From your own file.";
    out = std::move (l);
    return true;
}
/** v3.0: write a library / profile back as an .enochlib text file (the format parseLibraryText reads) */
inline std::string libraryToText (const Library& l)
{
    static const char* instNames[] = { "guitar", "sax", "violin", "piano", "trumpet", "bass" };
    std::ostringstream o;
    o << "# Enoch mapping profile (saved by the profile editor)\n";
    o << "name = " << l.name << "\n";
    if (! l.maker.empty()) o << "maker = " << l.maker << "\n";
    o << "instrument = " << instNames[std::clamp (l.inst, 0, 5)] << "\n";
    if (! l.note.empty()) o << "note = " << l.note << "\n";
    o << "expression_cc = " << l.ccExpr << "\nvibrato_cc = " << l.ccVib << "\nno_keyswitches = " << (l.noKeySwitches ? 1 : 0) << "\n";
    if (l.transpose != 0) o << "transpose = " << l.transpose << "\n";
    if (l.lo != 0 || l.hi != 127) o << "range = " << l.lo << "-" << l.hi << "\n";
    if (l.overlapMs > 0) o << "overlap_ms = " << l.overlapMs << "\n";
    if (l.repKey >= 0) o << "repetition_key = " << l.repKey << "\n";
    if (l.program >= 0) o << "program = " << l.program << "\n";
    if (l.bankMsb >= 0) o << "bank_msb = " << l.bankMsb << "\n";
    if (l.bankLsb >= 0) o << "bank_lsb = " << l.bankLsb << "\n";
    if (l.velMin != 1) o << "vel_min = " << l.velMin << "\n";
    if (l.velMax != 127) o << "vel_max = " << l.velMax << "\n";
    if (l.velGamma != 1.0) o << "vel_curve = " << l.velGamma << "\n";
    if (l.halfPedal) o << "half_pedal = 1\n";
    if (l.maxFret != 22) o << "max_fret = " << l.maxFret << "\n";
    auto list = [&] (const char* k, const std::vector<int>& v) { if (v.empty()) return; o << k << " = "; for (size_t i = 0; i < v.size(); ++i) o << (i ? ", " : "") << v[i]; o << "\n"; };
    list ("tuning", l.tuning); list ("string_keys", l.stringKeys);
    if (! l.init.empty()) { o << "init = "; for (size_t i = 0; i < l.init.size(); ++i) o << (i ? ", " : "") << l.init[i].note << "@" << l.init[i].vel; o << "\n"; }
    for (auto& a : l.arts)
    {
        o << "ks = " << artName (a.art) << ": ";
        if (a.press.empty()) o << "none";
        for (size_t i = 0; i < a.press.size(); ++i) o << (i ? ", " : "") << a.press[i].note << "@" << a.press[i].vel;
        if (a.hold) o << " hold";
        if (a.oneShot) o << " oneshot";
        o << "\n";
    }
    return o.str();
}
} // namespace enochsolo
