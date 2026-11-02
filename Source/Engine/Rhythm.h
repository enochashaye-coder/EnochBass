// v2.7 batch 2: speed and subdivision pass (header only, shared by Enoch Solo and Enoch Bass)
//
// Runs on the main line after generation, so the generator's own 16th-note logic is untouched:
//  - Double time (global or per bar): notes of an 8th or longer are split in two; the added note walks
//    by step (scale of the bar's chord) toward the next note, so the line keeps its shape at twice the rate.
//  - Subdivisions 1/16T, 1/32, 1/32T: approach runs at that rate lead into the next note. How many notes
//    get a run follows the bar's Notes Density; the run notes come from the chord scale and land on the
//    next note, wind players keep breathing room, the bass uses runs sparingly.
// Every choice is seeded from the take's seed, so a take always comes out the same.
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace enochsolo
{
namespace rhythm
{
enum Subdiv { SD_Auto = 0, SD_16, SD_16T, SD_32, SD_32T, SD_4, SD_8, SD_8T };   // v3.1: 1/4, 1/8, 1/8T appended (saved indices unchanged)

struct R
{
    uint64_t s;
    explicit R (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL) {}
    double f() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (double) (s >> 11) * (1.0 / 9007199254740992.0); }
    bool chance (double p) { return f() < p; }
};

inline const ChordSpan& chordAt (const std::vector<ChordSpan>& v, double beat)
{
    for (auto& c : v) if (beat + 1e-6 >= c.start && beat < c.end - 1e-6) return c;
    return v.back();
}

/** next pitch one scale step from 'from' toward 'to' (stays put when already there) */
inline int stepToward (int from, int to, uint16_t mask)
{
    if (from == to) return from;
    const int dir = to > from ? 1 : -1;
    for (int d = 1; d <= 3; ++d) { const int p = from + dir * d; if (inMask (mask, p)) return p; }
    return from + dir;
}

inline bool isWindInst (int inst) { return inst == I_Sax || inst == I_Trumpet; }

inline void apply (Pattern& p, const Settings& s)
{
    const bool anyBarSpeed = std::any_of (s.barSpeed.begin(), s.barSpeed.end(), [] (int v) { return v > 0; });
    if (s.speed == 0 && ! anyBarSpeed && (s.subdiv == SD_Auto || s.subdiv == SD_16)) return;
    if (p.chords.empty()) return;
    const double bb = std::max (0.5, p.barBeats);
    auto speedOf = [&] (int bar) { return (bar >= 0 && bar < (int) s.barSpeed.size() && s.barSpeed[(size_t) bar] >= 0) ? s.barSpeed[(size_t) bar] : s.speed; };
    auto densOf = [&] (int bar) { return (bar >= 0 && bar < (int) s.barBusy.size() && s.barBusy[(size_t) bar] >= 0.f) ? (double) s.barBusy[(size_t) bar] : (double) s.busy; };
    const bool wind = isWindInst (s.instrument), bass = (s.instrument == 5);          // 5 = bass (Enoch Bass)
    const bool bowed = (s.instrument == I_Violin), fretted = (s.instrument == I_Guitar || bass);

    std::vector<Note> main, rest;
    for (auto& n : p.raw) ((n.track == 0 && n.voice == 0 && ! n.mute) ? main : rest).push_back (n);
    std::stable_sort (main.begin(), main.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    R rng (s.seed ^ 0xD1B54A32D192ED03ULL);
    auto runArt = [&] (int from, int to)
    {
        if (fretted) return to > from ? A_HammerOn : A_PullOff;
        if (wind || bowed) return A_Legato;
        return A_Sustain;
    };

    // ---- 1) double time
    std::vector<Note> out;
    for (size_t i = 0; i < main.size(); ++i)
    {
        Note n = main[i];
        const int bar = (int) (n.start / bb);
        const bool last = (i + 1 == main.size());
        const int nextPitch = last ? n.pitch : main[i + 1].pitch;
        const double gapToNext = last ? 99.0 : main[i + 1].start - (n.start + n.length);
        if (speedOf (bar) == 1 && n.length >= 0.5 - 1e-6 && ! (n.length >= 2.0 && rng.chance (0.5)))
        {
            // long notes that end a phrase stay long half the time: double time still breathes
            const double half = n.length * 0.5;
            const auto& c = chordAt (p.chords, n.start + half);
            Note a = n; a.length = half;
            Note b = n; b.start = n.start + half; b.length = half; b.vel = std::max (1, n.vel - 8); b.weight = n.weight * 0.5;
            b.pitch = (gapToNext < 0.6 && std::abs (nextPitch - n.pitch) >= 2) ? stepToward (n.pitch, nextPitch, c.scaleMask)
                                                                                 : stepToward (n.pitch, n.pitch + (rng.chance (0.5) ? 2 : -2), c.scaleMask);
            b.art = runArt (a.pitch, b.pitch);
            out.push_back (a); out.push_back (b);
        }
        else out.push_back (n);
    }
    main.swap (out); out.clear();

    // ---- 2) fast subdivisions: approach runs into the next note
    if (s.subdiv == SD_16T || s.subdiv == SD_32 || s.subdiv == SD_32T)
    {
        const double q = s.subdiv == SD_16T ? 1.0 / 6.0 : s.subdiv == SD_32 ? 0.125 : 1.0 / 12.0;
        const int maxRun = wind ? 6 : bass ? 4 : 8;
        for (size_t i = 0; i < main.size(); ++i)
        {
            Note n = main[i];
            const int bar = (int) (n.start / bb);
            const bool last = (i + 1 == main.size());
            const double d = densOf (bar);
            double pr = 0.08 + 0.55 * d;                       // denser bars get more runs
            if (bass) pr *= 0.4;
            if (wind) pr *= 0.8;
            const bool connected = ! last && main[i + 1].start - (n.start + n.length) < 0.26;
            if (n.length < 2.0 * q - 1e-6 || ! connected || ! rng.chance (pr)) { out.push_back (n); continue; }
            const int target = main[i + 1].pitch;
            const double runLen = std::min (n.length, std::min ((double) maxRun * q, s.subdiv == SD_16T ? 0.5 : 0.375));
            const int count = std::max (2, (int) std::floor (runLen / q + 1e-6));
            const double runStart = n.start + n.length - count * q;
            if (runStart > n.start + 1e-6) { Note head = n; head.length = runStart - n.start; out.push_back (head); }
            // walk from the note toward the target; if it gets there early, turn around it (upper / lower neighbour)
            int pitch = n.pitch;
            const auto& c = chordAt (p.chords, runStart);
            for (int k = 0; k < count; ++k)
            {
                Note r = n;
                r.start = runStart + k * q; r.length = q;
                if (k > 0 || runStart > n.start + 1e-6)
                {
                    int np = stepToward (pitch, target, c.scaleMask);
                    if (np == pitch || np == target) np = stepToward (pitch, pitch + ((k & 1) ? -2 : 2), c.scaleMask);
                    r.art = runArt (pitch, np);
                    pitch = np;
                }
                r.pitch = pitch;
                r.vel = std::clamp (n.vel - 10 + (k == 0 ? 6 : 0), 1, 127);
                r.weight = 0.1;
                out.push_back (r);
            }
        }
        main.swap (out); out.clear();
    }

    // ---- 2b) v3.1 slower grids: the line moves in quarters, eighths or eighth triplets. Every note is moved to the nearest grid
    //      point; two notes on one point keep the stronger (on the beat, longer, louder) one; each note then lasts until the next
    //      (keeping the style's short notes short). A real re-phrasing, not a quantize of the 16th line.
    if (s.subdiv == SD_4 || s.subdiv == SD_8 || s.subdiv == SD_8T)
    {
        const double q = s.subdiv == SD_4 ? 1.0 : s.subdiv == SD_8 ? 0.5 : 1.0 / 3.0;
        std::vector<Note> kept;
        for (auto n : main)
        {
            const double g = std::round (n.start / q) * q;
            const double score = (std::abs (std::fmod (g, 1.0)) < 1e-6 ? 1.0 : 0.0) + n.length + n.vel / 254.0 + n.weight;
            n.start = g;
            if (! kept.empty() && std::abs (kept.back().start - g) < 1e-6)
            {
                const double ks = (std::abs (std::fmod (kept.back().start, 1.0)) < 1e-6 ? 1.0 : 0.0) + kept.back().length + kept.back().vel / 254.0 + kept.back().weight;
                if (score > ks) kept.back() = n;
                continue;
            }
            kept.push_back (n);
        }
        for (size_t i = 0; i < kept.size(); ++i)
        {
            auto& n = kept[i];
            const double next = i + 1 < kept.size() ? kept[i + 1].start : n.start + std::max (q, n.length);
            const double room = std::max (q * 0.5, next - n.start);
            const bool shortNote = (n.art == A_Staccato || n.art == A_Ghost || n.art == A_PalmMute || n.art == A_Pizz);
            n.length = shortNote ? std::min (room * 0.9, q * 0.6) : std::max (std::min (room, n.length + q), q * 0.9);
            n.length = std::min (n.length, room - 0.02);
            if (n.length < 0.05) n.length = 0.05;
        }
        main.swap (kept);
    }

    // ---- 3) winds breathe: at least a 16th of air every two bars
    if (wind && ! main.empty())
    {
        const int bars = std::max (1, p.bars);
        for (int b0 = 0; b0 < bars; b0 += 2)
        {
            const double t0 = b0 * bb, t1 = std::min (bars, b0 + 2) * bb;
            double longestGap = 0; int lastIdx = -1;
            for (size_t i = 0; i < main.size(); ++i)
            {
                if (main[i].start < t0 - 1e-6 || main[i].start >= t1 - 1e-6) continue;
                const double end = main[i].start + main[i].length;
                const double next = (i + 1 < main.size()) ? main[i + 1].start : t1 + 1.0;
                longestGap = std::max (longestGap, next - end);
                lastIdx = (int) i;
            }
            if (lastIdx >= 0 && longestGap < 0.25 && main[(size_t) lastIdx].length > 0.3) main[(size_t) lastIdx].length -= 0.25;
        }
    }

    p.raw = rest;
    p.raw.insert (p.raw.end(), main.begin(), main.end());
    std::stable_sort (p.raw.begin(), p.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
}

/** v3.1 Output "Fills" (EZkeys-style fills mode): the solo line keeps only what falls in the fill windows -
    the last beat and a half of every second bar (the end of each phrase) and the last beat of the final bar. */
inline void fills (Pattern& p, const Settings& s)
{
    if (! s.fills) return;
    const double bb = std::max (0.5, p.barBeats);
    std::vector<Note> keep;
    for (auto& n : p.raw)
    {
        if (n.track != 0) { keep.push_back (n); continue; }
        const int bar = (int) std::floor (n.start / bb + 1e-9);
        const double inBar = n.start - bar * bb;
        const bool phraseEnd = (bar % 2) == 1 || bar == p.bars - 1;
        if (phraseEnd && inBar >= bb - 1.5 - 1e-6) { Note k = n; k.length = std::min (k.length, (bar + 1) * bb - k.start + 0.25); keep.push_back (k); }
    }
    p.raw.swap (keep);
}

/** v3.1 two-hand piano (Solo + Accompaniment = EZkeys "Full"): the comping hand stays under the melody like one pianist's hands.
    A comp note that sounds with a melody note and sits on it or above it (or within a semitone) moves down an octave; if it has
    nowhere to go (below C2) or now doubles another note of its chord, it is left out. */
inline void twoHands (Pattern& p, const Settings& s)
{
    if (s.instrument != I_Piano || s.mode != 2) return;
    std::vector<const Note*> mel;
    for (auto& n : p.raw) if (n.track == 0 && ! n.mute) mel.push_back (&n);
    if (mel.empty()) return;
    auto melodyOver = [&] (const Note& c)
    {
        int low = 999;
        for (auto* m : mel) if (m->start < c.start + c.length - 0.01 && c.start < m->start + m->length - 0.01) low = std::min (low, m->pitch);
        return low;
    };
    std::vector<Note> out;
    for (auto& n : p.raw)
    {
        if (n.track != 1) { out.push_back (n); continue; }
        Note c = n;
        const int low = melodyOver (c);
        if (low < 999) while (c.pitch >= low - 1 && c.pitch - 12 >= 36) c.pitch -= 12;
        if (low < 999 && c.pitch >= low - 1) continue;                     // no room under the melody: the comping hand leaves it out
        bool dup = false;
        for (auto& o : out) if (o.track == 1 && o.pitch == c.pitch && std::abs (o.start - c.start) < 0.02) dup = true;
        if (! dup) out.push_back (c);
    }
    p.raw.swap (out);
}
} // namespace rhythm
} // namespace enochsolo
