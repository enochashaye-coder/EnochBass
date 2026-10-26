#pragma once
// Second voice (JUCE-free): parallel harmony, a horn section, or a "call and response" line that answers the solo in its rests.
// It is written on the accompaniment track (track 1) so it can be sent to its own instrument.
#include "Generator.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace enochsolo
{
enum SecondVoice { SV_Off = 0, SV_Below3, SV_Above3, SV_Horns, SV_CallResponse, SV_Count };

inline const char* secondVoiceName (int v)
{
    static const char* n[] = { "Off", "Harmony: 3rd below", "Harmony: 3rd above", "Horn section (3 parts)", "Call and response" };
    return n[std::clamp (v, 0, (int) SV_Count - 1)];
}

namespace detail
{
/** move `steps` scale steps (negative = down) inside a pitch-class mask */
inline int stepInMask (uint16_t mask, int pitch, int steps)
{
    const int dir = steps < 0 ? -1 : 1;
    int p = pitch, left = std::abs (steps);
    // start from a note that belongs to the scale
    for (int guard = 0; ! inMask (mask, p) && guard < 12; ++guard) p -= dir;
    while (left > 0 && p > 0 && p < 127) { p += dir; if (inMask (mask, p)) --left; }
    return p;
}
inline int scaleStepsBetween (uint16_t mask, int a, int b)       // signed number of scale steps from a to b
{
    int n = 0; const int dir = b >= a ? 1 : -1;
    for (int p = a; p != b; p += dir) if (inMask (mask, p + dir)) n += dir;
    return n;
}
inline int nearestInMask (uint16_t mask, int pitch)
{
    for (int d = 0; d < 7; ++d) { if (inMask (mask, pitch + d)) return pitch + d; if (inMask (mask, pitch - d)) return pitch - d; }
    return pitch;
}
inline bool plainArt (int a) { return a == A_Sustain || a == A_Legato || a == A_Staccato || a == A_Marcato || a == A_Vibrato; }
inline int harmonyArt (int a, bool horns)
{
    if (plainArt (a)) return a == A_Vibrato ? A_Sustain : a;
    if (horns && (a == A_Fall || a == A_Doit || a == A_Scoop)) return a;
    return A_Sustain;
}
}

inline void applySecondVoice (Pattern& p, const Settings& s, int rangeShift = 0)
{
    p.raw.erase (std::remove_if (p.raw.begin(), p.raw.end(), [] (const Note& n) { return n.track == 1; }), p.raw.end());
    p.hasComp = (s.mode == 1 || s.mode == 2) && s.voice2 == SV_Off;
    if (s.voice2 == SV_Off || p.chords.empty()) return;
    Range hr = instRange (s.instrument, styleFor (s.instrument, s.artist).variant);
    hr.lo = std::max (0, hr.lo + std::min (0, rangeShift)); hr.hi = std::min (127, hr.hi + std::max (0, rangeShift));   // notes already moved by the Octave setting
    std::vector<Note> mel;
    for (auto& n : p.raw) if (n.track == 0 && n.voice == 0 && ! n.mute) mel.push_back (n);
    std::stable_sort (mel.begin(), mel.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    if (mel.empty()) return;
    std::vector<Note> out;
    auto add = [&] (const Note& src, int pitch, double start, double len, double velScale, int art)
    {
        if (pitch < hr.lo || pitch > hr.hi || len < 0.05) return;
        Note n = src; n.pitch = pitch; n.start = start; n.length = len; n.track = 1; n.voice = 0;
        n.vel = std::clamp ((int) std::lround (src.vel * velScale), 20, 120); n.art = art; n.bend = 0; n.vib = false; n.mute = false;
        out.push_back (n);
    };

    if (s.voice2 == SV_Below3 || s.voice2 == SV_Above3 || s.voice2 == SV_Horns)
    {
        std::vector<int> steps;
        if (s.voice2 == SV_Below3) steps = { -2 }; else if (s.voice2 == SV_Above3) steps = { 2 }; else steps = { -2, -4 };
        for (auto& n : mel)
        {
            if (n.length < 0.12 || n.art == A_Ghost || n.art == A_Trill || n.art == A_Tremolo || n.art == A_Shake || n.art == A_Growl
                || n.art == A_Flutter || n.art == A_Arpeggio || n.art == A_Strum) continue;
            const ChordSpan& c = p.chordAt (n.start + 0.01);
            std::vector<int> used;
            for (int st : steps)
            {
                int hp = detail::stepInMask (c.scaleMask, n.pitch, st);
                // strong beats: sit on a chord tone when one is close
                if (n.weight >= 0.6 && n.length >= 0.4)
                    for (int d = 0; d <= 2; ++d)
                    {
                        if (inMask (c.toneMask, hp + d) && std::abs (n.pitch - (hp + d)) >= 3) { hp += d; break; }
                        if (inMask (c.toneMask, hp - d) && std::abs (n.pitch - (hp - d)) >= 3) { hp -= d; break; }
                    }
                // keep a musical distance from the lead and from the other harmony voice
                for (int guard = 0; guard < 4 && std::abs (n.pitch - hp) < 3; ++guard) hp = detail::stepInMask (c.scaleMask, hp, st < 0 ? -1 : 1);
                bool clash = false; for (int u : used) if (std::abs (u - hp) < 3) clash = true;
                if (clash) hp = detail::stepInMask (c.scaleMask, used.empty() ? hp : *std::min_element (used.begin(), used.end()), -1);
                if (hp < hr.lo) hp += 12;
                if (hp > hr.hi) hp -= 12;
                if (std::abs (n.pitch - hp) < 3)      // the octave fold landed on the lead: try the other octave, else leave this note alone
                {
                    const int alt = hp >= n.pitch ? hp - 12 : hp + 12;
                    if (alt >= hr.lo && alt <= hr.hi && std::abs (n.pitch - alt) >= 3) hp = alt; else continue;
                }
                bool clash2 = false; for (int u : used) if (std::abs (u - hp) < 3) clash2 = true;
                if (clash2) continue;
                used.push_back (hp);
                add (n, hp, n.start, n.length * 0.97, s.voice2 == SV_Horns ? 0.84 : 0.8, detail::harmonyArt (n.art, s.voice2 == SV_Horns));
            }
        }
    }
    else    // call and response: the second voice answers the phrase in the rest that follows it
    {
        const double bb = std::max (0.5, p.barBeats);
        const double restGap = std::max (0.75, bb * 0.2);
        std::vector<std::pair<size_t, size_t>> ph; size_t a = 0;
        for (size_t i = 1; i < mel.size(); ++i)
            if (mel[i].start - (mel[i - 1].start + mel[i - 1].length) >= restGap) { ph.push_back ({ a, i }); a = i; }
        ph.push_back ({ a, mel.size() });
        for (size_t k = 0; k < ph.size(); ++k)
        {
            const size_t i0 = ph[k].first, i1 = ph[k].second;
            if (i1 - i0 < 3) continue;
            const double callEnd = mel[i1 - 1].start + mel[i1 - 1].length;
            const double limit = (k + 1 < ph.size()) ? mel[ph[k + 1].first].start : p.lengthBeats;
            const double room = limit - callEnd;
            if (room < 1.0) continue;
            double t0 = std::ceil ((callEnd + 0.2) * 2.0 - 1e-9) / 2.0;                 // answer starts on the next eighth after a small breath
            const double end = std::min (limit - 0.25, t0 + std::min (room - 0.45, 1.5 * bb));
            if (end - t0 < 0.7) continue;
            // the answer reuses the rhythm of the call's opening notes, turned upside down (intervals inverted), ending on a chord tone
            const size_t take = std::min<size_t> (5, i1 - i0);
            const double base = mel[i0].start;
            const ChordSpan& c0 = p.chordAt (t0 + 0.01);
            int cur = detail::nearestInMask (c0.toneMask, mel[i0].pitch - 5);
            std::vector<Note> ans;
            for (size_t j = 0; j < take; ++j)
            {
                const Note& src = mel[i0 + j];
                const double st = t0 + (src.start - base) * 0.8;
                if (st >= end - 0.1) break;
                const ChordSpan& c = p.chordAt (st + 0.01);
                if (j > 0)
                {
                    const int ds = detail::scaleStepsBetween (c.scaleMask, mel[i0 + j - 1].pitch, src.pitch);
                    cur = detail::stepInMask (c.scaleMask, cur, -ds);
                }
                const double nextSt = (j + 1 < take) ? t0 + (mel[i0 + j + 1].start - base) * 0.8 : end;
                Note n = src; n.pitch = cur; n.start = st; n.length = std::max (0.1, std::min (nextSt, end) - st) * 0.92;
                ans.push_back (n);
            }
            if (ans.size() < 2) continue;
            // resolve: last note to the nearest chord tone, held until the end of the answer
            {
                Note& l = ans.back(); const ChordSpan& c = p.chordAt (l.start + 0.01);
                int tgt = l.pitch; for (int d = 0; d < 7; ++d) { if (inMask (c.toneMask, l.pitch + d)) { tgt = l.pitch + d; break; } if (inMask (c.toneMask, l.pitch - d)) { tgt = l.pitch - d; break; } }
                l.pitch = tgt; l.length = std::max (l.length, std::min (end - l.start, 1.0));
            }
            for (auto& n : ans) add (n, n.pitch, n.start, n.length, 0.9, detail::harmonyArt (n.art, false));
        }
    }
    for (auto& n : out) n.id = p.nextId++;
    p.raw.insert (p.raw.end(), out.begin(), out.end());
    std::stable_sort (p.raw.begin(), p.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    p.hasComp = true;
}
} // namespace enochsolo
