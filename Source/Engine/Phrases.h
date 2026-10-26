#pragma once
// Phrase detection and phrase locking (JUCE-free).
//
// A "phrase" is a run of main-line notes (track 0, voice 0) between two rests. Long unbroken runs are cut at the most natural spot
// (near a bar line, preferably at a small gap) so that no phrase is longer than about two bars. Every phrase owns a region of the
// timeline (the rests are shared half and half) so that locking a phrase also keeps the accompaniment under it.
#include "Generator.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace enochsolo
{

struct Phrase
{
    double t0 = 0, t1 = 0;      // first note start .. last note end
    double r0 = 0, r1 = 0;      // the region this phrase owns: [r0, r1)
    int notes = 0;
    int bar0 = 0;               // bar the phrase starts in
};

using LockRanges = std::vector<std::pair<double, double>>;

inline std::vector<Phrase> findPhrases (const Pattern& p)
{
    std::vector<Phrase> out;
    std::vector<const Note*> m;
    for (auto& n : p.raw) if (n.track == 0 && n.voice == 0) m.push_back (&n);
    if (m.empty()) return out;
    std::stable_sort (m.begin(), m.end(), [] (const Note* a, const Note* b) { return a->start < b->start; });

    const double bb = std::max (0.5, p.barBeats);
    const double restGap = std::max (0.75, bb * 0.2);                // a rest this long ends a phrase
    auto endOf = [&] (size_t i) { return m[i]->start + m[i]->length; };

    std::vector<std::pair<size_t, size_t>> segs;                    // [a, b) index ranges
    size_t a = 0;
    for (size_t i = 1; i < m.size(); ++i)
        if (m[i]->start - endOf (i - 1) >= restGap) { segs.push_back ({ a, i }); a = i; }
    segs.push_back ({ a, m.size() });

    // cut long runs
    std::vector<std::pair<size_t, size_t>> fin;
    std::vector<std::pair<size_t, size_t>> stack (segs.rbegin(), segs.rend());
    while (! stack.empty())
    {
        const auto sg = stack.back(); stack.pop_back();
        const double span = endOf (sg.second - 1) - m[sg.first]->start;
        if (span <= 2.0 * bb + 0.01 || sg.second - sg.first < 2) { fin.push_back (sg); continue; }
        const double mid = (m[sg.first]->start + endOf (sg.second - 1)) * 0.5;
        int best = -1; double bestScore = 1e18;
        for (size_t k = sg.first + 1; k < sg.second; ++k)
        {
            const double s0 = m[k]->start;
            if (s0 - m[sg.first]->start < 0.75 * bb || endOf (sg.second - 1) - s0 < 0.75 * bb) continue;
            const double prox = std::abs (s0 - std::round (s0 / bb) * bb);
            const double gap = std::max (0.0, s0 - endOf (k - 1));
            const double score = prox - gap * 2.0 + std::abs (s0 - mid) / bb * 0.5;
            if (score < bestScore) { bestScore = score; best = (int) k; }
        }
        if (best < 0) { fin.push_back (sg); continue; }
        stack.push_back ({ (size_t) best, sg.second });
        stack.push_back ({ sg.first, (size_t) best });
    }
    std::sort (fin.begin(), fin.end());

    for (auto& sg : fin)
    {
        Phrase ph; ph.t0 = m[sg.first]->start; ph.t1 = endOf (sg.second - 1);
        for (size_t i = sg.first; i < sg.second; ++i) ph.t1 = std::max (ph.t1, endOf (i));
        ph.notes = (int) (sg.second - sg.first);
        ph.bar0 = p.barOf (ph.t0);
        out.push_back (ph);
    }
    for (size_t i = 0; i < out.size(); ++i)
    {
        out[i].r0 = i == 0 ? 0.0 : out[i - 1].r1;
        if (i + 1 == out.size()) out[i].r1 = std::max (p.lengthBeats, out[i].t1);
        else
        {
            const double gap = out[i + 1].t0 - out[i].t1;
            out[i].r1 = gap > 0.2 ? (out[i].t1 + out[i + 1].t0) * 0.5 : out[i + 1].t0;
        }
    }
    return out;
}

inline bool inLockRanges (const LockRanges& r, double t)
{
    for (auto& x : r) if (t >= x.first - 1e-6 && t < x.second - 1e-6) return true;
    return false;
}

inline bool phraseLocked (const LockRanges& r, const Phrase& ph) { return inLockRanges (r, (ph.t0 + ph.t1) * 0.5); }

/** lock or unlock the phrase: ranges that touch its middle are dropped first, then (when locking) its own region is added */
inline void setPhraseLocked (LockRanges& r, const Phrase& ph, bool lock)
{
    const double mid = (ph.t0 + ph.t1) * 0.5;
    r.erase (std::remove_if (r.begin(), r.end(), [&] (const std::pair<double, double>& x) { return mid >= x.first - 1e-6 && mid < x.second - 1e-6; }), r.end());
    if (lock) r.push_back ({ ph.r0, ph.r1 });
}

/** Take a freshly generated pattern and put back, from `old`, every note (all tracks except the chord pad) that starts inside a locked range.
    New notes that start inside a locked range are dropped; new notes that would run into a locked range are shortened. */
inline void mergeLocked (Pattern& fresh, const Pattern& old, const LockRanges& keep)
{
    if (keep.empty()) return;
    auto keepStart = [&] (double s) -> double       // start of the first locked range after s (or a huge value)
    {
        double best = 1e18;
        for (auto& x : keep) if (x.first > s + 1e-9) best = std::min (best, x.first);
        return best;
    };
    std::vector<Note> out;
    for (auto n : fresh.raw)
    {
        if (n.track == 2) { out.push_back (n); continue; }
        if (inLockRanges (keep, n.start)) continue;
        const double lim = keepStart (n.start);
        if (n.start + n.length > lim) n.length = std::max (0.05, lim - n.start);
        out.push_back (n);
    }
    for (auto n : old.raw)
    {
        if (n.track == 2 || ! inLockRanges (keep, n.start)) continue;
        n.id = fresh.nextId++;
        out.push_back (n);
    }
    std::stable_sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });

    // keep each monophonic line tidy: a note never runs into the next one of its line
    for (int track = 0; track < 2; ++track)
    {
        std::vector<Note*> line;
        for (auto& n : out) if (n.track == track && n.voice == 0) line.push_back (&n);
        for (size_t i = 0; i + 1 < line.size(); ++i)
            if (line[i]->start + line[i]->length > line[i + 1]->start + 1e-9 && line[i + 1]->start - line[i]->start > 1e-6)
                line[i]->length = std::max (0.05, line[i + 1]->start - line[i]->start);
    }
    fresh.raw = std::move(out);
}

} // namespace enochsolo
