// Enoch Bass - generation engine (header only, no JUCE dependency)
#pragma once
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <memory>
#include "Theory.h"
#include "Styles.h"
#include "Reference.h"
#include "Modes.h"
#include "Groove.h"
#include "Libraries.h"

namespace enochsolo
{

//==============================================================================
struct Settings
{
    int instrument = I_Guitar;
    int artist = 0;                 // index inside the instrument's artist list
    int genre = G_Afrobeat;
    int keyRoot = 0;                // pitch class 0..11
    int scale = SC_Minor;
    int tsNum = 4, tsDen = 4;
    int bars = 8;
    float intensity = 0.6f;         // 0..1
    int mode = 0;                   // 0 = solo, 1 = accompaniment, 2 = both
    int octaveShift = 0;            // -2..+2
    int harmony = 0;                // 0 = key vamp (safest), 1 = auto progression, 2 = my chords
    std::string chordText;          // used when harmony == 2
    std::vector<BarChord> barChords;// chords picked bar by bar in the editor (override the harmony mode)
    float space = 0.5f;             // 0 busy .. 1 spacious
    float repeat = 0.5f;            // 0 always new ideas .. 1 repeat riffs
    bool chordTrack = false;        // also write a block-chord pad track
    float busy = 0.5f;              // 0 lazy .. 1 very busy (rapid 16ths); the default already plays a little busier than before
    std::vector<float> barBusy;     // per-bar override of 'busy' picked in the editor (< 0 = follow the global value)
    // reference recording / MIDI that steers the rhythm, feel and shape of the generated notes (not the notes themselves)
    std::shared_ptr<const RefProfile> refProfile;
    float refAmount = 0.7f;         // 0 = ignore .. 1 = follow closely
    int refTempo = 0;               // 0 as detected, 1 twice as fast, 2 half as fast
    double refShift = 0;            // beats the bar line is moved later
    // groove-aware fit: a drum / bass / keys loop of the project the solo locks to
    std::shared_ptr<const RefProfile> grooveProfile;
    float grooveAmount = 0.7f;      // 0 = ignore .. 1 = lock tightly
    int grooveTempo = 0;            // 0 as detected, 1 twice as fast, 2 half as fast
    double grooveShift = 0;         // beats the loop's bar line is moved later
    std::shared_ptr<const Library> lib;      // instrument of choice (null = Enoch standard layout)
    int clave = 1;                  // Latin genres: 0 off, 1 = 3-2 clave, 2 = 2-3 clave
    int voice2 = 0;                 // second voice: 0 off, 1 third below, 2 third above, 3 horn section, 4 call and response
    int exportMode = 0;             // 0 full (key-switches + expression), 1 no key-switches (articulations baked into the notes),
                                    // 2 plain notes only, 3 one MIDI track per articulation (no key-switches)

    int ksBase = 12;                // key-switch base note for the standard layout (C0 = 12, below the lowest bass note)
    int bendRange = 2;              // semitones, instrument bend range in the target plugin
    bool emitKS = true;
    bool emitBends = true;
    bool emitCC = true;
    bool realizeOrnaments = false;  // trills / shakes / tremolo written as real notes instead of key-switch only

    double bpm = 120.0;
    uint64_t seed = 1;
    uint64_t chordSeed = 0;         // 0 = use seed (keeps an auto-progression stable while the notes are re-rolled)

    bool humanize = true;
    float hTiming = 0.5f, hVel = 0.5f, hLength = 0.5f, hFeel = 0.5f;
    uint64_t hSeed = 1;
};

struct Note
{
    double start = 0, length = 0.25;
    int pitch = 60, vel = 90, art = A_Sustain;
    int id = 0;         // stable identity (editing / humanising)
    int track = 0;      // 0 = solo, 1 = accompaniment, 2 = chord pad
    int voice = 0;      // 0 = main line (gets key-switches / expression), 1 = supporting notes (left hand, bass, ...)
    double weight = 0;  // metric weight (0..1)
    int bend = 0;       // semitones of bend for A_Bend
    bool vib = false;
    bool mute = false;  // kept in the piano roll but never sounds / exported
    int role = 0;       // bass: the job of the note (root, fifth, approach...), so a chord change keeps the line musical (see BassGen.h)
};

enum { EV_OFF = 0, EV_ON = 1, EV_CC = 2, EV_BEND = 3, EV_AT = 4 };
struct MidiEv
{
    double t = 0;
    int type = EV_ON;
    int a = 0, b = 0;   // note/vel, cc/value, bend14/-
    int track = 0;
    int ord = 2;        // ordering at identical times: 0 = note off, 1 = key-switch / cc / bend, 2 = note on
    int art = 0;        // articulation of the note this event belongs to (note events only)
};

/** controller lanes the user can draw in the piano roll (always on the solo track) */
enum Lane { L_Bend = 0, L_Mod, L_Aftertouch, L_Expression, L_Sustain, L_Count };
struct CtrlPoint  { double t = 0; int lane = L_Mod; int value = 0; };          // bend 0..16383 (8192 = centre), others 0..127
struct CtrlRegion { int lane = L_Mod; double t0 = 0, t1 = 0; };               // where drawn data replaces the automatic data

struct Pattern
{
    Settings s;
    double lengthBeats = 0, barBeats = 4.0;
    int bars = 0;
    std::vector<Note> raw;          // quantised, un-humanised
    std::vector<Note> notes;        // final (humanised) notes
    std::vector<MidiEv> events;     // what gets exported (follows the export mode)
    std::vector<MidiEv> playEvents; // what the built-in test player plays (always the full articulated version)
    std::vector<CtrlPoint> userPts; std::vector<CtrlRegion> userRegions;
    std::vector<ChordSpan> chords;
    std::string progName, styleName, genreName;
    bool hasSolo = false, hasComp = false, hasChords = false;
    int nextId = 1;
    int barOf (double beat) const { return std::clamp ((int) std::floor (beat / std::max (0.01, barBeats) + 1e-6), 0, std::max (0, bars - 1)); }
    const ChordSpan& chordAt (double beat) const
    {
        for (auto& c : chords) if (beat + 1e-6 >= c.start && beat < c.end - 1e-6) return c;
        return chords.back();
    }
};

//==============================================================================
namespace detail
{
inline const std::vector<std::string>& cellsAfro()
{
    static const std::vector<std::string> c = {
        "x..x..x...x.x...", "x..x..x.x..x..x.", "x.x...x.x.x...x.", ".x..x.x..x..x.x.", "x..xx..xx..x.x..",
        "x.xx.xx.x.xx.xx.", "x...x...x...x...", "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx", "..x.x..x..x.x.xx",
        "x.....x.x.....x.", "x..x.x..x..x.x..", "xx.x.xx.x.xx.x.x", ".x.x..x.x.x..x.x", "x.x..x.x..x.x..x",
        "x......x..x...x.", "x.xx..x.x.xx..x.", "x..x..x..x..x.x.", "xx..x.x.xx..x.x.", "x.x.xx.x.x.xx.x." };
    return c;
}
inline const std::vector<std::string>& cellsJazz()
{
    static const std::vector<std::string> c = {
        "x.x.x.x.x.x.x.x.", "x...x.x.x...x.x.", "x.x...x.x.x.x...", "..x.x.x...x.x.x.", "x.x.x...x.x.x.x.",
        "x...x...x...x...", "x.x.x.x.x...x.x.", "x.x...x.x.x...x.", "..x...x.x.x.x.x.", "x.x.x.x.x.x.x..." };
    return c;
}
inline const std::vector<std::string>& cellsBallad()
{
    static const std::vector<std::string> c = {
        "x.......x.......", "x...x...x.......", "x.x...x.x.......", "x.....x.x.....x.",
        "x.......x...x...", "x...x.x.x.......", "x.x.....x.x.....", "..x.....x...x..." };
    return c;
}
inline const std::vector<std::string>& cellsHighlife()
{
    static const std::vector<std::string> c = {
        "x.x.x.x.x.x.x.x.", "x..x.x..x.x.x.x.", "x.xx.x.x.xx.x.x.", "x.x.xx.x.x.xx.x.", "x..x.xx.x..x.xx.", "x.x..x.x.x.x..x." };
    return c;
}
inline std::vector<std::string> cellsFor (int genre)
{
    static const std::vector<std::string> amapiano = { "x..x..x...x.x...", "..x..x..x..x..x.", "x.....x.x.....x.", ".x..x..x..x.x...", "x..x..x..x..x...", "x.x...x...x.x.x.", "x..x....x..x..x." };
    static const std::vector<std::string> bossa    = { "x..x..x...x..x..", "x..x..x..x..x...", "x..x.x..x..x..x.", "x.....x.x..x..x.", "x..x..x.x..x...." };
    static const std::vector<std::string> samba    = { "x.xx.x.xx.xx.x.x", "xx.xx.x.xx.xx.x.", "x.x.xx.x.x.xx.x.", "xxxxxxxxxxxxxxxx", "x.xx.xx.x.xx.xx." };
    static const std::vector<std::string> flamenco = { "xxxx..xx.x.x..x.", "x.x.xx.x..xxxx..", "x..x..x..x.xxxx.", "xxxxxxxx........", "x.xx.x.x.xx.x.x.", "xxxxxxxxxxxxxxxx" };
    static const std::vector<std::string> rock     = { "x.x.x.x.x.x.x.x.", "x...x.x.x...x.x.", "xx.x.xx.x.x.x.x.", "x.xx.x.xx.x.x.x.", "x.x...x.x.x.x...", "xxxxxxxx.x.x.x.x" };
    static const std::vector<std::string> pop      = { "x..x..x...x.x...", "x.x...x.x.x...x.", "x...x.x.x...x.x.", "..x.x..x..x.x...", "x..x.x..x..x.x..", "x.x.x.x.x.x.x..." };
    static const std::vector<std::string> gospel   = { "x.x.x...x.x.x.x.", "x..x..x.x..x.x..", "x.x...x.x.x.x...", "x...x.x.x.x.x..." };
    static const std::vector<std::string> juju     = { "x.x.x.x.x.x.x.x.", "x.xx.x.x.xx.x.x.", "x..x.x.x..x.x.x.", "x.x.xx.x.x.xx.x." };
    static const std::vector<std::string> soukous  = { "x.xx.xx.x.xx.xx.", "xxxx.xxxx.xx.xx.", "x.xxx.xx.xxx.xx.", "x.xx.x.xx.xx.x.x" };
    static const std::vector<std::string> ethio    = { "x..x..x..x..x..x", "x.x..x.x..x.x..x", "x..x.x..x..x.x..", "x.x..x..x.x..x.." };
    static const std::vector<std::string> township = { "x.x.xx.x.x.xx.x.", "x.x.x.xx.x.x.x.x", "x..xx.x.x..xx.x.", "x.x.x.x.x.x.x.x." };
    std::vector<std::string> r;
    auto add = [&] (const std::vector<std::string>& v) { r.insert (r.end(), v.begin(), v.end()); };
    switch (genre)
    {
        case G_Afrobeat: case G_AfroFusion: add (cellsAfro()); break;
        case G_AfroJazz: add (cellsAfro()); add (cellsJazz()); break;
        case G_Highlife: add (cellsHighlife()); add (cellsHighlife()); add (cellsAfro()); break;
        case G_Swing:    add (cellsJazz()); break;
        case G_Amapiano: add (amapiano); break;
        case G_Juju:     add (juju); add (cellsHighlife()); break;
        case G_Soukous:  add (soukous); add (cellsAfro()); break;
        case G_EthioJazz: add (ethio); break;
        case G_Township: add (township); add (cellsHighlife()); break;
        case G_Gospel:   add (gospel); add (cellsBallad()); break;
        case G_Pop:      add (pop); break;
        case G_Rock:     add (rock); break;
        case G_Bossa:    add (bossa); break;
        case G_Samba:    add (samba); break;
        case G_Flamenco: add (flamenco); break;
        default:         add (cellsBallad()); break;
    }
    return r;
}
inline int cellCount (const std::string& s) { return (int) std::count (s.begin(), s.end(), 'x'); }
inline bool isWind (int inst) { return inst == I_Sax || inst == I_Trumpet; }
} // namespace detail

//==============================================================================
inline void applySecondVoice (Pattern& p, const Settings& s, int rangeShift);   // Harmony.h
namespace detail {
inline std::vector<Note> generateBass (const Settings&, const Style&, const Meter&, Rng&, const std::vector<ChordSpan>&);   // BassGen.h
inline void bassReharmonize (Pattern&, const std::vector<ChordSpan>&, const std::vector<char>&, const Settings&);
}

class Generator
{
public:
    static Pattern generate (const Settings& s);
    /** humanise (if enabled) and rebuild key-switch / CC / bend / note events from pattern.raw */
    static void finalize (Pattern& p, const Settings& s);

    static std::vector<ChordSpan> buildChords (const Settings& s, const Meter& m, Rng& rng, std::string& progName);
    /** an empty pattern (no notes) with the right length, chords and flags - used when restoring saved notes */
    static Pattern makeShell (const Settings& s);
    /** the chord list for these settings (deterministic; honours per-bar chords) */
    static std::vector<ChordSpan> chordsFor (const Settings& s, std::string& progName);
    /** after chords changed: re-pitch only the notes of the bars whose chord changed */
    static void reharmonize (Pattern& p, const std::vector<ChordSpan>& oldChords, const std::vector<ChordSpan>& newChords, const Settings& s);
    static void rerollRegion (Pattern& p, const Settings& s, int bar0, int bar1, uint64_t newSeed);
};

//==============================================================================
namespace detail
{

struct SN { int step = 0; int dur = 1; double w = 0; int ci = 0; int pitch = 60;
            float rLoud = -1.f, rLen = -1.f; double rRel = 0; bool rHas = false; bool rBar = false; };

class SoloGen
{
public:
    SoloGen (const Settings& s, const Style& st, const Meter& m, Rng& r, const std::vector<ChordSpan>& ch)
        : S (s), st (st), g (genreInfo (s.genre)), m (m), rng (r), chords (ch)
    {
        const Range hr = instRange (st.inst, st.variant);
        hardLo = hr.lo; hardHi = hr.hi;
        const double c = hardLo + (hardHi - hardLo) * (0.38 + 0.25 * st.regBias);
        const double w = 14.0 + (hardHi - hardLo - 14.0) * st.range * 0.62;
        lo = std::max (hardLo, (int) std::lround (c - w * 0.5));
        hi = std::min (hardHi, (int) std::lround (c + w * 0.5));
        if (hi - lo < 12) { lo = std::max (hardLo, lo - 3); hi = std::min (hardHi, hi + 3); }
        swingAmt = g.fixedSwing >= 0 ? std::clamp (g.fixedSwing + st.swing * 0.1, 0.0, 1.0) : st.swing;
        afro = g.afro;
        const double baseTiming = 0;
        (void) baseTiming;
        mutedSolo = (st.inst == I_Trumpet) && rng.chance (st.muted);
        if (S.refProfile && S.refProfile->valid() && S.refAmount > 0.01f)
        {
            rv = makeRefView (*S.refProfile, m.barBeats, S.refTempo, S.refShift);
            if (rv.valid)
            {
                rw = std::clamp ((double) S.refAmount, 0.0, 1.0);
                refOff = rng.i (0, std::max (0, rv.bars - 1));
                if (rv.swingKnown && ! m.compound) swingAmt = swingAmt * (1.0 - 0.9 * rw) + rv.swing * 0.9 * rw;
            }
        }
        if (S.grooveProfile && S.grooveProfile->valid() && S.grooveAmount > 0.01f && ! m.compound)
        {
            gv = makeGrooveView (*S.grooveProfile, m.barBeats, S.grooveTempo, S.grooveShift);
            if (gv.valid && gv.steps == m.stepsPerBar)
            {
                gw = std::clamp ((double) S.grooveAmount, 0.0, 1.0);
                if (gv.swingKnown) swingAmt = swingAmt * (1.0 - 0.9 * gw) + gv.swing * 0.9 * gw;
            }
            else gv = GrooveView();
        }
    }

    std::vector<Note> run();

private:
    /** note lengths / short-vs-connected playing taken from the reference note each onset follows */
    void applyRefFeel (std::vector<Note>& notes, const std::vector<SN>& ns, int activeEnd)
    {
        for (size_t i = 0; i < ns.size(); ++i)
        {
            if (ns[i].rLen < 0.f) continue;
            const double t0 = stepBeat (ns[i].step);
            const double nxt = (i + 1 < ns.size()) ? stepBeat (ns[i + 1].step) : stepBeat (activeEnd);
            const double ioi = std::max (0.12, nxt - t0);
            for (auto& n : notes)
            {
                if (std::abs (n.start - t0) > 1e-9 || n.track != 0 || n.length < 0.05) continue;
                switch (n.art)
                {
                    case A_Fall: case A_Doit: case A_Growl: case A_Shake: case A_Trill: case A_Tremolo: case A_Arpeggio: case A_Strum:
                    case A_Gliss: case A_Slide: case A_Bend: case A_Scoop: case A_Flutter: continue;
                    default: break;
                }
                const double target = std::clamp (ioi * (double) ns[i].rLen, 0.07, ioi * 1.05);
                const double k = 0.85 * rw;
                n.length = std::max (0.07, n.length * (1.0 - k) + target * k);
                if (ns[i].rLen < 0.5f && (n.art == A_Sustain || n.art == A_Legato) && artIndex (st.inst, A_Staccato) >= 0) n.art = A_Staccato;
                else if (ns[i].rLen > 0.9f && n.art == A_Staccato) n.art = A_Sustain;
            }
        }
    }

    const Settings& S; const Style& st; const GenreInfo& g; const Meter& m; Rng& rng; const std::vector<ChordSpan>& chords;
    int hardLo = 40, hardHi = 88, lo = 50, hi = 80;
    double swingAmt = 0; bool afro = true; bool mutedSolo = false;
    GrooveView gv; double gw = 0;                        // groove loop (gw = 0: none)
    RefView rv; double rw = 0; int refOff = 0;           // reference (rw = 0: none)
    std::map<int, int> refAt;                            // absolute step -> index of the reference note it follows
    std::vector<Note> out;
    int lastPitch = -1;

    // previous phrase memory (riffs / call and response)
    std::vector<SN> prevPhrase; int prevPhraseStart = 0;

    double stepBeat (int absStep) const
    {
        const int spb = m.stepsPerBar;
        const int bar = absStep / spb, within = absStep % spb;
        double b = bar * m.barBeats + within * 0.25;
        if (! m.compound && swingAmt > 0)
        {
            const int r = within % 4;
            if (r == 2) b += swingAmt * (1.0 / 6.0);
            else if (r == 3) b += swingAmt * (1.0 / 12.0);
        }
        return b;
    }

    int chordIndexAtBeat (double beat) const
    {
        for (size_t i = 0; i < chords.size(); ++i)
            if (beat + 1e-6 >= chords[i].start && beat < chords[i].end - 1e-6) return (int) i;
        return (int) chords.size() - 1;
    }

    double intensityAt (double tg) const
    {
        const double arc = tg < 0.7 ? tg / 0.7 : 1.0 - (tg - 0.7) / 0.3 * 0.5;
        return std::clamp ((double) S.intensity * (0.55 + 0.45 * arc), 0.05, 1.0);
    }
    double arcAt (double tg) const { return tg < 0.7 ? tg / 0.7 : 1.0 - (tg - 0.7) / 0.3 * 0.5; }

    std::vector<int> buildPool (int ci, bool pent, bool blue, int loL, int hiL) const
    {
        const ChordSpan& c = chords[(size_t) ci];
        uint16_t mask = c.scaleMask;
        if (pent)
        {
            const uint16_t pm = (uint16_t) (pentSubsetFor (c.rootPc, c.q) & c.scaleMask);
            int bits = 0; for (int k = 0; k < 12; ++k) bits += (pm >> k) & 1;
            if (bits >= 4) mask = pm;
        }
        if (blue) mask = (uint16_t) (mask | blueNotesFor (c.rootPc, c.q));
        std::vector<int> pool;
        for (int p = loL; p <= hiL; ++p) if (inMask (mask, p)) pool.push_back (p);
        if (pool.size() < 5) { pool.clear(); for (int p = loL; p <= hiL; ++p) if (inMask (c.scaleMask, p)) pool.push_back (p); }
        return pool;
    }

    static int nearestIdx (const std::vector<int>& pool, int pitch)
    {
        int best = 0, bd = 1 << 20;
        for (size_t i = 0; i < pool.size(); ++i) { const int d = std::abs (pool[i] - pitch); if (d < bd) { bd = d; best = (int) i; } }
        return best;
    }

    //------------------------------------------------------------------ rhythm
    double busyOf (int bar) const
    {
        return (bar >= 0 && bar < (int) S.barBusy.size() && S.barBusy[(size_t) bar] >= 0.f) ? (double) S.barBusy[(size_t) bar] : (double) S.busy;
    }

    std::vector<int> barOnsets (double dens, double busy = 0.5)
    {
        const int spb = m.stepsPerBar;
        std::vector<int> r;
        if (m.simpleFour)
        {
            const auto cells = cellsFor (S.genre);
            const double target = dens * spb * (0.5 + 0.45 * std::max (0.0, (busy - 0.5) * 2.0));
            std::vector<double> w;
            for (auto& c : cells) w.push_back (std::exp (-std::abs (cellCount (c) - target) / 1.8) + 0.02);
            const std::string& cell = cells[(size_t) rng.weighted (w)];
            for (int s = 0; s < spb && s < (int) cell.size(); ++s) if (cell[(size_t) s] == 'x') r.push_back (s);
            // mutate: drop weak onsets (space), add onsets if too sparse, shift on-beat onsets early (anticipation)
            for (size_t k = 0; k < r.size() && r.size() > 2;)
            {
                if (rng.chance (st.rest * g.restMul * 0.3 * std::clamp (1.5 - busy, 0.2, 1.5) * (1.0 - m.weight[(size_t) r[k]]))) r.erase (r.begin() + (long) k); else ++k;
            }
            int guard = 0;
            while ((double) r.size() < target - 1.5 && guard++ < 20)
            {
                const int s = rng.i (1, spb - 1);
                if (std::find (r.begin(), r.end(), s) == r.end()) r.push_back (s);
            }
            for (auto& s : r)
                if (s > 0 && m.weight[(size_t) s] >= 0.55 && rng.chance (st.sync * 0.22) && std::find (r.begin(), r.end(), s - 1) == r.end()) s -= 1;
            std::sort (r.begin(), r.end()); r.erase (std::unique (r.begin(), r.end()), r.end());
        }
        else
        {
            int prev = -5;
            for (int s = 0; s < spb; ++s)
            {
                const double w = m.weight[(size_t) s];
                const double wf = (1.0 - st.sync) * w + st.sync * (1.0 - w) * 0.8;
                double p = std::clamp (dens * 0.85 * (0.25 + 0.75 * wf), 0.0, 0.95);
                if (w >= 0.6) p = std::max (p, 0.7 + 0.2 * (1.0 - st.sync));
                if (s - prev == 1 && w < 0.2) p *= 0.4;
                if (rng.chance (p)) { r.push_back (s); prev = s; }
            }
        }
        if (r.empty()) r.push_back (0);
        return r;
    }

    /** lock the onsets of a bar to the groove loop: add hits where the loop hits hard, leave space where it is busy */
    std::vector<int> grooveFit (std::vector<int> r, double busyness01)
    {
        const int spb = m.stepsPerBar;
        for (int st2 = 0; st2 < spb && st2 < (int) gv.occ.size(); ++st2)
        {
            const double o = gv.occ[(size_t) st2], sv = gv.str[(size_t) st2];
            if (o >= 0.5 && sv > 0.45 && std::find (r.begin(), r.end(), st2) == r.end() && rng.chance (gw * 0.45 * o * sv)) r.push_back (st2);
        }
        const double busyLoop = std::clamp ((gv.hitsPerBar - spb * 0.5) / (spb * 0.5), 0.0, 1.0);
        std::vector<int> out;
        for (int s2 : r)
        {
            const double o = s2 < (int) gv.occ.size() ? gv.occ[(size_t) s2] : 0.0;
            const bool weak = m.weight[(size_t) s2] < 0.4 && o < 0.25;
            if (weak && (int) r.size() > 2 && rng.chance (gw * 0.35 * busyLoop * (0.4 + 0.6 * busyness01))) continue;
            out.push_back (s2);
        }
        if (out.size() < 2) out = r;
        std::sort (out.begin(), out.end());
        out.erase (std::unique (out.begin(), out.end()), out.end());
        return out;
    }

    /** onsets of one bar that follow the reference's rhythm (mixed with the genre's own onsets by the reference amount) */
    std::vector<int> refBarOnsets (int bar, const std::vector<int>& native, double busy)
    {
        const int spb = m.stepsPerBar;
        const int rb = (bar + refOff) % std::max (1, rv.bars);
        std::vector<int> r;
        std::map<int, int> from;
        const double f = 1.0 + (busy - 0.5) * 1.2;                      // global Busy still scales the reference
        for (int idx : rv.byBar[(size_t) rb])
        {
            const RefNote& n = rv.notes[(size_t) idx];
            int step = (int) std::lround (n.pos * 4.0);
            if (step >= spb) continue;
            step = std::max (0, step);
            const double w = m.weight[(size_t) step];
            double keep = std::clamp (rw + (w >= 0.55 ? 0.25 : 0.0), 0.0, 1.0);
            keep *= std::clamp (f, 0.0, 1.0);
            if (rw < 0.999) keep = std::min (keep, 0.97);
            if (! rng.chance (keep)) continue;
            // a little life so it is 'similar', not a copy: occasionally slide a weak onset by one 16th
            if (w < 0.3 && rng.chance (0.10 * (1.0 - 0.5 * rw))) step = std::clamp (step + (rng.chance (0.5) ? 1 : -1), 0, spb - 1);
            if (std::find (r.begin(), r.end(), step) == r.end()) { r.push_back (step); from[step] = idx; }
        }
        for (int s : native)
            if (std::find (r.begin(), r.end(), s) == r.end() && rng.chance ((1.0 - rw) * 0.85)) r.push_back (s);
        // busier than the reference: sprinkle extra 16ths
        if (f > 1.0 && ! r.empty())
        {
            int extra = (int) std::lround ((double) r.size() * (f - 1.0));
            for (int guard = 0; extra > 0 && guard < 30; ++guard)
            {
                const int s = rng.i (0, spb - 1);
                if (std::find (r.begin(), r.end(), s) == r.end()) { r.push_back (s); --extra; }
            }
        }
        if (r.empty()) r.push_back (0);
        std::sort (r.begin(), r.end());
        for (auto& kv : from) refAt[bar * spb + kv.first] = kv.second;
        return r;
    }

    //------------------------------------------------------------------ pitch material
    int pickAnchor (const std::vector<SN>& ns, int i, int prev, const std::vector<int>& pool, double tg)
    {
        const ChordSpan& c = chords[(size_t) ns[(size_t) i].ci];
        const auto iv = chordIntervals (c.q);
        const int third = (c.rootPc + iv[1]) % 12, seventh = (c.rootPc + iv[3]) % 12, rootPc = c.rootPc % 12;
        const double lam = 2.0 + 9.0 * st.leap, tw = 6.0 + 8.0 * st.range;
        std::vector<double> w;
        for (int p : pool)
        {
            const int pc = p % 12;
            const bool tone = inMask (c.toneMask, pc);
            double pw;
            if (tone) pw = (pc == third || pc == seventh) ? 1.5 : (pc == rootPc ? 1.0 : 0.9);
            else      pw = 0.3;
            if (ns[(size_t) i].w >= 0.8) pw *= tone ? 1.4 : 0.5;
            pw *= std::exp (-std::abs (p - prev) / lam);
            pw *= std::exp (-std::abs (p - tg) / tw);
            if (p == prev) pw *= afro ? 0.8 : 0.25;
            w.push_back (pw);
        }
        return pool[(size_t) rng.weighted (w)];
    }

    void fillBetween (std::vector<SN>& ns, int a, int b, const std::vector<int>& pool, double intens, int hiLim)
    {
        const int cnt = b - a - 1;
        if (cnt <= 0) return;
        const int from = ns[(size_t) a].pitch, to = ns[(size_t) b].pitch;
        const ChordSpan& c = chords[(size_t) ns[(size_t) a + 1].ci];

        // ---- arpeggio run
        if (cnt >= 3 && rng.chance (st.arp * (0.6 + 0.8 * intens)))
        {
            std::vector<int> ladder;
            for (int p = lo; p <= hiLim; ++p) if (inMask (c.toneMask, p) || (rng.chance (0.15) && inMask (c.scaleMask, p) && (p % 12) == (c.rootPc + 2) % 12)) ladder.push_back (p);
            if (ladder.size() >= 4)
            {
                int idx = nearestIdx (ladder, from);
                int dir = (to >= from) ? 1 : -1;
                if (rng.chance (0.25)) dir = -dir;
                for (int j = 0; j < cnt; ++j)
                {
                    idx += dir;
                    if (idx < 0 || idx >= (int) ladder.size()) { dir = -dir; idx += 2 * dir; idx = std::clamp (idx, 0, (int) ladder.size() - 1); }
                    ns[(size_t) a + 1 + (size_t) j].pitch = ladder[(size_t) idx];
                }
                return;
            }
        }
        // ---- fourths
        if (cnt >= 3 && rng.chance (st.fourths))
        {
            int p = from; int dir = (to >= from) ? 1 : -1;
            for (int j = 0; j < cnt; ++j)
            {
                const int interval = (j % 2 == 0) ? 5 : (rng.chance (0.5) ? -3 : -2);
                p += interval * dir;
                if (p > hiLim) { p -= 12; } else if (p < lo) { p += 12; }
                const auto& pl = pool;
                ns[(size_t) a + 1 + (size_t) j].pitch = pl[(size_t) nearestIdx (pl, p)];
                p = ns[(size_t) a + 1 + (size_t) j].pitch;
            }
            return;
        }
        // ---- scalar with approach tones / enclosures
        int prev = from;
        bool forceBelow = false;
        int lastDir = (to >= from) ? 1 : -1;
        for (int j = 0; j < cnt; ++j)
        {
            const int remaining = cnt - j;   // notes left until the anchor, including this one
            int p = prev;
            if (j == cnt - 1 && forceBelow)                         p = to - 1;
            else if (j == cnt - 1 && rng.chance (st.chroma * 0.65))
            {
                const bool fromBelow = (from <= to) ? rng.chance (0.7) : rng.chance (0.3);
                p = fromBelow ? to - 1 : to + 1;
            }
            else if (cnt >= 2 && j == cnt - 2 && rng.chance (st.chroma * 0.35))
            {
                const int ti = nearestIdx (pool, to);
                p = pool[(size_t) std::min ((int) pool.size() - 1, ti + 1)];
                if (p == to) p = to + 2;
                forceBelow = true;
            }
            else
            {
                const int ci = nearestIdx (pool, prev), ti = nearestIdx (pool, to);
                std::vector<double> w; std::vector<int> idxs;
                for (int d = -4; d <= 4; ++d)
                {
                    const int k = ci + d;
                    if (k < 0 || k >= (int) pool.size()) continue;
                    double base;
                    switch (std::abs (d))
                    {
                        case 0: base = afro ? 0.12 + st.motif * 0.1 : 0.05; break;
                        case 1: base = st.step; break;
                        case 2: base = (1.0 - st.step) * 0.6 + 0.1; break;
                        case 3: base = st.leap * 0.35; break;
                        default: base = st.leap * 0.2; break;
                    }
                    const double pull = std::exp (-std::max (0.0, (double) std::abs (k - ti) - remaining * 2.0) * 0.8);
                    double wgt = base * pull;
                    if (d != 0 && ((d > 0) == (lastDir > 0))) wgt *= 1.25;
                    w.push_back (wgt); idxs.push_back (k);
                }
                if (w.empty()) p = prev;
                else
                {
                    p = pool[(size_t) idxs[(size_t) rng.weighted (w)]];
                    // bebop passing tone: fill whole steps going the same way with a chromatic note
                    if (std::abs (p - prev) == 2 && rng.chance (st.bebop * 0.5)) p = prev + (p > prev ? 1 : -1);
                }
            }
            p = std::clamp (p, std::max (hardLo, lo - 2), std::min (hardHi, hiLim + 2));
            if (p != prev) lastDir = (p > prev) ? 1 : -1;
            ns[(size_t) a + 1 + (size_t) j].pitch = p;
            prev = p;
        }
    }

    void assignPitches (std::vector<SN>& ns, bool usePent, bool useBlue, int contour, double intens, double gArc)
    {
        const int n = (int) ns.size();
        if (n == 0) return;
        const int hiLim = std::min (hardHi, hi + (intens > 0.7 ? 3 : 0));
        std::map<int, std::vector<int>> pools;
        auto poolFor = [&] (int ci) -> const std::vector<int>&
        {
            auto it = pools.find (ci);
            if (it == pools.end()) it = pools.emplace (ci, buildPool (ci, usePent, useBlue, lo, hiLim)).first;
            return it->second;
        };
        const double center = (lo + hiLim) * 0.5, amp = (hiLim - lo) * 0.5 * (0.5 + 0.5 * intens);
        auto target = [&] (int i)
        {
            const double t = n > 1 ? (double) i / (n - 1) : 0.5;
            double sh;
            switch (contour) { case 0: sh = 2.0 * std::sin (3.14159265 * t) - 1.0; break;      // arch
                               case 1: sh = 1.0 - 2.0 * t; break;                              // descending
                               case 2: sh = 2.0 * t - 1.0; break;                              // ascending
                               default: sh = std::sin (6.2831853 * t); }                       // wave
            const double ga = 2.0 * gArc - 1.0;
            const double base = center + amp * (0.65 * sh + 0.35 * ga);
            if (rw > 0 && ns[(size_t) i].rBar)
            {
                const SN& sn = ns[(size_t) i];
                const double rr = sn.rHas ? sn.rRel : rv.barRel[(size_t) ((sn.step / m.stepsPerBar + refOff) % std::max (1, rv.bars))] * 0.8;
                const double want = center + std::clamp (rr, -amp * 1.15, amp * 1.15);
                return base * (1.0 - 0.85 * rw) + want * 0.85 * rw;
            }
            return base;
        };

        int prev = lastPitch > 0 ? lastPitch : (int) center;
        int dir = prev < center ? 1 : -1;
        int prevMove = 0;                 // signed size (in scale steps) of the last move
        int runLeft = 0, runDir = 1;      // chord-tone arpeggio run

        for (int i = 0; i < n; ++i)
        {
            SN& cur = ns[(size_t) i];
            const auto& pool = poolFor (cur.ci);
            const ChordSpan& c = chords[(size_t) cur.ci];
            const bool changed = (i > 0 && cur.ci != ns[(size_t) i - 1].ci);
            const bool strong = cur.w >= 0.55 || changed || i == 0;
            const bool lastN = (i == n - 1);
            const int idx = nearestIdx (pool, prev);
            int chosen = prev;

            if (runLeft == 0 && i > 0 && n - i >= 3 && rng.chance (st.arp * 0.16 * (0.5 + intens)))
            {
                runLeft = std::min (n - i, rng.i (3, 5));
                runDir = (prev < target (i)) ? 1 : -1;
                if (rng.chance (0.25)) runDir = -runDir;
            }
            if (runLeft > 0)
            {
                auto nextTone = [&] (int d) { for (int k = 1; k <= 12; ++k) { const int q = prev + d * k; if (q < lo || q > hiLim) return prev; if (inMask (c.toneMask, q)) return q; } return prev; };
                int p = nextTone (runDir);
                if (p == prev) { runDir = -runDir; p = nextTone (runDir); }
                chosen = p;
                --runLeft;
            }
            else
            {
                std::vector<double> w; std::vector<int> cand;
                for (int d = -5; d <= 5; ++d)
                {
                    const int k = idx + d;
                    if (k < 0 || k >= (int) pool.size()) continue;
                    const int p = pool[(size_t) k];
                    double base;
                    switch (std::abs (d))
                    {
                        case 0:  base = afro ? 0.10 + 0.35 * st.motif : 0.03; break;
                        case 1:  base = 0.55 + 0.9 * st.step; break;
                        case 2:  base = 0.25 + 0.6 * (1.0 - st.step) + 0.2 * st.leap; break;
                        case 3:  base = 0.10 + 0.5 * st.leap; break;
                        case 4:  base = 0.05 + 0.35 * st.leap; break;
                        default: base = 0.02 + 0.2 * st.leap; break;
                    }
                    if (st.fourths > 0.1 && std::abs (p - prev) == 5) base *= 1.0 + 3.0 * st.fourths;
                    const int sgn = d > 0 ? 1 : (d < 0 ? -1 : 0);
                    if (sgn != 0 && sgn == dir) base *= 1.25;
                    if (std::abs (prevMove) >= 3 && sgn != 0)                       // after a leap, turn back by step
                    {
                        const int pm = prevMove > 0 ? 1 : -1;
                        base *= (sgn == -pm && std::abs (d) <= 2) ? 2.4 : (sgn == pm ? 0.35 : 1.0);
                    }
                    const double pull = std::exp (-std::abs ((double) p - target (i)) / ((4.0 + 6.0 * st.range) * (1.0 - 0.5 * rw)));
                    const bool tone = inMask (c.toneMask, p);
                    double tw = strong ? (tone ? 2.8 : 0.45) : (tone ? 1.2 : 1.0);
                    if (lastN) tw = tone ? 5.0 : 0.08;
                    if (i == 0) tw *= tone ? 1.6 : 0.8;
                    w.push_back (base * pull * tw); cand.push_back (p);
                }
                chosen = cand.empty() ? prev : cand[(size_t) rng.weighted (w)];
            }
            chosen = std::clamp (chosen, std::max (hardLo, lo - 2), std::min (hardHi, hiLim + 2));
            if (chosen != prev) { dir = chosen > prev ? 1 : -1; prevMove = nearestIdx (pool, chosen) - idx; } else prevMove = 0;
            cur.pitch = chosen; prev = chosen;
        }

        // chromatic approach notes leading into strong chord tones
        for (int i = 0; i + 1 < n; ++i)
        {
            SN& a = ns[(size_t) i]; const SN& b = ns[(size_t) i + 1];
            if (b.w >= 0.55 && a.w < 0.55 && a.dur <= 2 && rng.chance (st.chroma * 0.4 + st.bebop * 0.3))
            {
                const ChordSpan& c = chords[(size_t) b.ci];
                if (inMask (c.toneMask, b.pitch) && ! inMask (c.toneMask, a.pitch))
                    a.pitch = std::clamp (b.pitch + (rng.chance (0.6) ? -1 : 1), std::max (hardLo, lo - 2), std::min (hardHi, hiLim + 2));
            }
        }
        lastPitch = ns.back().pitch;
    }

    //------------------------------------------------------------------ riff repetition
    void adaptRepeat (std::vector<SN>& ns, bool usePent, bool useBlue)
    {
        const int hiLim = std::min (hardHi, hi + 3);
        std::map<int, std::vector<int>> pools;
        for (auto& n : ns)
        {
            auto it = pools.find (n.ci);
            if (it == pools.end()) it = pools.emplace (n.ci, buildPool (n.ci, usePent, useBlue, lo, hiLim)).first;
            const ChordSpan& c = chords[(size_t) n.ci];
            if (n.w >= 0.55 && ! inMask (c.toneMask, n.pitch) && rng.chance (0.8))
            {
                int best = n.pitch, bd = 99;
                for (int d = -3; d <= 3; ++d) if (inMask (c.toneMask, n.pitch + d) && std::abs (d) < bd) { bd = std::abs (d); best = n.pitch + d; }
                n.pitch = best;
            }
            else if (! inMask (c.scaleMask, n.pitch) && ! it->second.empty())
            {
                // allow chromatic approaches, only re-snap if far from anything useful
                if (rng.chance (0.5)) n.pitch = it->second[(size_t) nearestIdx (it->second, n.pitch)];
            }
        }
    }

public:
    std::vector<Note> assemble (std::vector<SN>& ns, int phraseIdx, int phraseCount, bool phraseLastOfSolo, int activeEnd,
                                double intens, bool pizzPhrase, bool octavePhrase);
private:
    void assignArts (std::vector<Note>& ns, int activeEndStepIsLast, bool pizzPhrase);
};

//------------------------------------------------------------------------------
inline std::vector<Note> SoloGen::run()
{
    const int spb = m.stepsPerBar;
    const int totalSteps = S.bars * spb;
    const double totalBeats = S.bars * m.barBeats;
    (void) totalBeats;

    // phrase layout
    struct Span { int a, b; };
    std::vector<Span> spans;
    for (int pos = 0; pos < totalSteps;)
    {
        int pb = g.phraseBars > 0 ? g.phraseBars : rng.pick (std::vector<int> { 1, 2, 2, 3 });
        if (S.bars <= 2) pb = 1;
        const int end = std::min (totalSteps, pos + pb * spb);
        spans.push_back ({ pos, end });
        pos = end;
    }

    const bool wind = isWind (st.inst);
    int phraseIdx = 0;
    for (auto& sp : spans)
    {
        const bool lastPhrase = (sp.b >= totalSteps);
        const double tg = (double) sp.a / totalSteps;
        const double I = intensityAt (tg);
        const double gArc = arcAt (tg);
        const double dens = std::clamp ((double) st.density * g.densMul * (0.55 + 0.9 * I), 0.12, 1.0);

        // breath / tail
        const int len = sp.b - sp.a;
        int tail = (int) std::lround (rng.f (0.6, 1.5) * (1.0 + st.rest * 8.0 * g.restMul) * (wind ? 1.0 : 0.6) * (afro ? 0.8 : 1.0) * 2.2 * std::clamp (1.35 - busyOf ((sp.b - 1) / spb), 0.25, 1.35));
        if (rw > 0) tail = (int) std::lround (tail * (1.0 - 0.8 * rw));          // the reference already contains the rests
        tail = std::clamp (tail, 0, len / 2);
        if (lastPhrase) tail = std::min (tail, std::max (2, spb / 4));
        const int activeEnd = sp.b - tail;
        int pickup = 0;
        if (phraseIdx > 0 && rng.chance (st.sync * 0.35)) pickup = rng.pick (std::vector<int> { 1, 2, 3 });
        const int activeStart = sp.a + pickup;

        const bool pentScale = (S.scale == SC_MajPent || S.scale == SC_MinPent);
        const bool usePent = rng.chance (afro ? std::max (0.6, (double) st.pent) : (double) st.pent) || pentScale;
        const bool useBlue = rng.chance (st.blues);

        // choose how to build the phrase
        enum { NEWP, REPEAT, VARY } mode = NEWP;
        if (phraseIdx > 0 && ! prevPhrase.empty())
        {
            const double pr = std::clamp (g.riffy * std::max ((double) st.motif, afro ? 0.6 : 0.0) * (0.4 + 1.2 * (double) S.repeat), 0.0, 1.0);
            const double r = rng.f();
            if (r < pr * 0.45) mode = REPEAT; else if (r < pr * 1.05) mode = VARY;
        }

        // following a reference closely: every phrase takes its rhythm from the recording
        if (rw > 0.45) mode = NEWP;
        // bars the user gave an explicit 'busy' value are always written fresh so the slider is audible
        for (int bar = sp.a / spb; bar * spb < sp.b; ++bar)
            if (bar < (int) S.barBusy.size() && S.barBusy[(size_t) bar] >= 0.f) mode = NEWP;

        std::vector<SN> ns;
        if (mode == NEWP)
        {
            for (int bar = sp.a / spb; bar * spb < sp.b; ++bar)
            {
                const double bz = busyOf (bar);
                auto native = barOnsets (std::max (std::clamp (dens * (0.55 + 1.7 * bz), 0.12, 1.0), bz > 0.5 ? 0.85 * (bz - 0.5) * 2.0 : 0.0), bz);
                const bool explicitBusy = bar < (int) S.barBusy.size() && S.barBusy[(size_t) bar] >= 0.f;
                if (rw > 0 && ! explicitBusy) native = refBarOnsets (bar, native, bz);
                if (gw > 0 && ! explicitBusy) native = grooveFit (native, bz);
                for (int s : native)
                {
                    const int abs = bar * spb + s;
                    if (abs >= activeStart && abs < activeEnd)
                    {
                        SN n; n.step = abs;
                        auto it = refAt.find (abs);
                        if (it != refAt.end())
                        {
                            const RefNote& rn = rv.notes[(size_t) it->second];
                            n.rLoud = rn.loud; n.rLen = rn.lenFrac; n.rHas = rn.hasPitch && rv.pitched; n.rRel = rn.rel;
                        }
                        n.rBar = rw > 0 && rv.pitched && ! explicitBusy;
                        ns.push_back (n);
                    }
                }
            }
            if (ns.empty()) { SN n; n.step = activeStart; ns.push_back (n); }
        }
        else
        {
            for (auto& pn : prevPhrase)
            {
                const int abs = pn.step - prevPhraseStart + sp.a + pickup;
                if (abs < activeEnd) { SN n = pn; n.step = abs; ns.push_back (n); }
            }
            if (ns.empty()) { SN n; n.step = activeStart; ns.push_back (n); mode = NEWP; }
        }

        // durations, weights, chord index
        for (size_t i = 0; i < ns.size(); ++i)
        {
            const int next = (i + 1 < ns.size()) ? ns[i + 1].step : activeEnd;
            ns[i].dur = std::max (1, next - ns[i].step);
            ns[i].w = m.weight[(size_t) (ns[i].step % spb)];
            ns[i].ci = chordIndexAtBeat (stepBeat (ns[i].step));
        }

        const int contour = rng.i (0, 3);
        if (mode == NEWP)
            assignPitches (ns, usePent, useBlue, contour, I, gArc);
        else
        {
            for (auto& n : ns) n.pitch = std::clamp (n.pitch, hardLo, hardHi);
            adaptRepeat (ns, usePent, useBlue);
            if (mode == VARY)
            {
                // vary the ending (answer phrase) and sometimes the whole phrase an octave / a step
                const int k = std::min ((int) ns.size(), rng.i (1, 2));
                std::vector<SN> tailPart (ns.end() - k, ns.end());
                const int keep = (int) ns.size() - k;
                lastPitch = keep > 0 ? ns[(size_t) keep - 1].pitch : lastPitch;
                std::vector<SN> tmp = ns;
                assignPitches (tmp, usePent, useBlue, contour, I, gArc);
                for (int i = keep; i < (int) ns.size(); ++i) ns[(size_t) i].pitch = tmp[(size_t) i].pitch;
                if (rng.chance (0.25) && ns.size() > 2)
                {
                    const int sh = rng.chance (0.5) ? 12 : -12;
                    bool ok = true; for (auto& n : ns) if (n.pitch + sh > std::min (hardHi, hi + 3) || n.pitch + sh < lo) ok = false;
                    if (ok) for (auto& n : ns) n.pitch += sh;
                }
            }
            lastPitch = ns.back().pitch;
        }

        prevPhrase = ns; prevPhraseStart = sp.a + pickup;

        // pizzicato phrase flag (violin) / octave phrase flag (guitar)
        const bool pizzPhrase = (st.inst == I_Violin) && rng.chance (st.pizz * 0.8);
        const bool octavePhrase = (st.inst == I_Guitar) && rng.chance (st.octave);
        auto notes = assemble (ns, phraseIdx, (int) spans.size(), lastPhrase, activeEnd, I, pizzPhrase, octavePhrase);
        if (rw > 0.3) applyRefFeel (notes, ns, activeEnd);
        out.insert (out.end(), notes.begin(), notes.end());
        ++phraseIdx;
    }

    // piano: left-hand voicings / bass so the solo has harmonic support
    if (st.inst == I_Piano)
    {
        for (size_t ci = 0; ci < chords.size(); ++ci)
        {
            const ChordSpan& c = chords[ci];
            if (! rng.chance (0.6 + st.chordal * 0.4 + (afro ? 0.1 : 0.0))) continue;
            const auto iv = chordIntervals (c.q);
            const int pcs[3] = { (c.rootPc + iv[1]) % 12, (c.rootPc + iv[3]) % 12, (c.rootPc + 2) % 12 };
            int p = 50;
            std::vector<int> vo;
            for (int k = 0; k < 3; ++k) { while (p % 12 != pcs[k]) ++p; vo.push_back (p); ++p; }
            int bass = 36; while (bass % 12 != c.rootPc % 12) ++bass;
            const double len = std::min (c.end - c.start, afro ? 1.5 : 2.5);
            for (int vp : vo) { Note n; n.start = c.start; n.length = len * 0.95; n.pitch = vp; n.vel = (int) (st.vel * 0.62); n.voice = 1; n.weight = 1; out.push_back (n); }
            Note b; b.start = c.start; b.length = std::min (c.end - c.start, 1.0) * 0.9; b.pitch = bass; b.vel = (int) (st.vel * 0.72); b.voice = 1; b.weight = 1; out.push_back (b);
            if (afro && c.end - c.start >= 2.0)
            {
                Note b2 = b; b2.start = c.start + 2.0; b2.length = 0.5; b2.pitch = bass + (rng.chance (0.5) ? 7 : 0); out.push_back (b2);
            }
        }
    }
    return out;
}

//------------------------------------------------------------------------------
inline std::vector<Note> SoloGen::assemble (std::vector<SN>& ns, int, int, bool lastOfSolo, int activeEnd,
                                            double intens, bool pizzPhrase, bool octavePhrase)
{
    std::vector<Note> notes;
    const int n = (int) ns.size();
    int maxP = -1000, maxIdx = 0;
    for (int i = 0; i < n; ++i) if (ns[(size_t) i].pitch > maxP) { maxP = ns[(size_t) i].pitch; maxIdx = i; }
    const double center = (lo + hi) * 0.5;

    for (int i = 0; i < n; ++i)
    {
        const SN& s = ns[(size_t) i];
        Note nt;
        nt.start = stepBeat (s.step);
        const double nextBeat = (i + 1 < n) ? stepBeat (ns[(size_t) i + 1].step) : stepBeat (activeEnd);
        nt.length = std::max (0.1, nextBeat - nt.start);
        nt.pitch = s.pitch;
        nt.weight = s.w;
        double v = st.vel * (0.85 + 0.3 * intens) + (s.w - 0.4) * 18.0 * (0.5 + st.dyn)
                   + (s.pitch - center) / std::max (1.0, (double) (hi - lo)) * 8.0 * st.dyn + rng.f (-4, 4);
        if (afro && s.w >= 0.8) v += 6;
        if (rw > 0 && s.rLoud >= 0.f) v = v * (1.0 - 0.8 * rw) + st.vel * (0.60 + 0.80 * s.rLoud) * 0.8 * rw;   // the reference's accents
        if (gw > 0 && s.step % m.stepsPerBar < (int) gv.occ.size())
        {
            const int gs = s.step % m.stepsPerBar;
            v += gw * (gv.occ[(size_t) gs] >= 0.5f ? 6.0 + 6.0 * gv.str[(size_t) gs] : -3.0);   // accent where the loop accents
        }
        nt.vel = std::clamp ((int) std::lround (v), 28, 127);
        notes.push_back (nt);
    }

    // ---------------------------------------------------------------- phrase end hold / rests / gating are done in assignArts
    // mark peak via the 'bend' sign trick is avoided: handled inside assignArts using pitch comparison
    (void) maxIdx; (void) lastOfSolo;

    assignArts (notes, lastOfSolo ? 1 : 0, pizzPhrase);

    // ---------------------------------------------------------------- grace notes
    {
        std::vector<Note> graces;
        for (size_t i = 1; i < notes.size(); ++i)
        {
            Note& t = notes[i]; Note& pv = notes[i - 1];
            if (t.weight >= 0.55 && rng.chance (st.grace * 0.5) && (t.start - pv.start) >= 0.45 && t.art != A_Ghost)
            {
                Note gr = t; gr.start = t.start - 0.07; gr.length = 0.07;
                gr.pitch = t.pitch + (rng.chance (0.6) ? -1 : 2);
                gr.vel = std::max (20, (int) (t.vel * 0.7)); gr.weight = 0.1;
                gr.art = (t.art == A_Legato || t.art == A_HammerOn || t.art == A_PullOff || t.art == A_Slide) ? A_Sustain : t.art;
                gr.bend = 0; gr.vib = false;
                if (pv.start + pv.length > gr.start - 0.005) pv.length = std::max (0.06, gr.start - 0.005 - pv.start);
                graces.push_back (gr);
            }
        }
        notes.insert (notes.end(), graces.begin(), graces.end());
    }

    // ---------------------------------------------------------------- guitar octaves / chord hits, violin double stops
    std::vector<Note> extra;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const Note& t = notes[i];
        if (t.voice != 0) continue;
        const bool lastN = (i + 1 == notes.size());
        if (st.inst == I_Guitar && octavePhrase && t.pitch - 12 >= hardLo && t.length > 0.12)
        {
            Note o = t; o.pitch = t.pitch - 12; o.vel = std::max (20, t.vel - 10); o.voice = 0; o.bend = t.bend; extra.push_back (o);
        }
        if ((st.inst == I_Guitar || st.inst == I_Piano) && lastN && t.length >= 0.75 && rng.chance (st.chordal * 0.7))
        {
            const int ci = chordIndexAtBeat (t.start);
            const ChordSpan& c = chords[(size_t) ci];
            int added = 0;
            const size_t before = extra.size();
            for (int p = t.pitch - 3; p > t.pitch - 14 && added < 2; --p)
                if (inMask (c.toneMask, p)) { Note h = t; h.pitch = p; h.vel = std::max (20, t.vel - 12); h.art = A_Sustain; h.bend = 0; h.vib = false; h.voice = 1; extra.push_back (h); ++added; p -= 2; }
            if (st.inst == I_Guitar && extra.size() > before) notes[i].art = A_Strum;
        }
        if (st.inst == I_Violin && t.length >= 0.9 && t.art != A_Pizz && rng.chance (st.dbl))
        {
            const ChordSpan& c = chords[(size_t) chordIndexAtBeat (t.start)];
            int iv = rng.pick (std::vector<int> { 3, 4, 5, 8, 9 });
            int p = t.pitch - iv;
            for (int d = 0; d < 3; ++d) { if (inMask (c.scaleMask, p - d)) { p -= d; break; } }
            if (p >= hardLo) { Note o = t; o.pitch = p; o.vel = std::max (20, t.vel - 8); o.bend = 0; extra.push_back (o); }
        }
    }
    notes.insert (notes.end(), extra.begin(), extra.end());
    return notes;
}

//------------------------------------------------------------------------------
inline void SoloGen::assignArts (std::vector<Note>& ns, int lastOfSolo, bool pizzPhrase)
{
    const int n = (int) ns.size();
    if (n == 0) return;
    int maxP = -1000; for (auto& x : ns) maxP = std::max (maxP, x.pitch);
    const int inst = st.inst;
    const bool wind = isWind (inst);
    const double rangeMid = (lo + hi) * 0.5;
    const std::vector<int>& ks = artList (inst);
    (void) ks;

    for (int i = 0; i < n; ++i)
    {
        Note& t = ns[(size_t) i];
        const bool first = (i == 0), last = (i == n - 1);
        const Note* pv = first ? nullptr : &ns[(size_t) i - 1];
        const double gap = pv ? t.start - (pv->start + pv->length) : 9.0;
        const double gapToPrevStart = pv ? t.start - pv->start : 9.0;
        const bool conn = pv && gapToPrevStart <= 0.55;       // melodically connected to previous note
        const int interval = pv ? t.pitch - pv->pitch : 0;
        const double dur = t.length;                           // = room until next onset (before gating)
        const bool peak = (t.pitch == maxP) && n > 2;
        int art = A_Sustain;
        (void) gap;

        // ------------------------------------------------ base articulation: connected stepwise motion is slurred
        if (conn && rng.chance (st.legato)) art = (inst == I_Piano) ? A_Sustain : A_Legato;
        if (pizzPhrase) art = A_Pizz;

        if (dur <= 0.3 && t.weight < 0.5 && rng.chance (st.stacc)) art = (inst == I_Piano || inst == I_Guitar || wind || inst == I_Violin) ? A_Staccato : art;
        if (afro && t.weight >= 0.8 && dur <= 0.55 && rng.chance (0.3)) art = A_Marcato;

        switch (inst)
        {
            case I_Guitar:
            {
                if (conn && std::abs (interval) >= 1 && std::abs (interval) <= 2)
                {
                    if (interval > 0 && rng.chance (st.hammer)) art = A_HammerOn;
                    if (interval < 0 && rng.chance (st.pull))   art = A_PullOff;
                }
                if (conn && std::abs (interval) >= 2 && std::abs (interval) <= 7 && rng.chance (st.slide)) art = A_Slide;
                if (dur >= 0.75 && rng.chance (st.bend)) { art = A_Bend; t.bend = rng.chance (0.4) ? 1 : 2; }
                if (dur >= 1.0 && (art == A_Sustain || art == A_Legato) && rng.chance (st.vib + 0.15)) art = A_Vibrato;
                if (dur <= 0.3 && t.weight < 0.3 && rng.chance (st.ghost)) { art = A_Ghost; t.vel = std::max (24, t.vel - 30); }
                if (afro && pv && interval == 0 && dur <= 0.3 && rng.chance (0.5)) art = A_PalmMute;
                if (t.pitch >= 76 && last && rng.chance (st.harm)) art = A_Harmonic;
                break;
            }
            case I_Piano:
            {
                if (dur <= 0.3 && t.weight < 0.3 && rng.chance (st.ghost)) { art = A_Ghost; t.vel = std::max (24, t.vel - 28); }
                if (dur >= 1.5 && rng.chance (st.trill)) art = A_Trill;
                else if (dur >= 2.0 && rng.chance (st.trem)) art = A_Tremolo;
                if (first && rng.chance (st.gliss * 0.4) && t.pitch >= 72) art = A_Gliss;
                if (dur >= 1.0 && rng.chance (0.04 + st.chordal * 0.12)) art = A_Arpeggio;
                break;
            }
            case I_Sax: case I_Trumpet:
            {
                if (mutedSolo && inst == I_Trumpet) art = A_Mute;
                if (dur <= 0.3 && t.weight < 0.3 && rng.chance (st.ghost)) { art = A_Ghost; t.vel = std::max (24, t.vel - 30); }
                if (dur >= 1.0 && rng.chance (st.growl * (peak ? 2.2 : 1.0))) art = A_Growl;
                else if (dur >= 1.0 && t.pitch >= rangeMid && rng.chance (st.flutter)) art = A_Flutter;
                else if (dur >= 1.0 && rng.chance (st.shake)) art = A_Shake;
                if ((first || (pv && gapToPrevStart > 1.2)) && dur >= 0.5 && rng.chance (st.scoop)) art = A_Scoop;
                if (last && dur >= 0.75 && art != A_Growl)
                {
                    if (rng.chance (std::min (0.9, st.fall * 2.2))) art = A_Fall;
                    else if (rng.chance (std::min (0.9, st.doit * 2.0))) art = A_Doit;
                }
                if (inst == I_Sax && t.pitch < rangeMid - 2 && t.vel < st.vel && rng.chance (st.subtone)) art = A_Subtone;
                if (dur >= 1.5 && rng.chance (std::max (0.04f, st.trill))) art = A_Trill;
                break;
            }
            case I_Violin:
            {
                if (pizzPhrase) break;
                if (conn && std::abs (interval) >= 2 && std::abs (interval) <= 7 && rng.chance (st.gliss)) art = A_Gliss;
                if (dur >= 1.5 && rng.chance (std::max (0.05f, st.trem * 1.5f))) art = A_Tremolo;
                if (dur >= 1.5 && rng.chance (st.trill)) art = A_Trill;
                if (dur <= 0.3 && t.weight < 0.5 && rng.chance (0.04 + st.stacc * 0.1)) art = A_ColLegno;
                if (t.pitch >= 88 && rng.chance (st.harm)) art = A_Harmonic;
                if (dur >= 0.75 && rng.chance (st.bend)) { art = A_Bend; t.bend = rng.chance (0.5) ? 1 : 2; }
                if (dur >= 1.0 && (art == A_Sustain || art == A_Legato) && rng.chance (st.vib)) art = A_Vibrato;
                if ((first || (pv && gapToPrevStart > 1.2)) && dur >= 0.5 && rng.chance (st.scoop)) art = A_Scoop;
                if (last && dur >= 0.75 && rng.chance (std::min (0.9, st.fall * 2.0))) art = A_Fall;
                break;
            }
        }

        // map to what the instrument really has; bend only if the instrument can bend
        if (art == A_Bend && artIndex (inst, A_Bend) < 0) { art = A_Legato; t.bend = 0; }
        if (artIndex (inst, art) < 0 && art != A_Bend) art = artMapped (inst, art);
        t.art = art;
        t.vib = (wind || inst == I_Violin) && dur >= 0.75 && rng.chance (0.35 + st.vib * 0.8);
        if (art == A_Vibrato) t.vib = true;

        // ------------------------------------------------ length / gating
        double len = dur;
        double gate = 0.92;
        switch (art)
        {
            case A_Staccato: gate = 0.42; break;
            case A_ColLegno: gate = 0.3; break;
            case A_Marcato:  gate = 0.55; break;
            case A_Ghost:    gate = 0.22; break;
            case A_PalmMute: gate = 0.40; break;
            case A_Pizz:     gate = 0.45; break;
            case A_Legato: case A_HammerOn: case A_PullOff: case A_Slide: case A_Gliss: gate = 1.0; break;
            default: gate = (inst == I_Guitar) ? 0.97 : 0.93; break;
        }
        if (last) { len = std::min (dur, 4.0); gate = (art == A_Fall || art == A_Doit || art == A_Growl || art == A_Shake) ? 1.0 : 0.97; }
        else if (dur > 1.5) len = dur * rng.f (0.55, 0.95);
        len *= gate;
        if ((art == A_Legato || art == A_HammerOn || art == A_PullOff || art == A_Slide || art == A_Gliss) && ! last && dur < 1.2) len += 0.03;   // slight overlap
        if (wind && art == A_Fall) len = std::max (len, std::min (dur, 0.75));
        t.length = std::max (0.07, len);
    }
    (void) lastOfSolo;
}

//==============================================================================
// Accompaniment generator
inline std::vector<Note> generateComp (const Settings& S, const Style& st, const Meter& m, Rng& rng, const std::vector<ChordSpan>& chords)
{
    std::vector<Note> out;
    const GenreInfo& g = genreInfo (S.genre);
    const bool afro = g.afro;
    const int inst = st.inst;
    const double swingAmt = g.fixedSwing >= 0 ? std::clamp (g.fixedSwing + st.swing * 0.1, 0.0, 1.0) : st.swing;
    const int spb = m.stepsPerBar;

    auto stepBeat = [&] (int absStep)
    {
        const int bar = absStep / spb, within = absStep % spb;
        double b = bar * m.barBeats + within * 0.25;
        if (! m.compound && swingAmt > 0) { const int r = within % 4; if (r == 2) b += swingAmt / 6.0; else if (r == 3) b += swingAmt / 12.0; }
        return b;
    };
    auto chordAt = [&] (double beat) -> const ChordSpan&
    {
        for (auto& c : chords) if (beat + 1e-6 >= c.start && beat < c.end - 1e-6) return c;
        return chords.back();
    };
    auto voicing = [&] (const ChordSpan& c, int low, int count)
    {
        const auto iv = chordIntervals (c.q);
        const int pcs[4] = { (c.rootPc + iv[1]) % 12, (c.rootPc + iv[3]) % 12, (c.rootPc + iv[2]) % 12, (c.rootPc + 2) % 12 };
        std::vector<int> res; int p = low;
        for (int k = 0; k < count && k < 4; ++k) { while (p % 12 != pcs[k]) ++p; res.push_back (p); ++p; }
        return res;
    };
    auto bassNote = [&] (const ChordSpan& c) { int p = 36; while (p % 12 != c.rootPc % 12) ++p; return p; };

    // choose rhythm cells
    std::vector<std::string> bank;
    if (inst == I_Violin || (detail::isWind (inst) && (S.genre == G_Ballad)))
        bank = { "x...............", "x.......x.......", "x.......x...x..." };
    else if (S.genre == G_Swing)
    {
        if (inst == I_Guitar) bank = { "x...x...x...x..." };
        else bank = { "x.....x.........", "..x.....x.....x.", "x.......x...x...", ".....x.....x....", "x.....x.x......." };
    }
    else bank = detail::cellsFor (S.genre);
    if (inst == I_Piano && afro && S.genre != G_Amapiano) bank = { "x..x..x...x.x...", "..x..x..x..x..x.", "x.....x.x.x.....", ".x.x..x.x.x..x.x", "x..x.x..x..x.x.." };
    if (inst == I_Guitar && S.genre == G_Highlife) bank = { "x.x.x.x.x.x.x.x." };
    if (inst == I_Guitar && S.genre == G_Ballad)   bank = { "x.x.x.x.x.x.x.x." };

    const std::string cellA = rng.pick (bank), cellB = rng.pick (bank);

    for (int bar = 0; bar < S.bars; ++bar)
    {
        const std::string& cell = (bar % 4 == 3 && rng.chance (0.5)) ? cellB : cellA;
        const bool scratchGuitar = (inst == I_Guitar && afro && S.genre != G_Highlife);
        for (int s = 0; s < spb; ++s)
        {
            const bool hit = (m.simpleFour ? (cell[(size_t) s] == 'x') : (m.weight[(size_t) s] >= 0.6 || (s % 2 == 0 && rng.chance (0.3))));
            if (! hit && ! scratchGuitar) continue;
            const int abs = bar * spb + s;
            const double beat = stepBeat (abs);
            if (beat >= S.bars * m.barBeats - 1e-6) continue;
            const ChordSpan& c = chordAt (beat);
            const double w = m.weight[(size_t) s];
            // next hit distance for durations
            int nextS = s + 1; while (nextS < spb && m.simpleFour && cell[(size_t) std::min (nextS, 15)] != 'x') ++nextS;
            const double room = std::max (0.25, (nextS - s) * 0.25);
            int vel = std::clamp ((int) std::lround (st.vel * 0.85 + (w - 0.4) * 14 + rng.f (-4, 4)), 30, 120);

            auto push = [&] (int pitch, double len, int v, int art)
            {
                Note n; n.start = beat; n.length = len; n.pitch = pitch; n.vel = std::clamp (v, 1, 127); n.art = art; n.track = 1; n.weight = w; out.push_back (n);
            };
            
            if (inst == I_Piano)
            {
                const bool pad = (S.genre == G_Ballad) || (!afro && S.genre != G_Swing);
                const auto vo = voicing (c, pad ? 53 : 55, 3 + (rng.chance (0.4) ? 1 : 0));
                const double len = pad ? std::min (room * 0.95, 4.0) : std::min (0.22 + (afro ? 0.0 : 0.15), room * 0.9);
                const int art = pad ? A_Sustain : (w >= 0.8 ? A_Marcato : A_Staccato);
                int k = 0;
                for (int p : vo) { push (p, len, vel - 6 + (k == (int) vo.size() - 1 ? 4 : 0), art); ++k; }
                if (s == 0) push (bassNote (c), std::min (room * 0.95, 1.5), vel - 10, A_Sustain);
            }
            else if (inst == I_Guitar)
            {
                if (scratchGuitar)
                {
                    const auto vo = voicing (c, 52, 3);
                    const bool acc = m.simpleFour ? (cell[(size_t) s] == 'x') : hit;
                    for (int p : vo) push (p, acc ? 0.2 : 0.09, acc ? vel : std::max (20, vel - 38), acc ? A_Staccato : A_Ghost);
                }
                else if (S.genre == G_Highlife || S.genre == G_Ballad)
                {
                    // arpeggiated picking: root, 5th, 3rd, 5th
                    const auto vo = voicing (c, 52, 3);
                    static const int seq[4] = { 0, 2, 1, 2 };
                    const int idx = seq[(s / 2) % 4];
                    push (vo[(size_t) std::min ((int) vo.size() - 1, idx)] , 0.45, vel - 8, A_Sustain);
                }
                else if (S.genre == G_Rock)
                {
                    int root = 40; while (root % 12 != c.rootPc % 12) ++root;
                    for (int p : { root, root + 7, root + 12 }) push (p, std::min (0.5, room * 0.8), vel, A_Strum);
                }
                else
                {
                    const auto vo = voicing (c, 52, 3);
                    for (int p : vo) push (p, std::min (0.45, room * 0.6), vel - 4, A_Staccato);
                }
            }
            else if (inst == I_Violin)
            {
                const auto vo = voicing (c, 60, 3);
                for (int p : vo) push (p, std::min (room * 0.98, 4.0), vel - 12, A_Legato);
            }
            else // sax / trumpet : stabs and pads
            {
                const auto iv = chordIntervals (c.q);
                const int pcs[3] = { (c.rootPc + iv[1]) % 12, (c.rootPc + iv[2]) % 12, (c.rootPc + iv[3]) % 12 };
                const Range r = instRange (inst, st.variant);
                const int center = (r.lo + r.hi) / 2;
                int p = nearestPitchWithPc (center + rng.i (-3, 4), pcs[rng.i (0, 2)]);
                if (S.genre == G_Ballad) push (p, std::min (room * 0.98, 4.0), vel - 14, A_Sustain);
                else push (p, afro ? 0.22 : 0.3, vel + 4, (w >= 0.6) ? A_Marcato : A_Staccato);
            }
        }
    }
    return out;
}

} // namespace detail

inline std::vector<ChordSpan> Generator::buildChords (const Settings& s, const Meter& m, Rng& rng, std::string& progName)
{
    const int fam = scaleFamily (s.scale);
    std::vector<ProgChord> def;

    if (s.harmony == 2 && ! s.chordText.empty())
    {
        std::vector<ParsedChord> pc; std::string err;
        if (parseChordText (s.chordText, pc, err))
        {
            for (auto& c : pc) def.push_back ({ ((c.rootPc - s.keyRoot) % 12 + 12) % 12, c.q, c.bars });
            progName = "Your chords";
        }
    }
    if (def.empty() && s.harmony == 1)
    {
        std::vector<const Prog*> cand;
        for (auto& p : allProgressions()) if (p.genre == s.genre && p.family == fam) cand.push_back (&p);
        if (cand.empty()) for (auto& p : allProgressions()) if (p.genre == s.genre) cand.push_back (&p);
        if (cand.empty()) for (auto& p : allProgressions()) if (p.family == fam) cand.push_back (&p);
        if (cand.empty()) for (auto& p : allProgressions()) cand.push_back (&p);
        const Prog& pr = *cand[(size_t) rng.i (0, (int) cand.size() - 1)];
        def = pr.chords; progName = pr.name;
    }
    if (def.empty())
    {
        // key vamp: one chord for the whole solo - the safest thing to play over almost any groove in that key
        const bool jazzy = (s.genre == G_Swing || s.genre == G_Ballad || s.genre == G_AfroJazz || s.genre == G_Bossa || s.genre == G_Samba || s.genre == G_Gospel);
        const bool triad = (s.genre == G_Pop || s.genre == G_Rock || s.genre == G_Flamenco || s.genre == G_Juju || s.genre == G_Soukous || s.genre == G_Township);
        const bool nine  = (s.genre == G_Amapiano);
        Quality q;
        if (fam == 1)      q = nine ? Quality::Min9 : (triad ? Quality::Min : Quality::Min7);
        else if (fam == 2) q = nine ? Quality::Dom9 : (triad && s.genre == G_Flamenco ? Quality::Maj : Quality::Dom7);
        else               q = nine ? Quality::Maj9 : (triad ? Quality::Maj : (jazzy ? Quality::Maj7 : Quality::Dom7));
        def.push_back ({ 0, q, 4 });
        progName = std::string ("Key vamp: ") + kNoteNames[s.keyRoot % 12] + qualitySuffix (q);
    }

    auto mkSpan = [&] (double a, double b, int degree, Quality q, int mode = -1)
    {
        ChordSpan sp;
        sp.start = a; sp.end = b; sp.degree = ((degree % 12) + 12) % 12; sp.q = q;
        sp.rootPc = (s.keyRoot + sp.degree) % 12;
        sp.scaleMask = chordScaleFor (s.keyRoot, s.scale, sp.degree, q);
        if (mode >= 0 && mode < M_Count) sp.scaleMask = modeMask (sp.rootPc, mode);
        sp.toneMask = chordToneMaskFor (sp.rootPc, q);
        sp.name = chordName (sp.rootPc, q);
        return sp;
    };

    std::vector<ChordSpan> res;
    const double total = s.bars * m.barBeats;
    double t = 0;
    while (t < total - 1e-6)
    {
        for (auto& c : def)
        {
            ChordSpan sp = mkSpan (t, std::min (total, t + c.bars * m.barBeats), c.degree, c.q);
            res.push_back (sp);
            t = sp.end;
            if (t >= total - 1e-6) break;
        }
    }

    // chords picked bar by bar in the editor replace whatever the harmony mode produced for that bar
    bool any = false;
    for (int b = 0; b < s.bars && b < (int) s.barChords.size(); ++b) if (s.barChords[(size_t) b].set) any = true;
    if (any)
    {
        std::vector<ChordSpan> out;
        for (int b = 0; b < s.bars; ++b)
        {
            const double b0 = b * m.barBeats, b1 = (b + 1) * m.barBeats;
            if (b < (int) s.barChords.size() && s.barChords[(size_t) b].set)
            {
                out.push_back (mkSpan (b0, b1, s.barChords[(size_t) b].degree, s.barChords[(size_t) b].q, s.barChords[(size_t) b].mode));
                continue;
            }
            for (auto& sp : res)
            {
                const double a = std::max (b0, sp.start), e = std::min (b1, sp.end);
                if (e - a > 1e-6) { ChordSpan c = sp; c.start = a; c.end = e; out.push_back (c); }
            }
        }
        // merge neighbours that are the same chord
        std::vector<ChordSpan> merged;
        for (auto& c : out)
        {
            if (! merged.empty() && merged.back().rootPc == c.rootPc && merged.back().q == c.q && std::abs (merged.back().end - c.start) < 1e-6) merged.back().end = c.end;
            else merged.push_back (c);
        }
        res = merged;
        progName = "Custom chords";
    }
    return res;
}

//==============================================================================
namespace detail
{
/** sustained block-chord pad (track 2), one strike per bar and per chord change */
inline std::vector<Note> generateChordPad (const Settings& S, const Meter& m, const std::vector<ChordSpan>& chords, int fromBar, int toBar)
{
    std::vector<Note> out;
    for (auto& c : chords)
    {
        const auto iv = chordIntervals (c.q);
        const int pcs[4] = { c.rootPc % 12, (c.rootPc + iv[1]) % 12, (c.rootPc + iv[3]) % 12, (c.rootPc + iv[2]) % 12 };
        double t0 = c.start;
        while (t0 < c.end - 1e-6)
        {
            const int bar = (int) std::floor (t0 / m.barBeats + 1e-6);
            const double t1 = std::min (c.end, (bar + 1) * m.barBeats);
            if (bar >= fromBar && bar < toBar)
            {
                int p = 50;
                for (int k = 0; k < 4; ++k)
                {
                    while (p % 12 != pcs[k]) ++p;
                    Note n; n.start = t0; n.length = (t1 - t0) * 0.97; n.pitch = p; n.vel = 62 + (k == 0 ? 6 : 0);
                    n.track = 2; n.voice = 1; n.weight = 1.0; out.push_back (n);
                    ++p;
                }
            }
            t0 = t1;
        }
    }
    (void) S;
    return out;
}
} // namespace detail

inline std::vector<ChordSpan> Generator::chordsFor (const Settings& s, std::string& progName)
{
    const Meter m = makeMeter (s.tsNum, s.tsDen);
    Rng crng ((s.chordSeed ? s.chordSeed : s.seed) * 2654435761ULL + 99991ULL);
    return buildChords (s, m, crng, progName);
}

inline Pattern Generator::makeShell (const Settings& sIn)
{
    Settings s = sIn;
    s.bars = std::clamp (s.bars, 1, 256);
    Pattern p; p.s = s;
    const Meter m = makeMeter (s.tsNum, s.tsDen);
    p.barBeats = m.barBeats; p.bars = s.bars; p.lengthBeats = s.bars * m.barBeats;
    p.styleName = styleFor (s.instrument, s.artist).name; p.genreName = genreInfo (s.genre).name;
    p.chords = chordsFor (s, p.progName);
    p.hasSolo = (s.instrument == I_Bass) || (s.mode == 0 || s.mode == 2); p.hasComp = (s.instrument != I_Bass) && (s.mode == 1 || s.mode == 2); p.hasChords = s.chordTrack;
    return p;
}

inline Pattern Generator::generate (const Settings& sIn)
{
    Settings s = sIn;
    s.bars = std::clamp (s.bars, 1, 256);
    Pattern p;
    p.s = s;
    Style st = styleFor (s.instrument, s.artist);
    // the Space and Repeat sliders nudge the artist's own tendencies
    const float sp = std::clamp (s.space, 0.f, 1.f) - 0.5f;
    st.rest = std::clamp (st.rest + sp * 0.7f, 0.f, 1.f);
    st.density = std::clamp (st.density * (1.f - sp * 0.7f), 0.1f, 1.f);
    st.motif = std::clamp (st.motif + (std::clamp (s.repeat, 0.f, 1.f) - 0.5f) * 0.6f, 0.f, 1.f);
    const Meter m = makeMeter (s.tsNum, s.tsDen);
    p.barBeats = m.barBeats; p.bars = s.bars; p.lengthBeats = s.bars * m.barBeats;
    p.styleName = st.name; p.genreName = genreInfo (s.genre).name;

    Rng rng (s.seed * 2654435761ULL + 12345);
    p.chords = chordsFor (s, p.progName);

    if (s.instrument == I_Bass)
    {
        auto notes = detail::generateBass (s, st, m, rng, p.chords);
        p.raw.insert (p.raw.end(), notes.begin(), notes.end());
        p.hasSolo = true;
    }
    else if (s.mode == 0 || s.mode == 2)
    {
        detail::SoloGen sg (s, st, m, rng, p.chords);
        auto notes = sg.run();
        for (auto& n : notes) n.track = 0;
        p.raw.insert (p.raw.end(), notes.begin(), notes.end());
        p.hasSolo = true;
    }
    if (s.instrument != I_Bass && (s.mode == 1 || s.mode == 2) && s.voice2 == 0)
    {
        auto notes = detail::generateComp (s, st, m, rng, p.chords);
        p.raw.insert (p.raw.end(), notes.begin(), notes.end());
        p.hasComp = true;
    }
    if (s.chordTrack)
    {
        auto pad = detail::generateChordPad (s, m, p.chords, 0, s.bars);
        p.raw.insert (p.raw.end(), pad.begin(), pad.end());
        p.hasChords = true;
    }
    // octave shift + range safety (the chord pad stays where it is)
    for (auto& n : p.raw) if (n.track != 2) n.pitch = std::clamp (n.pitch + 12 * s.octaveShift, 0, 127);
    if (p.hasSolo && s.voice2 != 0 && s.instrument != I_Bass) applySecondVoice (p, s, 12 * s.octaveShift);
    std::stable_sort (p.raw.begin(), p.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    for (auto& n : p.raw) n.id = p.nextId++;
    finalize (p, s);
    return p;
}

//==============================================================================
// Editing helpers (used by the piano roll and the per-bar chord menu)
inline void Generator::reharmonize (Pattern& p, const std::vector<ChordSpan>& oldChords, const std::vector<ChordSpan>& newChords, const Settings& s)
{
    const Meter m = makeMeter (s.tsNum, s.tsDen);
    auto at = [] (const std::vector<ChordSpan>& v, double beat) -> const ChordSpan&
    {
        for (auto& c : v) if (beat + 1e-6 >= c.start && beat < c.end - 1e-6) return c;
        return v.back();
    };
    std::vector<char> changed ((size_t) std::max (1, p.bars), 0);
    for (int b = 0; b < p.bars; ++b)
        for (double f : { 0.01, 0.5, 0.99 })
        {
            const double t = (b + f) * m.barBeats;
            const auto& a = at (oldChords, t); const auto& c = at (newChords, t);
            if (a.rootPc != c.rootPc || a.q != c.q || a.scaleMask != c.scaleMask) changed[(size_t) b] = 1;
        }
    const Range hr = instRange (s.instrument, styleFor (s.instrument, s.artist).variant);
    const int lo = std::max (0, hr.lo - 3 + 12 * s.octaveShift), hi = std::min (127, hr.hi + 4 + 12 * s.octaveShift);

    auto nearestIn = [&] (uint16_t mask, int pitch, int maxd) -> int
    {
        for (int d = 0; d <= maxd; ++d)
        {
            const bool upFirst = (d % 2 == 0);
            const int a = pitch + (upFirst ? d : -d), b = pitch + (upFirst ? -d : d);
            if (a >= lo && a <= hi && inMask (mask, a)) return a;
            if (b >= lo && b <= hi && inMask (mask, b)) return b;
        }
        return pitch;
    };

    // main line
    if (s.instrument == I_Bass) { detail::bassReharmonize (p, newChords, changed, s); for (auto& n : p.raw) if (n.track == 0 && n.voice == 0 && changed[(size_t) p.barOf (n.start)]) n.pitch = std::clamp (n.pitch + 12 * s.octaveShift, 0, 127); }
    else for (auto& n : p.raw)
    {
        if (n.track != 0 || n.voice != 0) continue;
        const int bar = p.barOf (n.start);
        if (! changed[(size_t) bar]) continue;
        const ChordSpan& c = at (newChords, n.start); const ChordSpan& o = at (oldChords, n.start);
        const bool strong = n.weight >= 0.55 || n.length >= 1.0;
        if (strong && ! inMask (c.toneMask, n.pitch)) n.pitch = nearestIn (c.toneMask, n.pitch, 6);
        else if (! inMask (c.scaleMask, n.pitch) && inMask (o.scaleMask, n.pitch)) n.pitch = nearestIn (c.scaleMask, n.pitch, 3);
    }
    // supporting notes: move stacks together onto the new chord tones without collisions
    struct Key { int track; double start; };
    std::vector<size_t> idx;
    for (size_t i = 0; i < p.raw.size(); ++i)
        if (((p.raw[i].track == 1 && s.voice2 == 0) || (p.raw[i].track == 0 && p.raw[i].voice == 1)) && changed[(size_t) p.barOf (p.raw[i].start)]) idx.push_back (i);
    std::sort (idx.begin(), idx.end(), [&] (size_t a, size_t b)
    {
        if (p.raw[a].track != p.raw[b].track) return p.raw[a].track < p.raw[b].track;
        if (std::abs (p.raw[a].start - p.raw[b].start) > 1e-6) return p.raw[a].start < p.raw[b].start;
        return p.raw[a].pitch < p.raw[b].pitch;
    });
    for (size_t k = 0; k < idx.size();)
    {
        size_t j = k; std::set<int> used;
        while (j < idx.size() && p.raw[idx[j]].track == p.raw[idx[k]].track && std::abs (p.raw[idx[j]].start - p.raw[idx[k]].start) < 1e-6) ++j;
        const ChordSpan& c = at (newChords, p.raw[idx[k]].start);
        for (size_t q = k; q < j; ++q)
        {
            Note& n = p.raw[idx[q]];
            int target;
            if (n.pitch < 48) { target = n.pitch; while (target % 12 != c.rootPc % 12) --target; if (n.pitch - target > 6) target += 12; }
            else
            {
                target = nearestIn (c.toneMask, n.pitch, 8);
                for (int guard = 0; used.count (target) && guard < 12; ++guard) target = nearestIn (c.toneMask, target + 1, 8);
            }
            used.insert (target); n.pitch = std::clamp (target, 0, 127);
        }
        k = j;
    }
    // chord pad: rebuild the changed bars
    p.chords = newChords;
    if (s.voice2 != 0 && p.hasSolo) applySecondVoice (p, s, 12 * s.octaveShift);     // the second voice is written again on the new chords
    if (p.hasChords)
    {
        p.raw.erase (std::remove_if (p.raw.begin(), p.raw.end(), [&] (const Note& n) { return n.track == 2 && changed[(size_t) p.barOf (n.start)]; }), p.raw.end());
        auto pad = detail::generateChordPad (s, m, p.chords, 0, p.bars);
        for (auto& n : pad) if (changed[(size_t) p.barOf (n.start)]) { n.id = p.nextId++; p.raw.push_back (n); }
    }
    std::stable_sort (p.raw.begin(), p.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
}

/** Replace the notes of bars [bar0, bar1) with freshly generated ones (same chords, new seed). */
inline void Generator::rerollRegion (Pattern& p, const Settings& s, int bar0, int bar1, uint64_t newSeed)
{
    Settings q = s;
    q.chordSeed = s.chordSeed ? s.chordSeed : s.seed;
    q.seed = newSeed; q.chordTrack = false; q.humanize = false;
    const Pattern np = generate (q);
    const double t0 = bar0 * p.barBeats, t1 = bar1 * p.barBeats;
    p.raw.erase (std::remove_if (p.raw.begin(), p.raw.end(), [&] (const Note& n) { return n.track != 2 && n.start >= t0 - 1e-6 && n.start < t1 - 1e-6; }), p.raw.end());
    for (auto n : np.raw) if (n.track != 2 && n.start >= t0 - 1e-6 && n.start < t1 - 1e-6) { n.id = p.nextId++; p.raw.push_back (n); }
    std::stable_sort (p.raw.begin(), p.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
}

} // namespace enochsolo

#include "Expression.h"
#include "Harmony.h"
#include "BassGen.h"
