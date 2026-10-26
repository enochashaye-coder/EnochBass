// Enoch Bass - instruments, articulations and artist style profiles
#pragma once
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <cassert>
#include "Theory.h"

namespace enochsolo
{

enum Inst { I_Guitar = 0, I_Sax, I_Violin, I_Piano, I_Trumpet, I_Bass, I_Count };   // Enoch Bass only ever uses I_Bass; the others stay so the shared engine keeps working

inline const char* instName (int i)
{
    static const char* n[] = { "Guitar", "Sax", "Violin", "Piano", "Trumpet", "Bass" };
    return n[std::clamp (i, 0, (int) I_Count - 1)];
}

//==============================================================================
// Global articulation ids. Every instrument exposes a subset (its key-switch list).
enum Art
{
    A_Sustain = 0, A_Legato, A_Staccato, A_Marcato, A_Ghost, A_PalmMute, A_HammerOn, A_PullOff,
    A_Slide, A_Bend, A_Vibrato, A_Harmonic, A_Growl, A_Flutter, A_Fall, A_Doit, A_Scoop,
    A_Subtone, A_Shake, A_Trill, A_Tremolo, A_Pizz, A_Gliss, A_ColLegno, A_Arpeggio, A_Mute,
    A_Strum, A_Slap, A_Count
};

inline const char* artName (int a)
{
    static const char* n[] = {
        "Sustain", "Legato", "Staccato", "Marcato / Accent", "Ghost / Dead Note", "Palm Mute", "Hammer-On", "Pull-Off",
        "Slide", "Bend", "Vibrato", "Harmonic", "Growl", "Flutter-Tongue", "Fall", "Doit", "Scoop",
        "Subtone", "Shake", "Trill", "Tremolo", "Pizzicato", "Glissando", "Col Legno", "Arpeggio / Roll", "Mute (Harmon)",
        "Strum / Chord", "Slap" };
    return n[std::clamp (a, 0, (int) A_Count - 1)];
}

/** Key-switch order for each instrument. Index in this list = semitone offset from the key-switch base note. */
inline const std::vector<int>& artList (int inst)
{
    static const std::vector<int> guitar  = { A_Sustain, A_Legato, A_Staccato, A_PalmMute, A_HammerOn, A_PullOff, A_Slide, A_Bend, A_Vibrato, A_Harmonic, A_Ghost, A_Strum };
    static const std::vector<int> sax     = { A_Sustain, A_Legato, A_Staccato, A_Marcato, A_Growl, A_Flutter, A_Fall, A_Doit, A_Scoop, A_Subtone, A_Shake, A_Trill, A_Ghost };
    static const std::vector<int> violin  = { A_Sustain, A_Legato, A_Staccato, A_Marcato, A_Pizz, A_Tremolo, A_Trill, A_Gliss, A_Harmonic, A_Vibrato, A_Scoop, A_Fall, A_ColLegno };
    static const std::vector<int> piano   = { A_Sustain, A_Staccato, A_Marcato, A_Ghost, A_Trill, A_Tremolo, A_Gliss, A_Arpeggio };
    static const std::vector<int> trumpet = { A_Sustain, A_Legato, A_Staccato, A_Marcato, A_Growl, A_Flutter, A_Fall, A_Doit, A_Scoop, A_Shake, A_Mute, A_Trill, A_Ghost };
    static const std::vector<int> bass    = { A_Sustain, A_Legato, A_Staccato, A_PalmMute, A_Ghost, A_HammerOn, A_PullOff, A_Slide, A_Harmonic, A_Slap, A_Marcato };
    switch (inst) { case I_Bass: return bass; case I_Guitar: return guitar; case I_Sax: return sax; case I_Violin: return violin; case I_Piano: return piano; default: return trumpet; }
}

/** index of an articulation inside the instrument's key-switch list, or -1 */
inline int artIndex (int inst, int art)
{
    const auto& l = artList (inst);
    for (size_t i = 0; i < l.size(); ++i) if (l[(size_t) i] == art) return (int) i;
    return -1;
}

/** map an articulation the instrument doesn't have to the nearest one it does */
inline int artMapped (int inst, int art)
{
    if (artIndex (inst, art) >= 0) return art;
    switch (art)
    {
        case A_Vibrato: case A_Legato: case A_HammerOn: case A_PullOff: case A_Slide: case A_Gliss: return artIndex (inst, A_Legato) >= 0 ? A_Legato : A_Sustain;
        case A_PalmMute: case A_Ghost: return artIndex (inst, A_Staccato) >= 0 ? A_Staccato : A_Sustain;
        case A_Marcato: return artIndex (inst, A_Staccato) >= 0 ? A_Staccato : A_Sustain;
        default: return A_Sustain;
    }
}

//==============================================================================
struct Range { int lo, hi; };
inline Range instRange (int inst, int variant)
{
    switch (inst)
    {
        case I_Guitar:  return { 40, 88 };
        case I_Sax:     return variant == 1 ? Range { 49, 81 } : Range { 44, 76 };   // alto / tenor (sounding pitch)
        case I_Violin:  return { 55, 100 };
        case I_Piano:   return { 48, 96 };
        case I_Bass:    return { 28, 60 };                                             // E1 .. C4 (4-string bass, plus a little room up the neck)
        default:        return { 54, 88 };                                            // trumpet (sounding pitch)
    }
}

//==============================================================================
struct Style
{
    std::string name;
    int inst = I_Guitar;
    int variant = 0;          // sax: 1 = alto, 2 = tenor
    std::string desc;
    std::string cat = "Jazz";   // menu group
    std::string fav;            // genres it sounds best with

    float density = .5f, rest = .3f, sync = .4f, leap = .35f, step = .6f, arp = .15f, chroma = .3f, bebop = .2f;
    float motif = .3f, blues = .3f, pent = .3f, fourths = .05f, range = .6f, regBias = .5f, swing = .5f;
    float legato = .5f, stacc = .2f, vib = .4f, dyn = .5f, laid = 0.f, vel = 82.f;
    float ghost = .05f, hammer = 0.f, pull = 0.f, slide = 0.f, bend = 0.f, growl = 0.f, flutter = 0.f;
    float fall = 0.f, doit = 0.f, scoop = 0.f, subtone = 0.f, shake = 0.f, trill = 0.f, trem = 0.f;
    float harm = 0.f, pizz = 0.f, gliss = 0.f, octave = 0.f, dbl = 0.f, chordal = 0.f, pedal = 0.f;
    float grace = .1f, muted = 0.f;
    float slap = 0.f, palm = 0.f;     // bass: slap-and-pop weight, palm-muted notes
};

inline bool applyStyleKey (Style& s, const std::string& k, float v)
{
    static const std::map<std::string, float Style::*> m = {
        {"density",&Style::density},{"rest",&Style::rest},{"sync",&Style::sync},{"leap",&Style::leap},{"step",&Style::step},
        {"arp",&Style::arp},{"chroma",&Style::chroma},{"bebop",&Style::bebop},{"motif",&Style::motif},{"blues",&Style::blues},
        {"pent",&Style::pent},{"fourths",&Style::fourths},{"range",&Style::range},{"regBias",&Style::regBias},{"swing",&Style::swing},
        {"legato",&Style::legato},{"stacc",&Style::stacc},{"vib",&Style::vib},{"dyn",&Style::dyn},{"laid",&Style::laid},{"vel",&Style::vel},
        {"ghost",&Style::ghost},{"hammer",&Style::hammer},{"pull",&Style::pull},{"slide",&Style::slide},{"bend",&Style::bend},
        {"growl",&Style::growl},{"flutter",&Style::flutter},{"fall",&Style::fall},{"doit",&Style::doit},{"scoop",&Style::scoop},
        {"subtone",&Style::subtone},{"shake",&Style::shake},{"trill",&Style::trill},{"trem",&Style::trem},{"harm",&Style::harm},
        {"pizz",&Style::pizz},{"gliss",&Style::gliss},{"octave",&Style::octave},{"dbl",&Style::dbl},{"chordal",&Style::chordal},
        {"pedal",&Style::pedal},{"grace",&Style::grace},{"muted",&Style::muted},{"slap",&Style::slap},{"palm",&Style::palm}
    };
    auto it = m.find (k);
    if (it == m.end()) return false;
    s.*(it->second) = v;
    return true;
}

inline Style mkStyle (const char* name, int inst, int variant, const char* desc,
                      std::initializer_list<std::pair<const char*, float>> kv)
{
    Style s; s.name = name; s.inst = inst; s.variant = variant; s.desc = desc;
    for (auto& p : kv) { const bool ok = applyStyleKey (s, p.first, p.second); (void) ok; assert (ok); }
    return s;
}

/** compact style: traits given as "density .5, rest .3, ..." */
inline Style mkS (const char* name, int inst, int variant, const char* cat, const char* fav, const char* desc, const char* spec)
{
    Style s; s.name = name; s.inst = inst; s.variant = variant; s.desc = desc; s.cat = cat; s.fav = fav;
    std::string item; std::stringstream ss (spec);
    while (std::getline (ss, item, ','))
    {
        std::stringstream is (item); std::string k; float v = 0;
        if (! (is >> k >> v)) continue;
        const bool ok = applyStyleKey (s, k, v); (void) ok; assert (ok);
    }
    return s;
}

inline std::vector<Style> baseStyles()
{
    std::vector<Style> v = {
        //=============================== GUITAR
        mkStyle ("Wes Montgomery", I_Guitar, 0, "Thumb-picked octaves, warm and melodic, building to block chords",
          {{"density",.55f},{"rest",.2f},{"sync",.4f},{"leap",.35f},{"step",.65f},{"arp",.2f},{"chroma",.3f},{"bebop",.3f},{"motif",.35f},{"blues",.45f},{"pent",.2f},
           {"range",.55f},{"regBias",.45f},{"swing",.55f},{"legato",.6f},{"stacc",.15f},{"vib",.1f},{"dyn",.5f},{"laid",6},{"vel",78},{"octave",.75f},{"chordal",.35f},
           {"hammer",.1f},{"pull",.1f},{"slide",.1f},{"bend",.05f},{"ghost",.1f},{"harm",.05f},{"grace",.1f}}),
        mkStyle ("George Benson", I_Guitar, 0, "Fast, bluesy, cutting lines with octave licks",
          {{"density",.75f},{"rest",.15f},{"sync",.5f},{"leap",.4f},{"step",.6f},{"arp",.25f},{"chroma",.35f},{"bebop",.35f},{"motif",.35f},{"blues",.6f},{"pent",.4f},
           {"range",.65f},{"regBias",.55f},{"swing",.5f},{"legato",.6f},{"stacc",.15f},{"vib",.15f},{"dyn",.6f},{"laid",0},{"vel",88},{"octave",.35f},{"chordal",.15f},
           {"hammer",.2f},{"pull",.2f},{"slide",.2f},{"bend",.25f},{"ghost",.1f},{"grace",.15f}}),
        mkStyle ("Norman Brown", I_Guitar, 0, "Smooth R&B pentatonic phrasing with slides and bends",
          {{"density",.5f},{"rest",.3f},{"sync",.45f},{"leap",.3f},{"step",.7f},{"arp",.1f},{"chroma",.1f},{"bebop",.05f},{"motif",.5f},{"blues",.6f},{"pent",.6f},
           {"range",.5f},{"regBias",.5f},{"swing",.15f},{"legato",.65f},{"stacc",.1f},{"vib",.35f},{"dyn",.45f},{"laid",8},{"vel",80},{"octave",.3f},
           {"hammer",.25f},{"pull",.25f},{"slide",.35f},{"bend",.35f},{"grace",.1f}}),
        mkStyle ("Django Reinhardt", I_Guitar, 0, "Blazing gypsy-jazz arpeggios, chromatic runs and slides",
          {{"density",.85f},{"rest",.12f},{"sync",.35f},{"leap",.45f},{"step",.5f},{"arp",.55f},{"chroma",.5f},{"bebop",.25f},{"motif",.3f},{"blues",.1f},{"pent",.05f},
           {"range",.75f},{"regBias",.6f},{"swing",.65f},{"legato",.45f},{"stacc",.2f},{"vib",.3f},{"dyn",.65f},{"laid",-3},{"vel",95},
           {"hammer",.3f},{"pull",.3f},{"slide",.25f},{"bend",.02f},{"ghost",.05f},{"harm",.05f},{"grace",.3f}}),
        mkStyle ("Charlie Christian", I_Guitar, 0, "Swing-era horn-like riffs, simple repeating motifs",
          {{"density",.6f},{"rest",.2f},{"sync",.45f},{"leap",.3f},{"step",.7f},{"arp",.25f},{"chroma",.2f},{"bebop",.1f},{"motif",.65f},{"blues",.6f},{"pent",.35f},
           {"range",.5f},{"regBias",.5f},{"swing",.7f},{"legato",.5f},{"stacc",.2f},{"vib",.1f},{"dyn",.5f},{"laid",4},{"vel",88},
           {"hammer",.2f},{"pull",.2f},{"slide",.1f},{"bend",.1f},{"ghost",.05f},{"grace",.1f}}),
        mkStyle ("Pat Metheny", I_Guitar, 0, "Lyrical wide-interval lines, fourths and smooth legato",
          {{"density",.55f},{"rest",.25f},{"sync",.35f},{"leap",.5f},{"step",.5f},{"arp",.3f},{"chroma",.15f},{"bebop",.1f},{"motif",.35f},{"blues",.1f},{"pent",.25f},
           {"range",.8f},{"regBias",.6f},{"swing",.3f},{"legato",.75f},{"stacc",.05f},{"vib",.3f},{"dyn",.55f},{"laid",2},{"vel",82},{"fourths",.35f},
           {"bend",.15f},{"slide",.15f},{"hammer",.3f},{"pull",.3f},{"grace",.1f}}),
        mkStyle ("Joe Pass", I_Guitar, 0, "Fluid bebop lines with chord-melody flourishes",
          {{"density",.75f},{"rest",.18f},{"sync",.35f},{"leap",.35f},{"step",.6f},{"arp",.3f},{"chroma",.45f},{"bebop",.45f},{"motif",.25f},{"blues",.25f},{"pent",.1f},
           {"range",.65f},{"regBias",.45f},{"swing",.7f},{"legato",.5f},{"stacc",.15f},{"vib",.1f},{"dyn",.5f},{"laid",2},{"vel",85},{"chordal",.3f},
           {"hammer",.2f},{"pull",.2f},{"slide",.1f},{"ghost",.08f},{"grace",.15f}}),
        mkStyle ("Jim Hall", I_Guitar, 0, "Sparse, conversational, interval-driven lines",
          {{"density",.35f},{"rest",.45f},{"sync",.4f},{"leap",.5f},{"step",.45f},{"arp",.15f},{"chroma",.15f},{"bebop",.1f},{"motif",.4f},{"blues",.2f},{"pent",.2f},
           {"range",.6f},{"regBias",.5f},{"swing",.4f},{"legato",.6f},{"stacc",.1f},{"vib",.05f},{"dyn",.4f},{"laid",6},{"vel",72},{"fourths",.2f},
           {"hammer",.1f},{"pull",.1f},{"slide",.1f},{"grace",.1f}}),

        //=============================== SAX
        mkStyle ("Charlie Parker (Alto)", I_Sax, 1, "Bebop master: enclosures, ghost notes, fast chromatic lines",
          {{"density",.9f},{"rest",.12f},{"sync",.55f},{"leap",.4f},{"step",.55f},{"arp",.35f},{"chroma",.55f},{"bebop",.6f},{"motif",.2f},{"blues",.35f},{"pent",.1f},
           {"range",.8f},{"regBias",.6f},{"swing",.75f},{"legato",.6f},{"stacc",.2f},{"vib",.1f},{"dyn",.65f},{"laid",-4},{"vel",96},{"grace",.25f},{"ghost",.2f},
           {"fall",.15f},{"doit",.1f},{"scoop",.05f},{"trill",.05f}}),
        mkStyle ("John Coltrane (Tenor)", I_Sax, 2, "Sheets of sound: pentatonic cascades and fourths",
          {{"density",.95f},{"rest",.1f},{"sync",.45f},{"leap",.45f},{"step",.45f},{"arp",.3f},{"chroma",.25f},{"bebop",.1f},{"motif",.4f},{"blues",.3f},{"pent",.55f},
           {"range",.9f},{"regBias",.6f},{"swing",.4f},{"legato",.6f},{"stacc",.1f},{"vib",.3f},{"dyn",.8f},{"laid",0},{"vel",100},{"fourths",.55f},
           {"growl",.3f},{"flutter",.15f},{"fall",.1f},{"doit",.1f},{"shake",.05f}}),
        mkStyle ("Sonny Rollins (Tenor)", I_Sax, 2, "Thematic improviser: motifs, humor, calypso rhythms",
          {{"density",.6f},{"rest",.25f},{"sync",.6f},{"leap",.4f},{"step",.55f},{"arp",.25f},{"chroma",.3f},{"bebop",.25f},{"motif",.75f},{"blues",.55f},{"pent",.35f},
           {"range",.7f},{"regBias",.5f},{"swing",.6f},{"legato",.45f},{"stacc",.35f},{"vib",.2f},{"dyn",.7f},{"laid",3},{"vel",96},
           {"growl",.15f},{"fall",.15f},{"scoop",.15f},{"shake",.05f},{"grace",.1f}}),
        mkStyle ("Lester Young (Tenor)", I_Sax, 2, "Light, behind-the-beat, smooth and relaxed",
          {{"density",.45f},{"rest",.3f},{"sync",.5f},{"leap",.25f},{"step",.75f},{"arp",.2f},{"chroma",.1f},{"bebop",.05f},{"motif",.45f},{"blues",.5f},{"pent",.3f},
           {"range",.5f},{"regBias",.6f},{"swing",.8f},{"legato",.8f},{"stacc",.1f},{"vib",.25f},{"dyn",.35f},{"laid",12},{"vel",72},
           {"subtone",.35f},{"fall",.1f},{"scoop",.1f},{"grace",.05f}}),
        mkStyle ("Cannonball Adderley (Alto)", I_Sax, 1, "Soulful, bluesy and joyous alto with a gospel feel",
          {{"density",.8f},{"rest",.15f},{"sync",.6f},{"leap",.35f},{"step",.6f},{"arp",.3f},{"chroma",.35f},{"bebop",.4f},{"motif",.4f},{"blues",.75f},{"pent",.45f},
           {"range",.7f},{"regBias",.6f},{"swing",.7f},{"legato",.5f},{"stacc",.25f},{"vib",.2f},{"dyn",.65f},{"laid",2},{"vel",98},
           {"growl",.15f},{"fall",.2f},{"doit",.15f},{"scoop",.2f},{"shake",.1f},{"ghost",.15f},{"grace",.15f}}),
        mkStyle ("Stan Getz (Tenor)", I_Sax, 2, "Cool, airy, lyrical tenor, bossa-friendly",
          {{"density",.5f},{"rest",.3f},{"sync",.45f},{"leap",.3f},{"step",.7f},{"arp",.3f},{"chroma",.2f},{"bebop",.15f},{"motif",.4f},{"blues",.2f},{"pent",.2f},
           {"range",.6f},{"regBias",.65f},{"swing",.5f},{"legato",.85f},{"stacc",.05f},{"vib",.2f},{"dyn",.4f},{"laid",5},{"vel",74},
           {"subtone",.45f},{"fall",.1f},{"scoop",.05f},{"grace",.05f}}),

        //=============================== VIOLIN
        mkStyle ("Stephane Grappelli", I_Violin, 0, "Elegant swing violin: arpeggios, slides, graceful chromaticism",
          {{"density",.8f},{"rest",.15f},{"sync",.4f},{"leap",.35f},{"step",.6f},{"arp",.45f},{"chroma",.4f},{"bebop",.2f},{"motif",.3f},{"blues",.15f},{"pent",.1f},
           {"range",.75f},{"regBias",.6f},{"swing",.7f},{"legato",.75f},{"stacc",.15f},{"vib",.35f},{"dyn",.55f},{"laid",0},{"vel",82},
           {"gliss",.25f},{"grace",.25f},{"scoop",.1f},{"fall",.05f},{"trill",.05f},{"harm",.02f}}),
        mkStyle ("Stuff Smith", I_Violin, 0, "Hard-swinging, aggressive riffing with double-stops",
          {{"density",.75f},{"rest",.15f},{"sync",.6f},{"leap",.3f},{"step",.6f},{"arp",.25f},{"chroma",.25f},{"bebop",.1f},{"motif",.6f},{"blues",.7f},{"pent",.4f},
           {"range",.6f},{"regBias",.55f},{"swing",.75f},{"legato",.5f},{"stacc",.35f},{"vib",.3f},{"dyn",.75f},{"laid",3},{"vel",100},
           {"dbl",.35f},{"gliss",.25f},{"bend",.25f},{"trem",.1f},{"pizz",.05f},{"scoop",.2f}}),
        mkStyle ("Jean-Luc Ponty", I_Violin, 0, "Fusion violin: fast modal lines, soaring high register",
          {{"density",.85f},{"rest",.12f},{"sync",.45f},{"leap",.45f},{"step",.5f},{"arp",.3f},{"chroma",.2f},{"bebop",.15f},{"motif",.35f},{"blues",.25f},{"pent",.5f},
           {"range",.9f},{"regBias",.75f},{"swing",.2f},{"legato",.7f},{"stacc",.1f},{"vib",.35f},{"dyn",.65f},{"laid",0},{"vel",92},
           {"gliss",.3f},{"fourths",.35f},{"bend",.15f},{"trill",.1f},{"harm",.1f}}),
        mkStyle ("Didier Lockwood", I_Violin, 0, "Virtuosic, explosive runs and wide leaps",
          {{"density",.95f},{"rest",.1f},{"sync",.45f},{"leap",.5f},{"step",.45f},{"arp",.4f},{"chroma",.35f},{"bebop",.3f},{"motif",.3f},{"blues",.2f},{"pent",.35f},
           {"range",.95f},{"regBias",.7f},{"swing",.5f},{"legato",.65f},{"stacc",.15f},{"vib",.3f},{"dyn",.8f},{"laid",-2},{"vel",98},
           {"gliss",.3f},{"trem",.1f},{"harm",.1f}}),
        mkStyle ("Regina Carter", I_Violin, 0, "Warm, soulful and lyrical with bluesy bends",
          {{"density",.55f},{"rest",.25f},{"sync",.45f},{"leap",.35f},{"step",.65f},{"arp",.25f},{"chroma",.2f},{"bebop",.15f},{"motif",.45f},{"blues",.55f},{"pent",.4f},
           {"range",.65f},{"regBias",.6f},{"swing",.45f},{"legato",.75f},{"stacc",.1f},{"vib",.4f},{"dyn",.55f},{"laid",5},{"vel",80},
           {"gliss",.2f},{"bend",.2f},{"scoop",.15f},{"fall",.1f}}),
        mkStyle ("Ray Nance", I_Violin, 0, "Ellington-band violin: bluesy, plaintive, vocal-like",
          {{"density",.55f},{"rest",.25f},{"sync",.5f},{"leap",.3f},{"step",.7f},{"arp",.2f},{"chroma",.2f},{"bebop",.05f},{"motif",.5f},{"blues",.75f},{"pent",.4f},
           {"range",.55f},{"regBias",.5f},{"swing",.75f},{"legato",.6f},{"stacc",.2f},{"vib",.3f},{"dyn",.55f},{"laid",6},{"vel",84},
           {"bend",.3f},{"gliss",.25f},{"scoop",.25f},{"fall",.15f}}),

        //=============================== PIANO
        mkStyle ("Duke Ellington", I_Piano, 0, "Sparse stride-flavored orchestral voicings, blue color tones",
          {{"density",.4f},{"rest",.4f},{"sync",.5f},{"leap",.45f},{"step",.45f},{"arp",.15f},{"chroma",.25f},{"bebop",.05f},{"motif",.55f},{"blues",.65f},{"pent",.3f},
           {"range",.6f},{"regBias",.4f},{"swing",.65f},{"legato",.3f},{"stacc",.35f},{"dyn",.6f},{"laid",5},{"vel",86},{"chordal",.55f},{"pedal",.3f},
           {"trem",.15f},{"gliss",.1f},{"grace",.25f}}),
        mkStyle ("Thelonious Monk", I_Piano, 0, "Angular, percussive, spacious and dissonant",
          {{"density",.35f},{"rest",.5f},{"sync",.7f},{"leap",.6f},{"step",.3f},{"arp",.05f},{"chroma",.45f},{"bebop",.05f},{"motif",.7f},{"blues",.6f},{"pent",.25f},
           {"range",.6f},{"regBias",.5f},{"swing",.6f},{"legato",.15f},{"stacc",.55f},{"dyn",.75f},{"laid",4},{"vel",94},{"chordal",.35f},{"pedal",.1f},
           {"grace",.45f},{"trill",.1f},{"gliss",.1f}}),
        mkStyle ("Art Tatum", I_Piano, 0, "Dazzling runs, cascades and reharmonized flourishes",
          {{"density",1.0f},{"rest",.08f},{"sync",.4f},{"leap",.4f},{"step",.5f},{"arp",.6f},{"chroma",.55f},{"bebop",.35f},{"motif",.15f},{"blues",.3f},{"pent",.1f},
           {"range",1.0f},{"regBias",.55f},{"swing",.65f},{"legato",.6f},{"stacc",.15f},{"dyn",.8f},{"laid",0},{"vel",94},{"chordal",.35f},{"pedal",.5f},
           {"trill",.25f},{"trem",.15f},{"gliss",.45f},{"grace",.35f}}),
        mkStyle ("Bill Evans", I_Piano, 0, "Lyrical, impressionistic, soft rootless voicings",
          {{"density",.45f},{"rest",.35f},{"sync",.4f},{"leap",.4f},{"step",.6f},{"arp",.25f},{"chroma",.15f},{"bebop",.1f},{"motif",.45f},{"blues",.15f},{"pent",.15f},
           {"range",.7f},{"regBias",.55f},{"swing",.35f},{"legato",.7f},{"stacc",.05f},{"dyn",.5f},{"laid",3},{"vel",70},{"chordal",.5f},{"pedal",.6f},{"grace",.15f}}),
        mkStyle ("Oscar Peterson", I_Piano, 0, "Powerful, blues-drenched swinging runs and block chords",
          {{"density",.9f},{"rest",.1f},{"sync",.5f},{"leap",.35f},{"step",.6f},{"arp",.35f},{"chroma",.35f},{"bebop",.4f},{"motif",.35f},{"blues",.65f},{"pent",.35f},
           {"range",.85f},{"regBias",.6f},{"swing",.8f},{"legato",.5f},{"stacc",.2f},{"dyn",.85f},{"laid",-2},{"vel",100},{"chordal",.35f},{"pedal",.25f},
           {"trill",.2f},{"gliss",.25f},{"trem",.15f},{"grace",.2f}}),
        mkStyle ("Herbie Hancock", I_Piano, 0, "Funky, fourths-based, rhythmically playful and modal",
          {{"density",.65f},{"rest",.22f},{"sync",.65f},{"leap",.5f},{"step",.45f},{"arp",.25f},{"chroma",.25f},{"bebop",.15f},{"motif",.5f},{"blues",.4f},{"pent",.45f},
           {"range",.75f},{"regBias",.55f},{"swing",.3f},{"legato",.45f},{"stacc",.3f},{"dyn",.65f},{"laid",0},{"vel",90},{"fourths",.45f},{"chordal",.5f},{"pedal",.3f},{"grace",.2f}}),
        mkStyle ("Chick Corea", I_Piano, 0, "Spanish-tinged, bright and virtuosic, with driving rhythms",
          {{"density",.8f},{"rest",.15f},{"sync",.6f},{"leap",.5f},{"step",.45f},{"arp",.35f},{"chroma",.25f},{"bebop",.2f},{"motif",.5f},{"blues",.2f},{"pent",.45f},
           {"range",.85f},{"regBias",.6f},{"swing",.2f},{"legato",.5f},{"stacc",.25f},{"dyn",.75f},{"laid",0},{"vel",94},{"fourths",.4f},{"chordal",.35f},{"pedal",.25f},
           {"grace",.2f},{"trill",.1f}}),

        //=============================== TRUMPET
        mkStyle ("Louis Armstrong", I_Trumpet, 0, "Warm, vocal melodic phrasing, blues and soaring high notes",
          {{"density",.5f},{"rest",.3f},{"sync",.55f},{"leap",.35f},{"step",.65f},{"arp",.3f},{"chroma",.1f},{"bebop",0},{"motif",.55f},{"blues",.7f},{"pent",.45f},
           {"range",.75f},{"regBias",.6f},{"swing",.8f},{"legato",.5f},{"stacc",.25f},{"vib",.5f},{"dyn",.8f},{"laid",5},{"vel",96},
           {"fall",.35f},{"scoop",.2f},{"shake",.25f},{"doit",.1f},{"growl",.05f}}),
        mkStyle ("Miles Davis", I_Trumpet, 0, "Spacious, lyrical and modal; space speaks, often Harmon muted",
          {{"density",.3f},{"rest",.55f},{"sync",.5f},{"leap",.4f},{"step",.6f},{"arp",.1f},{"chroma",.15f},{"bebop",.05f},{"motif",.5f},{"blues",.4f},{"pent",.4f},
           {"range",.5f},{"regBias",.45f},{"swing",.4f},{"legato",.6f},{"stacc",.15f},{"vib",.1f},{"dyn",.5f},{"laid",6},{"vel",74},{"muted",.5f},
           {"fall",.2f},{"scoop",.15f},{"doit",.1f},{"flutter",.05f}}),
        mkStyle ("Dizzy Gillespie", I_Trumpet, 0, "Blazing high-register bebop with wild leaps and Afro-Cuban flair",
          {{"density",.9f},{"rest",.12f},{"sync",.6f},{"leap",.5f},{"step",.45f},{"arp",.35f},{"chroma",.5f},{"bebop",.55f},{"motif",.3f},{"blues",.3f},{"pent",.15f},
           {"range",.95f},{"regBias",.75f},{"swing",.7f},{"legato",.5f},{"stacc",.35f},{"vib",.2f},{"dyn",.85f},{"laid",-2},{"vel",104},
           {"fall",.15f},{"doit",.2f},{"shake",.15f},{"flutter",.1f},{"growl",.1f}}),
        mkStyle ("Chet Baker", I_Trumpet, 0, "Soft, intimate and lyrical, restrained and vocal",
          {{"density",.35f},{"rest",.4f},{"sync",.35f},{"leap",.25f},{"step",.8f},{"arp",.15f},{"chroma",.1f},{"bebop",.05f},{"motif",.4f},{"blues",.2f},{"pent",.2f},
           {"range",.4f},{"regBias",.45f},{"swing",.5f},{"legato",.85f},{"stacc",.05f},{"vib",.1f},{"dyn",.3f},{"laid",8},{"vel",64},{"fall",.15f},{"scoop",.1f}}),
        mkStyle ("Clifford Brown", I_Trumpet, 0, "Fluent, warm-toned bebop with long clean flowing lines",
          {{"density",.85f},{"rest",.12f},{"sync",.5f},{"leap",.45f},{"step",.5f},{"arp",.4f},{"chroma",.4f},{"bebop",.5f},{"motif",.3f},{"blues",.35f},{"pent",.15f},
           {"range",.9f},{"regBias",.65f},{"swing",.7f},{"legato",.6f},{"stacc",.2f},{"vib",.25f},{"dyn",.75f},{"laid",0},{"vel",98},{"fall",.1f},{"doit",.1f}}),
        mkStyle ("Wynton Marsalis", I_Trumpet, 0, "Virtuosic neo-traditional lines with command of growls and flutters",
          {{"density",.8f},{"rest",.15f},{"sync",.5f},{"leap",.5f},{"step",.5f},{"arp",.35f},{"chroma",.3f},{"bebop",.3f},{"motif",.45f},{"blues",.45f},{"pent",.25f},
           {"range",.9f},{"regBias",.65f},{"swing",.7f},{"legato",.5f},{"stacc",.3f},{"vib",.2f},{"dyn",.8f},{"laid",0},{"vel",98},
           {"fall",.12f},{"doit",.1f},{"shake",.1f},{"growl",.15f},{"flutter",.15f},{"trill",.05f}}),
    };
    return v;
}


//==============================================================================
// Extra artists. Traits are tunable approximations of each player's recognisable approach, not transcriptions.
inline std::vector<Style> extraStyles()
{
    const char* RB = "Rock & Blues"; const char* PS = "Pop & Soul"; const char* AF = "African & Afrocentric";
    const char* BR = "Brazilian"; const char* SP = "Spanish & Latin"; const char* CL = "Classical & Cinematic"; const char* JZ = "Jazz";
    std::vector<Style> v;
    //=============================== GUITAR
    v.push_back (mkS ("Jimi Hendrix", I_Guitar, 0, RB, "Rock, Gospel / Soul", "Psychedelic blues-rock: big bends, hammer-ons, double-stop fills, fiery vibrato",
      "density .7, rest .2, sync .5, leap .35, step .6, arp .15, chroma .15, bebop 0, motif .45, blues .85, pent .8, range .8, regBias .6, swing .2, legato .6, vib .6, dyn .85, laid 4, vel 100, hammer .35, pull .35, slide .2, bend .55, ghost .15, grace .2, chordal .2"));
    v.push_back (mkS ("Prince", I_Guitar, 0, RB, "Pop, Rock, Gospel / Soul", "Funk-rock virtuoso: tight rhythmic licks, screaming bends, pentatonic fire",
      "density .8, rest .15, sync .75, leap .4, step .55, arp .1, chroma .15, motif .5, blues .7, pent .75, range .8, regBias .7, swing .1, legato .5, stacc .25, vib .45, dyn .8, laid 0, vel 100, hammer .3, pull .3, slide .25, bend .5, ghost .25, grace .2"));
    v.push_back (mkS ("Carlos Santana", I_Guitar, 0, RB, "Rock, Samba / Brazilian", "Singing sustained notes, Latin-rock lines, long bends and fluid pentatonics",
      "density .5, rest .3, sync .5, leap .3, step .7, motif .55, blues .5, pent .7, range .7, regBias .65, swing .15, legato .75, vib .6, dyn .7, laid 3, vel 92, hammer .3, pull .3, slide .3, bend .55, harm .1"));
    v.push_back (mkS ("B.B. King", I_Guitar, 0, RB, "Rock, Gospel / Soul", "Sparse vocal-like phrases, butterfly vibrato and single-note bends",
      "density .3, rest .5, sync .45, leap .3, step .7, motif .55, blues .95, pent .85, range .45, regBias .55, swing .5, legato .5, vib .75, dyn .7, laid 6, vel 90, bend .55, hammer .15, pull .15, slide .15, ghost .05"));
    v.push_back (mkS ("Eric Clapton", I_Guitar, 0, RB, "Rock, Pop", "Melodic, expressive blues-rock with smooth bends and vibrato",
      "density .5, rest .3, sync .4, step .65, motif .5, blues .8, pent .75, range .6, swing .3, legato .65, vib .55, dyn .6, laid 3, vel 90, bend .45, hammer .3, pull .3, slide .1"));
    v.push_back (mkS ("David Gilmour", I_Guitar, 0, RB, "Rock, Pop", "Spacious, soaring and emotional: long bends, sustain, singable phrases",
      "density .3, rest .5, sync .3, step .75, motif .6, blues .55, pent .7, range .65, regBias .7, swing 0, legato .85, vib .55, dyn .6, laid 5, vel 85, bend .65, slide .2"));
    v.push_back (mkS ("Stevie Ray Vaughan", I_Guitar, 0, RB, "Rock", "Fiery Texas blues: hard-hit pentatonic runs, ghost notes and fat vibrato",
      "density .8, rest .15, sync .5, leap .35, step .6, motif .4, blues .9, pent .85, range .7, swing .45, legato .5, vib .45, dyn .85, laid 0, vel 105, bend .5, hammer .35, pull .35, ghost .2, grace .2"));
    v.push_back (mkS ("Eddie Van Halen", I_Guitar, 0, RB, "Rock", "Hard-rock virtuosity: lightning hammer-on/pull-off runs and soaring high lines",
      "density .95, rest .1, sync .35, leap .5, step .45, arp .45, chroma .2, blues .35, pent .6, range .95, regBias .75, swing 0, legato .7, vib .35, dyn .85, vel 105, hammer .55, pull .55, slide .2, bend .35, harm .15"));
    v.push_back (mkS ("John Mayer", I_Guitar, 0, PS, "Pop, Rock, Gospel / Soul", "Smooth blues-pop: singing bends, soft hammer-ons, relaxed feel",
      "density .5, rest .3, sync .45, step .65, motif .5, blues .65, pent .7, range .55, swing .3, legato .65, vib .4, dyn .55, laid 4, vel 84, bend .4, hammer .3, pull .3, slide .15"));
    v.push_back (mkS ("Nile Rodgers", I_Guitar, 0, PS, "Pop, Amapiano, Afro-Fusion", "Choppy muted funk 16ths: rhythmic scratches and tight single-note hooks",
      "density .6, rest .3, sync .85, leap .2, step .5, arp .2, pent .6, blues .3, range .4, swing .1, legato .1, stacc .5, dyn .6, laid 0, vel 88, ghost .4, chordal .5"));
    v.push_back (mkS ("King Sunny Ade", I_Guitar, 0, AF, "Juju / Fuji, Afrobeat", "Shimmering interlocking juju lines, bright and singing, hypnotic repetition",
      "density .65, rest .25, sync .6, leap .25, step .75, arp .2, chroma .05, motif .85, blues .2, pent .75, range .55, regBias .65, swing .05, legato .55, stacc .2, vib .2, dyn .5, laid 0, vel 84, hammer .2, pull .2, slide .15, bend .1, ghost .1, grace .1, octave .15"));
    v.push_back (mkS ("Franco Luambo", I_Guitar, 0, AF, "Soukous, Highlife", "Rich sebene guitar: dancing, cascading pentatonic runs and repeated hooks",
      "density .85, rest .15, sync .65, leap .3, step .7, arp .35, chroma .05, motif .85, pent .7, range .65, swing 0, legato .5, stacc .25, vib .15, vel 88, hammer .3, pull .3, slide .1, grace .15, octave .2"));
    v.push_back (mkS ("Ali Farka Toure", I_Guitar, 0, AF, "Ethio-Jazz, Afro-Fusion", "Hypnotic desert blues: pentatonic ostinatos, repeated riffs, bluesy slides",
      "density .5, rest .25, sync .5, leap .2, step .8, motif .9, blues .8, pent .95, range .45, regBias .45, swing .15, legato .4, vib .1, hammer .35, pull .35, slide .25, bend .1, ghost .2, laid 4, vel 80"));
    v.push_back (mkS ("Sir Victor Uwaifo", I_Guitar, 0, AF, "Highlife, Juju / Fuji", "Jaunty highlife picking: bright melodic runs and playful syncopation",
      "density .7, rest .2, sync .6, leap .3, step .7, arp .25, motif .7, pent .6, blues .25, range .6, regBias .6, swing .1, legato .45, stacc .3, vel 86, hammer .2, pull .2, slide .15, grace .2"));
    v.push_back (mkS ("Oliver Mtukudzi", I_Guitar, 0, AF, "Township Jive, Afro-Fusion", "Warm, flowing Zimbabwean groove guitar with gentle repeated riffs",
      "density .5, rest .3, sync .5, step .75, motif .8, pent .8, range .5, swing .15, legato .5, vel 80, laid 3, hammer .25, pull .25, slide .1"));
    v.push_back (mkS ("Jonathan Butler", I_Guitar, 0, AF, "Afro-Fusion, Smooth / Ballad", "Smooth South African jazz-soul: lyrical, pentatonic and warm",
      "density .5, rest .3, sync .45, motif .5, pent .65, blues .5, step .7, legato .7, vib .3, slide .25, bend .25, laid 4, vel 80, swing .25"));
    v.push_back (mkS ("Joao Gilberto", I_Guitar, 0, BR, "Bossa Nova", "Whispered, intimate bossa nova: gentle syncopated nylon-string fingerings",
      "density .35, rest .4, sync .65, leap .3, step .6, arp .3, chroma .2, bebop .05, motif .5, blues 0, pent .1, range .45, regBias .4, swing 0, legato .5, stacc .3, vib 0, dyn .3, laid 3, vel 66, chordal .45, grace .1"));
    v.push_back (mkS ("Baden Powell", I_Guitar, 0, BR, "Samba / Brazilian, Bossa Nova", "Virtuosic afro-samba guitar: driving rhythm, rapid runs, Afro-Brazilian colour",
      "density .75, rest .15, sync .75, leap .4, step .55, arp .35, chroma .25, bebop .1, motif .45, blues .2, pent .25, range .75, regBias .55, swing 0, legato .45, stacc .3, dyn .65, vel 92, hammer .2, pull .2, slide .1, grace .25"));
    v.push_back (mkS ("Toquinho", I_Guitar, 0, BR, "Bossa Nova, Samba / Brazilian", "Warm, melodic MPB nylon guitar, flowing and lyrical",
      "density .5, rest .3, sync .55, step .65, arp .35, motif .45, range .55, legato .55, vel 76, chordal .4, swing 0, laid 2"));
    v.push_back (mkS ("Paco de Lucia", I_Guitar, 0, SP, "Flamenco / Spanish", "Fiery flamenco picado: lightning scale runs, rasgueado bursts, Phrygian colour",
      "density .95, rest .1, sync .6, leap .35, step .55, arp .45, chroma .3, bebop .1, motif .5, blues 0, pent .05, range .85, regBias .65, swing 0, legato .45, stacc .35, vib .15, dyn .85, vel 100, hammer .35, pull .35, slide .15, grace .35, harm .1, chordal .3"));
    v.push_back (mkS ("Vicente Amigo", I_Guitar, 0, SP, "Flamenco / Spanish", "Lyrical modern flamenco: expressive melodic lines with tender ornaments",
      "density .6, rest .25, sync .5, leap .4, step .6, arp .35, motif .55, range .75, legato .55, vib .25, dyn .65, vel 90, grace .35, hammer .3, pull .3, harm .1, laid 2"));
    v.push_back (mkS ("Andres Segovia", I_Guitar, 0, CL, "Flamenco / Spanish, Smooth / Ballad", "Noble classical guitar: clear singing lines, arpeggios and graceful rubato",
      "density .45, rest .3, sync .2, leap .35, step .7, arp .55, motif .4, range .65, legato .65, vib .25, dyn .6, vel 78, swing 0, grace .1, laid 3, chordal .2, harm .1"));

    //=============================== SAX
    v.push_back (mkS ("Clarence Clemons (Tenor)", I_Sax, 2, RB, "Rock, Pop, Gospel / Soul", "Big, heroic E-Street tenor: anthemic melodies, honking growls and soulful long notes",
      "density .4, rest .35, sync .45, leap .3, step .75, arp .15, chroma .1, motif .65, blues .75, pent .6, range .6, regBias .5, swing .2, legato .55, stacc .2, vib .3, dyn .85, laid 4, vel 104, growl .3, scoop .2, fall .15, shake .1, subtone .05"));
    v.push_back (mkS ("King Curtis (Tenor)", I_Sax, 2, RB, "Gospel / Soul, Rock", "Honking Texas R&B tenor: riffs, blues cries and swagger",
      "density .6, rest .25, sync .65, leap .3, step .65, motif .7, blues .9, pent .8, range .6, swing .45, legato .4, stacc .35, vib .2, dyn .85, laid 3, vel 104, growl .4, fall .25, scoop .2, shake .15, ghost .1"));
    v.push_back (mkS ("Fela Kuti (Tenor)", I_Sax, 2, AF, "Afrobeat, Afro-Jazz", "Afrobeat's founder: hypnotic repeating pentatonic horn riffs and long, burning modal solos",
      "density .7, rest .2, sync .65, leap .25, step .75, arp .15, chroma .05, bebop 0, motif .9, blues .55, pent .85, range .6, regBias .5, swing 0, legato .4, stacc .3, vib .2, dyn .8, laid 0, vel 98, growl .2, flutter .15, fall .15, doit .1, scoop .1, shake .05, ghost .1"));
    v.push_back (mkS ("Manu Dibango (Alto)", I_Sax, 1, AF, "Afro-Fusion, Afrobeat, Soukous", "Makossa-soul sax: funky rhythmic riffs with warm growl",
      "density .65, rest .25, sync .7, step .7, motif .85, pent .75, blues .5, range .55, swing .1, legato .4, stacc .35, vib .2, vel 98, growl .25, scoop .15, fall .2, doit .1, laid 0"));
    v.push_back (mkS ("Orlando Julius (Alto)", I_Sax, 1, AF, "Highlife, Afrobeat, Juju / Fuji", "Nigerian highlife-soul alto: bright, jubilant party horn lines",
      "density .7, rest .2, sync .65, step .7, motif .8, pent .7, blues .4, range .6, swing .1, vel 98, growl .1, scoop .15, fall .15, doit .15, stacc .3, vib .2, grace .15"));
    v.push_back (mkS ("Pharoah Sanders (Tenor)", I_Sax, 2, AF, "Ethio-Jazz, Afro-Jazz", "Spiritual jazz: searching overblown cries and modal chanting lines",
      "density .8, rest .15, sync .4, leap .6, step .4, chroma .25, motif .5, blues .3, pent .6, fourths .35, range 1, regBias .75, swing .3, legato .5, vib .3, dyn .95, vel 104, growl .45, flutter .3, shake .25, fall .15"));
    v.push_back (mkS ("Grover Washington Jr. (Alto)", I_Sax, 1, PS, "Smooth / Ballad, Gospel / Soul, Pop", "Smooth-jazz soul: sweet, groovy, melodic and polished",
      "density .45, rest .35, sync .5, step .75, motif .55, blues .6, pent .55, range .55, swing .2, legato .7, vib .3, dyn .5, vel 82, laid 4, scoop .15, fall .1, subtone .1, ghost .1"));
    v.push_back (mkS ("Kenny G (Alto)", I_Sax, 1, PS, "Smooth / Ballad, Pop", "Silky sustained pop-jazz lines with long singing notes",
      "density .35, rest .4, sync .3, step .85, motif .55, blues .15, pent .5, range .7, regBias .7, swing 0, legato .85, vib .55, dyn .45, vel 76, laid 3, scoop .1, fall .1"));
    v.push_back (mkS ("Dave Koz (Alto)", I_Sax, 1, PS, "Pop, Smooth / Ballad", "Warm, funky smooth-jazz alto with pop hooks",
      "density .55, rest .3, sync .55, step .7, motif .6, blues .4, pent .6, range .65, swing .1, legato .6, vib .3, dyn .65, vel 90, growl .05, scoop .1, fall .15, ghost .1, laid 2"));
    v.push_back (mkS ("Candy Dulfer (Alto)", I_Sax, 1, PS, "Pop, Gospel / Soul, Amapiano", "Funk-pop alto: punchy riffs, bluesy licks and a big groove",
      "density .65, rest .25, sync .7, step .65, motif .65, blues .6, pent .75, swing .1, stacc .35, vel 98, growl .1, scoop .15, fall .15, shake .05, ghost .15"));
    v.push_back (mkS ("Wayne Shorter (Tenor)", I_Sax, 2, JZ, "Afro-Jazz, Swing Jazz", "Mysterious, angular modern jazz: spacious motivic lines and surprising intervals",
      "density .5, rest .35, sync .5, leap .55, step .4, chroma .25, motif .55, blues .2, pent .3, fourths .3, range .75, swing .45, legato .6, vib .15, dyn .6, vel 88, grace .15, subtone .1"));
    v.push_back (mkS ("Dexter Gordon (Tenor)", I_Sax, 2, JZ, "Swing Jazz, Smooth / Ballad", "Big, laid-back bebop tenor with huge tone and witty quotes",
      "density .65, rest .25, sync .5, leap .35, step .6, bebop .35, chroma .3, motif .45, blues .5, swing .75, legato .55, vib .2, dyn .75, laid 10, vel 98, growl .1, fall .15, scoop .15"));
    v.push_back (mkS ("Ben Webster (Tenor)", I_Sax, 2, JZ, "Smooth / Ballad, Swing Jazz", "Breathy, tender ballad tenor with huge subtones and slow vibrato",
      "density .3, rest .45, sync .4, step .8, motif .45, blues .55, range .5, swing .6, legato .8, vib .3, subtone .5, laid 12, vel 70, growl .1, fall .15, dyn .4"));
    v.push_back (mkS ("Jorge Pardo (Tenor)", I_Sax, 2, SP, "Flamenco / Spanish", "Flamenco-jazz tenor: Phrygian lines, fast ornamented runs",
      "density .8, rest .15, sync .55, leap .4, step .55, arp .3, chroma .3, motif .45, blues .1, pent .1, range .8, swing .1, legato .5, vib .2, dyn .8, vel 96, trill .1, grace .35, fall .1"));
    v.push_back (mkS ("Paquito D'Rivera (Alto)", I_Sax, 1, SP, "Samba / Brazilian, Afro-Jazz", "Afro-Cuban / Latin jazz alto: fiery, joyful and virtuosic",
      "density .9, rest .1, sync .7, leap .4, step .5, arp .3, chroma .4, bebop .4, motif .4, blues .2, pent .2, range .85, swing .3, legato .5, stacc .3, dyn .85, vel 100, grace .3, fall .1, doit .1, growl .1"));
    v.push_back (mkS ("Paulo Moura (Alto)", I_Sax, 1, BR, "Samba / Brazilian, Bossa Nova", "Elegant Brazilian choro / samba alto: playful, sweet runs",
      "density .75, rest .15, sync .75, leap .3, step .65, arp .3, chroma .3, motif .5, range .7, swing 0, legato .55, stacc .3, vib .15, vel 90, grace .3"));

    //=============================== TRUMPET
    v.push_back (mkS ("Hugh Masekela", I_Trumpet, 0, AF, "Township Jive, Afro-Jazz, Afrobeat", "South African jazz and township jive: joyous, singing flugelhorn-like phrases",
      "density .55, rest .3, sync .6, leap .3, step .75, arp .2, motif .8, blues .4, pent .8, range .6, regBias .55, swing .15, legato .55, stacc .3, vib .25, dyn .7, laid 2, vel 92, fall .15, scoop .15, shake .1, doit .05"));
    v.push_back (mkS ("Fela Kuti (Trumpet)", I_Trumpet, 0, AF, "Afrobeat, Afro-Jazz", "Afrobeat trumpet: repeating pentatonic hooks and driving horn-section riffs",
      "density .6, rest .25, sync .65, leap .25, step .75, motif .9, blues .5, pent .85, range .55, swing 0, stacc .3, vel 96, growl .15, shake .1, fall .15, muted .1"));
    v.push_back (mkS ("Lee Morgan", I_Trumpet, 0, JZ, "Swing Jazz, Gospel / Soul", "Hard-bop swagger: bluesy, brash and rhythmically sharp",
      "density .8, rest .15, sync .6, leap .4, step .5, arp .3, chroma .4, bebop .4, blues .6, pent .3, range .85, regBias .65, swing .75, legato .5, stacc .35, vib .15, dyn .85, vel 104, fall .2, doit .15, shake .1, growl .1, scoop .1"));
    v.push_back (mkS ("Freddie Hubbard", I_Trumpet, 0, JZ, "Swing Jazz, Afro-Jazz", "Brilliant hard-bop/fusion fire: huge range and bold leaps",
      "density .9, rest .1, leap .5, chroma .35, bebop .35, blues .45, pent .35, fourths .2, range 1, regBias .7, swing .6, dyn .9, vel 106, fall .15, doit .2, growl .1, flutter .1, shake .1"));
    v.push_back (mkS ("Roy Hargrove", I_Trumpet, 0, PS, "Gospel / Soul, Swing Jazz, Afro-Fusion", "Soulful modern jazz trumpet: warm, bluesy and groove-minded",
      "density .65, rest .25, sync .6, step .6, chroma .3, bebop .3, blues .6, pent .4, range .75, swing .55, legato .55, dyn .7, vel 94, fall .15, scoop .15, laid 3, ghost .1"));
    v.push_back (mkS ("Chris Botti", I_Trumpet, 0, PS, "Smooth / Ballad, Pop", "Intimate, sweet and cinematic: long lyrical tones",
      "density .3, rest .45, step .85, motif .5, blues .15, range .55, legato .85, vib .25, dyn .4, vel 70, swing 0, laid 6, scoop .1, fall .1, muted .2"));
    v.push_back (mkS ("Herb Alpert", I_Trumpet, 0, PS, "Pop, Samba / Brazilian", "Sunny Tijuana-brass pop trumpet: bright, catchy hooks",
      "density .5, rest .3, sync .6, step .7, motif .8, pent .5, blues .15, swing 0, legato .4, stacc .4, vel 92, vib .15, range .5, regBias .6, doit .1, fall .1, scoop .1"));
    v.push_back (mkS ("Arturo Sandoval", I_Trumpet, 0, SP, "Samba / Brazilian, Afro-Jazz", "Fiery Afro-Cuban high-register fireworks",
      "density .95, rest .1, sync .7, leap .55, step .45, arp .3, chroma .45, bebop .5, blues .2, range 1, regBias .85, swing .3, stacc .35, dyn .95, vel 108, fall .1, doit .25, shake .15, flutter .15, growl .15"));
    v.push_back (mkS ("Maynard Ferguson", I_Trumpet, 0, JZ, "Rock, Swing Jazz", "Stratospheric screaming high notes and big-band power",
      "density .75, rest .15, leap .5, range 1, regBias .95, dyn .95, vel 110, doit .3, shake .2, flutter .2, growl .2, fall .15, swing .5, blues .4, pent .3, bebop .25, motif .4"));
    v.push_back (mkS ("Chuck Mangione", I_Trumpet, 0, PS, "Pop, Smooth / Ballad", "Warm flugelhorn pop-jazz: singable, sunny and gentle",
      "density .4, rest .35, step .8, motif .65, pent .5, blues .2, legato .75, vib .25, dyn .5, vel 78, swing 0, range .55, regBias .5, laid 3, fall .1, scoop .1"));
    v.push_back (mkS ("Tom Harrell", I_Trumpet, 0, JZ, "Swing Jazz, Bossa Nova", "Lyrical modern bebop: flowing, thoughtful, perfectly shaped lines",
      "density .65, rest .25, step .6, leap .45, chroma .25, bebop .25, motif .4, range .7, legato .65, dyn .5, vel 80, swing .5"));
    v.push_back (mkS ("Fats Navarro", I_Trumpet, 0, JZ, "Swing Jazz", "Sweet-toned bebop: clean, arpeggio-rich, fast and fluent",
      "density .85, rest .12, leap .4, step .5, arp .4, chroma .5, bebop .6, blues .3, range .85, swing .7, legato .6, dyn .7, vel 98, fall .1, doit .1"));
    v.push_back (mkS ("Claudio Roditi", I_Trumpet, 0, BR, "Bossa Nova, Samba / Brazilian", "Brazilian jazz trumpet / flugel: sweet, bossa-tinted bebop",
      "density .6, rest .25, sync .65, step .6, arp .3, chroma .3, bebop .3, motif .4, range .65, swing .15, legato .6, vel 84, laid 2"));
    v.push_back (mkS ("Rafael Mendez", I_Trumpet, 0, SP, "Flamenco / Spanish, Samba / Brazilian", "Spanish / Mexican bravura: brilliant, ornamented, triple-tongued virtuosity",
      "density .85, rest .1, sync .35, leap .45, step .55, arp .5, chroma .15, motif .3, blues 0, pent .1, range .85, swing 0, stacc .5, vib .3, dyn .8, vel 100, trill .15, grace .25"));

    //=============================== VIOLIN
    v.push_back (mkS ("Lindsey Stirling", I_Violin, 0, PS, "Pop, Amapiano", "Energetic pop-dance violin: hooky, rhythmic and bouncy",
      "density .65, rest .25, sync .55, leap .35, step .65, arp .3, motif .75, blues .1, pent .4, range .8, regBias .8, swing 0, legato .5, stacc .4, vib .3, dyn .8, vel 96, pizz .15, gliss .1, trem .05, grace .15, harm .05"));
    v.push_back (mkS ("Vanessa-Mae", I_Violin, 0, PS, "Pop, Flamenco / Spanish", "Pop-classical fusion: fast, brilliant and dramatic",
      "density .85, rest .12, sync .45, leap .4, step .55, arp .4, motif .5, pent .3, range .85, regBias .75, swing 0, legato .6, stacc .3, vib .3, vel 98, trem .1, gliss .15, trill .1"));
    v.push_back (mkS ("Nigel Kennedy", I_Violin, 0, RB, "Rock, Swing Jazz", "Emotive classical-jazz-rock hybrid: raw, rhapsodic, wild",
      "density .75, rest .2, sync .5, leap .45, step .5, arp .3, chroma .3, motif .4, blues .5, pent .5, range .85, swing .3, legato .55, stacc .3, vib .4, dyn .85, vel 98, gliss .2, bend .15, trem .1, grace .2"));
    v.push_back (mkS ("Itzhak Perlman", I_Violin, 0, CL, "Smooth / Ballad, Pop", "Sweet, singing classical line: warm vibrato and long legato phrases",
      "density .4, rest .3, sync .15, leap .35, step .8, arp .3, motif .4, range .7, regBias .65, swing 0, legato .85, vib .55, dyn .65, vel 82, gliss .15, scoop .1"));
    v.push_back (mkS ("Pablo de Sarasate", I_Violin, 0, SP, "Flamenco / Spanish", "Spanish virtuoso: dazzling flourishes, habanera colour and harmonics",
      "density .85, rest .12, sync .35, leap .5, step .5, arp .5, chroma .3, motif .4, blues 0, pent .05, range .95, regBias .75, legato .6, stacc .35, vib .3, dyn .8, vel 98, trill .15, harm .15, gliss .2, trem .1, pizz .08, grace .3"));
    v.push_back (mkS ("Niccolo Paganini", I_Violin, 0, CL, "Flamenco / Spanish, Rock", "Devilish virtuosity: lightning arpeggios, harmonics and wide leaps",
      "density 1, rest .06, sync .25, leap .55, step .45, arp .6, chroma .35, range 1, regBias .8, legato .5, stacc .4, dyn .9, vel 100, harm .2, pizz .12, trill .2, trem .1, dbl .3, gliss .2"));
    v.push_back (mkS ("Mark O'Connor", I_Violin, 0, RB, "Rock, Pop, Swing Jazz", "Fiddle-jazz-country blend: lively, bluesy, double-stops",
      "density .8, rest .15, sync .5, step .6, arp .3, blues .5, pent .6, motif .55, swing .4, stacc .3, vib .15, vel 94, dbl .35, gliss .2, bend .15, dyn .7"));
    v.push_back (mkS ("Roby Lakatos", I_Violin, 0, SP, "Flamenco / Spanish, Samba / Brazilian", "Gypsy virtuoso: fiery, ornamented, rubato-drenched czardas flair",
      "density .9, rest .12, sync .4, leap .45, step .5, arp .45, chroma .5, motif .35, blues .1, range .9, legato .55, stacc .3, vib .45, dyn .85, vel 98, gliss .3, trill .2, trem .15, grace .35, laid -2"));
    v.push_back (mkS ("Nicolas Krassik", I_Violin, 0, BR, "Samba / Brazilian, Bossa Nova", "Brazilian choro / jazz violin: nimble, playful and dancing",
      "density .75, rest .18, sync .65, step .65, arp .35, chroma .25, motif .55, swing 0, legato .5, stacc .3, vel 88, grace .3, range .75, gliss .1, pizz .1"));
    v.push_back (mkS ("Sahel Fiddle (traditional)", I_Violin, 0, AF, "Ethio-Jazz, Afro-Fusion, Township Jive", "Traditional West African / Sahel fiddle: pentatonic, repeating, ornamented, with slides",
      "density .6, rest .25, sync .5, leap .2, step .8, motif .9, blues .3, pent .9, range .5, swing .1, legato .5, vib .15, dyn .6, vel 86, gliss .3, bend .3, scoop .2, grace .25"));

    //=============================== PIANO
    v.push_back (mkS ("Kelvin Momo", I_Piano, 0, AF, "Amapiano", "Private-school amapiano: warm jazzy chords, soulful sparse piano riffs, laid-back groove",
      "density .45, rest .4, sync .65, leap .3, step .6, arp .2, chroma .1, bebop .05, motif .85, blues .3, pent .55, range .6, regBias .5, swing .18, legato .45, stacc .35, dyn .45, laid 5, vel 76, chordal .6, pedal .45, grace .1, ghost .1"));
    v.push_back (mkS ("Kabza De Small", I_Piano, 0, AF, "Amapiano", "King of Amapiano: melodic bubbling piano riffs, repeated hooks, rolling chord stabs",
      "density .6, rest .3, sync .75, leap .3, step .65, motif .9, pent .6, blues .25, range .6, swing .2, stacc .4, dyn .55, laid 3, vel 82, chordal .5, pedal .3, grace .1"));
    v.push_back (mkS ("Vigro Deep", I_Piano, 0, AF, "Amapiano", "Deep, soulful amapiano: melodic chord stabs and rolling riffs",
      "density .5, rest .35, sync .7, leap .25, step .7, motif .85, pent .6, blues .3, range .55, swing .2, stacc .35, dyn .5, laid 4, vel 78, chordal .55, pedal .35, ghost .1"));
    v.push_back (mkS ("De Mthuda", I_Piano, 0, AF, "Amapiano, Afro-Fusion", "Smooth soulful amapiano keys with jazzy touches",
      "density .5, rest .35, sync .65, leap .3, step .65, arp .2, motif .75, pent .5, blues .3, range .6, swing .2, legato .5, stacc .3, dyn .5, laid 4, vel 78, chordal .55, pedal .4, grace .1"));
    v.push_back (mkS ("Abdullah Ibrahim", I_Piano, 0, AF, "Township Jive, Ethio-Jazz, Smooth / Ballad", "Hymn-like South African jazz piano: spare modal township folk melodies",
      "density .35, rest .4, sync .4, leap .3, step .7, motif .85, blues .35, pent .7, range .5, swing .3, legato .5, stacc .2, dyn .5, laid 4, vel 72, chordal .55, pedal .5"));
    v.push_back (mkS ("Fela Kuti (Keys)", I_Piano, 0, AF, "Afrobeat, Afro-Jazz", "Afrobeat Farfisa/electric piano: hypnotic repeating figures and jabbing stabs",
      "density .6, rest .25, sync .75, leap .25, step .7, motif .9, pent .8, blues .5, range .5, swing 0, legato .2, stacc .55, dyn .65, vel 92, chordal .4, pedal .05, ghost .2"));
    v.push_back (mkS ("Dele Sosimi (Keys)", I_Piano, 0, AF, "Afrobeat, Afro-Fusion", "Modern Afrobeat keys: groovy riffs and rhythmic comping with Fela's DNA",
      "density .65, rest .25, sync .75, leap .25, step .7, motif .85, pent .75, blues .45, range .55, swing 0, legato .25, stacc .5, dyn .65, vel 90, chordal .45, pedal .05, ghost .2"));
    v.push_back (mkS ("Ray Charles", I_Piano, 0, PS, "Gospel / Soul, Rock", "Gospel-soaked soul piano: blues triplets, call and response, rollicking",
      "density .6, rest .25, sync .6, blues .9, pent .7, range .6, swing .5, legato .4, stacc .3, dyn .8, vel 96, chordal .5, pedal .3, trill .1, gliss .15, grace .3, laid 3"));
    v.push_back (mkS ("Stevie Wonder (Keys)", I_Piano, 0, PS, "Pop, Gospel / Soul", "Funky clavinet / Rhodes grooves and joyful soul-pop melodies",
      "density .7, rest .2, sync .75, leap .4, step .55, motif .65, blues .5, pent .5, range .7, swing .15, legato .35, stacc .45, dyn .75, vel 96, chordal .45, pedal .1, grace .2, ghost .3, laid 0"));
    v.push_back (mkS ("Elton John", I_Piano, 0, PS, "Pop, Rock", "Rollicking piano-pop: bright triadic fills, glissandi and big gospel-tinged hooks",
      "density .6, rest .25, sync .45, leap .4, step .6, arp .35, motif .65, blues .45, pent .5, range .75, swing .1, legato .5, dyn .8, vel 96, chordal .6, pedal .45, gliss .3, trill .1, grace .25"));
    v.push_back (mkS ("Billy Joel", I_Piano, 0, PS, "Pop, Rock", "Driving piano-rock: punchy chords, storytelling hooks and bluesy runs",
      "density .6, rest .25, sync .5, motif .6, blues .45, pent .45, range .7, stacc .3, dyn .8, vel 98, chordal .55, pedal .35, gliss .2, grace .15"));
    v.push_back (mkS ("Alicia Keys", I_Piano, 0, PS, "Pop, Gospel / Soul", "Soulful R&B / gospel-pop piano: emotive, bluesy triplet runs",
      "density .5, rest .3, sync .55, blues .6, pent .65, motif .6, legato .55, dyn .65, vel 84, chordal .6, pedal .55, laid 5, gliss .15, grace .2"));
    v.push_back (mkS ("Cory Henry", I_Piano, 0, PS, "Gospel / Soul, Afro-Fusion", "Gospel-funk fireworks: blues-gospel runs, fat chords, soulful licks",
      "density .85, rest .12, sync .65, leap .4, step .55, arp .3, chroma .2, motif .5, blues .75, pent .6, range .85, swing .25, legato .45, stacc .3, dyn .9, vel 100, chordal .6, pedal .3, gliss .3, trill .2, grace .25"));
    v.push_back (mkS ("Ramsey Lewis", I_Piano, 0, PS, "Gospel / Soul, Swing Jazz", "Soul-jazz groove: funky, bluesy, singable hooks",
      "density .55, rest .3, sync .6, blues .8, pent .7, motif .75, stacc .4, swing .4, vel 92, dyn .65, chordal .5, pedal .15"));
    v.push_back (mkS ("McCoy Tyner", I_Piano, 0, JZ, "Afro-Jazz, Ethio-Jazz", "Thunderous modal jazz: pentatonic fourths and a powerful left hand",
      "density .85, rest .1, sync .4, leap .5, step .45, arp .3, motif .4, blues .35, pent .75, fourths .6, range .9, swing .35, legato .5, dyn .95, vel 104, chordal .5, pedal .4"));
    v.push_back (mkS ("Keith Jarrett", I_Piano, 0, JZ, "Smooth / Ballad, Gospel / Soul", "Lyrical, gospel-tinged improviser: singing lines that develop patiently",
      "density .6, rest .25, sync .4, leap .35, step .65, arp .3, motif .6, blues .35, pent .4, range .8, swing .4, legato .65, dyn .65, vel 86, chordal .3, pedal .5, grace .2"));
    v.push_back (mkS ("Bud Powell", I_Piano, 0, JZ, "Swing Jazz", "Bebop piano pioneer: fast, intense single-note lines",
      "density .9, rest .12, sync .5, leap .4, step .55, arp .4, chroma .5, bebop .6, blues .3, range .85, swing .75, legato .5, dyn .8, vel 98, chordal .2, pedal .1, trill .1"));
    v.push_back (mkS ("Nina Simone", I_Piano, 0, PS, "Gospel / Soul, Smooth / Ballad", "Classical-meets-blues gospel: dramatic, bluesy, rhapsodic",
      "density .45, rest .35, blues .75, pent .5, motif .5, legato .6, dyn .8, vel 88, chordal .5, pedal .5, grace .2, swing .3"));
    v.push_back (mkS ("Ahmad Jamal", I_Piano, 0, JZ, "Swing Jazz, Bossa Nova", "Master of space and drama: sparse hits, surprising rests and vamps",
      "density .45, rest .45, sync .6, leap .4, motif .6, blues .4, swing .65, stacc .4, dyn .8, vel 86, chordal .4, pedal .15, grace .2"));
    v.push_back (mkS ("Antonio Carlos Jobim", I_Piano, 0, BR, "Bossa Nova, Samba / Brazilian", "Bossa nova's architect: sparse, gentle, chromatic harmonic colour",
      "density .3, rest .45, sync .55, leap .35, step .65, arp .25, chroma .15, motif .6, blues 0, pent .1, range .5, regBias .5, swing 0, legato .65, stacc .1, dyn .35, vel 64, chordal .55, pedal .5, laid 3, grace .05"));
    v.push_back (mkS ("Ivan Lins", I_Piano, 0, BR, "Bossa Nova, Samba / Brazilian, Pop", "Lyrical Brazilian MPB piano: warm, soulful and melodic",
      "density .5, rest .3, sync .6, step .65, arp .3, motif .55, pent .3, legato .6, vel 78, chordal .5, pedal .4, range .65"));
    v.push_back (mkS ("Eumir Deodato", I_Piano, 0, BR, "Samba / Brazilian, Gospel / Soul", "Brazilian jazz-funk electric piano grooves",
      "density .65, sync .7, motif .6, pent .5, swing 0, stacc .4, vel 92, chordal .5, pedal .2, fourths .15, range .65"));
    v.push_back (mkS ("Chucho Valdes", I_Piano, 0, SP, "Samba / Brazilian, Afro-Jazz, Afrobeat", "Afro-Cuban jazz powerhouse: montuno patterns, thundering octaves, rhythmic fire",
      "density .9, rest .1, sync .8, leap .4, step .5, arp .3, chroma .2, motif .7, blues .3, pent .5, range .9, swing .15, stacc .4, dyn .9, vel 104, chordal .5, pedal .2, gliss .3, trill .1, grace .15"));
    v.push_back (mkS ("Isaac Albeniz", I_Piano, 0, SP, "Flamenco / Spanish", "Spanish Romantic piano: guitar-like figures and flamenco-inspired colour",
      "density .65, rest .2, sync .45, leap .4, step .55, arp .45, chroma .2, motif .6, blues 0, pent .1, range .85, swing 0, legato .6, stacc .3, dyn .8, vel 88, chordal .45, pedal .55, gliss .15, trill .2, grace .3, trem .2"));
    v.push_back (mkS ("Dorantes", I_Piano, 0, SP, "Flamenco / Spanish", "Flamenco-jazz piano: percussive compas, sweeping runs, Andalusian colour",
      "density .75, rest .15, sync .6, leap .4, step .5, arp .4, chroma .2, motif .55, blues 0, pent .15, range .85, swing 0, stacc .35, legato .5, dyn .85, vel 96, chordal .4, pedal .3, gliss .3, trill .15, grace .3, trem .15"));
    v.push_back (mkS ("Ludovico Einaudi", I_Piano, 0, CL, "Pop, Smooth / Ballad", "Minimalist cinematic piano: rolling figures, patient and emotional",
      "density .5, rest .3, sync .15, leap .3, step .6, arp .8, motif .85, blues 0, pent .3, range .7, swing 0, legato .8, dyn .5, vel 66, chordal .35, pedal .75, laid 0"));
    v.push_back (mkS ("Yiruma", I_Piano, 0, CL, "Pop, Smooth / Ballad", "Gentle lyrical piano-pop: flowing right-hand melodies and soft arpeggios",
      "density .4, rest .35, sync .15, arp .6, motif .7, legato .8, dyn .4, vel 64, pedal .8, chordal .4, step .75, swing 0"));
    return v;
}

inline std::vector<Style> bassStyles();      // BassStyles.h

inline const std::vector<Style>& allStyles()
{
    static const std::vector<Style> all = []
    {
        auto v = baseStyles();
        auto e = extraStyles();
        v.insert (v.end(), e.begin(), e.end());
        auto b = bassStyles();
        v.insert (v.end(), b.begin(), b.end());
        return v;
    }();
    return all;
}

inline int categoryRank (const std::string& c)
{
    static const char* order[] = { "African & Afrocentric", "Pop & Soul", "Rock & Blues", "Jazz", "Brazilian", "Spanish & Latin", "Caribbean & Reggae", "Funk, Hip-Hop & Dance", "Classical & Cinematic" };   // Enoch Bass: African styles first
    for (int i = 0; i < 9; ++i) if (c == order[i]) return i;
    return 9;
}

/** styles belonging to one instrument (indices into allStyles) */
inline const std::vector<std::vector<int>>& styleIndexTable()
{
    static const std::vector<std::vector<int>> cache = []
    {
        std::vector<std::vector<int>> c ((size_t) I_Count);
        const auto& s = allStyles();
        for (size_t i = 0; i < s.size(); ++i) c[(size_t) s[i].inst].push_back ((int) i);
        for (auto& v : c)
            std::stable_sort (v.begin(), v.end(), [&] (int a, int b) { return categoryRank (s[(size_t) a].cat) < categoryRank (s[(size_t) b].cat); });
        return c;
    }();
    return cache;
}

inline const std::vector<int>& stylesForRef (int inst) { return styleIndexTable()[(size_t) std::clamp (inst, 0, (int) I_Count - 1)]; }
inline std::vector<int> stylesFor (int inst) { return stylesForRef (inst); }

inline int maxArtists() { int m = 0; for (int i = 0; i < I_Count; ++i) m = std::max (m, (int) stylesFor (i).size()); return m; }

inline const Style& styleFor (int inst, int artistIndex)
{
    const auto& idx = stylesForRef (inst);
    if (idx.empty()) return allStyles().front();
    return allStyles()[(size_t) idx[(size_t) std::clamp (artistIndex, 0, (int) idx.size() - 1)]];
}

} // namespace enochsolo

#include "BassStyles.h"
