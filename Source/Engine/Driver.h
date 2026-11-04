// Enoch v3.0 - External instrument driver (JUCE-free, shared by Enoch Solo and Enoch Bass).
// Runs after the event builder: it turns the musical events of every part into what the chosen instrument expects
// (channel, program / bank, mode keys, transposition, range, velocity curve, legato overlap, repetition keys, string choice).
// The built-in preview never goes through here, so it always plays what the notes mean.
#pragma once
#include "Libraries.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace enochsolo
{
namespace driver
{
/** what one part (solo line, comping, chord pad, log drum ...) is routed to */
struct RouteSpec
{
    bool on = true;
    int channel = 0;              // 0..15
    const Library* lib = nullptr; // the mapping profile (null = plain notes)
    int transpose = 0;            // extra semitones set on the route
    int program = -1;             // overrides the profile's program when >= 0
};

inline bool isLegatoArt (int a) { return a == A_Legato || a == A_HammerOn || a == A_PullOff || a == A_Slide || a == A_Gliss; }

/** String choice like a player's hand: a Viterbi search over (string, fret) that keeps the hand in position, prefers short moves,
    takes open strings when they fit and never asks for a stretch beyond 4 frets between notes closer than an 8th note.
    Returns the string index (into tuning) for every pitch, -1 when no string can play it. */
inline std::vector<int> solveStrings (const std::vector<int>& pitch, const std::vector<double>& start, const std::vector<int>& tuning, int maxFret)
{
    const int n = (int) pitch.size(), S = (int) tuning.size();
    std::vector<int> out ((size_t) n, -1);
    if (n == 0 || S == 0) return out;
    const double INF = 1e18;
    std::vector<std::vector<double>> cost ((size_t) n, std::vector<double> ((size_t) S, INF));
    std::vector<std::vector<int>> from ((size_t) n, std::vector<int> ((size_t) S, -1));
    auto fretOf = [&] (int i, int s) { return pitch[(size_t) i] - tuning[(size_t) s]; };
    auto playable = [&] (int i, int s) { const int f = fretOf (i, s); return f >= 0 && f <= maxFret; };
    for (int s = 0; s < S; ++s) if (playable (0, s)) cost[0][(size_t) s] = 0.15 * fretOf (0, s) + (fretOf (0, s) == 0 ? -0.2 : 0.0);
    for (int i = 1; i < n; ++i)
    {
        const double dt = std::max (0.0, start[(size_t) i] - start[(size_t) (i - 1)]);
        const double speed = dt < 0.13 ? 3.0 : dt < 0.26 ? 1.6 : dt < 0.51 ? 1.0 : 0.4;   // fast notes must stay close
        for (int s = 0; s < S; ++s)
        {
            if (! playable (i, s)) continue;
            const int f = fretOf (i, s);
            for (int p = 0; p < S; ++p)
            {
                if (cost[(size_t) (i - 1)][(size_t) p] >= INF) continue;
                const int pf = fretOf (i - 1, p);
                double c = cost[(size_t) (i - 1)][(size_t) p];
                const int span = (f == 0 || pf == 0) ? 0 : std::abs (f - pf);
                if (span > 4) c += (span - 4) * 2.5 * speed + 1.0;          // a position shift
                else c += span * 0.15 * speed;
                c += std::abs (s - p) * 0.25 * speed;                        // string crossing
                if (s == p && f == pf && pitch[(size_t) i] == pitch[(size_t) (i - 1)]) c -= 0.1;
                c += 0.02 * f + (f == 0 ? -0.15 : 0.0);
                if (c < cost[(size_t) i][(size_t) s]) { cost[(size_t) i][(size_t) s] = c; from[(size_t) i][(size_t) s] = p; }
            }
        }
    }
    int best = -1; double bc = INF;
    for (int s = 0; s < S; ++s) if (cost[(size_t) (n - 1)][(size_t) s] < bc) { bc = cost[(size_t) (n - 1)][(size_t) s]; best = s; }
    for (int i = n - 1; i >= 0 && best >= 0; --i) { out[(size_t) i] = best; best = from[(size_t) i][(size_t) best]; }
    return out;
}

/** Apply the routes to one pattern's events. routes[track]: 0 = solo line, 1 = accompaniment / second part, 2 = chord pad. */
inline void apply (std::vector<MidiEv>& ev, const std::array<RouteSpec, 3>& routes, double bpm, int playerInst, bool allowKs = true)
{
    const double beatsPerMs = std::max (20.0, bpm) / 60000.0;
    std::vector<MidiEv> out; out.reserve (ev.size() + 64);
    for (int track = 0; track < 3; ++track)
    {
        const RouteSpec& r = routes[(size_t) track];
        std::vector<MidiEv> t;
        for (auto& e : ev) if (e.track == track) t.push_back (e);
        if (! r.on || t.empty()) continue;
        const Library* L = (r.lib && (r.lib->inst == playerInst || track == 2)) ? r.lib : nullptr;
        const int tr = (L ? L->transpose : 0) + r.transpose;
        const int lo = L ? L->lo : 0, hi = L ? L->hi : 127;

        // ---- 1) pair note-ons with their note-offs (played notes only, key-switches stay as they are)
        std::vector<int> onIdx;
        std::vector<int> offOf (t.size(), -1);
        {
            std::vector<std::vector<int>> open (128);
            std::vector<int> order (t.size()); for (size_t i = 0; i < t.size(); ++i) order[i] = (int) i;
            std::stable_sort (order.begin(), order.end(), [&] (int a, int b) { return t[(size_t) a].t != t[(size_t) b].t ? t[(size_t) a].t < t[(size_t) b].t : t[(size_t) a].ord < t[(size_t) b].ord; });
            for (int i : order)
            {
                auto& e = t[(size_t) i];
                if (e.ks || (e.type != EV_ON && e.type != EV_OFF)) continue;
                auto& q = open[(size_t) std::clamp (e.a, 0, 127)];
                if (e.type == EV_ON) { q.push_back (i); onIdx.push_back (i); }
                else if (! q.empty()) { offOf[(size_t) q.front()] = i; q.erase (q.begin()); }
            }
        }
        // the main line in time order (single notes; chords keep their top note as the line)
        std::vector<int> line;
        for (int i : onIdx) if (t[(size_t) i].ord == 2) line.push_back (i);
        std::stable_sort (line.begin(), line.end(), [&] (int a, int b) { return t[(size_t) a].t < t[(size_t) b].t; });

        if (L && track != 2)
        {
            // ---- 2) legato by overlap (also without key-switches: it is just note length): the previous note holds into the next legato note
            if (L->overlapMs > 0.0)
            {
                const double ov = L->overlapMs * beatsPerMs;
                for (size_t k = 1; k < line.size(); ++k)
                {
                    const auto& cur = t[(size_t) line[k]];
                    const int prev = line[k - 1], po = offOf[(size_t) prev];
                    if (! isLegatoArt (cur.art) || po < 0 || t[(size_t) prev].a == cur.a) continue;
                    if (std::abs (t[(size_t) po].t - cur.t) < 0.25) t[(size_t) po].t = cur.t + ov;
                }
            }
            // ---- 3) string selection: the solver's string, sent only when it changes
            if (allowKs && ! L->tuning.empty() && L->stringKeys.size() == L->tuning.size() && line.size() > 0)
            {
                std::vector<int> pitches; std::vector<double> starts;
                for (int i : line) { pitches.push_back (t[(size_t) i].a); starts.push_back (t[(size_t) i].t); }
                const auto strings = solveStrings (pitches, starts, L->tuning, L->maxFret);
                int last = -1;
                const double lead = 30.0 * beatsPerMs;
                for (size_t k = 0; k < line.size(); ++k)
                {
                    const int s = strings[k];
                    if (s < 0 || s == last) continue;
                    MidiEv on; on.t = std::max (0.0, t[(size_t) line[k]].t - lead); on.type = EV_ON; on.a = L->stringKeys[(size_t) s]; on.b = 100; on.track = track; on.ord = 1; on.ks = true;
                    MidiEv off = on; off.t = on.t + 10.0 * beatsPerMs; off.type = EV_OFF; off.b = 0; off.ord = 0;
                    t.push_back (on); t.push_back (off);
                    last = s;
                }
            }
            // ---- 4) repetition keys: fast repeats of one pitch alternate with the repetition key (no machine-gun retrigger)
            if (allowKs && L->repKey >= 0)
            {
                bool alt = false;
                for (size_t k = 1; k < line.size(); ++k)
                {
                    auto& cur = t[(size_t) line[k]];
                    const auto& prv = t[(size_t) line[k - 1]];
                    const bool fastRepeat = cur.a == prv.a && cur.t - prv.t < 0.51;
                    alt = fastRepeat ? ! alt : false;
                    if (alt)
                    {
                        const int off = offOf[(size_t) line[k]];
                        cur.a = L->repKey; cur.ks = true;                 // played on the repetition key, never transposed
                        if (off >= 0) { t[(size_t) off].a = L->repKey; t[(size_t) off].ks = true; }
                    }
                }
            }
        }
        // ---- 5) transpose into the library's range + velocity curve (played notes only)
        // v3.8 safety: a played note never lands on a key the profile uses as a switch (the instrument would switch instead of
        // sounding: the "silent note" fault). Such a note moves by an octave, to the nearest free key, staying in range when it can.
        std::array<bool, 128> keys {}; keys.fill (false);
        if (L) keys = libKeyMask (*L);
        auto freeKey = [&] (int p)
        {
            if (! keys[(size_t) p]) return p;
            int best = -1, bestCost = 1 << 20;
            for (int d : { 12, -12, 24, -24, 36, -36 })
            {
                const int q = p + d;
                if (q < 0 || q > 127 || keys[(size_t) q]) continue;
                const int cost = std::abs (d) + ((q < lo || q > hi) ? 100 : 0);
                if (cost < bestCost) { bestCost = cost; best = q; }
            }
            return best >= 0 ? best : p;
        };
        for (auto& e : t)
        {
            if (e.ks || (e.type != EV_ON && e.type != EV_OFF)) continue;
            int p = e.a + tr;
            while (p < lo && p + 12 <= 127) p += 12;
            while (p > hi && p - 12 >= 0) p -= 12;
            e.a = freeKey (std::clamp (p, 0, 127));
            if (e.type == EV_ON && L)
            {
                const double u = std::pow (std::clamp (e.b / 127.0, 0.0, 1.0), L->velGamma);
                e.b = std::clamp ((int) std::lround (L->velMin + (L->velMax - L->velMin) * u), 1, 127);
            }
        }
        // ---- 6) set-up at the start: bank, program, mode keys
        auto addAt = [&] (int type, int a, int b, int ord, bool ks) { MidiEv e; e.t = 0; e.type = type; e.a = a; e.b = b; e.track = track; e.ord = ord; e.ks = ks; t.push_back (e); };
        const int prog = r.program >= 0 ? r.program : (L ? L->program : -1);
        if (L && L->bankMsb >= 0) addAt (EV_CC, 0, L->bankMsb, -3, true);
        if (L && L->bankLsb >= 0) addAt (EV_CC, 32, L->bankLsb, -3, true);
        if (prog >= 0) addAt (EV_PC, std::clamp (prog, 0, 127), 0, -2, true);
        if (L && allowKs)
            for (auto& k : L->init)
            {
                addAt (EV_ON, k.note, k.vel, -1, true);
                MidiEv off; off.t = 12.0 * beatsPerMs; off.type = EV_OFF; off.a = k.note; off.b = 0; off.track = track; off.ord = 0; off.ks = true; t.push_back (off);
            }
        // ---- 7) the route's channel
        for (auto& e : t) { e.ch = std::clamp (r.channel, 0, 15); out.push_back (e); }
    }
    std::stable_sort (out.begin(), out.end(), [] (const MidiEv& a, const MidiEv& b) { return std::abs (a.t - b.t) > 1e-9 ? a.t < b.t : a.ord < b.ord; });
    ev.swap (out);
}
} // namespace driver
} // namespace enochsolo
