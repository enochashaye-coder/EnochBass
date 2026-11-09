#pragma once
// v2.14.1 Enoch Bass: the TRANSITION PLANNER and SONG SECTIONS.
//
// The bar engine (BassGen.h) writes each bar from the genre's rhythm cell and motif. This pass then looks at every bar line and decides,
// the way a session bassist does, whether the line should MOVE into the next bar, and how:
//   1. a need score at each bar line: a new chord (more for a 4th / 5th or a step), a resolution (V-I, IV-I), the end of a 4- or 8-bar
//      phrase, the end of a section; less when the chord stays and we are mid-phrase, and right after another transition
//   2. the score against Transition Amount gives the chance; Placement can limit where they may happen
//   3. the kind comes from the genre's vocabulary (funk ghost / dead pickups, gospel chromatic walk-ups, jazz approach and enclosure,
//      Afrobeats anticipation, Amapiano space, Kizomba slides, pop octaves...), sized by the style (Subtle .. Virtuosic, Auto = from the
//      bar's density) and checked against what the routed library can play (no ghost or dead notes on a profile without them)
//   4. it is written into the end of the bar, always aiming at the next bar's first note, inside the key (chromatic notes only as half-step
//      approaches that resolve), inside the bass's range and hand
//   5. the last bar of the song resolves (held tonic) or, with Loop on, turns around to bar 1; intro and outro endings leave space
// Every transition note carries the bar it leads out of (Note.trans) so the piano roll can lock it or try another one alone.
#include "Generator.h"

namespace enochsolo {
namespace transitions {

// (sections: Generator.h, namespace transitions)

enum Kind { K_Approach = 0, K_Chromatic, K_Scale, K_Slide, K_Octave, K_Fifth, K_Ghost, K_Dead, K_Antic, K_Rest, K_Fill, K_Count, K_Turnaround = 100, K_Resolve };
inline const char* kindName (int k)
{
    static const char* n[] = { "approach note", "chromatic walk", "scale run", "slide in", "octave jump", "fifth", "ghost pickup", "dead-note pickup",
                               "anticipation", "space, then the downbeat", "fill" };
    if (k == K_Turnaround) return "turnaround";
    if (k == K_Resolve) return "resolution";
    return (k >= 0 && k < K_Count) ? n[k] : "";
}

/** the genre's transition vocabulary: a weight per kind */
inline std::array<double, K_Count> vocabulary (int genre)
{
    //                         appr chrom scale slide oct  5th  ghost dead antic rest fill
    auto W = [] (std::initializer_list<double> l) { std::array<double, K_Count> a {}; int i = 0; for (double x : l) a[(size_t) i++] = x; return a; };
    if (isAmapianoFamily (genre))                                   return W ({ 0.8, 0.2, 0.3, 1.5, 1.0, 0.6, 0.3, 0.1, 2.0, 3.0, 1.0 });
    if (isJazzFamily (genre) && genre != G_JazzFunk && genre != G_SmoothJazz) return W ({ 3.0, 2.5, 1.5, 0.3, 0.3, 1.0, 0.2, 0.1, 0.4, 0.2, 0.6 });
    if (genre == G_Kizomba)                                         return W ({ 1.2, 0.3, 1.0, 3.0, 0.6, 2.0, 0.1, 0.0, 0.6, 0.8, 0.4 });
    switch (baseGenre (genre))
    {
        case G_Funk:      return W ({ 0.8, 3.0, 2.5, 1.5, 1.0, 0.4, 1.0, 0.8, 0.6, 0.4, 1.2 });
        case G_Gospel:    return W ({ 2.0, 3.0, 2.5, 1.0, 0.8, 0.8, 0.5, 0.2, 0.5, 0.2, 1.5 });
        case G_Afrobeat: case G_Highlife: case G_AfroFusion: case G_Juju: case G_Soukous: case G_Township: case G_AfroHouse:
                          return W ({ 1.5, 0.5, 0.6, 0.6, 1.2, 1.0, 1.0, 0.5, 3.0, 1.0, 0.8 });
        case G_Reggae: case G_Caribbean:
                          return W ({ 1.5, 0.3, 1.2, 0.6, 0.8, 1.0, 0.3, 0.2, 0.6, 2.0, 0.5 });
        case G_Pop: case G_Rock:
                          return W ({ 2.0, 0.3, 0.6, 0.4, 2.5, 1.5, 0.3, 0.3, 0.6, 0.8, 1.2 });
        case G_Ballad:    return W ({ 1.5, 0.4, 1.5, 0.8, 0.5, 0.8, 0.1, 0.0, 0.3, 1.5, 0.3 });
        case G_HipHop:    return W ({ 0.8, 0.3, 0.3, 0.5, 1.0, 0.6, 0.8, 0.6, 1.5, 2.0, 0.4 });
        case G_Salsa: case G_AfroCuban: case G_AfroBrazilian: case G_Samba:
                          return W ({ 1.5, 0.4, 0.6, 0.3, 0.5, 1.5, 0.2, 0.2, 3.0, 0.6, 0.5 });
        case G_Bossa:     return W ({ 1.5, 0.5, 0.8, 0.4, 0.4, 2.0, 0.1, 0.0, 1.2, 0.6, 0.3 });
        case G_Flamenco:  return W ({ 1.5, 1.0, 2.0, 0.6, 0.6, 0.8, 0.2, 0.2, 0.6, 0.6, 0.8 });
        case G_AfroJazz: case G_EthioJazz:
                          return W ({ 2.0, 1.5, 1.5, 0.6, 0.6, 1.0, 0.4, 0.2, 1.0, 0.6, 0.8 });
        default:          return W ({ 2.0, 0.6, 1.0, 0.6, 1.0, 1.0, 0.4, 0.3, 0.8, 0.5, 0.6 });
    }
}

struct Report { int boundaries = 0, written = 0; std::array<int, K_Count> kinds {}; int turnaround = 0, resolve = 0; };

inline const ChordSpan& chordAt (const std::vector<ChordSpan>& cs, double beat)
{
    for (auto& c : cs) if (beat + 1e-6 >= c.start && beat < c.end - 1e-6) return c;
    return beat < cs.front().start ? cs.front() : cs.back();
}
inline bool inMaskPc (uint16_t mask, int pitch) { return ((mask >> (((pitch % 12) + 12) % 12)) & 1) != 0; }

/** the planner: runs on the bass line (track 0, main voice) right after the bars are written */
inline Report plan (std::vector<Note>& notes, const Settings& S, const Meter& m, const std::vector<ChordSpan>& chords, int lo, int hi, bool allowGhosts)
{
    Report rep;
    if (chords.empty() || S.bars <= 0) return rep;
    const double amount = std::clamp ((double) S.transAmount, 0.0, 1.0);
    const double bb = m.barBeats, beat = bb / std::max (1, m.num);          // one beat of the meter
    const double s16 = beat / 4.0, e8 = beat / 2.0;
    const auto vocab = vocabulary (S.genre);
    bool prevHad = false;
    // ghost and dead pickups keep within the articulation balance: when the take already has its share (about one note in ten),
    // the transitions use other kinds
    int ghosts = 0, lineN = 0;
    for (auto& n : notes) if (n.track == 0 && n.voice == 0) { ++lineN; if (n.art == A_Ghost) ++ghosts; }

    auto lineIdx = [&] (double a, double b)
    {
        std::vector<size_t> v;
        for (size_t i = 0; i < notes.size(); ++i) if (notes[i].track == 0 && notes[i].voice == 0 && notes[i].start >= a - 1e-6 && notes[i].start < b - 1e-6) v.push_back (i);
        std::sort (v.begin(), v.end(), [&] (size_t x, size_t y) { return notes[x].start < notes[y].start; });
        return v;
    };
    auto fold = [&] (int p, int near) { while (p > hi && p - 12 >= lo) p -= 12; while (p < lo && p + 12 <= hi) p += 12; (void) near; return std::clamp (p, lo, hi); };
    auto nearOct = [&] (int pc, int near) { int p = lo + (((pc - lo) % 12) + 12) % 12; while (p + 12 <= hi && std::abs (p + 12 - near) < std::abs (p - near)) p += 12; return p; };
    auto scaleStep = [&] (int p, int dir, uint16_t mask) { for (int k = 1; k <= 3; ++k) { const int q = p + dir * k; if (inMaskPc (mask, q)) return q; } return p + dir * 2; };

    // the song's ending is decided first, so the transition into the last bar aims at the note the song really ends on
    for (int step = 0; step < S.bars; ++step)
    {
        const int bar = step == 0 ? S.bars - 1 : step - 1;
        const double e = (bar + 1) * bb;                       // the bar line this transition leads into
        const bool last = bar == S.bars - 1;
        const int secHere = sectionOf (S, bar), secNext = last ? -1 : sectionOf (S, bar + 1);
        const bool secEnd = last || secNext != secHere;
        const bool phrase4 = (bar % 4 == 3), phrase8 = (bar % 8 == 7);
        const ChordSpan& cHere = chordAt (chords, e - 0.01);
        const ChordSpan& cNext = chordAt (chords, last ? 0.0 : e + 0.01);
        const bool chordChange = last ? (cNext.rootPc != cHere.rootPc) : (cNext.rootPc != cHere.rootPc || cNext.q != cHere.q);
        const int motion = ((cNext.rootPc - cHere.rootPc) % 12 + 12) % 12;
        ++rep.boundaries;
        // user locks: a bar whose density you set by hand still gets transitions; the lock of a transition is a locked range (processor)
        Rng rng ((uint64_t) S.seed * 6364136223846793005ULL + (uint64_t) (bar + 1) * 1442695040888963407ULL
                 + (uint64_t) (bar < (int) S.transSalt.size() ? S.transSalt[(size_t) bar] : 0) * 2862933555777941757ULL + 77);

        // ---- 1) need score
        double score = 0.0;
        if (chordChange) score += 0.30 + (motion == 5 || motion == 7 ? 0.10 : (motion == 1 || motion == 2 || motion == 10 || motion == 11 ? 0.05 : 0.0));
        if (chordChange && motion == 5) score += 0.20;                                                     // V-I and ii-V: down a fifth (up a fourth)
        if (chordChange && motion == 7) score += 0.10;                                                     // IV-I: down a fourth
        if (phrase4) score += 0.35;                // the end of a phrase is where a bassist moves, chord change or not
        if (phrase8) score += 0.15;
        if (secEnd && ! last) score += 0.40;
        if (! chordChange && ! phrase4 && ! secEnd) score -= 0.30;
        if (prevHad) score -= 0.20;
        // placement: where transitions may go at all
        bool allowed = true;
        switch (S.transPlace)
        {
            case 1: score = std::max (score, 0.45); break;                       // end of every bar
            case 2: allowed = chordChange || last; break;                        // before chord changes
            case 3: allowed = secEnd; break;                                     // before section changes
            case 4: allowed = phrase4 || last; break;                            // before fills (phrase ends)
            default: break;
        }
        // ---- the last bar of the song: always handled
        if (last)
        {
            const bool loopBack = S.loopTake && secHere != SEC_Outro;
            if (amount <= 0.001) { prevHad = false; continue; }
            auto in = lineIdx (bar * bb, e);
            if (in.empty()) continue;
            if (loopBack)
            {
                // turnaround: the fifth of bar 1's chord, then a half step into its root
                const int T = nearOct (cNext.rootPc, notes[in.back()].pitch);
                const double w0 = e - beat;
                for (auto it = notes.begin(); it != notes.end();)
                    if (it->track == 0 && it->voice == 0 && it->start >= w0 - 1e-6 && it->start < e - 1e-6) it = notes.erase (it); else ++it;
                for (auto& n : notes) if (n.track == 0 && n.voice == 0 && n.start < w0 && n.start + n.length > w0) n.length = std::max (0.05, w0 - n.start - 0.02);
                const int v5 = fold (T + 7 <= hi ? T + 7 : T - 5, T);
                const int ap = fold ((T - 1 >= lo ? T - 1 : T + 1), T);
                Note a; a.start = w0; a.length = e8 * 0.9; a.pitch = v5; a.vel = 92; a.track = 0; a.voice = 0; a.role = 2; a.trans = bar; a.weight = 0.5; notes.push_back (a);
                Note b2 = a; b2.start = w0 + e8; b2.pitch = ap; b2.role = 5; notes.push_back (b2);
                ++rep.turnaround; ++rep.written;
            }
            else
            {
                // resolution: the tonic (the key's root) on beat 1, held to the end of the song
                const int first = (int) in.front();
                const int keyRoot = nearOct (S.keyRoot, notes[(size_t) first].pitch);
                for (auto it = notes.begin(); it != notes.end();)
                    if (it->track == 0 && it->voice == 0 && it->start > bar * bb + 1e-6 && it->start < e - 1e-6) it = notes.erase (it); else ++it;
                for (auto& n : notes)
                    if (n.track == 0 && n.voice == 0 && std::abs (n.start - bar * bb) < 1e-6)
                    { n.pitch = fold (keyRoot, keyRoot); n.length = bb - 0.05; n.role = 1; n.trans = bar; n.art = A_Sustain; n.vel = std::min (127, n.vel + 4); }
                ++rep.resolve; ++rep.written;
            }
            continue;
        }
        if (amount <= 0.001 || ! allowed) { prevHad = false; continue; }
        // at the default 50%: a strong move (V-I, a phrase end with a new chord) almost always gets one, a plain mid-phrase chord change
        // about one time in three, a repeated chord mid-phrase almost never
        const double thr = 1.0 - amount * 1.2;
        const double p = std::clamp (0.5 + (score - thr) * 2.5, 0.0, 1.0);
        if (! rng.chance (p)) { prevHad = false; continue; }

        // ---- 2) the target: the next bar's first note (the root it lands on)
        auto nextIn = lineIdx (e, e + beat);
        auto here = lineIdx (bar * bb, e);
        if (here.empty()) { prevHad = false; continue; }
        const int prevP = notes[here.back()].pitch;
        int T = nextIn.empty() ? nearOct (cNext.rootPc, prevP) : notes[nextIn.front()].pitch;
        const bool targetOnLine = ! nextIn.empty() && std::abs (notes[nextIn.front()].start - e) < 1e-6;

        // ---- 3) size (style) and kind
        const double busy = (bar < (int) S.barBusy.size() && S.barBusy[(size_t) bar] >= 0.f) ? S.barBusy[(size_t) bar] : std::clamp (S.busy + sectionBusy (secHere), 0.0, 1.0);
        int style = S.transStyle;             // 1 Subtle, 2 Moderate, 3 Bold, 4 Virtuosic
        if (style <= 0)
        {
            style = busy < 0.3 ? 1 : (busy < 0.62 ? 2 : 3);
            if (busy > 0.85 && (isJazzFamily (S.genre) || baseGenre (S.genre) == G_Gospel || baseGenre (S.genre) == G_Funk)) style = 4;
            if (secEnd || (secNext == SEC_Chorus)) style = std::min (4, style + 1);           // the strongest lead-in before the chorus
        }
        std::array<double, K_Count> w = vocab;
        auto off = [&] (int k) { w[(size_t) k] = 0.0; };
        if (style == 1) { off (K_Chromatic); off (K_Scale); off (K_Fill); off (K_Dead); }
        if (style <= 2) off (K_Fill);
        if (! allowGhosts || S.transArt == 1) { off (K_Ghost); off (K_Dead); }
        { const double gs = lineN > 0 ? (double) ghosts / lineN : 0.0; if (gs > 0.08) { const double k = std::max (0.0, 1.0 - (gs - 0.08) * 12.0); w[K_Ghost] *= k; w[K_Dead] *= k; } }
        if (S.transArt == 1) off (K_Slide);
        if (! targetOnLine || bar + 1 == S.bars - 1) off (K_Antic);       // the song's last downbeat (its ending) is never swallowed
        if (secNext == SEC_Outro || secHere == SEC_Intro) { w[K_Rest] *= 2.5; w[K_Fill] *= 0.3; }
        if (secNext == SEC_Chorus)
        {   // the biggest lead-in of the song: runs and fills, not a single note
            style = std::max (style, busy >= 0.45 ? 4 : 3);
            w[K_Fill] = std::max (w[K_Fill], 1.0) * 6.0; w[K_Scale] = std::max (w[K_Scale], 0.6) * 3.0; w[K_Chromatic] *= 1.5;
            for (int k : { (int) K_Approach, (int) K_Fifth, (int) K_Ghost, (int) K_Dead, (int) K_Antic, (int) K_Rest, (int) K_Slide }) w[(size_t) k] *= 0.3;
        }
        if (! chordChange)
        {
            // over a vamp the end of a phrase is where the bassist plays the fill; inside the phrase only a small push
            if (phrase4) { if (style >= 2) w[K_Fill] = std::max (w[K_Fill], 0.8) * 2.0; w[K_Scale] *= 1.5; w[K_Chromatic] *= 1.2; }
            else { w[K_Approach] *= 0.5; w[K_Chromatic] *= 0.5; w[K_Octave] *= 1.5; w[K_Ghost] *= 1.5; }
        }
        if (std::abs (T - prevP) <= 2) { w[K_Scale] *= 0.3; w[K_Chromatic] *= 0.4; }
        if (phrase4 && style >= 2 && (baseGenre (S.genre) == G_Funk || baseGenre (S.genre) == G_Gospel)) { w[K_Fill] = std::max (w[K_Fill], 1.0) * 2.5; w[K_Scale] *= 1.5; }
        int kind = -1;
        if (S.transType > 0)
        {
            const int forced = S.transType - 1;
            if (forced < K_Count && (w[(size_t) forced] > 0.0 || forced == K_Rest || forced == K_Approach)) kind = forced;
        }
        if (kind < 0)
        {
            double tot = 0; for (double x : w) tot += x;
            if (tot <= 0.0) { prevHad = false; continue; }
            double r = rng.f() * tot;
            for (int k = 0; k < K_Count; ++k) { r -= w[(size_t) k]; if (r <= 0.0) { kind = k; break; } }
            if (kind < 0) kind = K_Approach;
        }

        // ---- 4) write it
        struct NN { double at; double len; int pitch; int role; int art; int vdel; };
        std::vector<NN> add;
        const int dirUp = T >= prevP ? 1 : -1;                                // come from below when the line rises
        // passing notes come from what the two chords' scales share (G major after Dm in C: no F#), so a run never leaves the key
        uint16_t scl = (uint16_t) (cNext.scaleMask & cHere.scaleMask);
        { int cnt = 0; for (int k = 0; k < 12; ++k) cnt += (scl >> k) & 1; if (cnt < 5) scl = cNext.scaleMask ? cNext.scaleMask : cHere.scaleMask; }
        double win = e8;                                                      // how much of the bar it takes
        const bool sixteen = style >= 3 || baseGenre (S.genre) == G_Funk;
        switch (kind)
        {
            case K_Approach:
            {
                const bool chrom = rng.chance (isJazzFamily (S.genre) || baseGenre (S.genre) == G_Gospel ? 0.75 : 0.45);
                const int ap = chrom ? T - dirUp : scaleStep (T, -dirUp, scl);
                add.push_back ({ e - e8, e8 * 0.9, ap, 5, A_Sustain, 0 });
                break;
            }
            case K_Chromatic:
            {
                const int n = (style >= 3 || (sixteen && style >= 2)) ? 3 : 2;
                const double st = sixteen ? s16 : e8;
                win = n * st;
                for (int k = n; k >= 1; --k) add.push_back ({ e - k * st, st * 0.9, T - dirUp * k, 5, A_Sustain, -2 });
                break;
            }
            case K_Scale:
            {
                const int n = style >= 4 ? 6 : (style == 3 ? 4 : 3);
                const double st = (style >= 3 || n > 3) ? s16 : e8;
                win = n * st;
                std::vector<int> ps; int q = T;
                for (int k = 0; k < n; ++k) { q = scaleStep (q, -dirUp, scl); ps.push_back (q); }
                std::reverse (ps.begin(), ps.end());
                for (int k = 0; k < n; ++k) add.push_back ({ e - (n - k) * st, st * 0.9, ps[(size_t) k], 6, A_Sustain, -3 });
                break;
            }
            case K_Slide:
            {
                const int from = T - 2 * dirUp;
                add.push_back ({ e - e8, e8, from, 6, A_Sustain, -2 });
                break;    // the target note gets the slide (below)
            }
            case K_Octave:
            {
                const int R = nearOct (cHere.rootPc, prevP);
                const int up = R + 12 <= hi ? R + 12 : R - 12;
                if (style >= 2) { add.push_back ({ e - beat, e8 * 0.9, R, 1, A_Sustain, 0 }); win = beat; }
                add.push_back ({ e - e8, e8 * 0.85, up, 3, A_Sustain, 4 });
                break;
            }
            case K_Fifth:
            {
                const int f = T + 7 <= hi && dirUp < 0 ? T + 7 : T - 5;
                add.push_back ({ e - e8, e8 * 0.9, f, 2, A_Sustain, 0 });
                break;
            }
            case K_Ghost:
            {
                const int gp = prevP;
                const int n = style >= 2 ? 2 : 1;
                win = n * s16;
                for (int k = n; k >= 1; --k) add.push_back ({ e - k * s16, 0.1, gp, 7, A_Ghost, -30 });
                break;
            }
            case K_Dead:
            {
                add.push_back ({ e - e8, 0.1, prevP, 7, A_Ghost, -24 });
                add.push_back ({ e - s16, s16 * 0.9, T - dirUp, 5, A_Sustain, -4 });
                break;
            }
            case K_Antic:
            {
                // the next root arrives an eighth early and rings over the bar line (its downbeat note is not struck again)
                add.push_back ({ e - e8, e8 + (targetOnLine ? notes[nextIn.front()].length : beat), T, 1, A_Sustain, 6 });
                break;
            }
            case K_Rest:
            {
                win = beat;
                break;            // the last beat is left empty; the downbeat is struck harder (below)
            }
            case K_Fill:
            {
                win = 2.0 * beat;
                const int R = nearOct (cHere.rootPc, prevP);
                const int f5 = R + 7 <= hi ? R + 7 : R - 5;
                const int o = R + 12 <= hi ? R + 12 : R;
                const bool melodicFill = baseGenre (S.genre) == G_Funk || baseGenre (S.genre) == G_Gospel || isJazzFamily (S.genre)
                                       || baseGenre (S.genre) == G_Flamenco || S.genre == G_AfroJazz || S.genre == G_EthioJazz;
                if (style >= 4 || melodicFill)
                {
                    // a stepwise 16th run into the next root (funk, gospel, jazz: the fretting hand, played legato)
                    const int nRun = style >= 4 ? 8 : 6;
                    win = nRun * s16;
                    std::vector<int> ps; int q = T; for (int k = 0; k < nRun; ++k) { q = scaleStep (q, -dirUp, scl); ps.push_back (q); }
                    std::reverse (ps.begin(), ps.end());
                    for (int k = 0; k < nRun; ++k) add.push_back ({ e - (nRun - k) * s16, s16 * 0.9, ps[(size_t) k], 6, A_Sustain, -3 });
                }
                else if (false)
                {
                    std::vector<int> ps; int q = T; for (int k = 0; k < 8; ++k) { q = scaleStep (q, -dirUp, scl); ps.push_back (q); }
                    std::reverse (ps.begin(), ps.end());
                    for (int k = 0; k < 8; ++k) add.push_back ({ e - 2.0 * beat + k * s16, s16 * 0.9, ps[(size_t) k], 6, A_Sustain, -3 });
                }
                else
                {
                    add.push_back ({ e - 2.0 * beat, e8 * 0.9, R, 1, A_Sustain, 2 });
                    add.push_back ({ e - 1.5 * beat, e8 * 0.9, f5, 2, A_Sustain, -2 });
                    add.push_back ({ e - 1.0 * beat, e8 * 0.9, o, 3, A_Sustain, 2 });
                    add.push_back ({ e - 0.5 * beat, e8 * 0.9, T - dirUp, 5, A_Sustain, 0 });
                }
                break;
            }
            default: break;
        }
        const double w0 = e - win;
        // never over a new chord that starts inside the window: it keeps its root
        bool chordInside = false; for (auto& c : chords) if (c.start > w0 + 1e-6 && c.start < e - 1e-6) chordInside = true;
        if (chordInside && kind != K_Rest) { win = e8; }
        const double wStart = e - std::min (win, chordInside ? e8 : win);
        // clear the window on the line, shorten what rings into it
        int velRef = 0, velN = 0;
        for (auto i : here) { velRef += notes[i].vel; ++velN; }
        velRef = velN > 0 ? velRef / velN : 90;
        for (auto it = notes.begin(); it != notes.end();)
            if (it->track == 0 && it->voice == 0 && it->start >= wStart - 1e-6 && it->start < e - 1e-6) it = notes.erase (it); else ++it;
        for (auto& n : notes) if (n.track == 0 && n.voice == 0 && n.start < wStart && n.start + n.length > wStart - 1e-6) n.length = std::max (0.05, wStart - n.start - 0.02);
        const int vHum = (int) std::lround (S.transHumanize * 8.0);
        for (auto& a : add)
        {
            if (a.at < wStart - 1e-6) continue;
            Note n; n.start = a.at; n.length = std::max (0.06, a.len); n.pitch = fold (a.pitch, T); n.track = 0; n.voice = 0; n.role = a.role; n.art = a.art;
            n.vel = std::clamp (velRef + a.vdel + sectionVel (secHere) + (vHum > 0 ? rng.i (-vHum, vHum) : 0), 25, 127);
            n.weight = std::abs (std::fmod (a.at, beat)) < 1e-6 ? 0.5 : 0.25; n.trans = bar;
            // a quick run by steps is played with the fretting hand: hammer-ons up, pull-offs down (not on its first note)
            if (S.transArt != 1 && (kind == K_Scale || kind == K_Chromatic || kind == K_Fill) && a.len <= s16 + 1e-6 && &a != &add.front())
            {
                const int prevPitch = (&a - 1)->pitch;
                if (std::abs (a.pitch - prevPitch) >= 1 && std::abs (a.pitch - prevPitch) <= 2) n.art = a.pitch > prevPitch ? A_HammerOn : A_PullOff;
            }
            // inside the key: a note outside the next chord's scale must resolve by a half step into the target
            if (n.art != A_Ghost && ! inMaskPc (scl, n.pitch) && ! inMaskPc (cHere.scaleMask, n.pitch) && std::abs (n.pitch - T) != 1)
                n.pitch = fold (scaleStep (n.pitch, n.pitch < T ? 1 : -1, scl), T);
            notes.push_back (n);
        }
        // the target note: struck harder after space, slid into, or absorbed by the anticipation
        if (targetOnLine)
        {
            auto& tn = notes[nextIn.front()];
            // (indexes are stale after erase/push: find it again by time)
            for (auto& x : notes)
                if (x.track == 0 && x.voice == 0 && std::abs (x.start - e) < 1e-6 && x.trans != bar)
                {
                    if (kind == K_Rest) { x.vel = std::min (127, x.vel + 8); x.trans = bar; }     // space, then the downbeat: the accent is the transition
                    // a step into the downbeat is played legato: the last note of the walk / run / approach rings into it and the target is
                    // hammered on (up) or pulled off (down), the way the fretting hand finishes a run
                    if (S.transArt != 1 && (kind == K_Approach || kind == K_Chromatic || kind == K_Scale || kind == K_Fill) && ! add.empty() && x.art == A_Sustain)
                    {
                        const int lastP = fold (add.back().pitch, T);
                        const int iv = x.pitch - lastP;
                        if (std::abs (iv) >= 1 && std::abs (iv) <= 2 && add.back().len <= e8 + 1e-6)
                        {
                            x.art = iv > 0 ? A_HammerOn : A_PullOff; x.trans = bar;
                            for (auto& y : notes) if (y.trans == bar && y.track == 0 && y.voice == 0 && std::abs (y.start - add.back().at) < 1e-6) y.length = std::max (y.length, e - y.start);
                        }
                    }
                    if (kind == K_Slide && S.transArt != 1) { x.art = A_Slide; x.trans = bar; }
                    if (kind == K_Antic) x.mute = true;                         // played by the anticipated note
                    break;
                }
            (void) tn;
        }
        notes.erase (std::remove_if (notes.begin(), notes.end(), [&] (const Note& n) { return n.mute && n.track == 0 && n.voice == 0 && std::abs (n.start - e) < 1e-6 && kind == K_Antic; }), notes.end());
        std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        ++rep.written; ++rep.kinds[(size_t) kind];
        if (kind == K_Ghost || kind == K_Dead) ghosts += kind == K_Ghost ? (style >= 2 ? 2 : 1) : 1;
        prevHad = true;
    }
    // a hand never stretches more than an octave and a fifth between two neighbouring line notes inside a transition
    for (size_t i = 1; i < notes.size(); ++i)
        if (notes[i].trans >= 0 && notes[i].track == 0 && notes[i].voice == 0)
        {
            int j = (int) i - 1; while (j >= 0 && ! (notes[(size_t) j].track == 0 && notes[(size_t) j].voice == 0)) --j;
            if (j >= 0) while (std::abs (notes[i].pitch - notes[(size_t) j].pitch) > 19) notes[i].pitch += notes[i].pitch > notes[(size_t) j].pitch ? -12 : 12;
            notes[i].pitch = std::clamp (notes[i].pitch, lo, hi);
        }
    return rep;
}

} // namespace transitions

namespace detail
{
    inline int planTransitions (std::vector<Note>& notes, const Settings& S, const Meter& m, const std::vector<ChordSpan>& chords, int lo, int hi, bool allowGhosts)
    { return transitions::plan (notes, S, m, chords, lo, hi, allowGhosts).written; }
}
} // namespace enochsolo
