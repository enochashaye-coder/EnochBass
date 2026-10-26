// Enoch Bass - ornament realisation, humanisation and MIDI event building (header only)
// Included at the end of Generator.h
#pragma once

namespace enochsolo
{
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

inline void humanizeNotes (Pattern& p, std::vector<Note>& notes, const Settings& s)
{
    const Style& st = styleFor (s.instrument, s.artist);
    const double msToBeats = s.bpm / 60000.0;
    const double T = s.hTiming, V = s.hVel, L = s.hLength, F = s.hFeel;
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
        off += st.laid * F * (w < 0.5 ? 1.0 : 0.45);                      // laid back more on off-beats
        off = std::clamp (off, -28.0, 38.0);
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
            n.start = std::max (0.0, n.start + (off + spread) * msToBeats);
            double v = n.vel + gauss (hs + 1, id) * 5.0 * V + (n.weight - 0.45) * 10.0 * V;
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
        for (auto& n : notes) if (n.track == track) { all.push_back (&n); if (n.voice == 0) line.push_back (&n); }
        if (all.empty()) continue;
        std::stable_sort (line.begin(), line.end(), [] (const Note* a, const Note* b) { return a->start < b->start; });

        // ---------------- notes
        for (auto* n : all)
        {
            MidiEv on; on.t = n->start; on.type = EV_ON; on.a = std::clamp (n->pitch, 0, 127); on.b = std::clamp (n->vel, 1, 127); on.track = track; on.ord = 2; on.art = n->art;
            MidiEv off = on; off.t = n->start + std::max (0.02, n->length); off.type = EV_OFF; off.b = 0; off.ord = 0;
            out.push_back (on); out.push_back (off);
        }

        if (track == 2) continue;      // the chord pad is plain notes: no key-switches / expression

        // ---------------- key-switches (on change of articulation)
        if (s.emitKS && L && L->inst == inst && ! L->standard)
        {
            // instrument of choice: the library's own key-switch table
            if (! L->noKeySwitches && ! L->arts.empty())
            {
                int last = -1;
                for (auto* n : line)
                {
                    const int art = libResolve (*L, n->art);
                    const LibArt* e = libFind (*L, art);
                    if (! e) continue;
                    const double t = std::max (0.0, n->start - ksLead);
                    if (e->oneShot || art != last)
                    {
                        for (auto& pr : e->press)
                        {
                            MidiEv on; on.t = t; on.type = EV_ON; on.a = std::clamp (pr.note, 0, 127); on.b = std::clamp (pr.vel, 1, 127); on.track = track; on.ord = 1;
                            MidiEv off = on; off.t = e->hold ? n->start + 0.1 : t + 0.05; off.type = EV_OFF; off.b = 0; off.ord = 0;
                            out.push_back (on); out.push_back (off);
                        }
                        if (! e->oneShot) last = art;
                    }
                }
            }
        }
        else if (s.emitKS)
        {
            int last = -1;
            for (auto* n : line)
            {
                const int art = artMapped (inst, n->art == A_Bend && artIndex (inst, A_Bend) < 0 ? A_Legato : n->art);
                const int idx = artIndex (inst, art);
                if (idx >= 0 && art != last)
                {
                    const double t = std::max (0.0, n->start - ksLead);
                    MidiEv on; on.t = t; on.type = EV_ON; on.a = s.ksBase + idx; on.b = 100; on.track = track; on.ord = 1;
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
            const double prob = st.pedal * (afro ? 0.5 : 1.0) + (s.genre == G_Ballad ? 0.3 : 0.0);
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
inline void Generator::finalize (Pattern& p, const Settings& sIn)
{
    Settings s = sIn;
    p.s = s;
    // muted notes stay in the piano roll but never sound
    auto prepare = [&] (bool realize)
    {
        std::vector<Note> v; v.reserve (p.raw.size());
        for (auto& n : p.raw) if (! n.mute) v.push_back (n);
        if (realize) detail::realizeOrnaments (p, v, s);
        if (s.humanize) detail::humanizeNotes (p, v, s);
        else std::stable_sort (v.begin(), v.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        for (auto& n : v) { n.pitch = std::clamp (n.pitch, 0, 127); n.vel = std::clamp (n.vel, 1, 127); }
        return v;
    };
    p.notes = prepare (s.realizeOrnaments);

    // what the test player plays: always the full articulated version, so you hear what the notes are meant to be
    { Settings sp = s; sp.emitKS = sp.emitBends = sp.emitCC = true; detail::buildEvents (p, p.notes, sp, p.playEvents, true, false); }

    // what gets exported
    Settings se = s;
    if (s.exportMode == 0) detail::buildEvents (p, p.notes, se, p.events, true, false);
    else
    {
        se.emitKS = false;
        if (s.exportMode == 2) { se.emitBends = false; se.emitCC = false; }
        auto ne = prepare (true);
        detail::bakeArticulations (ne);
        detail::buildEvents (p, ne, se, p.events, s.exportMode != 2, true);
    }
}

} // namespace enochsolo
