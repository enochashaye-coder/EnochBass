#include "Driver.h"
#include "LogDrum.h"
// Enoch Bass - ornament realisation, humanisation and MIDI event building (header only)
// Included at the end of Generator.h
#pragma once
#include <map>

namespace enochsolo
{
/** v3.9: Note.voice of a key-switch note you drew in the piano roll (on one of the instrument's key-switch rows) */
constexpr int kKsVoice = 7;
namespace detail
{

inline bool isLegatoArt (int a)
{
    return a == A_Legato || a == A_HammerOn || a == A_PullOff || a == A_Slide || a == A_Gliss;
}

inline const ChordSpan& chordAtBeat (const Pattern& p, double beat)
{
    for (auto& c : p.chords) if (beat + 1e-6 >= c.start && beat < c.end - 1e-6) return c;
    return p.chords.back();
}

//------------------------------------------------------------------------------ ornaments written as real notes
inline void realizeOrnaments (Pattern& p, std::vector<Note>& notes, const Settings& s)
{
    std::vector<Note> add;
    const Style& st = styleFor (s.instrument, s.artist);
    for (auto& n : notes)
    {
        if (n.voice != 0 || n.track != 0) continue;
        const ChordSpan& c = chordAtBeat (p, n.start);
        if (n.art == A_Trill || n.art == A_Shake || n.art == A_Tremolo)
        {
            const double step = 0.125;
            const int count = std::max (2, (int) std::floor (n.length / step));
            int upper = inMask (c.scaleMask, n.pitch + 1) ? 1 : 2;
            if (n.art == A_Shake) upper = 3;
            const bool trem = (n.art == A_Tremolo);
            const Note base = n;
            n.art = (s.instrument == I_Piano) ? A_Sustain : A_Legato;
            n.length = step * 0.97;
            for (int k = 1; k < count; ++k)
            {
                Note x = base;
                x.start = base.start + k * step;
                x.length = step * 0.97;
                x.pitch = (k % 2 == 1 && ! trem) ? base.pitch + upper : base.pitch;
                x.vel = std::max (20, base.vel - (k % 2 == 1 ? 7 : 2));
                x.art = n.art; x.bend = 0; x.vib = false;
                add.push_back (x);
            }
        }
        else if (n.art == A_Gliss && s.instrument == I_Piano)
        {
            // rapid scale run that lands on the note
            std::vector<int> run;
            for (int pp = n.pitch - 1; pp > n.pitch - 16 && (int) run.size() < 7; --pp)
                if (inMask (c.scaleMask, pp)) run.push_back (pp);
            std::reverse (run.begin(), run.end());
            const double stp = 0.0625;
            double t = n.start - stp * (double) run.size();
            for (int pp : run)
            {
                Note x = n; x.start = std::max (0.0, t); x.length = stp * 1.1; x.pitch = pp;
                x.vel = std::max (20, n.vel - 14); x.art = A_Sustain; add.push_back (x); t += stp;
            }
            n.art = A_Sustain;
        }
    }
    (void) st;
    notes.insert (notes.end(), add.begin(), add.end());
}

//------------------------------------------------------------------------------ humanise
// Every random draw is tied to the note's own id, so editing one note never changes how the others are humanised.
inline uint64_t hashMix (uint64_t a, uint64_t b)
{
    uint64_t x = a * 0x9E3779B97F4A7C15ULL + b * 0xBF58476D1CE4E5B9ULL + 0x94D049BB133111EBULL;
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ULL; x ^= x >> 27; x *= 0x94D049BB133111EBULL; x ^= x >> 31;
    return x;
}
inline double hashUnit (uint64_t a, uint64_t b) { return (double) (hashMix (a, b) >> 11) / 9007199254740992.0; }
inline double smoothNoise (uint64_t seed, double x)
{
    const double f = std::floor (x), t = x - f, u = t * t * (3.0 - 2.0 * t);
    const double a = hashUnit (seed, (uint64_t) (int64_t) f) * 2.0 - 1.0, b = hashUnit (seed, (uint64_t) (int64_t) f + 1) * 2.0 - 1.0;
    return a + (b - a) * u;
}

/** v3.1 timing-feel presets: how the player sits on the grid. ms are at 0.5 "Feel"; swing is the long/short split of each pair. */
struct FeelPreset { const char* name; double jitter, laidMs, offbeatMs, swing16, swing8, accent24, styleLaid; };
inline const std::vector<FeelPreset>& feelPresets()
{
    static const std::vector<FeelPreset> v = {
        { "Style's own feel",             1.0,   0,  0, 0.50, 0.50, 0.0, 1.0 },
        { "Tight studio grid",            0.35,  0,  0, 0.50, 0.50, 0.0, 0.2 },
        { "Laid-back pocket",             0.9,  10,  6, 0.50, 0.50, 0.0, 1.0 },
        { "Pushed / ahead of the beat",   0.8,  -8, -3, 0.50, 0.50, 0.0, 0.0 },
        { "Afrobeats pocket",             0.8,   3,  7, 0.54, 0.50, 0.1, 0.8 },
        { "Amapiano swing",               0.7,   2,  5, 0.58, 0.50, 0.0, 0.6 },
        { "Gospel - behind the beat",     0.9,  14,  4, 0.53, 0.50, 0.5, 1.0 },
        { "Neo-soul drunk",               1.5,  16, 10, 0.60, 0.50, 0.2, 1.0 },
        { "Jazz swing (8ths)",            0.9,   4,  0, 0.50, 0.64, 0.4, 0.8 },
        { "Live and loose",               1.7,   2,  3, 0.51, 0.50, 0.2, 1.0 },
    };
    return v;
}
/** the grid shift (in beats) a feel preset gives a note that sits at 'start' */
inline double feelSwingShift (const FeelPreset& f, double start)
{
    const double inBeat = start - std::floor (start + 1e-9);
    if (f.swing8 > 0.5 + 1e-6 && std::abs (inBeat - 0.5) < 0.04) return (f.swing8 - 0.5);                // the off-beat 8th moves late
    if (f.swing16 > 0.5 + 1e-6)
    {
        const double in8 = inBeat - (inBeat >= 0.5 - 1e-6 ? 0.5 : 0.0);
        if (std::abs (in8 - 0.25) < 0.03) return (f.swing16 - 0.5) * 0.5;                             // the 2nd 16th of each 8th
    }
    return 0.0;
}

inline void humanizeNotes (Pattern& p, std::vector<Note>& notes, const Settings& s)
{
    const Style& st = styleFor (s.instrument, s.artist);
    const double msToBeats = s.bpm / 60000.0;
    const FeelPreset& fp = feelPresets()[(size_t) std::clamp (s.feel, 0, (int) feelPresets().size() - 1)];
    const double T = s.hTiming * fp.jitter, V = s.hVel, L = s.hLength, F = s.hFeel;
    const uint64_t hs = s.hSeed * 7919ULL + 17ULL;

    std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
    { return a.track != b.track ? a.track < b.track : a.start < b.start; });

    struct Cl { size_t a, b; int track; };
    std::vector<Cl> clusters;
    for (size_t i = 0; i < notes.size();)
    {
        size_t j = i;
        while (j < notes.size() && notes[j].track == notes[i].track && std::abs (notes[j].start - notes[i].start) < 1e-6) ++j;
        clusters.push_back ({ i, j, notes[i].track });
        i = j;
    }

    auto gauss = [] (uint64_t seed, uint64_t k)         // deterministic normal(0,1) from two hashes
    {
        const double u1 = std::max (1e-9, hashUnit (seed, k * 2 + 1)), u2 = hashUnit (seed, k * 2 + 2);
        return std::sqrt (-2.0 * std::log (u1)) * std::cos (6.283185307179586 * u2);
    };

    for (auto& cl : clusters)
    {
        const uint64_t nid = (uint64_t) std::max (1, notes[cl.a].id);
        const double drift = smoothNoise (hs + (uint64_t) cl.track * 131ULL, notes[cl.a].start * 0.35) * 1.4 * T;   // slow human tempo wander (ms)
        const double w = notes[cl.a].weight;
        double off = gauss (hs, nid) * 6.5 * T + drift;
        off += st.laid * fp.styleLaid * F * (w < 0.5 ? 1.0 : 0.45);         // laid back more on off-beats
        off += (fp.laidMs + (w < 0.5 ? fp.offbeatMs : 0.0)) * F * 2.0;        // the feel preset's pocket
        off = std::clamp (off, -28.0, 45.0);
        const double swingShift = feelSwingShift (fp, notes[cl.a].start);
        const double beatIn = notes[cl.a].start - std::floor (notes[cl.a].start + 1e-9);
        const int beatNo = (int) std::floor (notes[cl.a].start + 1e-9);
        const double accent = (fp.accent24 > 0 && beatIn < 0.03 && (beatNo % 2) == 1) ? fp.accent24 * 8.0 : 0.0;   // backbeat accent
        for (size_t k = cl.a; k < cl.b; ++k)
        {
            Note& n = notes[k];
            const uint64_t id = (uint64_t) std::max (1, n.id);
            double spread = 0;
            if (cl.b - cl.a > 1)
            {
                const double per = (n.track == 1) ? (s.instrument == I_Guitar ? 9.0 : 4.0) : 3.0;
                spread = (double) (k - cl.a) * per * (0.3 + T);
            }
            n.start = std::max (0.0, n.start + swingShift + (off + spread) * msToBeats);
            double v = n.vel + gauss (hs + 1, id) * 5.0 * V + (n.weight - 0.45) * 10.0 * V + accent;
            n.vel = std::clamp ((int) std::lround (v), 1, 127);
            const double sc = std::clamp (1.0 + gauss (hs + 2, id) * 0.07 * L, 0.8, 1.2);
            n.length = std::max (0.05, n.length * sc);
        }
    }

    // keep monophonic lines in order and tidy overlaps (voice 0 only)
    for (int track = 0; track < 3; ++track)
    {
        std::vector<Cl*> line;
        for (auto& cl : clusters) if (cl.track == track && notes[cl.a].voice == 0) line.push_back (&cl);
        for (size_t i = 1; i < line.size(); ++i)
        {
            const double prevStart = notes[line[i - 1]->a].start;
            const double curStart = notes[line[i]->a].start;
            if (curStart < prevStart + 0.03)
            {
                const double d = prevStart + 0.03 - curStart;
                for (size_t k = line[i]->a; k < line[i]->b; ++k) notes[k].start += d;
            }
        }
        for (size_t i = 0; i + 1 < line.size(); ++i)
        {
            const double nextStart = notes[line[i + 1]->a].start;
            for (size_t k = line[i]->a; k < line[i]->b; ++k)
            {
                Note& n = notes[k];
                const double ov = isLegatoArt (n.art) ? 0.035 : 0.0;
                if (n.start + n.length > nextStart + ov) n.length = std::max (0.04, nextStart + ov - n.start);
            }
        }
    }
    (void) p;
}

//------------------------------------------------------------------------------ events

//------------------------------------------------------------------------------ articulations that cannot be sent as key-switches
// Used when the target instrument ignores key-switches: the articulation is written into the notes themselves
// (length / velocity), and trills / shakes / tremolo are written as real notes.
inline void bakeArticulations (std::vector<Note>& notes)
{
    for (auto& n : notes)
    {
        if (n.track == 2) continue;
        switch (n.art)
        {
            case A_Staccato:  n.length = std::max (0.06, n.length * 0.45); break;
            case A_Marcato:   n.vel = std::min (127, n.vel + 12); n.length = std::max (0.06, n.length * 0.8); break;
            case A_Ghost:     n.vel = std::max (8, (int) (n.vel * 0.45)); n.length = std::max (0.05, n.length * 0.5); break;
            case A_PalmMute:  n.vel = (int) (n.vel * 0.85); n.length = std::max (0.06, n.length * 0.6); break;
            case A_Pizz:      n.length = std::max (0.06, n.length * 0.5); break;
            case A_ColLegno:  n.vel = (int) (n.vel * 0.8); n.length = std::max (0.05, n.length * 0.4); break;
            case A_Subtone:   n.vel = std::max (8, (int) (n.vel * 0.75)); break;
            case A_Flutter: case A_Growl: n.vel = std::min (127, n.vel + 6); break;
            default: break;
        }
    }
}

inline void buildEvents (const Pattern& p, const std::vector<Note>& notes, const Settings& s, std::vector<MidiEv>& out, bool userLanes, bool baked)
{
    out.clear();
    const Style& st = styleFor (s.instrument, s.artist);
    Rng rng (s.hSeed * 104729ULL + s.seed + 5);
    const int inst = s.instrument;
    const bool sustained = (inst == I_Sax || inst == I_Trumpet || inst == I_Violin);
    const int range = std::max (1, s.bendRange);
    const double ksLead = 0.03;
    const Library* L = s.lib.get();
    const int ccE = (L && L->inst == inst) ? L->ccExpr : 11, ccV = (L && L->inst == inst) ? L->ccVib : 1;

    auto bendVal = [&] (double semis) { return std::clamp ((int) std::lround (8192.0 + semis / range * 8192.0), 0, 16383); };
    auto addCC = [&] (double t, int track, int cc, int val)
    {
        if (! s.emitCC) return;
        MidiEv e; e.t = std::max (0.0, t); e.type = EV_CC; e.a = cc; e.b = std::clamp (val, 0, 127); e.track = track; e.ord = 1; out.push_back (e);
    };
    auto addBend = [&] (double t, int track, int v)
    {
        if (! s.emitBends) return;
        MidiEv e; e.t = std::max (0.0, t); e.type = EV_BEND; e.a = v; e.track = track; e.ord = 1; out.push_back (e);
    };
    auto ramp = [&] (double t0, double t1, int track, double s0, double s1, int steps)
    {
        for (int k = 0; k <= steps; ++k)
        {
            const double u = (double) k / steps;
            addBend (t0 + (t1 - t0) * u, track, bendVal (s0 + (s1 - s0) * u));
        }
    };

    for (int track = 0; track < 3; ++track)
    {
        std::vector<const Note*> all, line;
        for (auto& n : notes) if (n.track == track && ! n.keyPlayed) { all.push_back (&n); if (n.voice == 0) line.push_back (&n); }
        if (all.empty()) continue;
        std::stable_sort (line.begin(), line.end(), [] (const Note* a, const Note* b) { return a->start < b->start; });

        // ---------------- notes
        const bool ksLive = s.emitKS && (! s.ksGate || s.ksActive);
        // v3.9: a key-switch note you drew in the roll (on one of the instrument's key-switch rows) is sent at that key, held as drawn
        const Library* LK = (s.routed && track < 3 && s.routes[track].lib) ? s.routes[track].lib.get() : L;
        const int keyShift = (LK && LK->inst == inst && ! LK->standard) ? LK->transpose + ((s.routed && track < 3) ? s.routes[track].transpose : 0) : 0;
        // v3.12: an instrument whose key IS the sound for some articulations (EZbass E0 ghost tap): that note is sent as the key alone
        const Library* LR = (s.routed && track < 3 && s.routes[track].lib && s.routes[track].lib->inst == inst) ? s.routes[track].lib.get() : L;
        const bool perfLib = ksLive && track != 2 && LR && LR->inst == inst && ! LR->standard && ! LR->noKeySwitches;
        for (auto* n : all)
        {
            if (perfLib && n->voice == 0)
            {
                const LibArt* re = libFind (*LR, libResolve (*LR, n->art));
                if (re && re->when == KW_Replace && ! re->press.empty() && n->ksManual) continue;   // its key (the sound) is a note you edit
                if (re && re->when == KW_Replace && ! re->press.empty())
                {
                    const double bpmsR = std::max (20.0, s.bpm) / 60000.0;
                    for (auto& pr : re->press)
                    {
                        MidiEv on; on.t = n->start; on.type = EV_ON; on.a = std::clamp (pr.note, 0, 127);
                        on.b = std::clamp ((int) std::lround (pr.vel * std::clamp (n->vel, 1, 127) / 64.0), 20, 127);   // the note's own dynamics
                        on.track = track; on.ord = 2; on.ks = true; on.noteId = n->id; on.art = n->art;
                        MidiEv off = on; off.type = EV_OFF; off.b = 0; off.ord = 0;
                        off.t = n->start + std::max (0.01, re->pressMs > 0 ? std::min (re->pressMs * bpmsR, std::max (0.02, n->length)) : std::max (0.02, n->length));
                        out.push_back (on); out.push_back (off);
                    }
                    continue;
                }
            }
            if (n->voice == kKsVoice && ksLive && track != 2)
            {
                MidiEv on; on.t = n->start; on.type = EV_ON; on.a = std::clamp (n->pitch + keyShift, 0, 127); on.b = std::clamp (n->vel, 1, 127); on.track = track; on.ord = 1; on.ks = true; on.srcId = n->id; on.drawn = true;   // a key-switch you drew (the roll shows it as the note itself)
                MidiEv off = on; off.t = n->start + std::max (0.02, n->length); off.type = EV_OFF; off.b = 0; off.ord = 0;
                out.push_back (on); out.push_back (off);
                continue;
            }
            MidiEv on; on.t = n->start; on.type = EV_ON; on.a = std::clamp (n->pitch, 0, 127); on.b = std::clamp (n->vel, 1, 127); on.track = track; on.ord = 2; on.art = n->art; on.srcId = n->id; on.manualKs = n->ksManual;
            MidiEv off = on; off.t = n->start + std::max (0.02, n->length); off.type = EV_OFF; off.b = 0; off.ord = 0;
            out.push_back (on); out.push_back (off);
        }

        if (track == 2) continue;      // the chord pad is plain notes: no key-switches / expression

        // ---------------- key-switches (on change of articulation)
        // v3.8: a part routed to an instrument with its own mapping profile gets that profile's key-switches (not the main panel's)
        const Library* LT = (s.routed && track < 3 && s.routes[track].lib && s.routes[track].lib->inst == inst) ? s.routes[track].lib.get() : L;
        if (ksLive && LT && LT->inst == inst && ! LT->standard)
        {
            // instrument of choice: the library's own key-switch table. Each key goes down just before its note (about 20 ms at any
            // tempo) and is held until that note ends, the way a player holds a key-switch (EZbass: hold G0 while the note plays).
            if (! LT->noKeySwitches && ! LT->arts.empty())
            {
                // v3.12: every articulation is a technique with its own timing (Library.h: when / pressMs / preRollMs)
                const double bpms = std::max (20.0, s.bpm) / 60000.0;
                const double lead = std::clamp (LT->preRollMs * bpms, 0.002, 0.25);
                std::vector<MidiEv> ksOut;
                int last = -1;
                const Note* prevN = nullptr;
                for (auto* n : line)
                {
                    int art = libResolve (*LT, n->art);
                    const LibArt* e0 = libFind (*LT, art);
                    if (e0 && e0->when == KW_Replace) continue;                 // sent as the key alone (above); not a note to join
                    if (n->ksManual) { prevN = n; continue; }                   // its key-switches are notes you edit in the roll
                    // a joined technique (legato, hammer-on, pull-off, slide) needs a sounding note to join: without one it is played plain
                    // (the same reach as the driver's overlap: the previous note ends at most a quarter beat before this one)
                    if (driver::isLegatoArt (n->art) && (prevN == nullptr || n->start - (prevN->start + prevN->length) > 0.25 || prevN->pitch == n->pitch))
                        art = libResolve (*LT, A_Sustain);
                    prevN = n;
                    const LibArt* e = libFind (*LT, art);
                    if (! e) continue;
                    const double nEnd = n->start + std::max (0.02, n->length);
                    double t = 0, tOff = 0;
                    if (e->when == KW_During)
                    {
                        // pressed while the note sounds (EZbass F0 / G0: the held note slides away), never in its first 30 ms or last 80 ms
                        const double minIn = 30.0 * bpms, maxIn = std::max (minIn, std::max (0.02, n->length) - 80.0 * bpms);
                        t = n->start + std::clamp (std::max (0.02, n->length) * e->atFrac, minIn, maxIn);
                        tOff = std::min (nEnd - 0.002, t + (e->pressMs > 0 ? e->pressMs : 40.0) * bpms);
                        if (tOff <= t) tOff = t + 0.005;
                    }
                    else
                    {
                        t = std::max (0.0, n->start - lead);
                        tOff = (! e->hold && e->pressMs > 0) ? t + e->pressMs * bpms : std::max (t + 0.02, nEnd - 0.002);
                    }
                    if (e->oneShot || art != last || e->when == KW_During)
                    {
                        for (auto& pr : e->press)
                        {
                            MidiEv on; on.t = t; on.type = EV_ON; on.a = std::clamp (pr.note, 0, 127); on.b = std::clamp (pr.vel, 1, 127); on.track = track; on.ord = 1; on.ks = true; on.noteId = n->id;
                            MidiEv off = on; off.t = tOff; off.type = EV_OFF; off.b = 0; off.ord = 0;
                            ksOut.push_back (on); ksOut.push_back (off);
                        }
                        if (! e->oneShot && e->when == KW_Before) last = art;
                    }
                    if (e->latch)
                    {
                        // v3.14: a mode key (Efimov FX glissando) stays on until another key: the next note always sends its own key, and when
                        // no note follows within a beat the plain-note key goes down right after this one ends, so nothing later plays the FX
                        last = -2;
                        const Note* nx = nullptr;
                        for (auto* m : line) if (m->start > n->start + 1e-6) { nx = m; break; }
                        const LibArt* plain = libFind (*LT, libResolve (*LT, A_Sustain));
                        if (plain && plain != e && ! plain->press.empty() && (nx == nullptr || nx->start - nEnd > 1.0 || nx->ksManual))
                        {
                            // inside the clip: when the effect rings to the very end, the key goes down just before the clip ends
                            const double press = 30.0 * bpms;
                            double tc = nEnd + 0.01;
                            if (p.lengthBeats > 0 && tc + press > p.lengthBeats - 0.005) tc = p.lengthBeats - press - 0.01;
                            if (tc > n->start + 0.05)
                            for (auto& pr : plain->press)
                            {
                                MidiEv on; on.t = tc; on.type = EV_ON; on.a = std::clamp (pr.note, 0, 127); on.b = std::clamp (pr.vel, 1, 127); on.track = track; on.ord = 1; on.ks = true; on.noteId = -1;   // belongs to no note: it switches the mode off
                                MidiEv off = on; off.t = tc + press; off.type = EV_OFF; off.b = 0; off.ord = 0;
                                ksOut.push_back (on); ksOut.push_back (off);
                            }
                            last = libResolve (*LT, A_Sustain);
                        }
                    }
                }
                // the same key used again before its last use ended. A held key (EZbass G#0 legato) simply stays down through both notes,
                // the way a player keeps holding it through a run (v3.11); a short press lets go just before the new one goes down.
                std::stable_sort (ksOut.begin(), ksOut.end(), [] (const MidiEv& a, const MidiEv& b) { return a.t != b.t ? a.t < b.t : a.ord < b.ord; });
                std::array<bool, 128> holdK {}; holdK.fill (false);
                for (auto& a : LT->arts) if (a.hold) for (auto& pr : a.press) if (pr.note >= 0 && pr.note < 128) holdK[(size_t) pr.note] = true;
                std::vector<char> gone (ksOut.size(), 0);
                auto offOf = [&] (size_t from, int key, int id) -> int
                { for (size_t k = from; k < ksOut.size(); ++k) if (! gone[k] && ksOut[k].type == EV_OFF && ksOut[k].a == key && ksOut[k].noteId == id) return (int) k; return -1; };
                for (size_t i = 0; i < ksOut.size(); ++i)
                {
                    if (gone[i] || ksOut[i].type != EV_ON) continue;
                    const int key = ksOut[i].a;
                    const int myOff = offOf (i + 1, key, ksOut[i].noteId);
                    if (myOff < 0) continue;
                    // every later press of this key that comes while it is still down (by time: a merge can move the release later)
                    for (size_t j = i + 1; j < ksOut.size(); ++j)
                    {
                        if (gone[j] || ksOut[j].a != key || ksOut[j].type != EV_ON) continue;
                        if (ksOut[j].t >= ksOut[(size_t) myOff].t - 1e-9) break;      // released in time
                        if (ksOut[j].t - ksOut[i].t < 0.01)                          // the same key for a chord / double-stop: pressed once
                        {
                            const int nextOff = offOf (j + 1, key, ksOut[j].noteId);
                            if (nextOff >= 0) { ksOut[(size_t) myOff].t = std::max (ksOut[(size_t) myOff].t, ksOut[(size_t) nextOff].t); gone[(size_t) nextOff] = 1; }
                            gone[j] = 1;
                            continue;
                        }
                        if (holdK[(size_t) key])
                        {
                            const int nextOff = offOf (j + 1, key, ksOut[j].noteId);
                            if (nextOff >= 0) { ksOut[(size_t) myOff].t = std::max (ksOut[(size_t) myOff].t, ksOut[(size_t) nextOff].t); gone[(size_t) nextOff] = 1; }
                            gone[j] = 1;                                           // keep holding: no second press
                            continue;
                        }
                        ksOut[(size_t) myOff].t = std::max (ksOut[i].t + 0.005, ksOut[j].t - 0.004);
                        break;
                    }
                }
                // two different held keys never overlap: the earlier one lets go just before the next one goes down
                for (size_t i = 0; i < ksOut.size(); ++i)
                {
                    if (gone[i] || ksOut[i].type != EV_ON || ! holdK[(size_t) ksOut[i].a]) continue;
                    const int myOff = offOf (i + 1, ksOut[i].a, ksOut[i].noteId);
                    if (myOff < 0) continue;
                    for (size_t j = i + 1; j < ksOut.size(); ++j)
                    {
                        if (gone[j] || ksOut[j].type != EV_ON || ksOut[j].a == ksOut[i].a || ! holdK[(size_t) ksOut[j].a]) continue;
                        if (ksOut[j].t >= ksOut[(size_t) myOff].t - 1e-9) break;
                        ksOut[(size_t) myOff].t = std::max (ksOut[i].t + 0.005, ksOut[j].t - 0.002);
                        break;
                    }
                }
                { std::vector<MidiEv> kept; for (size_t i = 0; i < ksOut.size(); ++i) if (! gone[i]) kept.push_back (ksOut[i]); ksOut.swap (kept); }
                out.insert (out.end(), ksOut.begin(), ksOut.end());
            }
        }
        else if (ksLive)
        {
            int last = -1;
            for (auto* n : line)
            {
                const int art = artMapped (inst, n->art == A_Bend && artIndex (inst, A_Bend) < 0 ? A_Legato : n->art);
                const int idx = artIndex (inst, art);
                if (idx >= 0 && art != last)
                {
                    const double t = std::max (0.0, n->start - ksLead);
                    MidiEv on; on.t = t; on.type = EV_ON; on.a = s.ksBase + idx; on.b = 100; on.track = track; on.ord = 1; on.ks = true;
                    MidiEv off = on; off.t = t + 0.05; off.type = EV_OFF; off.b = 0; off.ord = 0;
                    out.push_back (on); out.push_back (off);
                    last = art;
                }
            }
        }

        // ---------------- initial controller state
        if (sustained) { addCC (0, track, ccE, 100); addCC (0, track, ccV, 12); }
        if (inst == I_Piano) addCC (0, track, 64, 0);
        addBend (0, track, 8192);

        // ---------------- expression (CC11 phrasing + CC1 vibrato) and pitch-bend gestures
        int lastE = -100;
        for (size_t i = 0; i < line.size(); ++i)
        {
            const Note& n = *line[i];
            if (i > 0 && std::abs (line[i - 1]->start - n.start) < 1e-6) continue;   // doubled notes share expression
            const double nextStart = (i + 1 < line.size()) ? line[i + 1]->start : n.start + n.length + 1.0;
            const double end = n.start + n.length;

            if (sustained)
            {
                const int e = std::clamp (52 + n.vel / 2, 40, 127);
                if (std::abs (e - lastE) >= 3) { addCC (n.start - 0.002, track, ccE, e); lastE = e; }
                if (n.length >= 1.0)
                {
                    addCC (n.start + n.length * 0.55, track, ccE, e + 8);
                    addCC (n.start + n.length * 0.95, track, ccE, e - 14);
                    lastE = e - 14;
                }
                if (n.vib || n.art == A_Vibrato)
                {
                    const int depth = std::clamp ((int) std::lround (30 + st.vib * 90 + hashUnit (s.hSeed * 3ULL + 7ULL, (uint64_t) std::max (1, n.id)) * 16.0 - 8.0), 20, 120);
                    addCC (n.start + 0.01, track, ccV, 12);
                    addCC (n.start + n.length * 0.35, track, ccV, depth / 2);
                    addCC (n.start + n.length * 0.7, track, ccV, depth);
                    addCC (end, track, ccV, 12);
                }
                else if (n.art == A_Shake) { addCC (n.start, track, ccV, 120); addCC (end, track, ccV, 12); }
                else if (baked && (n.art == A_Growl || n.art == A_Flutter)) { addCC (n.start, track, ccV, 110); addCC (end, track, ccV, 12); }
            }
            else if (inst == I_Guitar && (n.art == A_Vibrato))
            {
                addCC (n.start + n.length * 0.4, track, ccV, 70);
                addCC (end, track, ccV, 0);
            }

            // pitch-bend gestures (monophonic instruments only)
            if (inst != I_Piano && s.emitBends && ! (inst == I_Bass && L && L->inst == inst && ! L->standard))
            {
                bool touched = false;
                const double len = n.length;
                const double maxSemis = (double) range;
                switch (n.art)
                {
                    case A_Scoop:
                    {
                        const double depth = std::min (1.5, maxSemis);
                        ramp (n.start, n.start + std::min (0.2, len * 0.4), track, -depth, 0.0, 5); touched = true; break;
                    }
                    case A_Fall:
                    {
                        const double fl = std::min (len * 0.6, 0.6);
                        ramp (end - fl, end, track, 0.0, -maxSemis, 6); touched = true; break;
                    }
                    case A_Doit:
                    {
                        const double dl = std::min (len * 0.5, 0.5);
                        ramp (end - dl, end, track, 0.0, maxSemis, 6); touched = true; break;
                    }
                    case A_Bend:
                    {
                        const double semis = std::min ((double) std::max (1, n.bend), maxSemis);
                        const double t0 = n.start + 0.06;
                        ramp (t0, t0 + std::min (0.25, len * 0.35), track, 0.0, semis, 5); touched = true; break;
                    }
                    case A_Slide: case A_Gliss:
                    {
                        if (i > 0)
                        {
                            const int iv = n.pitch - line[i - 1]->pitch;
                            if (iv != 0 && std::abs (iv) <= range)
                            { ramp (n.start, n.start + 0.1, track, (double) -iv, 0.0, 4); touched = true; }
                        }
                        break;
                    }
                    default: break;
                }
                if (touched)
                {
                    const double t = std::max (n.start + 0.01, std::min (end + 0.02, nextStart - 0.002));
                    addBend (t, track, 8192);
                }
            }
        }

        // ---------------- piano pedal on chord changes
        if (inst == I_Piano && s.emitCC)
        {
            const bool afro = genreInfo (s.genre).afro;
            const double prob = st.pedal * (afro ? 0.5 : 1.0) + (baseGenre (s.genre) == G_Ballad ? 0.3 : 0.0);
            if (rng.chance (std::min (1.0, prob * 1.4)) || prob > 0.45)
            {
                for (auto& c : p.chords)
                {
                    addCC (c.start - 0.03, track, 64, 0);
                    addCC (c.start + 0.03, track, 64, 127);
                }
                addCC (p.lengthBeats, track, 64, 0);
            }
        }
    }

    // ---------------- drawn controller lanes replace the automatic data inside the regions they were drawn over
    if (userLanes && ! p.userRegions.empty())
    {
        auto laneMatches = [] (const MidiEv& e, int lane)
        {
            switch (lane)
            {
                case L_Bend: return e.type == EV_BEND;
                case L_Mod: return e.type == EV_CC && e.a == 1;
                case L_Aftertouch: return e.type == EV_AT;
                case L_Expression: return e.type == EV_CC && e.a == 11;
                default: return e.type == EV_CC && e.a == 64;
            }
        };
        out.erase (std::remove_if (out.begin(), out.end(), [&] (const MidiEv& e)
        {
            if (e.track != 0 || e.type == EV_ON || e.type == EV_OFF) return false;
            for (auto& r : p.userRegions) if (laneMatches (e, r.lane) && e.t >= r.t0 - 1e-6 && e.t <= r.t1 + 1e-6) return true;
            return false;
        }), out.end());
    }
    if (userLanes)
        for (auto& c : p.userPts)
        {
            MidiEv e; e.t = std::max (0.0, c.t); e.track = 0; e.ord = 1;
            switch (c.lane)
            {
                case L_Bend: e.type = EV_BEND; e.a = std::clamp (c.value, 0, 16383); break;
                case L_Mod: e.type = EV_CC; e.a = 1; e.b = std::clamp (c.value, 0, 127); break;
                case L_Aftertouch: e.type = EV_AT; e.a = std::clamp (c.value, 0, 127); break;
                case L_Expression: e.type = EV_CC; e.a = 11; e.b = std::clamp (c.value, 0, 127); break;
                default: e.type = EV_CC; e.a = 64; e.b = std::clamp (c.value, 0, 127); break;
            }
            out.push_back (e);
        }

    std::stable_sort (out.begin(), out.end(), [] (const MidiEv& a, const MidiEv& b)
    {
        if (std::abs (a.t - b.t) > 1e-9) return a.t < b.t;
        return a.ord < b.ord;
    });
}

} // namespace detail

//==============================================================================
namespace detail
{
/** v3.1 articulation intelligence for fretted instruments: the line is laid out on the neck like a player would (driver::solveStrings),
    and a hammer-on, pull-off or slide that would have to jump to another string becomes a plain legato note (a hand can't hammer across
    strings); a hammer-on / pull-off wider than the fretting hand's reach (4 frets) becomes a slide. Main line only. */
inline void fretCheck (std::vector<Note>& v, const Settings& s)
{
    const bool bass = (s.instrument == 5);
    if (! bass && s.instrument != I_Guitar) return;
    static const std::vector<int> gtr { 40, 45, 50, 55, 59, 64 }, bs4 { 28, 33, 38, 43 };
    std::vector<size_t> idx;
    for (size_t i = 0; i < v.size(); ++i) if (v[i].track == 0 && v[i].voice == 0) idx.push_back (i);
    std::stable_sort (idx.begin(), idx.end(), [&] (size_t a, size_t b) { return v[a].start < v[b].start; });
    if (idx.size() < 2) return;
    std::vector<int> pitch; std::vector<double> start;
    for (auto i : idx) { pitch.push_back (v[i].pitch); start.push_back (v[i].start); }
    const auto& tun = bass ? bs4 : gtr;
    std::vector<char> joined;
    for (auto i : idx) { const int a = v[i].art; joined.push_back ((char) (a == A_HammerOn || a == A_PullOff || a == A_Slide)); }
    const auto str = driver::solveStrings (pitch, start, tun, bass ? 20 : 22, &joined);
    for (size_t k = 1; k < idx.size(); ++k)
    {
        Note& n = v[idx[k]];
        if (n.art != A_HammerOn && n.art != A_PullOff && n.art != A_Slide) continue;
        if (str[k] < 0 || str[k - 1] < 0) continue;
        if (str[k] != str[k - 1]) { n.art = A_Legato; continue; }
        if ((n.art == A_HammerOn || n.art == A_PullOff) && std::abs (pitch[k] - pitch[k - 1]) > 4) n.art = A_Slide;
    }
}
}

inline void Generator::finalize (Pattern& p, const Settings& sIn)
{
    Settings s = sIn;
    p.s = s;
    // muted notes stay in the piano roll but never sound
    auto prepare = [&] (bool realize)
    {
        std::vector<Note> v, keyNotes; v.reserve (p.raw.size());
        // v3.9: key-switches you drew stay exactly where you put them (never humanized, never turned into ornaments)
        for (auto& n : p.raw) if (! n.mute) (n.voice == kKsVoice ? keyNotes : v).push_back (n);
        if (realize) detail::realizeOrnaments (p, v, s);
        detail::fretCheck (v, s);                                   // v3.1: no hammer-ons across strings
        if (s.humanize) detail::humanizeNotes (p, v, s);
        v.insert (v.end(), keyNotes.begin(), keyNotes.end());
        std::stable_sort (v.begin(), v.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        for (auto& n : v) { n.pitch = std::clamp (n.pitch, 0, 127); n.vel = std::clamp (n.vel, 1, 127); }
        // v3.11: a note never rings into the next note of the same pitch on its part (that would re-strike a key still held)
        {
            std::map<long, size_t> lastOf;
            for (size_t i = 0; i < v.size(); ++i)
            {
                if (v[i].voice == kKsVoice) continue;
                const long key = (long) v[i].track * 128 + v[i].pitch;
                auto it = lastOf.find (key);
                if (it != lastOf.end())
                {
                    auto& pn = v[it->second];
                    if (pn.start + pn.length > v[i].start - 0.004) pn.length = std::max (0.02, v[i].start - pn.start - 0.004);
                }
                lastOf[key] = i;
            }
        }
        return v;
    };
    p.notes = prepare (s.realizeOrnaments);

    // what the test player plays: always the full articulated version, so you hear what the notes are meant to be
    { Settings sp = s; sp.emitKS = sp.emitBends = sp.emitCC = true; detail::buildEvents (p, p.notes, sp, p.playEvents, true, false); }

    // what gets exported
    Settings se = s; se.ksGate = true;            // v3.9: the MIDI that leaves Enoch only has key-switches for an external instrument
    if (s.exportMode == 0) detail::buildEvents (p, p.notes, se, p.events, true, false);
    else
    {
        se.emitKS = false;
        if (s.exportMode == 2) { se.emitBends = false; se.emitCC = false; }
        auto ne = prepare (true);
        detail::bakeArticulations (ne);
        detail::buildEvents (p, ne, se, p.events, s.exportMode != 2, true);
    }
    // v3.0: the external instrument driver (channel, profile, mode keys, legato overlap, strings, repetition keys, range, velocity curve)
    if (s.routed)
    {
        if (s.dupToTrack1)
        {
            // v3.1: the log drum part is its own player, locked to the bass line (Engine/LogDrum.h)
            std::vector<double> bassOn;
            for (auto& e : p.events) if (e.track == 0 && ! e.ks && e.type == EV_ON) bassOn.push_back (e.t);
            p.events.erase (std::remove_if (p.events.begin(), p.events.end(), [] (const MidiEv& e) { return e.track == 1; }), p.events.end());
            const auto copy = logdrum::generate (p, s, bassOn);
            p.events.insert (p.events.end(), copy.begin(), copy.end());
            if (! copy.empty()) p.hasComp = true;           // the log drum part gets its own track in the exported file
        }
        std::array<driver::RouteSpec, 3> rs;
        for (int i = 0; i < 3; ++i)
        {
            const auto& rc = s.routes[i];
            rs[(size_t) i].on = rc.on;
            rs[(size_t) i].channel = rc.channel >= 0 ? rc.channel : i;
            rs[(size_t) i].lib = rc.lib ? rc.lib.get() : (i == 0 ? s.lib.get() : nullptr);
            rs[(size_t) i].transpose = rc.transpose;
            rs[(size_t) i].program = rc.program;
            rs[(size_t) i].repMode = s.famLock[11] == 1 ? 0 : (s.famLock[11] == 2 ? 2 : 1); rs[(size_t) i].repAmt = s.famAmt[11];
        }
        driver::apply (p.events, rs, s.bpm, s.instrument, s.exportMode == 0 && s.emitKS && s.ksActive);
    }
    else if (s.lib && ! s.lib->standard && s.lib->inst == s.instrument)
    {
        // no routing set: the instrument of choice still gets its octave, range, legato overlap, string and repetition keys
        // (the same file as a routed part, on the usual channels 1 / 2 / 3)
        std::array<driver::RouteSpec, 3> rs;
        for (int i = 0; i < 3; ++i) { rs[(size_t) i].channel = i; rs[(size_t) i].lib = i == 0 ? s.lib.get() : nullptr;
                                      rs[(size_t) i].repMode = s.famLock[11] == 1 ? 0 : (s.famLock[11] == 2 ? 2 : 1); rs[(size_t) i].repAmt = s.famAmt[11]; }
        driver::apply (p.events, rs, s.bpm, s.instrument, s.exportMode == 0 && s.emitKS && s.ksActive);
    }
    // v3.8 safety for Enoch's standard key-switch layout (keys from the Key-switch Base Note up): a played note that lands on one of
    // those keys moves up an octave, so the instrument plays it instead of switching on it (profiles are guarded in the driver)
    const Library* L0 = (s.routed && s.routes[0].lib) ? s.routes[0].lib.get() : s.lib.get();
    if (s.emitKS && s.ksActive && s.exportMode == 0 && ! (L0 && ! L0->standard && L0->inst == s.instrument))
    {
        const int nks = (int) artList (s.instrument).size();
        auto onKey = [&] (int a) { return a >= s.ksBase && a < s.ksBase + nks; };
        for (auto& e : p.events)
            if (! e.ks && e.track != 2 && (e.type == EV_ON || e.type == EV_OFF) && onKey (e.a))
            {
                int q = e.a + 12; while (onKey (q) && q + 12 <= 127) q += 12;
                if (q <= 127 && ! onKey (q)) e.a = q;
            }
    }
}

} // namespace enochsolo
