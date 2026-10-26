#pragma once
// Groove-aware fit (JUCE-free): a loop of your project (drums, bass, keys: audio or MIDI) becomes a per-step map of where its hits fall,
// so the solo can lock with them, leave room where the loop is busy and swing like it does.
#include "Reference.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace enochsolo
{
struct GrooveView
{
    bool valid = false;
    int bars = 0, steps = 16;
    double hitsPerBar = 0;
    double swing = 0; bool swingKnown = false;
    std::vector<float> occ;      // fraction of bars that have a hit on this 16th step (0..1)
    std::vector<float> str;      // average loudness of the hits on this step (0..1)
};

inline GrooveView makeGrooveView (const RefProfile& p, double barBeats, int tempoMode, double shift)
{
    GrooveView g;
    const RefView rv = makeRefView (p, barBeats, tempoMode, shift);
    if (! rv.valid || rv.bars < 1) return g;
    g.steps = std::max (4, (int) std::lround (rv.barBeats * 4.0));
    g.bars = rv.bars;
    g.occ.assign ((size_t) g.steps, 0.f); g.str.assign ((size_t) g.steps, 0.f);
    std::vector<int> cnt ((size_t) g.steps, 0);
    for (auto& n : rv.notes)
    {
        const int s = (int) std::lround (n.pos * 4.0);
        if (s < 0 || s >= g.steps) continue;
        ++cnt[(size_t) s]; g.str[(size_t) s] += n.loud;
    }
    double total = 0;
    for (int s = 0; s < g.steps; ++s)
    {
        if (cnt[(size_t) s] > 0) g.str[(size_t) s] /= (float) cnt[(size_t) s];
        g.occ[(size_t) s] = std::min (1.f, (float) cnt[(size_t) s] / (float) g.bars);
        total += g.occ[(size_t) s];
    }
    g.hitsPerBar = total;
    g.swing = rv.swing; g.swingKnown = rv.swingKnown;
    g.valid = true;
    return g;
}

inline std::string describeGroove (const RefProfile& p, int tempoMode, double shift, double barBeats = 4.0)
{
    const auto g = makeGrooveView (p, barBeats, tempoMode, shift);
    if (! g.valid) return "no groove found";
    const auto v = makeRefView (p, barBeats, tempoMode, shift);
    char b[200];
    std::snprintf (b, sizeof (b), "%.0f BPM  |  %.0f hits/bar  |  %s", v.bpm, g.hitsPerBar,
                   g.swingKnown ? (g.swing < 0.2 ? "straight" : (g.swing < 0.6 ? "light swing" : "swung")) : "feel n/a");
    return b;
}
} // namespace enochsolo
