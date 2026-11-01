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
enum Subdiv { SD_Auto = 0, SD_16, SD_16T, SD_32, SD_32T };

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
} // namespace rhythm
} // namespace enochsolo
