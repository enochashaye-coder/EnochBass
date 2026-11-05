// Enoch v3.11 / v2.11 - the articulation planner (identical in Enoch Solo and Enoch Bass).
//
// How a session player decides, and how this file does it:
//   1. READ THE LINE. Every note of the main line gets its context: the previous and next note (interval, direction, gap, how long
//      each one rings), its place in the bar and in the phrase (start, middle, peak, longest note, ending, section end), whether it is
//      a chord tone of the chord under it, whether the chord changes on it, how fast the passage moves.
//   2. PLAN THE PHRASE. Each 4-bar phrase gets a budget per articulation family (legato, slides, ghost notes, palm mute, vibrato,
//      bends, harmonics, slap, short notes, accents, colour). The budget comes from the instrument, the genre and the player's style,
//      times the ARTICULATION knob and the amount of each family on the ARTICULATION panel. Budgets are ceilings, never quotas: a
//      family is only used where the music asks for it.
//   3. SCORE EVERY CANDIDATE. Each rule says how well an articulation fits a note (0..1) and why. Nothing is chosen by a coin flip:
//      the best-fitting candidates of a phrase win until that family's budget is used up, and only candidates above a quality bar
//      are ever used. "Variation" only nudges close scores.
//   4. CHECK THE HANDS. A hammer-on or slur needs the previous note still ringing (not a ghost, dead note or staccato), a slide never
//      follows a slide, a violin slur is at most one bow long, a horn phrase gets a breath, a bend must land on a chord tone.
//   5. KEEP IT CONSISTENT. Palm mute and slap are passages, not single notes; a repeated riff repeats its articulations.
//   6. EXPLAIN IT. Every note keeps the reason it got its articulation (hover a note in the piano roll).
// How each articulation reaches a particular instrument (its key-switch, or the closest thing it can do) is decided later by the
// instrument's profile, so the plan never asks for a key the library does not have.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace enochsolo
{
namespace artbrain
{
template <class N, class = void> struct HasRole : std::false_type {};
template <class N> struct HasRole<N, std::void_t<decltype (std::declval<N&> ().role)>> : std::true_type {};
template <class N> inline int roleOf (const N& n) { if constexpr (HasRole<N>::value) return n.role; else return 0; }
template <class S, class = void> struct HasSlap : std::false_type {};
template <class S> struct HasSlap<S, std::void_t<decltype (std::declval<S&> ().slap)>> : std::true_type {};
template <class S> inline float slapOf (const S& s) { if constexpr (HasSlap<S>::value) return s.slap; else return 0.f; }
template <class S> inline float palmOf (const S& s) { if constexpr (HasSlap<S>::value) return s.palm; else return 0.f; }

// bass note jobs (BassGen.h): 1 root, 2 fifth, 3 octave, 4 third, 5 approach, 6 other, 7 ghost, 8 harmonic, 9 seventh
#ifdef ENOCH_BASS_ENGINE
constexpr int kBassInst = (int) I_Bass;
constexpr int kSlapArt = (int) A_Slap, kPopArt = (int) A_Pop;
#else
constexpr int kBassInst = -100;            // Enoch Solo has no bass instrument and no slap / pop
constexpr int kSlapArt = -100, kPopArt = -101;
#endif
enum { R_Root = 1, R_Fifth = 2, R_Octave = 3, R_Third = 4, R_Approach = 5, R_Other = 6, R_Ghost = 7, R_Harmonic = 8, R_Seventh = 9 };

//------------------------------------------------------------------------------ articulation families (the ARTICULATION panel's rows)
enum Family { F_Legato = 0, F_Slide, F_Ghost, F_Palm, F_Vibrato, F_Bend, F_Harmonic, F_Slap, F_Short, F_Accent, F_Colour, F_Repeat, F_Count };

inline const char* familyName (int f)
{
    static const char* n[] = { "Legato / hammer / pull", "Slides", "Ghost / dead notes", "Palm mute", "Vibrato", "Bends", "Harmonics",
                               "Slap / pop", "Short (staccato / pizz)", "Accents", "Colour (growl, shake, subtone, tremolo)", "Repetition keys" };
    return n[std::clamp (f, 0, (int) F_Count - 1)];
}
inline const char* familyShort (int f)
{
    static const char* n[] = { "LEGATO", "SLIDE", "GHOST", "PALM", "VIBRATO", "BEND", "HARMONIC", "SLAP", "SHORT", "ACCENT", "COLOUR", "REPEAT" };
    return n[std::clamp (f, 0, (int) F_Count - 1)];
}

/** which family an articulation belongs to (-1 = plain / none) */
inline int familyOf (int a)
{
    switch (a)
    {
        case A_Legato: case A_HammerOn: case A_PullOff: return F_Legato;
        case A_Slide: case A_Gliss: case A_Fall: case A_Doit: case A_Scoop: return F_Slide;
        case A_Ghost: case A_Mute: return F_Ghost;
        case A_PalmMute: return F_Palm;
        case A_Vibrato: return F_Vibrato;
        case A_Bend: return F_Bend;
        case A_Harmonic: return F_Harmonic;
        case A_Staccato: case A_Pizz: return F_Short;
        case A_Marcato: return F_Accent;
        case A_Growl: case A_Shake: case A_Subtone: case A_Tremolo: case A_Trill: case A_Flutter: return F_Colour;
        default: return (a == kSlapArt || a == kPopArt) ? (int) F_Slap : -1;
    }
}

/** which families make sense for an instrument (the panel shows only these) */
inline bool familyFor (int inst, int f)
{
    const bool bass = inst == kBassInst, guitar = inst == I_Guitar, wind = inst == I_Sax || inst == I_Trumpet, violin = inst == I_Violin, piano = inst == I_Piano;
    switch (f)
    {
        case F_Legato: return ! piano;
        case F_Slide: return ! piano;
        case F_Ghost: return bass || guitar;
        case F_Palm: return bass || guitar;
        case F_Vibrato: return guitar || wind || violin || bass;
        case F_Bend: return guitar;
        case F_Harmonic: return bass || guitar || violin;
        case F_Slap: return bass;
        case F_Short: return true;
        case F_Accent: return true;
        case F_Colour: return wind || violin;
        case F_Repeat: return bass || guitar;
        default: return false;
    }
}

//------------------------------------------------------------------------------ why a note got its articulation
enum Why
{
    W_None = 0, W_Generator, W_HammerUp, W_PullDown, W_SlurStep, W_SlurBow, W_SlideTarget, W_SlideShift, W_SlideIntoPhrase, W_SlideOut,
    W_GhostGroove, W_GhostPickup, W_DeadPunch, W_PalmPassage, W_VibLong, W_VibPeak, W_BendPeak, W_HarmEnding, W_SlapLow, W_PopHigh,
    W_ShortOffbeat, W_PizzPassage, W_AccentChange, W_Growl, W_Shake, W_Subtone, W_Tremolo, W_Scoop, W_Fall, W_Doit, W_Riff, W_Plain,
    W_PlainStrong, W_Breath, W_Forced, W_Count
};

inline const char* whyText (int w)
{
    switch (w)
    {
        case W_Generator:       return "Chosen by the style itself (part of the lick)";
        case W_HammerUp:        return "Quick small step up from a ringing note: hammer-on, no new pluck";
        case W_PullDown:        return "Quick small step down from a ringing note: pull-off, no new pluck";
        case W_SlurStep:        return "Stepwise movement close in time: joined smoothly (legato)";
        case W_SlurBow:         return "Slurred: these notes share one bow / one breath";
        case W_SlideTarget:     return "Slides into a chord tone where the chord changes or on the beat";
        case W_SlideShift:      return "Position shift: a jump the hand slides across";
        case W_SlideIntoPhrase: return "First note of a phrase: a short slide into it";
        case W_SlideOut:        return "Exposed phrase / section ending: slides out of the note";
        case W_GhostGroove:     return "Groove ghost note between the main notes (this style uses them)";
        case W_GhostPickup:     return "Weak pickup right before a strong note: ghosted";
        case W_DeadPunch:       return "Short off-beat hit: dead / muted note for punch";
        case W_PalmPassage:     return "Palm-muted passage: driving repeated rhythm in a style that mutes";
        case W_VibLong:         return "Long held note: vibrato";
        case W_VibPeak:         return "Peak / ending of the phrase held long: vibrato";
        case W_BendPeak:        return "Phrase peak reached by a bend that lands on a chord tone";
        case W_HarmEnding:      return "Long chord tone at an ending: harmonic for colour";
        case W_SlapLow:         return "Slap passage: thumb on the low root / fifth";
        case W_PopHigh:         return "Slap passage: pop on the octave / high note";
        case W_ShortOffbeat:    return "Short off-beat note followed by space: staccato";
        case W_PizzPassage:     return "Pizzicato passage (this style plucks)";
        case W_AccentChange:    return "Downbeat where the chord changes: accented";
        case W_Growl:           return "Climax of the phrase: growl";
        case W_Shake:           return "Long peak note: shake";
        case W_Subtone:         return "Soft, low, long note: subtone";
        case W_Tremolo:         return "Long peak note: tremolo";
        case W_Scoop:           return "Phrase entry: scooped into from below";
        case W_Fall:            return "Exposed phrase ending: fall off";
        case W_Doit:            return "Phrase ends on its peak: doit (rise)";
        case W_Riff:            return "The riff repeats: same articulation as the first time";
        case W_Plain:           return "Plain note: nothing in the phrase calls for more";
        case W_PlainStrong:     return "Main note of the groove: played plain and full";
        case W_Breath:          return "Plain note, shortened to leave a breath";
        case W_Forced:          return "Set to 'Always' on the ARTICULATION panel";
        default:                return "";
    }
}

//------------------------------------------------------------------------------ the plan's budgets (fractions of a phrase's notes)
struct Budget { std::array<double, F_Count> frac {}; bool groove = false, sparse = false, rock = false, jazz = false, funk = false; };

inline std::string lower (std::string s) { for (auto& c : s) c = (char) std::tolower ((unsigned char) c); return s; }
inline bool has (const std::string& s, std::initializer_list<const char*> words) { for (auto* w : words) if (s.find (w) != std::string::npos) return true; return false; }

template <class StyleT>
inline Budget budgetFor (int inst, const std::string& genre, const std::string& style, const StyleT& st)
{
    const std::string g = lower (genre + " " + style);
    Budget b; auto& F = b.frac;
    const bool funk = has (g, { "funk", "disco", "neo-soul", "r&b", "slap", "timba", "go-go", "jamerson", "graham", "miller" });
    const bool gospel = has (g, { "gospel", "soul" });
    const bool afro = has (g, { "afrobeat", "afro", "highlife", "juju", "fuji", "makossa", "soukous", "township", "kwaito", "gqom", "bikutsi", "mbalax", "benga", "apala", "kumalo" });
    const bool amap = has (g, { "amapiano", "private-school", "log drum", "log-drum" });
    const bool jazz = has (g, { "jazz", "bebop", "swing", "walking", "bossa", "samba", "mpb", "carter" });
    const bool latin = has (g, { "salsa", "son ", "mambo", "cha-cha", "cumbia", "merengue", "bachata", "tumbao", "rumba", "guaguanco", "forro", "baiao", "maracatu", "afoxe", "axe", "latin", "descarga" });
    const bool reggae = has (g, { "reggae", "dancehall", "dub", "ska", "rocksteady", "riddim" });
    const bool rock = has (g, { "rock", "punk", "metal", "grunge", "palm-muted", "driving eighths", "blues" });
    const bool hiphop = has (g, { "hip-hop", "trap", "808", "boom-bap", "drill" });
    const bool sparse = has (g, { "ballad", "smooth", "bolero", "kizomba", "zouk", "lounge", "cinematic", "dub heavy", "slow" });
    const bool fretless = has (g, { "fretless", "palladino", "bona", "jaco" });
    b.groove = funk || gospel || afro || amap || latin || reggae || hiphop; b.sparse = sparse; b.rock = rock; b.jazz = jazz; b.funk = funk;

    if (inst == kBassInst)
    {
        F[F_Legato] = 0.14; F[F_Slide] = 0.06; F[F_Ghost] = 0.04; F[F_Palm] = 0.0; F[F_Vibrato] = 0.0; F[F_Harmonic] = 0.01;
        F[F_Short] = 0.06; F[F_Accent] = 0.04; F[F_Repeat] = 1.0;
        if (funk)   { F[F_Ghost] = 0.12; F[F_Legato] = 0.20; F[F_Short] = 0.08; F[F_Slide] = 0.05; }
        if (gospel) { F[F_Ghost] = std::max (F[F_Ghost], 0.08); F[F_Legato] = 0.22; F[F_Slide] = 0.08; }
        if (afro)   { F[F_Ghost] = std::max (F[F_Ghost], 0.08); F[F_Slide] = 0.08; F[F_Legato] = std::max (F[F_Legato], 0.16); F[F_Palm] = 0.06; }
        if (amap)   { F[F_Slide] = 0.14; F[F_Ghost] = 0.03; F[F_Legato] = 0.12; F[F_Palm] = 0.0; }
        if (jazz)   { F[F_Ghost] = 0.05; F[F_Legato] = 0.16; F[F_Slide] = 0.05; F[F_Short] = 0.03; }
        if (latin)  { F[F_Ghost] = std::max (F[F_Ghost], 0.06); F[F_Short] = 0.10; }
        if (reggae) { F[F_Palm] = 0.12; F[F_Short] = 0.12; F[F_Ghost] = 0.04; F[F_Slide] = 0.06; }
        if (rock)   { F[F_Palm] = 0.30; F[F_Ghost] = 0.02; F[F_Legato] = 0.08; }
        if (hiphop) { F[F_Slide] = 0.12; F[F_Ghost] = 0.03; }
        if (sparse) { F[F_Ghost] = 0.0; F[F_Palm] = 0.0; F[F_Legato] = 0.20; F[F_Slide] = 0.08; F[F_Harmonic] = 0.025; F[F_Short] = 0.02; }
        if (fretless) { F[F_Slide] = 0.14; F[F_Legato] = 0.20; F[F_Harmonic] = 0.03; F[F_Vibrato] = 0.10; F[F_Palm] = 0.0; }
        if (! sparse) F[F_Palm] = std::min (0.40, std::max (F[F_Palm], (double) palmOf (st) * 0.5 + (double) st.muted * 0.3));
        if (slapOf (st) > 0.05f) F[F_Slap] = std::min (0.6, 0.15 + (double) slapOf (st) * 0.4);
        // the player's own habits
        F[F_Legato] *= 0.8 + (double) (st.hammer + st.pull) * 1.5;
        F[F_Slide] *= 0.8 + (double) st.slide * 1.2;
    }
    else if (inst == I_Guitar)
    {
        F[F_Legato] = 0.22; F[F_Slide] = 0.08; F[F_Ghost] = 0.02; F[F_Vibrato] = 0.10; F[F_Bend] = 0.05; F[F_Harmonic] = 0.01;
        F[F_Short] = 0.08; F[F_Accent] = 0.04; F[F_Repeat] = 1.0;
        if (rock)   { F[F_Bend] = 0.10; F[F_Vibrato] = 0.14; F[F_Palm] = 0.08; }
        if (funk)   { F[F_Ghost] = 0.08; F[F_Short] = 0.15; F[F_Legato] = 0.15; }
        if (jazz)   { F[F_Legato] = 0.25; F[F_Bend] = 0.01; F[F_Slide] = 0.10; }
        if (gospel) { F[F_Bend] = 0.06; F[F_Vibrato] = 0.12; F[F_Slide] = 0.10; }
        if (afro)   { F[F_Legato] = 0.25; F[F_Slide] = 0.06; F[F_Short] = 0.10; F[F_Bend] = 0.02; }
        if (sparse) { F[F_Legato] = 0.25; F[F_Vibrato] = 0.15; F[F_Harmonic] = 0.03; F[F_Short] = 0.03; F[F_Ghost] = 0.0; }
        F[F_Palm] = std::max (F[F_Palm], (double) st.muted * 0.3);
        F[F_Legato] *= 0.8 + (double) (st.hammer + st.pull) * 1.2;
        F[F_Legato] = std::min (F[F_Legato], 0.25);
        F[F_Slide] *= 0.8 + (double) st.slide * 1.2;
        F[F_Bend] *= 0.6 + (double) st.vib * 0.8;
    }
    else if (inst == I_Sax || inst == I_Trumpet)
    {
        F[F_Legato] = 0.35; F[F_Slide] = 0.08; F[F_Vibrato] = 0.12; F[F_Short] = 0.10; F[F_Accent] = 0.05; F[F_Colour] = 0.04;
        if (funk || rock) { F[F_Colour] = 0.06; F[F_Short] = 0.14; }
        if (jazz)   { F[F_Slide] = 0.10; }
        if (gospel) { F[F_Colour] = 0.05; F[F_Vibrato] = 0.15; }
        if (sparse) { F[F_Legato] = 0.45; F[F_Colour] = 0.05; F[F_Short] = 0.04; F[F_Vibrato] = 0.18; }
        F[F_Slide] *= 0.8 + (double) (st.scoop + st.fall) * 1.2;
    }
    else if (inst == I_Violin)
    {
        F[F_Legato] = 0.40; F[F_Slide] = 0.04; F[F_Vibrato] = 0.30; F[F_Short] = 0.10; F[F_Accent] = 0.04; F[F_Colour] = 0.01; F[F_Harmonic] = 0.01;
        if (latin || afro) F[F_Short] = 0.16;
        if (sparse) { F[F_Legato] = 0.5; F[F_Short] = 0.03; F[F_Vibrato] = 0.4; }
        F[F_Short] += (double) st.pizz * 0.3;
    }
    else    // piano / keys: touch only (voicing and pedal are the generator's and the expression engine's job)
    {
        F[F_Short] = 0.12; F[F_Accent] = 0.05;
    }
    for (auto& v : F) v = std::clamp (v, 0.0, 1.0);
    return b;
}

struct Feat
{
    int idx = -1;                  // into p.raw
    double start = 0, len = 0, end = 0, gapPrev = 4, gapNext = 4, restAfter = 4, restBefore = 4;
    int pitch = 60, iv = 0, ivNext = 0, bar = 0, role = 0, phrase = 0;
    double pos = 0, weight = 0;
    bool down = false, onBeat = false, chordChange = false, phraseStart = false, phraseEnd = false, fast = false, peak = false, longest = false, sectionEnd = false;
    bool chordTone = false, rootTone = false, special = false;
};

/** articulations a melody generator chose for musical reasons of its own (part of a lick), which the plan keeps */
inline bool generatorSpecial (int a)
{
    switch (a)
    {
        case A_Bend: case A_Trill: case A_Shake: case A_Fall: case A_Doit: case A_Scoop: case A_Growl: case A_Flutter: case A_Gliss:
        case A_Harmonic: case A_Pizz: case A_ColLegno: case A_Tremolo: case A_Arpeggio: case A_Strum: case A_Ghost: case A_Mute: case A_Subtone:
            return true;
        default: return false;
    }
}

struct Cand { int k = 0; int art = A_Sustain; double score = 0; int why = W_None; };

/** amount: the ARTICULATION knob (0 = plain, 0.6 = natural, 1 = very expressive). The panel's per-family amounts and locks live in
    the settings (famAmt: 0..2, 1 = the style's natural amount; famLock: 0 auto, 1 never, 2 always where it fits). */
template <class PatternT, class SettingsT, class StyleT>
inline void apply (PatternT& p, const SettingsT& s, const StyleT& st, float amount)
{
    amount = std::clamp (amount, 0.f, 1.f);
    if (p.raw.empty()) return;
    const int inst = s.instrument;
#ifdef ENOCH_BASS_ENGINE
    const bool bass = inst == I_Bass;
#else
    const bool bass = false;
#endif
    const bool guitar = inst == I_Guitar, sax = inst == I_Sax, violin = inst == I_Violin, piano = inst == I_Piano;
    const bool wind = sax || inst == I_Trumpet;
    const bool fretted = bass || guitar;
    const Budget B = budgetFor (inst, p.genreName, p.styleName, st);
    const double bb = std::max (0.5, p.barBeats);
    const double secPerBeat = 60.0 / std::max (30.0, s.bpm);
    Rng rng (s.seed * 1103515245ULL + 2654435761ULL);
    const double G = amount <= 0.01 ? 0.0 : std::min (1.67, amount / 0.6);       // the knob: 0 nothing new, 0.6 natural, 1 x1.67
    const double vary = std::clamp ((double) s.artVary, 0.0, 1.0);

    const double thrBase = 0.45;
    std::array<double, F_Count> frac {};
    std::array<int, F_Count> lock {};
    for (int fm = 0; fm < F_Count; ++fm)
    {
        lock[(size_t) fm] = std::clamp (s.famLock[fm], 0, 2);
        frac[(size_t) fm] = B.frac[(size_t) fm] * G * std::clamp ((double) s.famAmt[fm], 0.0, 2.0);
        if (lock[(size_t) fm] == 1 || ! familyFor (inst, fm)) frac[(size_t) fm] = 0.0;
        if (lock[(size_t) fm] == 2 && familyFor (inst, fm)) frac[(size_t) fm] = 1.0;
    }

    // ---------------- 0) the groove's own ghost fills (bass) are settled first: the ones that win stay ghost notes, the others are
    //      taken out before anything else is decided, so no hammer-on, slide or slur is ever planned against a note that then goes
    if (bass)
    {
        std::vector<int> ord;
        for (int i = 0; i < (int) p.raw.size(); ++i) { const auto& n = p.raw[(size_t) i]; if (n.track == 0 && n.voice == 0 && ! n.mute) ord.push_back (i); }
        std::stable_sort (ord.begin(), ord.end(), [&] (int a, int b) { return p.raw[(size_t) a].start < p.raw[(size_t) b].start; });
        std::vector<std::pair<double, int>> cand;
        for (size_t j = 0; j < ord.size(); ++j)
        {
            const auto& n = p.raw[(size_t) ord[j]];
            if (! (roleOf (n) == R_Ghost || n.art == A_Ghost)) continue;
            const double pos = n.start - std::floor (n.start / bb + 1e-6) * bb;
            const bool onBeat = std::abs (pos - std::round (pos)) < 0.02;
            const bool strongNext = j + 1 < ord.size() && p.raw[(size_t) ord[j + 1]].weight >= 0.55 && p.raw[(size_t) ord[j + 1]].start - n.start <= 0.3 + 1e-6;
            const bool prevGhost = j > 0 && (roleOf (p.raw[(size_t) ord[j - 1]]) == R_Ghost || p.raw[(size_t) ord[j - 1]].art == A_Ghost);
            const double sc = 0.5 + (B.groove ? 0.12 : 0.0) + (strongNext ? 0.1 : 0.0) - (onBeat ? 0.25 : 0.0) - (prevGhost ? 0.15 : 0.0) + (rng.f() - 0.5) * 0.25 * vary;
            cand.push_back ({ sc, ord[j] });
        }
        if (! cand.empty())
        {
            std::stable_sort (cand.begin(), cand.end(), [] (auto& a, auto& b) { return a.first > b.first; });
            const int keep = frac[F_Ghost] >= 0.999 ? (int) cand.size() : (int) std::floor (frac[F_Ghost] * (double) ord.size() + rng.f());
            std::vector<int> drop;
            for (size_t j = 0; j < cand.size(); ++j)
            {
                auto& n = p.raw[(size_t) cand[j].second];
                if ((int) j < keep && cand[j].first >= thrBase - (lock[F_Ghost] == 2 ? 0.17 : 0.0)) { n.art = A_Ghost; n.why = lock[F_Ghost] == 2 ? W_Forced : W_GhostGroove; }
                else drop.push_back (cand[j].second);
            }
            std::sort (drop.begin(), drop.end());
            for (auto it = drop.rbegin(); it != drop.rend(); ++it) p.raw.erase (p.raw.begin() + *it);
        }
    }

    // ---------------- 1) read the line, in time order
    std::vector<Feat> f;
    for (int i = 0; i < (int) p.raw.size(); ++i)
    {
        const auto& n = p.raw[(size_t) i];
        if (n.track != 0 || n.voice != 0 || n.mute) continue;
        Feat x; x.idx = i; x.start = n.start; x.len = n.length; x.end = n.start + n.length; x.pitch = n.pitch; x.weight = n.weight; x.role = roleOf (n);
        x.bar = (int) std::floor (n.start / bb + 1e-6); x.pos = n.start - x.bar * bb; x.down = x.pos < 0.02;
        x.onBeat = std::abs (x.pos - std::round (x.pos)) < 0.02;
        x.special = bass ? (x.role == R_Harmonic || n.art == A_Harmonic) : generatorSpecial (n.art);
        if (! p.chords.empty())
        {
            const auto& c = p.chordAt (n.start + 1e-4);
            x.chordTone = (c.toneMask >> (n.pitch % 12)) & 1;
            x.rootTone = (n.pitch % 12) == c.rootPc;
        }
        else x.chordTone = x.rootTone = true;
        f.push_back (x);
    }
    if (f.size() < 2) return;
    std::stable_sort (f.begin(), f.end(), [] (const Feat& a, const Feat& b) { return a.start < b.start; });
    const int N = (int) f.size();
    for (int k = 0; k < N; ++k)
    {
        auto& x = f[(size_t) k];
        if (k > 0) { x.gapPrev = x.start - f[(size_t) k - 1].start; x.iv = x.pitch - f[(size_t) k - 1].pitch; x.restBefore = x.start - f[(size_t) k - 1].end; }
        if (k + 1 < N) { x.gapNext = f[(size_t) k + 1].start - x.start; x.ivNext = f[(size_t) k + 1].pitch - x.pitch; x.restAfter = f[(size_t) k + 1].start - x.end; }
        else x.restAfter = std::max (0.0, p.lengthBeats - x.end) + 2.0;
        x.fast = x.gapPrev <= 0.3 + 1e-6 && x.gapNext <= 0.3 + 1e-6;
        if (k > 0 && ! p.chords.empty())
        {
            const auto& a = p.chordAt (f[(size_t) k - 1].start + 1e-4); const auto& c = p.chordAt (x.start + 1e-4);
            x.chordChange = a.rootPc != c.rootPc || a.q != c.q;
        }
        else x.chordChange = k == 0;
    }
    // phrases: a rest of about a beat (or a new 4-bar section) closes a phrase
    {
        int ph = 0; int phraseStartBar = f[0].bar;
        for (int k = 0; k < N; ++k)
        {
            auto& x = f[(size_t) k];
            const bool brk = k == 0 || x.restBefore >= 0.9 || (x.down && x.bar - phraseStartBar >= 4);
            if (brk) { if (k > 0) { ++ph; f[(size_t) k - 1].phraseEnd = true; } x.phraseStart = true; phraseStartBar = x.bar; }
            x.phrase = ph;
        }
        f.back().phraseEnd = true;
        for (int k = 0; k + 1 < N; ++k) if (f[(size_t) k + 1].bar / 4 != f[(size_t) k].bar / 4) f[(size_t) k].sectionEnd = true;
        f.back().sectionEnd = true;
        std::map<int, int> hi, lg;
        for (int k = 0; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (! hi.count (x.phrase) || x.pitch > f[(size_t) hi[x.phrase]].pitch) hi[x.phrase] = k;
            if (! lg.count (x.phrase) || x.len > f[(size_t) lg[x.phrase]].len) lg[x.phrase] = k;
        }
        for (auto& kv : hi) f[(size_t) kv.second].peak = true;
        for (auto& kv : lg) f[(size_t) kv.second].longest = true;
    }
    const int phrases = f.back().phrase + 1;
    std::vector<std::vector<int>> phNotes ((size_t) phrases);
    for (int k = 0; k < N; ++k) phNotes[(size_t) f[(size_t) k].phrase].push_back (k);

    // ---------------- the plan's state
    std::vector<int> art ((size_t) N, -1), why ((size_t) N, W_None);
    std::vector<char> removed ((size_t) N, 0), vib ((size_t) N, 0);
    std::vector<int> bendBy ((size_t) N, 0);
    auto thr = [&] (int fm) { return lock[(size_t) fm] == 2 ? 0.28 : thrBase; };
    auto jitter = [&] { return (rng.f() - 0.5) * 0.25 * vary; };
    // budgets: a ceiling per phrase; the fractional part carries over so short phrases still get their share over a whole take
    std::array<double, F_Count> carry {};
    for (auto& c : carry) c = rng.f();             // dithered start: a rare colour (harmonics) still turns up at its rate over many takes
    auto budgetOf = [&] (int fm, int phrase) -> int
    {
        if (frac[(size_t) fm] >= 0.999) return 1 << 20;
        const double want = frac[(size_t) fm] * (double) phNotes[(size_t) phrase].size() + carry[(size_t) fm];
        const int n = (int) std::floor (want);
        carry[(size_t) fm] = want - n;
        return n;
    };
    auto isRinging = [&] (int k) -> bool         // can a hammer-on / slur / slide leave note k?
    {
        if (k < 0 || removed[(size_t) k]) return false;
        const int a = art[(size_t) k];
        return ! (a == A_Ghost || a == A_Mute || a == A_Staccato || a == A_Pizz || a == A_Harmonic);
    };
    auto prevAlive = [&] (int k) { int j = k - 1; while (j >= 0 && removed[(size_t) j]) --j; return j; };

    // notes the melody generator chose on purpose stay as they are
    for (int k = 0; k < N; ++k) if (f[(size_t) k].special) { art[(size_t) k] = p.raw[(size_t) f[(size_t) k].idx].art; why[(size_t) k] = W_Generator; }
    for (int k = 0; k < N; ++k) if (bass && p.raw[(size_t) f[(size_t) k].idx].art == A_Ghost && (p.raw[(size_t) f[(size_t) k].idx].why == W_GhostGroove || p.raw[(size_t) f[(size_t) k].idx].why == W_Forced))
                                    { art[(size_t) k] = A_Ghost; why[(size_t) k] = p.raw[(size_t) f[(size_t) k].idx].why; }
    // ... but they obey the plan too: a family set to Never loses them, and no family floods a phrase through the style's own licks
    for (int ph = 0; ph < phrases; ++ph)
    {
        std::map<int, std::vector<int>> byFam;
        for (int k : phNotes[(size_t) ph]) if (f[(size_t) k].special) { const int fm = familyOf (art[(size_t) k]); if (fm >= 0) byFam[fm].push_back (k); }
        for (auto& kv : byFam)
        {
            const double fr = frac[(size_t) kv.first];
            if (fr >= 0.999) continue;
            const int allowed = fr <= 0.0 ? 0 : (int) std::ceil (fr * 1.5 * (double) phNotes[(size_t) ph].size());
            auto v = kv.second;
            if ((int) v.size() <= allowed) continue;
            // keep the most exposed ones (longest, then strongest)
            std::stable_sort (v.begin(), v.end(), [&] (int x, int y) { return f[(size_t) x].len * 2 + f[(size_t) x].weight > f[(size_t) y].len * 2 + f[(size_t) y].weight; });
            for (size_t j = (size_t) std::max (0, allowed); j < v.size(); ++j) { f[(size_t) v[j]].special = false; art[(size_t) v[j]] = -1; why[(size_t) v[j]] = W_None; }
        }
    }

    // the per-family passes, phrase by phrase, best candidates first
    // elastic: a phrase that is mostly made for this family (a stepwise run for legato) may use it on most of those notes
    auto runPass = [&] (int fm, const std::vector<Cand>& all, const std::function<bool (const Cand&)>& ok, bool elastic = false)
    {
        if (frac[(size_t) fm] <= 0.0) return;
        std::vector<std::vector<Cand>> byPh ((size_t) phrases);
        for (auto c : all) { c.score += jitter(); if (c.score >= thr (fm)) byPh[(size_t) f[(size_t) c.k].phrase].push_back (c); }
        for (int ph = 0; ph < phrases; ++ph)
        {
            auto& v = byPh[(size_t) ph];
            int left = budgetOf (fm, ph);
            for (int k : phNotes[(size_t) ph]) if (familyOf (art[(size_t) k]) == fm) --left;
            if (elastic && frac[(size_t) fm] > 0.0)
            {
                int strong = 0; for (auto& c : v) if (c.score >= 0.72) ++strong;
                const double scale = std::min (1.0, G * std::clamp ((double) s.famAmt[fm], 0.0, 2.0));
                left = std::max (left, (int) std::floor (strong * 0.6 * scale));
            }
            std::stable_sort (v.begin(), v.end(), [] (const Cand& a, const Cand& b) { return a.score > b.score; });
            for (auto& c : v)
            {
                if (left <= 0) break;
                if (art[(size_t) c.k] >= 0 || removed[(size_t) c.k]) continue;
                if (! ok (c)) continue;
                art[(size_t) c.k] = c.art; why[(size_t) c.k] = lock[(size_t) fm] == 2 ? W_Forced : c.why; --left;
            }
        }
    };

    // ---------------- 2) passages first: slap (bass) and palm mute are kept for a whole phrase, like a player commits to a sound
    auto pickPassages = [&] (int fm, double perPhraseShare, const std::function<double (int)>& drive) -> std::vector<char>
    {
        std::vector<char> on ((size_t) phrases, 0);
        if (frac[(size_t) fm] <= 0.0) return on;
        std::vector<std::pair<double, int>> order;
        for (int ph = 0; ph < phrases; ++ph) order.push_back ({ drive (ph) + jitter(), ph });
        std::stable_sort (order.begin(), order.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        const int count = frac[(size_t) fm] >= 0.999 ? phrases : (int) std::lround (frac[(size_t) fm] / perPhraseShare * phrases);
        for (int i = 0; i < std::min (count, phrases); ++i) if (order[(size_t) i].first > 0.15 || frac[(size_t) fm] >= 0.999) on[(size_t) order[(size_t) i].second] = 1;
        return on;
    };
    // how much a phrase drives: short, even, repeated notes
    auto driveOf = [&] (int ph) -> double
    {
        const auto& v = phNotes[(size_t) ph];
        if (v.size() < 4) return 0.0;
        int shortN = 0, rep = 0;
        for (size_t j = 0; j < v.size(); ++j) { const auto& x = f[(size_t) v[j]]; if (x.len <= 0.6) ++shortN; if (j > 0 && x.iv == 0) ++rep; }
        return (shortN + rep * 0.7) / (double) v.size();
    };
    std::vector<char> slapPh ((size_t) phrases, 0), palmPh ((size_t) phrases, 0), pizzPh ((size_t) phrases, 0);
#ifdef ENOCH_BASS_ENGINE
    if (bass)
    {
        slapPh = pickPassages (F_Slap, 0.8, [&] (int ph) { return driveOf (ph) + 0.2; });
        for (int ph = 0; ph < phrases; ++ph)
            if (slapPh[(size_t) ph])
                for (int k : phNotes[(size_t) ph])
                {
                    const auto& x = f[(size_t) k];
                    if (art[(size_t) k] >= 0 || x.role == R_Ghost) continue;
                    if (k > 0 && x.iv != 0 && std::abs (x.iv) <= 4 && x.gapPrev <= 0.3 + 1e-6) continue;   // a quick small step: left for a hammer-on / pull-off
                    if (x.role == R_Octave || x.iv >= 7 || (x.role != R_Root && x.pitch >= 48 && ! x.onBeat)) { art[(size_t) k] = A_Pop; why[(size_t) k] = W_PopHigh; }
                    else if (x.role == R_Root || x.role == R_Fifth || x.weight >= 0.55 || x.onBeat) { art[(size_t) k] = A_Slap; why[(size_t) k] = W_SlapLow; }
                }
    }
#endif
    if (fretted)
    {
        palmPh = pickPassages (F_Palm, 0.7, driveOf);
        for (int ph = 0; ph < phrases; ++ph)
            if (palmPh[(size_t) ph] && ! slapPh[(size_t) ph])
                for (int k : phNotes[(size_t) ph])
                {
                    const auto& x = f[(size_t) k];
                    const bool exposedEnd = x.phraseEnd && x.len >= 0.45 && x.restAfter >= 0.75;
                    if (art[(size_t) k] >= 0 || x.role == R_Ghost || exposedEnd || x.len > 0.65 || (guitar && x.pitch >= 64)) continue;
                    art[(size_t) k] = A_PalmMute; why[(size_t) k] = W_PalmPassage;
                }
    }
    if (violin && frac[F_Short] > 0.0 && B.frac[F_Short] >= 0.14)
    {
        pizzPh = pickPassages (F_Short, 2.0, driveOf);
        for (int ph = 0; ph < phrases; ++ph) if (pizzPh[(size_t) ph]) for (int k : phNotes[(size_t) ph]) if (art[(size_t) k] < 0) { art[(size_t) k] = A_Pizz; why[(size_t) k] = W_PizzPassage; }
    }

    // ---------------- 3) legato: hammer-ons, pull-offs, slurs
    {
        std::vector<Cand> c;
        const int maxIv = fretted ? 4 : (wind || violin ? 7 : 0);
        for (int k = 1; k < N && maxIv > 0; ++k)
        {
            const auto& x = f[(size_t) k];
            const int a = std::abs (x.iv);
            if (a == 0 || a > maxIv || x.gapPrev > (fretted ? (bass && a <= 2 ? 1.05 : 0.55) : 1.05) + 1e-6 || x.restBefore > 0.26 || x.role == R_Ghost) continue;
            double sc = 0.5;
            if (a <= 2) sc += 0.2;
            if (x.gapPrev <= 0.3 + 1e-6) sc += 0.15;
            if (k >= 2 && f[(size_t) k - 1].iv != 0 && (f[(size_t) k - 1].iv > 0) == (x.iv > 0) && std::abs (f[(size_t) k - 1].iv) <= maxIv) sc += 0.1;   // inside a run
            if (! f[(size_t) k - 1].chordTone && x.chordTone) sc += 0.08;                                  // a passing / approach note resolving
            if (x.chordChange && x.down && x.weight >= 0.8) sc -= 0.3;                                      // a new chord wants its own attack
            if (fretted && x.len > 1.5) sc -= 0.1;
            int artK = A_Legato, w = W_SlurStep;
            if (fretted && x.gapPrev <= 0.55 + 1e-6) { artK = x.iv > 0 ? A_HammerOn : A_PullOff; w = x.iv > 0 ? W_HammerUp : W_PullDown; }
            if (violin || wind) w = W_SlurBow;
            c.push_back ({ k, artK, sc, w });
        }
        // a slur is at most one bow (violin, 4 notes / 2 bars) or one breath (horns, ~5 s)
        auto chainOk = [&] (int k) -> bool
        {
            if (! (violin || wind)) return true;
            int len = 1; double startT = f[(size_t) k].start;
            for (int j = k - 1; j >= 0 && familyOf (art[(size_t) j]) == F_Legato; --j) { ++len; startT = f[(size_t) j].start; }
            if (violin && len > 4) return false;
            if (wind && (f[(size_t) k].end - startT) * secPerBeat > 5.0) return false;
            return true;
        };
        runPass (F_Legato, c, [&] (const Cand& cd) { const int j = prevAlive (cd.k); return j >= 0 && isRinging (j) && chainOk (cd.k); }, true);
    }

    // ---------------- 4) slides (into a target, across a shift, into a phrase, out of an ending), scoops and falls
    {
        std::vector<Cand> c;
        for (int k = 0; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (x.role == R_Ghost || x.special) continue;
            const int a = std::abs (x.iv);
            const bool exposedEnd = x.phraseEnd && x.len >= 0.45 && x.restAfter >= 0.75;
            if (fretted)
            {
                if (k > 0 && x.gapPrev <= 1.01 && a >= 2 && a <= 7 && x.len >= 0.25 && x.chordTone && (x.chordChange || x.down))
                {
                    double sc = 0.55 + (x.chordChange ? 0.15 : 0.0) + (x.down ? 0.08 : 0.0) + (f[(size_t) k - 1].role == R_Approach ? 0.1 : 0.0);
                    c.push_back ({ k, A_Slide, sc, a >= 5 ? W_SlideShift : W_SlideTarget });
                }
                else if (k > 0 && x.gapPrev <= 1.01 && a >= 5 && a <= 9 && x.len >= 0.3)
                    c.push_back ({ k, A_Slide, 0.5 + (x.chordTone ? 0.1 : 0.0), W_SlideShift });
                else if (x.phraseStart && k > 0 && x.len >= 0.35 && x.chordTone)
                    c.push_back ({ k, A_Slide, 0.47, W_SlideIntoPhrase });
                if (exposedEnd || (x.sectionEnd && x.len >= 0.3 && ! x.down))
                    c.push_back ({ k, bass ? A_Fall : A_Gliss, 0.5 + (x.sectionEnd ? 0.2 : 0.0) + (x.longest ? 0.08 : 0.0), W_SlideOut });
            }
            else if (wind)
            {
                if (x.phraseStart && x.len >= 0.35) c.push_back ({ k, A_Scoop, 0.5 + (x.weight >= 0.55 ? 0.1 : 0.0), W_Scoop });
                if (exposedEnd) c.push_back ({ k, x.peak ? A_Doit : A_Fall, 0.52 + (x.sectionEnd ? 0.15 : 0.0) + (x.peak ? 0.05 : 0.0) - (x.peak ? 0.25 * (1.0 - (double) st.doit * 3.0) : 0.0), x.peak ? W_Doit : W_Fall });
            }
            else if (violin)
            {
                if (k > 0 && x.gapPrev <= 1.01 && a >= 3 && a <= 8 && (x.phraseStart || x.peak) && x.len >= 0.4) c.push_back ({ k, A_Gliss, 0.5 + (x.peak ? 0.1 : 0.0), W_SlideShift });
            }
        }
        runPass (F_Slide, c, [&] (const Cand& cd)
        {
            const int j = prevAlive (cd.k);
            if (cd.art == A_Slide || cd.art == A_Gliss) { if (j >= 0 && familyOf (art[(size_t) j]) == F_Slide && ! B.groove) return false; }
            if ((cd.art == A_Slide || (violin && cd.art == A_Gliss)) && cd.why != W_SlideIntoPhrase && ! isRinging (j)) return false;
            return true;
        });
    }

    // ---------------- 5) bends (guitar): a peak / long note reached by bending up from a scale step below, landing on a chord tone
    if (guitar)
    {
        std::vector<Cand> c;
        for (int k = 1; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (x.special || x.len < 0.5 || ! x.chordTone) continue;
            if (! (x.peak || x.longest || x.phraseEnd)) continue;
            c.push_back ({ k, A_Bend, 0.5 + (x.peak ? 0.15 : 0.0) + (x.len >= 1.0 ? 0.08 : 0.0) + (x.iv > 0 ? 0.05 : 0.0), W_BendPeak });
        }
        // a bend is written as the lower note bent up: never right before or after a hammer-on / pull-off (the written step would be wrong)
        runPass (F_Bend, c, [&] (const Cand& cd) { return (cd.k + 1 >= N || familyOf (art[(size_t) cd.k + 1]) != F_Legato); });
        for (int k = 0; k < N; ++k)
            if (art[(size_t) k] == A_Bend && why[(size_t) k] != W_Generator)
            {
                // start a whole step below (a half step when the whole step leaves the scale), bend up to the written note
                const auto& x = f[(size_t) k];
                int semis = 2;
                if (! p.chords.empty()) { const auto& ch = p.chordAt (x.start + 1e-4); if (! ((ch.scaleMask >> ((x.pitch + 10) % 12)) & 1)) semis = 1; }
                bendBy[(size_t) k] = semis;
            }
    }

    // ---------------- 6) ghost / dead notes: pickups and dead notes only where the groove needs them, never on the main beats
    if (fretted)
    {
        std::vector<Cand> c;
        for (int k = 0; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (art[(size_t) k] >= 0 || x.role == R_Ghost) continue;
            const bool strongNext = k + 1 < N && f[(size_t) k + 1].weight >= 0.55;
            const bool inRun = x.fast && std::abs (x.iv) <= 2 && std::abs (x.ivNext) <= 2 && x.iv != 0;      // part of a melodic run: never ghosted
            if (x.len <= 0.26 && x.weight < 0.35 && ! x.onBeat && strongNext && x.gapNext <= 0.3 && ! inRun)
                c.push_back ({ k, A_Ghost, 0.47 + (B.groove ? 0.08 : 0.0), W_GhostPickup });
            else if (bass && x.len <= 0.3 && ! x.onBeat && x.restAfter >= 0.2 && (B.funk || has (lower (p.genreName + p.styleName), { "reggae", "dancehall", "ska" })))
                c.push_back ({ k, A_Mute, 0.46, W_DeadPunch });
        }
        runPass (F_Ghost, c, [&] (const Cand& cd)
        {
            if (cd.k + 1 < N && familyOf (art[(size_t) cd.k + 1]) == F_Legato) return false;   // a hammer-on needs a ringing note before it
            return true;
        });
    }

    // ---------------- 7) harmonics: a long chord tone at an ending, for colour
    if (fretted || violin)
    {
        std::vector<Cand> c;
        for (int k = 0; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (x.special || x.len < (violin ? 1.5 : (bass ? 0.75 : 1.0)) || ! x.chordTone || ! (x.phraseEnd || x.sectionEnd)) continue;
            const bool high = bass ? x.pitch >= 40 : x.pitch >= 64;
            c.push_back ({ k, A_Harmonic, 0.5 + (x.sectionEnd ? 0.15 : 0.0) + (B.sparse ? 0.1 : 0.0) + (high ? 0.05 : -0.1) + (x.rootTone ? 0.05 : 0.0), W_HarmEnding });
        }
        runPass (F_Harmonic, c, [&] (const Cand& cd) { return cd.k + 1 >= N || familyOf (art[(size_t) cd.k + 1]) != F_Legato; });
    }

    // ---------------- 8) vibrato: long notes, more at the peak and the ending (a flag on the note, so a legato note can still sing)
    if (frac[F_Vibrato] > 0.0)
    {
        std::vector<Cand> c;
        const double minLenSec = 0.35;
        for (int k = 0; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (removed[(size_t) k] || x.len * secPerBeat < minLenSec || x.len < (violin ? 0.6 : 0.75)) continue;
            const int a = art[(size_t) k];
            if (a == A_Ghost || a == A_Mute || a == A_PalmMute || a == A_Staccato || a == A_Pizz || a == A_Harmonic || a == kSlapArt) continue;
            c.push_back ({ k, A_Vibrato, 0.5 + (x.longest ? 0.15 : 0.0) + (x.peak ? 0.12 : 0.0) + (x.phraseEnd ? 0.08 : 0.0) + std::min (0.25, (x.len - 0.75) * 0.25),
                           (x.peak || x.phraseEnd) ? W_VibPeak : W_VibLong });
        }
        std::vector<std::vector<Cand>> byPh ((size_t) phrases);
        for (auto cd : c) { cd.score += jitter(); if (cd.score >= thr (F_Vibrato)) byPh[(size_t) f[(size_t) cd.k].phrase].push_back (cd); }
        for (int ph = 0; ph < phrases; ++ph)
        {
            auto& v = byPh[(size_t) ph]; int left = budgetOf (F_Vibrato, ph);
            { int strong = 0; for (auto& cd : v) if (cd.score >= 0.7) ++strong; left = std::max (left, (int) std::floor (strong * 0.5 * std::min (1.0, G * std::clamp ((double) s.famAmt[F_Vibrato], 0.0, 2.0)))); }
            std::stable_sort (v.begin(), v.end(), [] (const Cand& a, const Cand& b) { return a.score > b.score; });
            for (auto& cd : v)
            {
                if (left-- <= 0) break;
                vib[(size_t) cd.k] = 1;
                if (guitar && art[(size_t) cd.k] < 0) { art[(size_t) cd.k] = A_Vibrato; why[(size_t) cd.k] = cd.why; }
                else if (art[(size_t) cd.k] < 0) why[(size_t) cd.k] = cd.why;
            }
        }
    }

    // ---------------- 9) colour: growl, shake, subtone (horns), tremolo (violin)
    if (wind || violin)
    {
        std::vector<Cand> c;
        for (int k = 0; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (x.special) continue;
            if (wind && (x.peak || x.longest) && x.len >= 0.75 && (B.funk || B.rock || has (lower (p.genreName), { "gospel", "soul", "afro" })))
                c.push_back ({ k, A_Growl, 0.48 + (double) st.growl * 1.5 + (x.peak ? 0.1 : 0.0), W_Growl });
            if (wind && x.peak && x.len >= 1.0 && (B.jazz || has (lower (p.genreName), { "gospel", "soul" })))
                c.push_back ({ k, A_Shake, 0.46 + (double) st.shake * 1.5, W_Shake });
            if (sax && x.len >= 0.75 && x.pitch < 62 && x.weight < 0.8 && (B.sparse || B.jazz))
                c.push_back ({ k, A_Subtone, 0.5 + (double) st.subtone * 1.5, W_Subtone });
            if (violin && x.peak && x.len >= 1.5) c.push_back ({ k, A_Tremolo, 0.46 + (double) st.trem * 1.5, W_Tremolo });
        }
        runPass (F_Colour, c, [&] (const Cand&) { return true; });
    }

    // ---------------- 10) short notes: staccato on off-beat notes followed by space (never right before a slur or hammer-on)
    {
        std::vector<Cand> c;
        for (int k = 0; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (x.special || x.len > 0.3 || x.role == R_Ghost) continue;
            double sc = 0.42 + (x.restAfter >= x.len * 0.8 ? 0.12 : 0.0) + (! x.onBeat ? 0.06 : 0.0) + (B.groove ? 0.06 : 0.0) + (x.fast ? -0.1 : 0.0);
            c.push_back ({ k, violin && B.frac[F_Short] >= 0.14 && x.fast ? A_Staccato : A_Staccato, sc, W_ShortOffbeat });
        }
        runPass (F_Short, c, [&] (const Cand& cd) { return cd.k + 1 >= N || familyOf (art[(size_t) cd.k + 1]) != F_Legato; });
    }

    // ---------------- 11) accents: the downbeat where the chord changes
    {
        std::vector<Cand> c;
        for (int k = 0; k < N; ++k)
        {
            const auto& x = f[(size_t) k];
            if (x.special || ! x.down || x.weight < 0.8) continue;
            c.push_back ({ k, A_Marcato, 0.42 + (x.chordChange ? 0.12 : 0.0) + (x.bar % 4 == 0 ? 0.06 : 0.0) + (double) st.dyn * 0.2, W_AccentChange });
        }
        runPass (F_Accent, c, [&] (const Cand&) { return true; });
    }

    // ---------------- 12) everything else is played plain (and says so)
    for (int k = 0; k < N; ++k)
        if (art[(size_t) k] < 0 && ! removed[(size_t) k])
        {
            art[(size_t) k] = (wind || violin) && p.raw[(size_t) f[(size_t) k].idx].art == A_Legato ? A_Sustain : A_Sustain;
            if (why[(size_t) k] == W_None) why[(size_t) k] = f[(size_t) k].weight >= 0.8 ? W_PlainStrong : W_Plain;
        }

    // ---------------- 13) a repeated riff repeats its articulations (the first time it is played sets them)
    {
        std::map<std::string, int> firstBar;
        std::map<int, std::vector<int>> barNotes;
        for (int k = 0; k < N; ++k) barNotes[f[(size_t) k].bar].push_back (k);
        const double keep = 1.0 - vary * 0.5;
        for (auto& kv : barNotes)
        {
            std::string sig;
            for (int k : kv.second) sig += std::to_string ((int) std::lround (f[(size_t) k].pos * 48.0)) + ":" + std::to_string (f[(size_t) k].pitch) + ",";
            auto it = firstBar.find (sig);
            if (it == firstBar.end()) { firstBar[sig] = kv.first; continue; }
            const auto& src = barNotes[it->second];
            if (src.size() != kv.second.size() || ! rng.chance (keep)) continue;
            for (size_t j = 0; j < src.size(); ++j)
            {
                const int from = src[j], to = kv.second[j];
                if (f[(size_t) to].special || f[(size_t) from].special) continue;
                const int fa = familyOf (art[(size_t) from]);
                if (fa >= 0 && frac[(size_t) fa] <= 0.0) continue;
                // never a short / dead note right before a joined note, never a joined note after a short / dead one
                auto shortish = [] (int a) { return a == A_Ghost || a == A_Mute || a == A_Staccato || a == A_Pizz || a == A_Harmonic; };
                if (shortish (art[(size_t) from]) && to + 1 < N && familyOf (art[(size_t) to + 1]) == F_Legato) continue;
                if (familyOf (art[(size_t) from]) == F_Legato && to > 0 && shortish (art[(size_t) to - 1])) continue;
                // a joined note (hammer-on, pull-off, slur, slide) is only copied when it is reached the same way as the first time
                if ((fa == F_Legato || fa == F_Slide || familyOf (art[(size_t) to]) == F_Legato || familyOf (art[(size_t) to]) == F_Slide) && (f[(size_t) to].iv != f[(size_t) from].iv || std::abs (f[(size_t) to].gapPrev - f[(size_t) from].gapPrev) > 1e-6)) continue;
                if (art[(size_t) to] == art[(size_t) from] && removed[(size_t) to] == removed[(size_t) from]) continue;
                art[(size_t) to] = art[(size_t) from]; removed[(size_t) to] = removed[(size_t) from]; vib[(size_t) to] = vib[(size_t) from]; bendBy[(size_t) to] = bendBy[(size_t) from];
                why[(size_t) to] = W_Riff;
            }
        }
    }

    // ---------------- 14) horns breathe: a long unbroken run gets a short breath before a strong beat
    if (wind)
    {
        double runStart = f[0].start;
        for (int k = 1; k < N; ++k)
        {
            if (f[(size_t) k].restBefore >= 0.2) { runStart = f[(size_t) k].start; continue; }
            if ((f[(size_t) k].start - runStart) * secPerBeat > 7.0 && f[(size_t) k].onBeat)
            {
                const int j = k - 1;
                auto& pn = p.raw[(size_t) f[(size_t) j].idx];
                pn.length = std::max (0.1, std::min (pn.length, f[(size_t) k].start - pn.start - 0.25));
                if (familyOf (art[(size_t) k]) == F_Legato) { art[(size_t) k] = A_Sustain; }
                why[(size_t) j] = W_Breath;
                runStart = f[(size_t) k].start;
            }
        }
    }

    // ---------------- 15) write the plan into the notes (with what each articulation does to the note itself)
    for (int k = 0; k < N; ++k)
    {
        auto& n = p.raw[(size_t) f[(size_t) k].idx];
        n.why = why[(size_t) k];
        if (removed[(size_t) k]) continue;
        const int a = art[(size_t) k];
        if (vib[(size_t) k] && ! guitar) n.vib = true;
        if (f[(size_t) k].special) continue;
        const int was = n.art;
        n.art = a;
        if (familyOf (a) == F_Legato || ((a == A_Slide || a == A_Gliss) && why[(size_t) k] != W_SlideIntoPhrase))
        {
            // joined to the note before: it rings right up to this one (the profile adds its own overlap where the library needs it)
            const int j = prevAlive (k);
            if (j >= 0) { auto& pn = p.raw[(size_t) f[(size_t) j].idx]; if (n.start - pn.start > 0.02 && pn.pitch != n.pitch) pn.length = std::max (pn.length, n.start - pn.start); }
        }
        if (a == A_Bend && bendBy[(size_t) k] > 0) { n.pitch -= bendBy[(size_t) k]; n.bend = bendBy[(size_t) k]; }
        if (a == was) continue;
        switch (a)
        {
            case A_PalmMute: n.length = std::max (0.06, n.length * (bass ? 0.75 : 0.8)); break;
            case A_Staccato: n.length = std::max (0.05, std::min (n.length, 0.22)); break;
            case A_Pizz:     n.length = std::max (0.05, std::min (n.length, 0.3)); break;
            case A_Mute:     n.length = std::max (0.05, std::min (n.length, 0.16)); n.vel = std::min (127, n.vel + 4); break;
            case A_Ghost:    n.vel = std::max (18, std::min (n.vel, 52)); n.length = std::max (0.05, std::min (n.length, 0.16)); break;
            case A_Marcato:  n.vel = std::min (127, n.vel + 10); break;
            default: break;
        }
#ifdef ENOCH_BASS_ENGINE
        if (bass && (a == A_Slap || a == A_Pop)) { n.vel = std::min (127, n.vel + (a == A_Pop ? 10 : 6)); n.length = std::min (n.length, a == A_Pop ? 0.3 : 0.45); }
#endif
    }
    // ghost fills that lost out are taken away (from the end, so the indices stay valid)
    std::vector<int> drop;
    for (int k = 0; k < N; ++k) if (removed[(size_t) k]) drop.push_back (f[(size_t) k].idx);
    std::sort (drop.begin(), drop.end());
    for (auto it = drop.rbegin(); it != drop.rend(); ++it) p.raw.erase (p.raw.begin() + *it);
}

/** what the main line of a take uses, per family (for the ARTICULATION panel's usage bars) */
template <class NoteVec>
inline std::array<int, F_Count + 1> usage (const NoteVec& raw)
{
    std::array<int, F_Count + 1> c {};
    for (auto& n : raw)
    {
        if (n.track != 0 || n.voice != 0 || n.mute) continue;
        const int fm = familyOf (n.art);
        if (fm >= 0) ++c[(size_t) fm]; else ++c[F_Count];
        if (n.vib && fm != F_Vibrato) ++c[F_Vibrato];
    }
    return c;
}
} // namespace artbrain
} // namespace enochsolo
