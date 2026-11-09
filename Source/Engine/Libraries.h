#pragma once
// "Instrument of choice": how a particular sample library / virtual instrument wants its articulations and controllers (JUCE-free).
// Every library is a small table: for each articulation, which key-switch notes to press (and how), plus the expression / vibrato CC numbers.
// Built-in tables come only from official manuals; anything more is loaded from the user's own .enochlib text files (see README).
#include "Styles.h"
#include <array>
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace enochsolo
{
struct KsPress { int note = 24; int vel = 100; };

/** v3.8: a profile's own keys (anything the library has that Enoch's articulation list does not: "Pop", "Set fret position 3",
    "Feedback" ...) use articulation numbers from kCustomArt up. They are placed by hand from the piano roll's Articulation menu. */
constexpr int kCustomArt = 100, kMaxCustom = 64;
inline bool isCustomArt (int a) { return a >= kCustomArt && a < kCustomArt + kMaxCustom; }

struct LibArt
{
    int art = A_Sustain;
    std::vector<KsPress> press;
    bool hold = false;        // the key-switch must be held while the note starts (released a moment after it)
    bool oneShot = false;     // sent before every note with this articulation (it does not stay switched on)
    std::string name;         // v3.8: the library's own name for a custom key (empty for Enoch's articulations)
    // v3.12 / v2.12: when the key goes down, relative to its note (a technique, not a label)
    int when = 0;             // KW_Before: down a pre-roll before the note; KW_During: pressed while the note sounds (EZbass F0 / G0 slide
                              // down / up the held note); KW_Replace: the key is the sound itself (EZbass E0 ghost tap): no pitched note
    double atFrac = 0.6;      // KW_During: where in the note the key is pressed (never closer than 80 ms to its end)
    double pressMs = 0;       // a press that is not held: how long it stays down (0 = until the note ends, as before)
    // v3.14 / v2.14: a mode key that stays switched on until another key is pressed (Ilya Efimov G#0 FX glissando: every following note would
    // play its own FX). Pressed just before its note and switched off by the next note's own key (always sent), or by the plain-note key
    // right after the note when nothing follows soon. fxLo..fxHi: the keys that carry an effect (Efimov FX glissando: B0..D4)
    bool latch = false;
    int fxLo = -1, fxHi = -1;
};
enum KsWhen { KW_Before = 0, KW_During = 1, KW_Replace = 2 };

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
    // ---- v3.12 / v2.12 performance settings
    double preRollMs = 20;        // how long before its note a key-switch goes down (never on the note's own tick)
    bool overlapLegato = false;   // the instrument plays any two overlapping notes as legato (EZbass, Efimov auto legato): plain notes
                                  // therefore never overlap the next note; only notes meant to be joined do
    int breathKey = -1, breathVel = 45;   // v3.14.1: a key whose sound is a breath (VHorns A1), pressed before a phrase that follows a rest
    int detachKey = -1;           // held while notes must sound together without legato (EZbass A-1 "polyphony")
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
    if (art >= (int) A_Count) return A_Sustain;           // a custom key this profile does not have
    // v2.10: pop -> the profile's own "Pop" key, else its slap key; slide out (fall) -> its slide-down / gliss key; dead mute -> its mute
    if (art == A_Pop)
    {
        for (auto& a : l.arts) if (a.art >= (int) A_Count) { std::string nm; for (char c : a.name) nm += (char) std::tolower ((unsigned char) c); if (nm == "pop") return a.art; }
        return libFind (l, A_Slap) ? (int) A_Slap : (int) A_Sustain;
    }
    if (art == A_Fall) return libFind (l, A_Gliss) ? (int) A_Gliss : (int) A_Sustain;
    if (art == A_Mute) return libFind (l, A_PalmMute) ? (int) A_PalmMute : (int) A_Sustain;
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

/** v3.8: the name shown for an articulation: a custom key's own name, otherwise Enoch's articulation name */
inline std::string artLabel (const Library* l, int art)
{
    if (l != nullptr && art >= (int) A_Count)
        for (auto& a : l->arts) if (a.art == art && ! a.name.empty()) return a.name;
    return artName (art);
}

/** v3.8: every key the profile uses as a switch (articulations, custom keys, mode keys, strings, repetition key).
    A played note must never land on one of these: the instrument would switch instead of sounding (the "silent note" fault). */
inline std::array<bool, 128> libKeyMask (const Library& l)
{
    std::array<bool, 128> m {}; m.fill (false);
    if (l.standard) return m;
    auto mark = [&] (int n) { if (n >= 0 && n < 128) m[(size_t) n] = true; };
    if (! l.noKeySwitches) for (auto& a : l.arts) for (auto& p : a.press) mark (p.note);
    for (auto& p : l.init) mark (p.note);
    for (int k : l.stringKeys) mark (k);
    mark (l.repKey);
    mark (l.breathKey);     // v3.14.1
    mark (l.detachKey);
    return m;
}

/** v3.8: the played range a profile really allows, with every key-switch outside it: [lo, hi] shrunk past any key inside it */
inline void libSafeRange (const Library& l, int& lo, int& hi)
{
    const auto m = libKeyMask (l);
    lo = std::clamp (l.lo, 0, 127); hi = std::clamp (l.hi, 0, 127);
    // the longest run of free keys inside [lo, hi] is the range (keys usually sit below or above the instrument)
    int bestA = lo, bestB = lo - 1, a = lo;
    for (int n = lo; n <= hi + 1; ++n)
    {
        if (n > hi || m[(size_t) n]) { if (n - 1 - a > bestB - bestA) { bestA = a; bestB = n - 1; } a = n + 1; }
    }
    if (bestB >= bestA) { lo = bestA; hi = bestB; }
}

/** v3.8: problems with a profile (empty = fine): a key-switch inside the played range, a key used twice for different things */
inline std::vector<std::string> libCheck (const Library& l)
{
    std::vector<std::string> out;
    if (l.standard) return out;
    const auto m = libKeyMask (l);
    const int lo = std::clamp (l.lo + 0, 0, 127), hi = std::clamp (l.hi, 0, 127);
    for (int n = lo; n <= hi; ++n) if (m[(size_t) n]) out.push_back ("key-switch " + std::to_string (n) + " is inside the played range " + std::to_string (lo) + "-" + std::to_string (hi));
    for (auto& a : l.arts) if (a.art >= (int) A_Count && ! isCustomArt (a.art)) out.push_back ("articulation number " + std::to_string (a.art) + " is out of range");
    return out;
}

inline const char* verifyText (int v) { return v == 0 ? "checked against the manual" : (v == 1 ? "partly checked" : "standard layout"); }

//------------------------------------------------------------------------------ built-in tables
namespace libdetail
{
inline LibArt la (int art, std::vector<KsPress> p, bool hold = false, bool one = false) { LibArt a; a.art = art; a.press = std::move (p); a.hold = hold; a.oneShot = one; return a; }

// v3.8: the four Ilya Efimov guitars, key for key from their Kontakt key-switch pages (screenshots, Nov 2026).
// Kontakt numbering (middle C = C3 = 60). Two layouts:
//   Acoustic (AG) and Nylon (NG): keys 21-35 only.  LP and TC Electric: the same 21-35, plus 36-39 and 87-109.
// "(velo)" keys: the first name is a low velocity, the second a high one (Mute / X-Note, Flag. nat. / Flag. art., Non vibrato / Vibrato,
// Slide slow / fast, Pull off / Hammer on); "on/off" mode keys: 101-127 = on, 1-100 = off (as in the Efimov basses).
enum EfimovGuitar { EG_Acoustic, EG_Nylon, EG_LP, EG_TC };
inline Library ilyaGuitar (const char* name, EfimovGuitar g)
{
    const bool electric = (g == EG_LP || g == EG_TC);
    const bool vibKey = (g != EG_Acoustic);              // G0: "Non vibrato / Vibrato" (the Acoustic has "Sustain" there)
    Library l; l.name = name; l.maker = "Ilya Efimov"; l.inst = I_Guitar; l.verify = 0;
    auto cu = [] (int k, const char* nm, std::vector<KsPress> p, bool hold = false) { LibArt a = la (kCustomArt + k, std::move (p), hold, hold); a.name = nm; return a; };
    std::vector<KsPress> sus;                            // back to plain notes: legato mode off (electric), vibrato off
    if (electric) sus.push_back ({ 39, 40 });
    if (vibKey)   sus.push_back ({ 31, 40 });
    std::vector<KsPress> vib = sus; if (vibKey) vib.back() = { 31, 120 };
    l.arts = {
        la (A_Sustain,   sus),
        la (A_Staccato,  sus),
        la (A_Slide,     { { 32, 30 } }, true, true),           // G#0 held: Slide up 1, slow
        la (A_HammerOn,  { { 33, 110 } }, true, true),          // A0 held, high velocity: Hammer on
        la (A_PullOff,   { { 33, 30 } }, true, true),           // A0 held, low velocity: Pull off
        la (A_PalmMute,  { { 35, 30 } }, true, true),           // B0 held, low velocity: Mute
        la (A_Ghost,     { { 35, 110 } }, true, true),          // B0 held, high velocity: X-Note (dead note)
        la (A_Harmonic,  { { 30, 110 } }, true, true),          // F#0 held, high velocity: artificial flageolet
    };
    if (vibKey) l.arts.push_back (la (A_Vibrato, vib));          // G0 high: vibrato on
    if (electric)
    {
        std::vector<KsPress> leg { { 39, 120 } }; if (vibKey) leg.push_back ({ 31, 40 });
        l.arts.push_back (la (A_Legato, leg));                   // D#1 high: auto legato on
        l.arts.push_back (la (A_Gliss, { { 37, 100 } }, true, true));   // C#1 held: Gliss up / down
    }
    else l.arts.push_back (la (A_Legato, { { 33, 110 } }, true, true)); // no legato mode: legato notes are hammer-ons
    // the library's other keys, by their own names
    int k = 0;
    l.arts.push_back (cu (k++, "Slide up 1 fast", { { 32, 110 } }, true));
    l.arts.push_back (cu (k++, "Slide up 2 slow", { { 34, 30 } }, true));
    l.arts.push_back (cu (k++, "Slide up 2 fast", { { 34, 110 } }, true));
    l.arts.push_back (cu (k++, "Natural flageolet", { { 30, 30 } }, true));
    l.arts.push_back (cu (k++, "Chord mode B on", { { 22, 120 } }));
    l.arts.push_back (cu (k++, "Chord mode B off", { { 22, 40 } }));
    l.arts.push_back (cu (k++, "Open chord mode on", { { 25, 120 } }));
    l.arts.push_back (cu (k++, "Open chord mode off", { { 25, 40 } }));
    l.arts.push_back (cu (k++, "Open string mode on", { { 27, 120 } }));
    l.arts.push_back (cu (k++, "Open string mode off", { { 27, 40 } }));
    if (! vibKey) l.arts.push_back (cu (k++, "Sustain (G0)", { { 31, 100 } }));
    if (electric)
    {
        l.arts.push_back (cu (k++, "Repetition down", { { 38, 100 } }, true));
        l.arts.push_back (cu (k++, "Feedback", { { 87, 100 } }, true));
        l.arts.push_back (cu (k++, "Set fret position 0 / Shift fret position", { { 88, 100 } }));
        l.arts.push_back (cu (k++, "Set fret position 1 / Shift fret position", { { 89, 100 } }));
        for (int f = 2; f <= 19; ++f) l.arts.push_back (cu (k++, ("Set fret position " + std::to_string (f)).c_str(), { { 88 + f, 100 } }));
        l.arts.push_back (cu (k++, "FX", { { 108, 100 } }, true));
        l.arts.push_back (cu (k++, "Fretnoise", { { 109, 100 } }, true));
        l.repKey = 36;                                           // C1: Repetition up (fast repeats alternate onto it)
    }
    l.init = { { 22, 40 }, { 25, 40 }, { 27, 40 } };             // chord modes and open-string mode off
    if (vibKey) l.init.push_back ({ 31, 40 });                  // vibrato off
    if (electric) l.init.push_back ({ 39, 40 });                // auto legato off until a legato note
    l.tuning = { 40, 45, 50, 55, 59, 64 };                       // E A D G B E
    l.stringKeys = { 21, 23, 24, 26, 28, 29 };                   // Select string E6, A, D, G, B, E1
    l.maxFret = electric ? 22 : 19;
    l.ccExpr = 11; l.ccVib = 1;
    l.overlapMs = electric ? 25 : 0;                             // legato mode plays overlapping notes as hammer-ons / pull-offs
    l.lo = 40; l.hi = electric ? 86 : 83;                        // the open low E up to the last fret: every key-switch stays outside
    l.note = electric
        ? "Kontakt key-switch page, key for key: strings A-1..F0, slide G#0 / A#0, hammer / pull A0, mute / X-note B0, flageolet F#0, vibrato G0, repetition C1 / D1, gliss C#1, auto legato D#1, feedback D#5, fret positions E5-B6, FX C7, fret noise C#7."
        : (g == EG_Nylon ? "Kontakt key-switch page, key for key (keys 21-35): strings A-1..F0, chord modes, open strings, flageolet F#0, vibrato G0, slides G#0 / A#0, hammer / pull A0, mute / X-note B0."
                         : "Kontakt key-switch page, key for key (keys 21-35): strings A-1..F0, chord modes, open strings, flageolet F#0, Sustain G0, slides G#0 / A#0, hammer / pull A0, mute / X-note B0.");
    l.source = "Ilya Efimov Kontakt key-switch page (your screenshots). Velocity pairs: first name = low velocity";
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
        add (ilyaGuitar ("Ilya Efimov Nylon Guitar", EG_Nylon));
        add (ilyaGuitar ("Ilya Efimov Acoustic Guitar", EG_Acoustic));
        add (ilyaGuitar ("Ilya Efimov LP Electric Guitar", EG_LP));
        add (ilyaGuitar ("Ilya Efimov TC Electric Guitar", EG_TC));
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
        // v3.14.1 from the user guide: no articulation keys (H.A.T.: staccato, legato and double-tonguing come from the air flow and how notes
        // overlap); effect keys pressed while the note plays: C#1 fall, D#1 rise, F1 / F#1 bend down / up; E1 repeats the current note
        // (static tonguing); A1 is a breath sound, C1 key noise. Legato = overlapping notes. Vibrato is the plugin's own (Auto): no CC sent,
        // because in Manual mode that controller bends the pitch around 64.
        { LibArt f = la (A_Fall, { { 37, 100 } }); f.when = KW_During; f.atFrac = 0.6; f.pressMs = 60; h.arts.push_back (f); }
        { LibArt d = la (A_Doit, { { 39, 100 } }); d.when = KW_During; d.atFrac = 0.55; d.pressMs = 60; h.arts.push_back (d); }
        { LibArt b = la (A_Bend, { { 42, 100 } }); b.when = KW_During; b.atFrac = 0.35; b.pressMs = 80; h.arts.push_back (b); }
        { LibArt c = la (A_Scoop, { { 41, 100 } }); c.when = KW_During; c.atFrac = 0.05; c.pressMs = 50; h.arts.push_back (c); }
        h.repKey = 40; h.breathKey = 45; h.breathVel = 45;
        h.overlapLegato = true; h.overlapMs = 30;
        h.ccExpr = 1; h.ccVib = -1;
        h.note = "Air flow = mod wheel (CC1), shaped per phrase. Legato by overlapping notes. Fall C#1, rise D#1, bend up F#1, scoop (bend down) F1, "
                 "repeated notes E1 (static tonguing), a breath (A1) before phrases. Vibrato: leave the plugin on Auto. Keys assume UVI numbering "
                 "(middle C = C3); if they do not trigger, move them an octave in this profile.";
        h.source = "VHorns Saxophones user guide (acousticsamples.net, product manual 101): keyswitch list, air flow, legato by overlap, vibrato modes";
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
        // Toontrack EZbass (Toontrack numbering, middle C = C3 = 60, so C0 = 24). v2.12: every key as you tested and described it in
        // EZbass (Nov 2026), as a technique with its own timing:
        //   G#0 legato: held through a run of overlapping notes (EZbass also legatos overlapping notes by itself; G#0 makes it a real
        //       hammer-on / pull-off, and trills). F#0 slide: held, glides between two overlapping notes at any distance.
        //   F0 / G0 slide down / up: pressed WHILE a note is held, the held note slides away (a slide-out). E0 ghost: the key makes the
        //   ghost tap itself (no pitched note). A#-1 harmonic and B-1 slap / pop: held before and through their note. A-1 polyphony:
        //   held while notes must overlap without legato. D0 repeater: a stutter (repeats the previous note), so it is NOT used for repeated
        //   notes, only placed by hand. C0 palm mute: from Toontrack's release notes (not in the list you tested).
        Library m; m.name = "Toontrack EZbass"; m.maker = "Toontrack"; m.inst = I_Bass; m.verify = 0;
        auto during = [] (int art, int key) { LibArt a = la (art, { { key, 100 } }, false, true); a.when = KW_During; a.atFrac = 0.55; a.pressMs = 40; return a; };
        LibArt ghost = la (A_Ghost, { { 28, 100 } }, false, true); ghost.when = KW_Replace; ghost.pressMs = 60;
        m.arts = { la (A_Sustain, {}),
                   la (A_Legato,    { { 32, 100 } }, true, true),    // G#0 legato (held through the run)
                   la (A_HammerOn,  { { 32, 100 } }, true, true),
                   la (A_PullOff,   { { 32, 100 } }, true, true),
                   la (A_Slide,     { { 30, 100 } }, true, true),    // F#0 slide between two overlapping notes (held)
                   la (A_Gliss,     { { 30, 100 } }, true, true),
                   during (A_Fall, 29),                              // F0 slide down: pressed while the note is held
                   during (A_Doit, 31),                              // G0 slide up: pressed while the note is held
                   ghost,                                            // E0 ghost tap (the key is the sound)
                   la (A_PalmMute,  { { 24, 100 } }, true, true),    // C0 palm mute (release notes)
                   la (A_Harmonic,  { { 22, 100 } }, true, true),    // A#-1 harmonic (held)
                   la (A_Slap,      { { 23, 100 } }, true, true) };  // B-1 slap / pop (held)
        { LibArt c1 = la (kCustomArt + 0, { { 21, 100 } }, true, true); c1.name = "Polyphony (no legato)"; m.arts.push_back (c1); }   // A-1 (held)
        { LibArt c2 = la (kCustomArt + 1, { { 26, 100 } }, false, true); c2.name = "Repeater (stutter)"; c2.pressMs = 40; m.arts.push_back (c2); }   // D0
        m.ccExpr = 11; m.ccVib = 1;
        m.repKey = -1;                                               // D0 is a stutter, not a repetition key: repeated notes are just played
        m.transpose = 12; m.lo = 35; m.hi = 81;
        m.overlapMs = 20; m.overlapLegato = true; m.detachKey = 21; m.preRollMs = 20;
        m.note = "EZbass (Toontrack numbering, C3 = 60): legato G#0 held through a run, slide F#0 between overlapping notes, slide down F0 / up G0 pressed while a note is held, "
                 "ghost tap E0, harmonic A#-1, slap / pop B-1, polyphony A-1 for notes that ring together, palm mute C0. Repeater D0 is in the Articulation menu. Sent an octave up (low E = E1).";
        m.source = "Your EZbass test and description (Nov 2026); palm mute C0 from Toontrack's release notes";
        add (m);
        Library pl; pl.name = "Plain bass (any bass sampler, no key-switches)"; pl.maker = ""; pl.inst = I_Bass; pl.verify = 1; pl.noKeySwitches = true;
        pl.note = "Notes only: no key-switch notes are written. Ghost, slide and mute are still shown in the piano roll and in the MIDI as note length and velocity.";
        pl.source = "plain MIDI";
        add (pl);
        // v3.8: the Ilya Efimov basses, key for key from their Kontakt key-switch pages (Modern and Classic screenshots, Nov 2026;
        // the Fretless, Retro and Rock use the same keys). Kontakt numbering, C3 = 60. "on/off (velo)": 101-127 = on, 1-100 = off.
        //   strings A-1 (B, five-string only), B-1 (E), C0 (A), D0 (D), E0 (G); C#0 chord mode, D#0 open strings, F0 X-note, F#0 easy legato,
        //   G0 auto legato, G#0 FX glissando, A0 pop, A#0 gliss up / down; E4 repetition, F4 repetition mute, F#4 fingered, G4 hammer on / pull off,
        //   G#4 slap, A4 flageolet, A#4 mute, B4 slide up, C5 hit 1, C#5 shift fret position -1/-3, D5 hit 2, D#5 shift fret position +1/+3,
        //   E5..C#6 set fret position 0..9.
        auto ilyaBass = [&] (const char* name, bool fiveString)
        {
            Library b; b.name = name; b.maker = "Ilya Efimov"; b.inst = I_Bass; b.verify = 0;
            const KsPress xOff { 29, 40 }, fing { 78, 100 };
            auto cu = [] (int k, const char* nm, std::vector<KsPress> p, bool hold = false) { LibArt a = la (kCustomArt + k, std::move (p), hold, hold); a.name = nm; return a; };
            b.arts = { la (A_Sustain,  { fing, xOff }), la (A_Legato, { fing, xOff }), la (A_Staccato, { fing, xOff }), la (A_Marcato, { fing, xOff }),
                       la (A_Ghost,    { { 29, 120 } }),                    // F0 X-note mode on (dead notes) until the next articulation
                       la (A_HammerOn, { { 79, 100 } }, true, true),        // G4 Hammer on / Pull off (hold)
                       la (A_PullOff,  { { 79, 100 } }, true, true),
                       la (A_PalmMute, { { 82, 100 }, xOff }),              // A#4 Mute
                       la (A_Harmonic, { { 81, 100 } }, true, true),        // A4 Flageolet (hold)
                       la (A_Slide,    { { 83, 100 } }, true, true),        // B4 Slide up (hold)
                       la (A_Gliss,    { { 34, 100 } }, true, true),        // A#0 Gliss up / down (hold)
                       la (A_Slap,     { { 80, 100 }, xOff }) };            // G#4 Slap
            int k = 0;
            b.arts.push_back (cu (k++, "Pop", { { 33, 100 } }, true));
            { LibArt fx = cu (k++, "FX glissando", { { 32, 100 } }); fx.latch = true; fx.pressMs = 40; fx.fxLo = 35; fx.fxHi = 74; b.arts.push_back (fx); }   // G#0: one FX per key B0..D4
            b.arts.push_back (cu (k++, "Repetition mute", { { 77, 100 } }, true));
            b.arts.push_back (cu (k++, "Hit 1", { { 84, 100 } }, true));
            b.arts.push_back (cu (k++, "Hit 2", { { 86, 100 } }, true));
            b.arts.push_back (cu (k++, "Shift fret position -1", { { 85, 40 } }));
            b.arts.push_back (cu (k++, "Shift fret position -3", { { 85, 120 } }));
            b.arts.push_back (cu (k++, "Shift fret position +1", { { 87, 40 } }));
            b.arts.push_back (cu (k++, "Shift fret position +3", { { 87, 120 } }));
            b.arts.push_back (cu (k++, "Set fret position 0 / Shift fret position", { { 88, 100 } }));
            b.arts.push_back (cu (k++, "Set fret position 1 / Shift fret position", { { 89, 100 } }));
            for (int f = 2; f <= 9; ++f) b.arts.push_back (cu (k++, ("Set fret position " + std::to_string (f)).c_str(), { { 88 + f, 100 } }));
            b.arts.push_back (cu (k++, "Chord mode on", { { 25, 120 } }));
            b.arts.push_back (cu (k++, "Chord mode off", { { 25, 40 } }));
            b.arts.push_back (cu (k++, "Open string mode on", { { 27, 120 } }));
            b.arts.push_back (cu (k++, "Open string mode off", { { 27, 40 } }));
            b.arts.push_back (cu (k++, "Easy legato on", { { 30, 120 } }));
            b.arts.push_back (cu (k++, "Easy legato off", { { 30, 40 } }));
            b.arts.push_back (cu (k++, "Auto legato on", { { 31, 120 } }));
            b.arts.push_back (cu (k++, "Auto legato off", { { 31, 40 } }));
            b.init = { { 31, 120 }, { 30, 40 }, { 29, 40 }, { 27, 40 }, { 25, 40 } };   // auto legato on, easy legato / X-note / open strings / chord mode off
            b.transpose = 12; b.hi = 75; b.overlapMs = 25; b.repKey = 76;            // E4 Repetition
            b.overlapLegato = true; b.preRollMs = 20;                                // auto legato is on: only joined notes may overlap
            b.tuning = fiveString ? std::vector<int> { 23, 28, 33, 38, 43 } : std::vector<int> { 28, 33, 38, 43 };
            b.stringKeys = fiveString ? std::vector<int> { 21, 23, 24, 26, 28 } : std::vector<int> { 23, 24, 26, 28 };
            b.lo = fiveString ? 35 : 40; b.maxFret = 20;
            b.ccExpr = 11; b.ccVib = 1;
            b.note = std::string (fiveString ? "Five strings" : "Four strings") + ", key for key from the Kontakt key-switch page: auto legato by overlap, hammer / pull G4, X-notes F0, mute A#4, slap G#4, flageolet A4, slide B4, gliss A#0, repetition E4; FX glissando G#0 (a different effect on every key B0..D4, switched off by the next key) at some section endings; Pop, hits and fret positions are in the Articulation menu. Sent an octave up.";
            b.source = "Ilya Efimov Kontakt key-switch page (Modern and Classic screenshots; same keys on every Efimov bass)";
            add (b);
        };
        ilyaBass ("Ilya Efimov Modern Bass", true);
        ilyaBass ("Ilya Efimov Classic Bass", false);
        ilyaBass ("Ilya Efimov Fretless Bass", true);
        ilyaBass ("Ilya Efimov Retro Bass", false);
        ilyaBass ("Ilya Efimov Rock Bass", true);
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
      custom = Down stroke: 25@100 hold          (v3.8: the library's own key, placed by hand from the Articulation menu)
      init = 31@120, 30@40                       (mode keys sent once at the start)
      ks = Fall: 29@100 oneshot during at=0.6 press=40   (v3.12: pressed while the note sounds, 60% into it, for 40 ms)
      ks = Ghost: 28@100 oneshot replace press=60        (v3.12: the key is the sound; no pitched note is sent)
      custom = FX glissando: 32@100 latch press=40 fx=35-74 (v3.14: a mode key; the next note's key switches it off; fx = the keys with effects)
      preroll_ms = 20   overlap_is_legato = 1   detach_key = 21   (v3.12 performance settings)
      breath_key = 45@45   vibrato_cc = -1   (v3.14.1: a breath sound before phrases; -1 = no vibrato controller)
    Articulation names are the ones shown in Enoch Bass (Sustain, Legato, Staccato, Marcato, Ghost, PalmMute, HammerOn, PullOff, Slide, Bend, Vibrato,
    Harmonic, Growl, Flutter, Fall, Doit, Scoop, Subtone, Shake, Trill, Tremolo, Pizzicato, Gliss, ColLegno, Arpeggio, Mute, Strum). */
inline int articulationFromName (std::string s)
{
    std::string k; for (char c : s) if (std::isalnum ((unsigned char) c)) k += (char) std::tolower ((unsigned char) c);
    struct E { const char* n; int a; };
    static const E t[] = { { "sustain", A_Sustain }, { "legato", A_Legato }, { "staccato", A_Staccato }, { "marcato", A_Marcato }, { "accent", A_Marcato }, { "ghost", A_Ghost },
        { "dead", A_Ghost }, { "palmmute", A_PalmMute }, { "hammeron", A_HammerOn }, { "pulloff", A_PullOff }, { "slide", A_Slide }, { "slap", A_Slap }, { "pop", A_Pop }, { "bend", A_Bend }, { "vibrato", A_Vibrato },
        { "harmonic", A_Harmonic }, { "growl", A_Growl }, { "flutter", A_Flutter }, { "flutter-tongue", A_Flutter }, { "fall", A_Fall }, { "slideout", A_Fall }, { "doit", A_Doit }, { "scoop", A_Scoop },
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
        else if (key == "vibrato_cc") l.ccVib = std::clamp (std::atoi (val.c_str()), -1, 127);     // -1 = no vibrato controller (the instrument makes its own)
        else if (key == "no_keyswitches") l.noKeySwitches = std::atoi (val.c_str()) != 0;
        // ---- v3.0 driver settings
        else if (key == "transpose") l.transpose = std::clamp (std::atoi (val.c_str()), -48, 48);
        else if (key == "range") { int a2 = 0, b2 = 127; if (std::sscanf (val.c_str(), "%d - %d", &a2, &b2) >= 1 || std::sscanf (val.c_str(), "%d-%d", &a2, &b2) >= 1) { l.lo = std::clamp (a2, 0, 127); l.hi = std::clamp (b2, 0, 127); } }
        else if (key == "overlap_ms") l.overlapMs = std::clamp (std::atof (val.c_str()), 0.0, 500.0);
        else if (key == "preroll_ms") l.preRollMs = std::clamp (std::atof (val.c_str()), 0.0, 200.0);
        else if (key == "overlap_is_legato") l.overlapLegato = std::atoi (val.c_str()) != 0;
        else if (key == "detach_key") l.detachKey = std::clamp (std::atoi (val.c_str()), -1, 127);
        else if (key == "repetition_key") l.repKey = std::clamp (std::atoi (val.c_str()), -1, 127);
        else if (key == "breath_key") { int k = -1, v = 45; std::sscanf (val.c_str(), "%d@%d", &k, &v); l.breathKey = std::clamp (k, -1, 127); l.breathVel = std::clamp (v, 1, 127); }
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
        else if (key == "ks" || key == "custom")
        {
            const size_t c = val.find (':'); if (c == std::string::npos) { error = "line " + std::to_string (lineNo) + ": ks needs 'Articulation: note@velocity'"; return false; }
            int art = -1; std::string cname;
            if (key == "custom")
            {
                int used = 0; for (auto& x : l.arts) if (x.art >= (int) A_Count) ++used;
                if (used >= kMaxCustom) { error = "line " + std::to_string (lineNo) + ": too many custom keys (" + std::to_string (kMaxCustom) + " at most)"; return false; }
                art = kCustomArt + used; cname = trim (val.substr (0, c));
                if (cname.empty()) cname = "Key " + std::to_string (used + 1);
            }
            else art = articulationFromName (trim (val.substr (0, c)));
            if (art < 0) { error = "line " + std::to_string (lineNo) + ": unknown articulation '" + trim (val.substr (0, c)) + "' (use custom = Name: note@velocity for the library's own keys)"; return false; }
            LibArt a; a.art = art; a.name = cname;
            std::string rest = val.substr (c + 1); std::string lower; for (char ch : rest) lower += (char) std::tolower ((unsigned char) ch);
            a.hold = lower.find ("hold") != std::string::npos; a.oneShot = lower.find ("oneshot") != std::string::npos || lower.find ("one-shot") != std::string::npos;
            if (lower.find ("during") != std::string::npos) a.when = KW_During;
            else if (lower.find ("replace") != std::string::npos) a.when = KW_Replace;
            { const size_t pm = lower.find ("press="); if (pm != std::string::npos) a.pressMs = std::clamp (std::atof (lower.c_str() + pm + 6), 0.0, 1000.0); }
            { const size_t af = lower.find ("at="); if (af != std::string::npos) a.atFrac = std::clamp (std::atof (lower.c_str() + af + 3), 0.05, 0.95); }
            a.latch = lower.find ("latch") != std::string::npos;
            { const size_t fx = lower.find ("fx="); if (fx != std::string::npos) { int x = -1, y = -1; if (std::sscanf (lower.c_str() + fx + 3, "%d-%d", &x, &y) == 2 && x >= 0 && y <= 127 && x <= y) { a.fxLo = x; a.fxHi = y; } } }
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
    if (l.preRollMs != 20) o << "preroll_ms = " << l.preRollMs << "\n";
    if (l.overlapLegato) o << "overlap_is_legato = 1\n";
    if (l.detachKey >= 0) o << "detach_key = " << l.detachKey << "\n";
    if (l.repKey >= 0) o << "repetition_key = " << l.repKey << "\n";
    if (l.breathKey >= 0) o << "breath_key = " << l.breathKey << "@" << l.breathVel << "\n";
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
        if (a.art >= (int) A_Count) o << "custom = " << (a.name.empty() ? std::string ("Key") : a.name) << ": ";
        else o << "ks = " << artName (a.art) << ": ";
        if (a.press.empty()) o << "none";
        for (size_t i = 0; i < a.press.size(); ++i) o << (i ? ", " : "") << a.press[i].note << "@" << a.press[i].vel;
        if (a.hold) o << " hold";
        if (a.oneShot) o << " oneshot";
        if (a.when == KW_During) o << " during at=" << a.atFrac;
        if (a.when == KW_Replace) o << " replace";
        if (a.pressMs > 0) o << " press=" << a.pressMs;
        if (a.latch) o << " latch";
        if (a.fxLo >= 0 && a.fxHi >= a.fxLo) o << " fx=" << a.fxLo << "-" << a.fxHi;
        o << "\n";
    }
    return o.str();
}
} // namespace enochsolo
