// Enoch v3.1 - Amapiano log drum part (header only, shared by Enoch Bass and Enoch Solo; only Enoch Bass uses it).
//
// The log drum is its own player, not a copy of the bass line:
//  - two-bar phrases built from the off-beat cells amapiano log drums use (hits that dodge the kick, a pickup into bar 2,
//    a roll or a held "drop" note at the end of the phrase);
//  - it locks to the bass: hits avoid the bass line's onsets (except the phrase downbeat), so the two interlock instead of doubling;
//  - pitches follow the chords: the root, its octave, fifth and seventh, with falling figures and held notes that overlap the
//    next hit (a log drum synth with glide, or Enoch's built-in log drum, slides between them);
//  - Note Density (global or per bar) chooses sparse / normal / busy cells. Every choice is seeded, so a take repeats exactly.
#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace enochsolo
{
namespace logdrum
{
struct R
{
    uint64_t s;
    explicit R (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ULL + 0xA0761D6478BD642FULL) {}
    double f() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (double) (s >> 11) * (1.0 / 9007199254740992.0); }
    bool chance (double p) { return f() < p; }
    int i (int a, int b) { return b <= a ? a : a + (int) (f() * (b - a + 1)) % (b - a + 1); }
};

/** 16-step cells. x = hit, h = held hit (lasts to the next hit, glides into it), r = 32nd roll pair, . = rest */
inline const std::vector<std::string>& cells (int level)
{
    static const std::vector<std::string> sparse = {
        "x.....x...x.....", "...x......x.....", "x.........x..x..", "......x...x.....", "x..h......x.....", "...x..x.........",
    };
    static const std::vector<std::string> normal = {
        "x..x..x...x.x...", "...x..x...x..x..", "x.....x.x.x...x.", "..x..x....x.x..x", "x..x......x.x...", "...x..h...x..x..",
        "x..h..x...x.....", "..x...x.x...x...", "x..x..x...h.....", "...x.x....x..x..",
    };
    static const std::vector<std::string> busy = {
        "x..x..x.x.x.x..x", "..xx..x...x.x.x.", "x.xx..x.x..x..x.", "x..x.xx...x.x.x.", "...x..x.x.xx..x.", "x..x..x.x..x.x.x",
    };
    return level == 0 ? sparse : level == 2 ? busy : normal;
}
/** the last bar of a phrase ends with one of these over its last 4 steps (beat 4) */
inline const std::vector<std::string>& endings()
{
    static const std::vector<std::string> v = { "rrrr", "x.x.", ".xxx", "h...", "x.rr", "..xx" };
    return v;
}

struct ChordInfo { int rootPc; uint16_t tones; };

/** builds the log drum notes for track 1 (written pitch: the octave of a bass part, a DX10-style log drum profile moves it up) */
inline std::vector<MidiEv> generate (const Pattern& p, const Settings& S, const std::vector<double>& bassOnsets)
{
    std::vector<MidiEv> out;
    if (p.chords.empty() || p.bars <= 0) return out;
    const double bb = std::max (1.0, p.barBeats);
    const int steps = std::max (4, (int) std::lround (bb * 4.0));
    const double q = bb / steps;
    R rng ((uint64_t) S.seed ^ 0x5851F42D4C957F2DULL);
    auto densOf = [&] (int bar) { return (bar >= 0 && bar < (int) S.barBusy.size() && S.barBusy[(size_t) bar] >= 0.f) ? (double) S.barBusy[(size_t) bar] : (double) S.busy; };
    auto chordAt = [&] (double t) -> const ChordSpan& { for (auto& c : p.chords) if (t + 1e-6 >= c.start && t < c.end - 1e-6) return c; return p.chords.back(); };
    auto bassNear = [&] (double t) { for (double b : bassOnsets) if (std::abs (b - t) < q * 0.5) return true; return false; };

    // the log drum's register: the chord root in the octave starting at A0..G#1 (MIDI 33-44), like an 808 an octave above the sub
    auto rootPitch = [&] (int pc) { return 33 + (((pc - 9) % 12) + 12) % 12; };   // A0 (33) .. G#1 (44)

    std::string phraseCellA, phraseCellB;
    int lastPitch = -1;
    struct Hit { double t; int pitch; int vel; bool hold; };
    std::vector<Hit> hits;
    for (int bar = 0; bar < p.bars; ++bar)
    {
        // a bar the bass leaves empty (cleared in the piano roll) is empty for the log drum too: they are one rhythm section
        bool bassInBar = false; for (double t : bassOnsets) if (t >= bar * bb - 1e-6 && t < (bar + 1) * bb - 1e-6) bassInBar = true;
        if (! bassInBar) continue;
        const bool phraseStart = (bar % 2) == 0, phraseEnd = (bar % 2) == 1 || bar == p.bars - 1;
        const double d = densOf (bar);
        const int level = d < 0.3 ? 0 : d < 0.7 ? 1 : 2;
        const auto& cs = cells (level);
        std::string cell;
        // how many hits of a cell land on the bass line's onsets in this bar (the downbeat excepted)
        auto clashes = [&] (const std::string& c) { int n = 0; for (int s = 1; s < (int) c.size() && s < steps; ++s) if (c[(size_t) s] != '.' && bassNear (bar * bb + s * q)) ++n; return n; };
        auto pickInterlocking = [&]
        {
            int best = 0; double bs = -1e9;
            for (size_t k = 0; k < cs.size(); ++k)
            {
                const double sc = rng.f() * 0.6 - 1.5 * clashes (cs[k]);
                if (sc > bs) { bs = sc; best = (int) k; }
            }
            return cs[(size_t) best];
        };
        if (phraseStart) { cell = pickInterlocking(); phraseCellA = cell; }
        else
        {
            // the answer bar repeats the riff (log drum riffs repeat) unless it would now collide with the bass
            cell = phraseCellA.empty() ? cs[0] : phraseCellA;
            if (clashes (cell) > 1 || rng.chance (0.3)) cell = pickInterlocking();
        }
        // a hit that still lands on a bass note moves to the free 16th next to it (or rests)
        for (int s2 = 1; s2 < (int) cell.size() && s2 < steps; ++s2)
        {
            if (cell[(size_t) s2] == '.' || ! bassNear (bar * bb + s2 * q)) continue;
            const char c = cell[(size_t) s2]; cell[(size_t) s2] = '.';
            for (int d : { 1, -1 })
            {
                const int t2 = s2 + d;
                if (t2 > 0 && t2 < steps && cell[(size_t) t2] == '.' && ! bassNear (bar * bb + t2 * q) && (t2 < 1 || cell[(size_t) (t2 - 1)] == '.' || t2 - 1 == s2)) { cell[(size_t) t2] = c; break; }
            }
        }
        cell.resize ((size_t) steps, '.');
        if (phraseEnd && steps >= 16 && rng.chance (0.75))
        {
            const auto& e = endings()[(size_t) rng.i (0, (int) endings().size() - 1)];
            for (int k = 0; k < 4; ++k) cell[(size_t) (steps - 4 + k)] = e[(size_t) k];
        }
        // pitches: the riff walks root -> octave / fifth / seventh and falls back, the held notes sit on the root
        for (int s = 0; s < steps; ++s)
        {
            const char c = cell[(size_t) s];
            if (c == '.') continue;
            const double t = bar * bb + s * q;
            const auto& ch = chordAt (t);
            const int root = rootPitch (ch.rootPc);
            int pitch = root;
            const double r = rng.f();
            if (c == 'h') pitch = root;
            else if (r < 0.40) pitch = root;
            else if (r < 0.62) pitch = root + 12;
            else if (r < 0.80) pitch = root + 7 <= 52 ? root + 7 : root - 5;
            else if (r < 0.92) pitch = (ch.toneMask & (1u << ((ch.rootPc + 10) % 12))) ? root + 10 : root + 12;
            else pitch = lastPitch > 0 ? lastPitch : root;
            if (lastPitch > 0 && std::abs (pitch - lastPitch) > 12) pitch += pitch > lastPitch ? -12 : 12;
            const int vel = std::clamp ((s % 4 == 0 ? 112 : (s % 2 == 0 ? 100 : 92)) + rng.i (-6, 6), 60, 127);
            if (c == 'r')
            {
                hits.push_back ({ t, pitch, vel - 14, false });
                hits.push_back ({ t + q * 0.5, pitch, vel - 20, false });
            }
            else hits.push_back ({ t, pitch, vel, c == 'h' });
            lastPitch = pitch;
        }
    }
    std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.t < b.t; });
    const double total = p.bars * bb;
    for (size_t i = 0; i < hits.size(); ++i)
    {
        const auto& h = hits[i];
        const double next = i + 1 < hits.size() ? hits[i + 1].t : total;
        // log drums ring, but each hit is cut by the next; a held hit overlaps the next by a 32nd so a glide synth slides into it
        const bool samePitchNext = i + 1 < hits.size() && hits[i + 1].pitch == h.pitch;
        double len = (h.hold && ! samePitchNext) ? (next - h.t) + q * 0.5 : std::min (next - h.t, h.hold ? 99.0 : q * 3.0) * 0.92;
        len = std::max (0.04, std::min (len, total - h.t));
        MidiEv on; on.t = h.t; on.type = EV_ON; on.a = std::clamp (h.pitch, 24, 60); on.b = h.vel; on.track = 1; on.ord = 2; on.art = A_Sustain;
        MidiEv off = on; off.t = h.t + len; off.type = EV_OFF; off.b = 0; off.ord = 0;
        out.push_back (on); out.push_back (off);
    }
    return out;
}
} // namespace logdrum
} // namespace enochsolo
