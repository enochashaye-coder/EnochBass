#pragma once
// "Instrument of choice": how a particular sample library / virtual instrument wants its articulations and controllers (JUCE-free).
// Every library is a small table: for each articulation, which key-switch notes to press (and how), plus the expression / vibrato CC numbers.
// Built-in tables come only from official manuals; anything more is loaded from the user's own .enochlib text files (see README).
#include "Styles.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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
        // Toontrack EZbass: its key-switches sit below the lowest bass note (E1 = 28), so with C-1 = 0 they are the notes 0..20.
        // Names: B-1 slap, A#-1 harmonics, E-1 ghost note, F#0 slide, G#0 legato (hammer-on / pull-off), C0 palm mute (newer EBX libraries).
        Library m; m.name = "Toontrack EZbass"; m.maker = "Toontrack"; m.inst = I_Bass; m.verify = 1;
        m.arts = { la (A_Sustain, {}),
                   la (A_Legato,   { { 20, 100 } }, true, true),     // G#0 legato (hold)
                   la (A_HammerOn, { { 20, 100 } }, true, true),
                   la (A_PullOff,  { { 20, 100 } }, true, true),
                   la (A_Slide,    { { 18, 100 } }, true, true),     // F#0 slide to the next note played (hold)
                   la (A_Ghost,    { {  4, 100 } }, true, true),     // E-1 ghost note
                   la (A_PalmMute, { { 12, 100 } }, true, true),     // C0 palm mute (EBX libraries that have recorded palm mutes)
                   la (A_Harmonic, { { 10, 100 } }, true, true),     // A#-1 harmonics
                   la (A_Slap,     { { 11, 100 } }, true, true) };   // B-1 slap / pop
        m.ccExpr = 11; m.ccVib = 1;
        m.note = "Key-switches below the bass range: legato G#0, slide F#0, ghost E-1, palm mute C0, harmonic A#-1, slap B-1 (C-1 = note 0). Check one bar in EZbass.";
        m.source = "Toontrack EZbass release notes (palm mute C0, slides F0/G0, key switch list) and Toontrack help text; notes are not in one official table";
        add (m);
        Library pl; pl.name = "Plain bass (any bass sampler, no key-switches)"; pl.maker = ""; pl.inst = I_Bass; pl.verify = 1; pl.noKeySwitches = true;
        pl.note = "Notes only: no key-switch notes are written. Ghost, slide and mute are still shown in the piano roll and in the MIDI as note length and velocity.";
        pl.source = "plain MIDI";
        add (pl);
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
            if (a.press.empty()) { error = "line " + std::to_string (lineNo) + ": no key-switch note found"; return false; }
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
} // namespace enochsolo
