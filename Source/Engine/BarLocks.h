#pragma once
// v3.4 bar locks (identical in Enoch Solo and Enoch Bass).
//   scale lock - the notes of the bar stay inside the chord scale. Dragging steps to the next scale tone in the
//                direction of the drag; notes that are outside the scale are fitted to the nearest scale tone.
//   bar lock   - the notes of the bar are kept exactly as they are when the song is regenerated (genre, artist,
//                key, instrument, ...). Editing a locked bar updates its lock.
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "Generator.h"
#include "Theory.h"

namespace enochsolo { namespace barlock {

// bar index -> that bar's notes; the starts are relative to the first beat of the bar
using Saved = std::map<int, std::vector<Note>>;

// Note.role exists in Enoch Bass only (the bass job of a note); Solo notes have none, so it is read and written when present
template <class N, class = void> struct HasRole : std::false_type {};
template <class N> struct HasRole<N, std::void_t<decltype (std::declval<N&> ().role)>> : std::true_type {};
template <class N> inline int roleOf (const N& n) { if constexpr (HasRole<N>::value) return n.role; else return 0; }
template <class N> inline void setRole (N& n, int r) { if constexpr (HasRole<N>::value) n.role = r; else (void) r; }

inline double barLen (const Pattern& p) { return std::max (0.5, p.barBeats); }

inline int barIndexOf (const Pattern& p, double beat) { return (int) std::floor (beat / barLen (p) + 1e-6); }

// copy the notes of the given bars out of p.raw (every bar in the set gets an entry, even an empty one: an empty locked bar stays silent)
inline Saved captureBars (const Pattern& p, const std::set<int>& bars)
{
    Saved out;
    for (int b : bars) out[b] = {};
    for (const auto& n : p.raw)
    {
        const int b = barIndexOf (p, n.start);
        if (bars.count (b) == 0) continue;
        Note c = n;
        c.start = n.start - b * barLen (p);
        out[b].push_back (c);
    }
    return out;
}

// replace the notes of the saved bars in p.raw with the saved notes
inline void restoreBars (Pattern& p, const Saved& saved)
{
    if (saved.empty()) return;
    p.raw.erase (std::remove_if (p.raw.begin(), p.raw.end(), [&] (const Note& n) { return saved.count (barIndexOf (p, n.start)) > 0; }), p.raw.end());
    for (const auto& kv : saved)
    {
        if (kv.first < 0 || kv.first >= p.bars) continue;
        for (Note n : kv.second)
        {
            n.start += kv.first * barLen (p);
            n.id = p.nextId++;
            p.raw.push_back (n);
        }
    }
    std::stable_sort (p.raw.begin(), p.raw.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
}

// chord-scale pitch classes at a beat (0 = no chords yet, so nothing is constrained)
inline uint16_t scaleMaskAt (const Pattern& p, double beat)
{
    return p.chords.empty() ? (uint16_t) 0 : p.chordAt (beat).scaleMask;
}

// nearest pitch inside the mask: dir > 0 looks up only, dir < 0 looks down only, dir == 0 takes the nearest (a tie goes up)
inline int snapInScale (int pitch, uint16_t mask, int dir)
{
    pitch = std::clamp (pitch, 0, 127);
    if (mask == 0 || inMask (mask, pitch)) return pitch;
    for (int d = 1; d <= 12; ++d)
    {
        const int up = pitch + d, dn = pitch - d;
        if (dir >= 0 && up <= 127 && inMask (mask, up)) return up;
        if (dir <= 0 && dn >= 0 && inMask (mask, dn)) return dn;
    }
    return pitch;
}

// fit every note of the scale-locked bars (or of every bar when allBars) into its chord scale; returns how many notes moved
inline int enforceScale (Pattern& p, const std::set<int>& scaleBars, bool allBars)
{
    int moved = 0;
    for (auto& n : p.raw)
    {
        if (! allBars && scaleBars.count (barIndexOf (p, n.start)) == 0) continue;
        const uint16_t m = scaleMaskAt (p, n.start);
        if (m == 0 || inMask (m, n.pitch)) continue;
        n.pitch = snapInScale (n.pitch, m, 0);
        ++moved;
    }
    return moved;
}

// ---- state text (the project file stores locks as plain text)
inline std::string saveSet (const std::set<int>& s)
{
    std::ostringstream o;
    for (int b : s) o << b << ' ';
    return o.str();
}

inline std::set<int> loadSet (const std::string& t)
{
    std::set<int> s;
    std::istringstream i (t);
    int b = 0;
    while (i >> b) s.insert (b);
    return s;
}

inline std::string saveSaved (const Saved& sv)
{
    std::ostringstream o;
    o.precision (17);
    for (const auto& kv : sv)
    {
        o << kv.first << ' ' << kv.second.size() << ' ';
        for (const auto& n : kv.second)
            o << n.start << ' ' << n.length << ' ' << n.pitch << ' ' << n.vel << ' ' << n.art << ' ' << n.track << ' ' << n.voice << ' '
              << (n.mute ? 1 : 0) << ' ' << n.weight << ' ' << n.bend << ' ' << (n.vib ? 1 : 0) << ' ' << roleOf (n) << ' ';
    }
    return o.str();
}

inline Saved loadSaved (const std::string& t)
{
    Saved sv;
    std::istringstream i (t);
    int bar = 0;
    size_t count = 0;
    while (i >> bar >> count)
    {
        auto& v = sv[bar];
        for (size_t k = 0; k < count; ++k)
        {
            Note n;
            int mute = 0, vib = 0, role = 0;
            if (! (i >> n.start >> n.length >> n.pitch >> n.vel >> n.art >> n.track >> n.voice >> mute >> n.weight >> n.bend >> vib >> role))
                return sv;
            n.mute = mute != 0; n.vib = vib != 0; setRole (n, role);
            v.push_back (n);
        }
    }
    return sv;
}

}} // namespace enochsolo::barlock
