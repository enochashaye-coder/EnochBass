#pragma once
// Enoch Bass - the bass-line generator (JUCE-free).
// A bass line is built bar by bar from a rhythm cell of the genre (16 steps: R root, f fifth, o octave, t third, b seventh, a approach / passing note,
// x any chord or scale tone, g ghost note, . rest), turned into pitches by the chord of the bar, then dressed with articulations.
#include "Generator.h"
#include "Groove.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

namespace enochsolo
{
enum BassRole { BR_None = 0, BR_Root, BR_Fifth, BR_Octave, BR_Third, BR_Approach, BR_Other, BR_Ghost, BR_Harmonic, BR_Seventh };

namespace bassdetail
{
inline const std::vector<std::string>& bassCells (int genre)
{
    static const std::vector<std::string> afrobeat = {
        "R..R..R...R.f.o.", "R.g.R..Rg..R.f.", "R..R..a.R..f..R.", "R.R..R.R..f.R.o.", "R...R.R..R.R..f.", "Rg.R.g.R..g.R.fa", "R..R.gR...R.f.a." };
    static const std::vector<std::string> afrofusion = {
        "R.....R..R....f.", "R..R.....R..R.f.", "R.......R..R..a.", "R...R.....R..R..", "R..R..R.......f.", "R.....R..R..f..a" };
    static const std::vector<std::string> amapiano = {
        "R..R....R..R.f..", "R.....R.R.....a.", ".R..R..R..R.f...", "R..R..R...R..o..", "R.....R..R..R.f.", "R...R..R....R.fa" };
    static const std::vector<std::string> highlife = {
        "R..f.R..f..R.f..", "R.f.R.f.R.f.R.f.", "R.f..R.f.R.f..R.", "R..fR..f..R.f.R.", "R.f.o.f.R.f.o.f." };
    static const std::vector<std::string> juju = {
        "R.R.f.R.R.f.o.R.", "R..Rf..RR..f.o.R", "R.R.R.f.R.R.f.a.", "R.f.R.R.f.R.o.f." };
    static const std::vector<std::string> soukous = {
        "R.oRf.oRR.o.f.oR", "R.o.R.o.f.o.R.o.", "RgoRgfoR.go.fgoR", "R.oRf.oR.fo.R.oa" };
    static const std::vector<std::string> ethio = {
        "R..f..R..o..f..a", "R...f..R..f...a.", "R..f..R.f..R..oa", "R..t..f..R..f..a" };
    static const std::vector<std::string> township = {
        "R.f.R.f.R.f.R.f.", "R.f.Rtf.R.f.R.a.", "R.f.t.f.R.f.o.fa", "R.t.f.t.R.f.t.a." };
    static const std::vector<std::string> gospel = {
        "R...f...R.a.f...", "R..t..f.R...a.f.", "R.....f.t...f.a.", "R...t.a.f...R.a." };
    static const std::vector<std::string> pop = {
        "R..R..R...R.R...", "R.R...R.R.R...R.", "R...R.R.R...R.a.", "R..R..R...R..f.a", "R..R.R..R...R..f" };
    static const std::vector<std::string> rock = {
        "R.R.R.R.R.R.R.R.", "R.R.R.R.R.R.R.o.", "R.R.R.RgR.R.R.fa", "R..RR.R.R..RR.R.", "R.R.R.f.R.R.R.o." };
    static const std::vector<std::string> ballad = {
        "R.......f...a...", "R...........f.a.", "R.......R...f..a", "R.....f.....R..a" };
    static const std::vector<std::string> bossa = {
        "R..f..R.f...R..f", "R..f..R..f..R..f", "R..f..R.f..R..f.", "R..t..R.f...R.fa" };
    static const std::vector<std::string> samba = {
        "R..f..R.R..f..R.", "R.f.R.R.f.R.f.R.", "R..f.R.fR..f..R.", "R.f.R.f.R.R.f.a." };
    static const std::vector<std::string> latin = {
        "R..f..R.f..R..R.", "..R..f..R..x..R.", "R..f..R.f..t..R.", "R..f..R..f.R..Ra" };
    // ---- Latin / Afro-Cuban: two sides of the clave. The "3 side" bar answers the three-hit side of the clave, the "2 side" bar the two-hit side.
    static const std::vector<std::string> salsa3 = {
        "......R.....f.R.", "R.....R.....f.R.", "......R...a.f.R.", "R.....R...R.f.R.", "..R...R.....f.R.", "......R.R...f.R." };
    static const std::vector<std::string> salsa2 = {
        "......R.....R.R.", "R.....f.....R.R.", "......R.....f.a.", "R.....R...f.R.R.", "......R...R.R.R.", "R.....R.....o.R." };
    static const std::vector<std::string> son3 = {
        "......R...R...f.", "R.....R...R.f.R.", "......f...R.f.R.", "R..R..R.....f.R." };
    static const std::vector<std::string> son2 = {
        "....R.....R...R.", "R.......R.....R.", "......R...R.R.R.", "R.....R.....o.f." };
    static const std::vector<std::string> mambo = {
        "R.R..R.R..f.R..R", "R..R..R.R..f.R.R", "R.o..R.o..f.R.o.", "R..R.R..R.f.R.f." };
    static const std::vector<std::string> songo = {
        "R.gR..R.g.R.f.gR", "..R.gR..R.g.f.R.", "R.g.R.gR..gR.f.a", "R..gR.g.R.R.gf.R" };
    static const std::vector<std::string> chacha = {
        "R...f...R..R.f..", "R...R...f...R..a", "R.....R.f...R.f.", "R..R....f..R..f." };
    static const std::vector<std::string> bolero = {
        "R.....f.....R.f.", "R.......f...a.R.", "R.....R.f.....f.", "R...f.....R...a." };
    static const std::vector<std::string> rumba = {
        "R..R..R...R.R..R", "R..R...R..R..R.R", "R.R...R.R..R.R..", "R..R..R..R..R.a." };
    // ---- Afro-Brazilian
    static const std::vector<std::string> baiao = {
        "R.....f.R.....f.", "R..f..f.R..f..R.", "R.....f.R...f.R.", "R..R..f.R.....f." };
    static const std::vector<std::string> maracatu = {
        "R.R..R.RR.R..R.R", "R.R.R..RR.R.R..R", "R..R.R.RR..R.R.R", "R.RR.R.RR.RR.R.f" };
    static const std::vector<std::string> ijexa = {
        "R..f..R..f..R.f.", "R..R..f..R..f.R.", "R.f..R..f.R..f.a", "R..f.R..f..R..fa" };
    static const std::vector<std::string> sambareggae = {
        "R..R.R..R..R.R.R", "R.R..R..R.R..R.f", "R..R..R.R..R.Rf.", "RR..R.R.R..RR.o." };
    static const std::vector<std::string> axe = {
        "R.R.R.f.R.R.R.o.", "R..R.R.fR..R.R.o", "R.R.f.R.R.f.R.o.", "R.RR.R.fR.RR.R.o" };
    static const std::vector<std::string> forro = {
        "R...f...R...f...", "R.f.R.f.R.f.R.f.", "R..f.R..R..f.R..", "R...f..fR...f.a." };
    // ---- Caribbean
    static const std::vector<std::string> cumbia = {
        "R.....f...R...f.", "R..R..f..R..f...", "R.....R...f...R.", "R.R...f.R.R...f." };
    static const std::vector<std::string> merengue = {
        "R.f.R.f.R.f.R.f.", "R.R.f.f.R.R.f.f.", "R.f.R.f.R.f.f.a.", "R.R.R.f.R.R.R.f." };
    static const std::vector<std::string> bachata = {
        "R..R..R.f..R..R.", "R...R..Rf..R.f..", "R.....R.f..R..f.", "R..R...Rf...R..a" };
    static const std::vector<std::string> zouk = {
        "R.....f...R...a.", "R..R....f...R.f.", "R.R.....f.R...a.", "R.....f..R..f..R" };
    // ---- Reggae & dub
    static const std::vector<std::string> reggae = {
        "......R...R...f.", "........R..R.f..", "R.......R..f.R..", "..R...R...R.f...", "......R.R.R.f..a" };
    static const std::vector<std::string> dub = {
        "R...............", "R.......f.......", "........R...a...", "R.....R.........", "......f.....R..." };
    // ---- Funk, disco, hip-hop, house
    static const std::vector<std::string> funk = {
        "R..R.gR.g.R.f.gR", "R.gR..g.R.gR.f.a", "R..gR.R.g.gR.o.f", "R.R.gR.gR..gR.fa" };
    static const std::vector<std::string> disco = {
        "R.o.R.o.R.o.R.o.", "RoR.RoR.RoR.RoR.", "R.o.R.o.R.o.f.o.", "R.o.R.oaR.o.R.f." };
    static const std::vector<std::string> hiphop = {
        "R.......R..R....", "R..R....R.....R.", "R.......R.R.....", "R....R..R.......", "R.R.....R...f..a" };
    static const std::vector<std::string> trap = {
        "R..R.R..R...R.R.", "R.......R..R..R.", "R..R....R.R.R...", "R.R...R.R.....Ra" };
    static const std::vector<std::string> afrohouse = {
        "..R...R...R...R.", ".R..R..R.R..R.f.", "..R...R.R.R...f.", "R..R..R...R.R...", ".R.R...R.R.R...a" };
    static const std::vector<std::string> jazz = {
        "R..R..R...R.f.o.", "R.....R.t...f.a.", "R.R...R.f.R...fa" };
    switch (genre)
    {
        case G_Afrobeat: return afrobeat;
        case G_AfroJazz: return jazz;
        case G_Highlife: return highlife;
        case G_AfroFusion: return afrofusion;
        case G_Ballad: return ballad;
        case G_Amapiano: return amapiano;
        case G_Juju: return juju;
        case G_Soukous: return soukous;
        case G_EthioJazz: return ethio;
        case G_Township: return township;
        case G_Gospel: return gospel;
        case G_Pop: return pop;
        case G_Rock: return rock;
        case G_Bossa: return bossa;
        case G_Samba: return samba;
        case G_Flamenco: return latin;
        case G_Salsa: return salsa3;
        case G_AfroCuban: { static const std::vector<std::string> m = [&] { std::vector<std::string> r; for (auto* b : { &son3, &son2, &mambo, &songo, &chacha, &bolero, &rumba }) r.insert (r.end(), b->begin(), b->end()); return r; }(); return m; }
        case G_AfroBrazilian: { static const std::vector<std::string> m = [&] { std::vector<std::string> r; for (auto* b : { &baiao, &maracatu, &ijexa, &sambareggae, &axe, &forro }) r.insert (r.end(), b->begin(), b->end()); return r; }(); return m; }
        case G_Caribbean: { static const std::vector<std::string> m = [&] { std::vector<std::string> r; for (auto* b : { &cumbia, &merengue, &bachata, &zouk }) r.insert (r.end(), b->begin(), b->end()); return r; }(); return m; }
        case G_Reggae: { static const std::vector<std::string> m = [&] { std::vector<std::string> r; for (auto* b : { &reggae, &reggae, &dub }) r.insert (r.end(), b->begin(), b->end()); return r; }(); return m; }
        case G_Funk: { static const std::vector<std::string> m = [&] { std::vector<std::string> r; for (auto* b : { &funk, &disco }) r.insert (r.end(), b->begin(), b->end()); return r; }(); return m; }
        case G_HipHop: { static const std::vector<std::string> m = [&] { std::vector<std::string> r; for (auto* b : { &hiphop, &trap }) r.insert (r.end(), b->begin(), b->end()); return r; }(); return m; }
        case G_AfroHouse: return afrohouse;
        default: return afrobeat;
    }
}

/** The two bars of a clave cycle for the Latin genres: side 3 (the bar with the three-hit side) or side 2. Other genres use the same bank for both. */
inline const std::vector<std::string>& bassCellsSide (int genre, bool threeSide)
{
    static const std::vector<std::string> sa3 = { "......R.....f.R.", "R.....R.....f.R.", "......R...a.f.R.", "R.....R...R.f.R.", "..R...R.....f.R.", "......R.R...f.R." };
    static const std::vector<std::string> sa2 = { "......R.....R.R.", "R.....f.....R.R.", "......R.....f.a.", "R.....R...f.R.R.", "......R...R.R.R.", "R.....R.....o.R." };
    static const std::vector<std::string> sn3 = { "......R...R...f.", "R.....R...R.f.R.", "......f...R.f.R.", "R..R..R.....f.R.", "R.R..R.R..f.R..R", "R.g..R.g..R.f.gR" };
    static const std::vector<std::string> sn2 = { "....R.....R...R.", "R.......R.....R.", "......R...R.R.R.", "R.....R.....o.f.", "R..R..R.R..f.R.R", "R..gR.g.R.R.gf.R" };
    if (genre == G_Salsa) return threeSide ? sa3 : sa2;
    if (genre == G_AfroCuban) return threeSide ? sn3 : sn2;
    return bassCells (genre);
}

struct Pit { int lo, hi; };

inline int rootPitchOf (int pc, int lo) { return lo + (((pc - lo) % 12) + 12) % 12; }
inline int fifthIv (uint16_t tones, int rootPc)
{
    if (inMask (tones, rootPc + 7)) return 7;
    if (inMask (tones, rootPc + 6)) return 6;
    if (inMask (tones, rootPc + 8)) return 8;
    return 7;
}
inline int thirdIv (uint16_t tones, int rootPc)
{
    if (inMask (tones, rootPc + 4)) return 4;
    if (inMask (tones, rootPc + 3)) return 3;
    if (inMask (tones, rootPc + 5)) return 5;
    if (inMask (tones, rootPc + 2)) return 2;
    return 4;
}
inline int seventhIv (uint16_t tones, int rootPc)
{
    if (inMask (tones, rootPc + 10)) return 10;
    if (inMask (tones, rootPc + 11)) return 11;
    if (inMask (tones, rootPc + 9)) return 9;
    return 10;
}
/** nearest pitch inside a pitch-class mask in a direction (dir 0 = either), within [lo, hi] */
inline int nearIn (uint16_t mask, int pitch, int dir, int lo, int hi)
{
    for (int d = 1; d <= 6; ++d)
    {
        const int a = pitch + d, b = pitch - d;
        if (dir >= 0 && a <= hi && a >= lo && inMask (mask, a)) return a;
        if (dir <= 0 && b >= lo && b <= hi && inMask (mask, b)) return b;
        if (dir > 0 && b >= lo && b <= hi && d > 4 && inMask (mask, b)) return b;
    }
    return pitch;
}

/** the pitch a role plays on a chord (also used when a bar's chord is changed in the editor) */
inline int pitchForRole (int role, const ChordSpan& c, const ChordSpan& next, int prev, int lo, int hi, Rng& rng, float step)
{
    const int root = rootPitchOf (c.rootPc, lo + 0);
    const int fifth = root + fifthIv (c.toneMask, c.rootPc);
    switch (role)
    {
        case BR_Root: case BR_Ghost: return root;
        case BR_Fifth:
        {
            // the fifth above, or sometimes the fifth below the root (a classic bass move)
            const int below = root + fifthIv (c.toneMask, c.rootPc) - 12;
            if (below >= lo && rng.chance (0.3) && prev <= root + 2) return below;
            return fifth <= hi ? fifth : fifth - 12;
        }
        case BR_Octave: return root + 12 <= hi ? root + 12 : root;
        case BR_Harmonic: return root + 12 <= hi ? root + 12 : root;
        case BR_Third: { const int t = root + thirdIv (c.toneMask, c.rootPc); return t <= hi ? t : t - 12; }
        case BR_Seventh: { const int t = root + seventhIv (c.toneMask, c.rootPc) - 12; return t >= lo ? t : t + 12; }
        case BR_Approach:
        {
            const int T = rootPitchOf (next.rootPc, lo);
            const bool fromBelow = rng.chance (0.62);
            int a = fromBelow ? T - 1 : T + 1;
            if (a < lo) a = T + 1;
            if (a > hi) a = T - 1;
            if (step < 0.3f && rng.chance (0.5)) a = std::clamp (fifth <= hi ? fifth : fifth - 12, lo, hi);     // low "step" style: plain fifth instead of a chromatic approach
            return a;
        }
        default:
        {
            // any chord / scale tone close to the previous note
            std::vector<int> cand; std::vector<double> w;
            for (int p = std::max (lo, prev - 5); p <= std::min (hi, prev + 5); ++p)
            {
                if (! inMask (c.scaleMask, p)) continue;
                cand.push_back (p); w.push_back (inMask (c.toneMask, p) ? 3.0 : 1.0);
            }
            if (cand.empty()) return root;
            return cand[(size_t) rng.weighted (w)];
        }
    }
}
} // namespace bassdetail

namespace detail
{
inline std::vector<Note> generateBass (const Settings& S, const Style& st, const Meter& m, Rng& rng, const std::vector<ChordSpan>& chords)
{
    using namespace bassdetail;
    std::vector<Note> out;
    if (chords.empty()) return out;
    const GenreInfo& gi = genreInfo (S.genre);
    const Range R = instRange (I_Bass, 0);
    const int lo = R.lo, hi = R.hi;
    const int steps = m.stepsPerBar;
    const double stepLen = m.barBeats / steps;
    auto at = [&] (double beat) -> const ChordSpan& { for (auto& c : chords) if (beat + 1e-6 >= c.start && beat < c.end - 1e-6) return c; return chords.back(); };
    const double swingAmt = gi.fixedSwing >= 0 ? std::clamp (gi.fixedSwing + st.swing * 0.1, 0.0, 1.0) : st.swing;
    const bool walking = (S.genre == G_Swing) || st.name.rfind ("Ron Carter", 0) == 0 || st.name.rfind ("Blues Shuffle", 0) == 0;

    // groove lock (a drum / bass / keys loop of the project) and style reference (a bass recording whose rhythm is borrowed)
    GrooveView gv;
    if (S.grooveProfile && S.grooveAmount > 0.01f) gv = makeGrooveView (*S.grooveProfile, m.barBeats, S.grooveTempo, S.grooveShift);
    GrooveView rv;
    if (S.refProfile && S.refAmount > 0.01f) rv = makeGrooveView (*S.refProfile, m.barBeats, S.refTempo, S.refShift);

    // the rhythm cells of this genre (4/4); other meters build a cell from the beat groups
    auto genericCell = [&] ()
    {
        std::string c ((size_t) steps, '.');
        int pos = 0, gidx = 0;
        for (int g : m.groups)
        {
            if (pos >= steps) break;
            c[(size_t) pos] = (gidx == 0) ? 'R' : (rng.chance (0.5) ? 'R' : 'f');
            if (g >= 4 && rng.chance (0.5 * (0.4 + st.density))) c[(size_t) std::min (steps - 1, pos + g / 2)] = rng.chance (0.5) ? 'x' : 'o';
            pos += g; ++gidx;
        }
        return c;
    };
    const auto& bank = bassCells (S.genre);
    auto pickCell = [&] () -> std::string
    {
        if (! m.simpleFour) return genericCell();
        return bank[(size_t) rng.i (0, (int) bank.size() - 1)];
    };

    const int phrase = gi.phraseBars > 0 ? gi.phraseBars : 4;
    const bool claveOn = S.clave > 0 && (S.genre == G_Salsa || S.genre == G_AfroCuban) && m.simpleFour;
    auto pickSide = [&] (bool three) -> std::string { const auto& b = bassCellsSide (S.genre, three); return b[(size_t) rng.i (0, (int) b.size() - 1)]; };
    std::string mainCell = claveOn ? pickSide (true) : pickCell(), varCell = claveOn ? pickSide (true) : pickCell();
    std::string mainCell2 = claveOn ? pickSide (false) : std::string(), varCell2 = claveOn ? pickSide (false) : std::string();
    int prevPitch = rootPitchOf (chords.front().rootPc, lo);

    for (int bar = 0; bar < S.bars; ++bar)
    {
        const double b0 = bar * m.barBeats;
        const bool phraseStart = (bar % phrase == 0);
        const bool phraseEnd = (bar % phrase == phrase - 1);
        if (phraseStart && bar > 0 && ! rng.chance (std::clamp (0.35 + 0.6 * (double) st.motif * (0.5 + S.repeat), 0.0, 0.97))) { if (claveOn) { mainCell = pickSide (true); varCell = pickSide (true); mainCell2 = pickSide (false); varCell2 = pickSide (false); } else { mainCell = pickCell(); varCell = pickCell(); } }
        const bool threeSide = ((bar % 2 == 0) == (S.clave != 2));        // clave 1 = 3-2 (bar 1 is the 3 side), clave 2 = 2-3
        std::string cell = (claveOn && ! threeSide) ? mainCell2 : mainCell;
        if (! phraseStart && rng.chance (std::clamp (0.55 - (double) S.repeat * 0.5 - st.motif * 0.25, 0.04, 0.6))) cell = (claveOn && ! threeSide) ? varCell2 : varCell;

        // style reference: borrow the rhythm of the recording
        bool fromRef = false;
        if (rv.valid && rng.chance (std::min (1.0, (double) S.refAmount * 1.05)) && rv.steps == steps)
        {
            std::string rc ((size_t) steps, '.'); int cnt = 0;
            for (int i = 0; i < steps; ++i) if (rv.occ[(size_t) i] >= 0.45f) { rc[(size_t) i] = 'x'; ++cnt; }
            if (cnt >= 2)
            {
                rc[0] = 'R';
                for (int i = 1; i < steps; ++i) if (rc[(size_t) i] == 'x') rc[(size_t) i] = rng.chance (0.5) ? 'R' : (rng.chance (0.5) ? 'f' : 'o');
                cell = rc; fromRef = true;
            }
        }

        // busy slider: per-bar override or global
        const double busy = (bar < (int) S.barBusy.size() && S.barBusy[(size_t) bar] >= 0.f) ? S.barBusy[(size_t) bar] : S.busy;
        const double keep = std::clamp ((double) st.density * (0.3 + busy * 1.1) * gi.densMul * (0.7 + 0.5 * S.intensity) - (S.space - 0.5) * 0.4, 0.1, 1.0);
        std::string c2 = cell;
        for (int i = 1; i < steps; ++i)
        {
            if (c2[(size_t) i] == '.' ) continue;
            const double w = m.weight[(size_t) i];
            if (! fromRef && w < 0.8 && ! rng.chance (std::min (1.0, keep + w * 0.35))) c2[(size_t) i] = '.';
        }
        // busy: extra hits on the empty eighths
        if (busy > 0.55 && ! fromRef)
            for (int i = 2; i < steps; i += 2)
                if (c2[(size_t) i] == '.' && rng.chance ((busy - 0.55) * 0.6 * (double) st.density)) c2[(size_t) i] = 'x';
        // anticipations: a hit on the beat is pushed one 16th early
        for (int i = 4; i < steps; i += 4)
            if (! fromRef && c2[(size_t) i] != '.' && c2[(size_t) (i - 1)] == '.' && rng.chance (st.sync * 0.28)) { c2[(size_t) (i - 1)] = c2[(size_t) i]; c2[(size_t) i] = '.'; }
        // ghost-note fills on empty 16ths when busy
        for (int i = 1; i < steps; ++i)
            if (! fromRef && c2[(size_t) i] == '.' && (i % 2 == 1) && rng.chance (std::max (0.0, (busy - 0.3)) * 0.8 * (0.3 + (double) st.ghost * 3.0) * (0.5 + S.intensity)))
                c2[(size_t) i] = 'g';
        // style reference: the bass line is about as busy as the recording
        if (rv.valid && rv.steps == steps)
        {
            int cnt = 0; for (char ch : c2) if (ch != '.') ++cnt;
            const double target = std::max (2.0, rv.hitsPerBar * 1.1 + 0.5);
            while (cnt > target && rng.chance (0.5 + S.refAmount * 0.5))
            {
                int worst = -1; double wv = 9;
                for (int i = 1; i < steps; ++i) if (c2[(size_t) i] != '.' && m.weight[(size_t) i] < wv) { wv = m.weight[(size_t) i]; worst = i; }
                if (worst < 0) break;
                c2[(size_t) worst] = '.'; --cnt;
            }
        }
        // groove lock: hits where the loop is hit, room where it is not
        if (gv.valid && gv.steps == steps)
        {
            for (int i = 1; i < steps; ++i)
            {
                const double a = S.grooveAmount;
                if (c2[(size_t) i] == '.' && gv.occ[(size_t) i] >= 0.6f && rng.chance (a * 0.55)) c2[(size_t) i] = 'R';
                else if (c2[(size_t) i] != '.' && c2[(size_t) i] != 'g' && gv.occ[(size_t) i] < 0.1f && m.weight[(size_t) i] < 0.8 && rng.chance (a * 0.35)) c2[(size_t) i] = '.';
            }
        }
        // a chord change inside the bar always gets its own root
        for (auto& ch : chords)
        {
            if (ch.start <= b0 + 1e-6 || ch.start >= b0 + m.barBeats - 1e-6) continue;
            const int i = (int) std::lround ((ch.start - b0) / stepLen);
            if (i > 0 && i < steps) c2[(size_t) i] = 'R';
        }
        // the first step of a bar is played unless the pattern deliberately starts with a rest; a new chord on the bar line always gets its root
        if (c2[0] == '.' && cell[0] != '.') c2[0] = 'R';
        for (auto& ch : chords) if (std::abs (ch.start - b0) < 1e-6) c2[0] = (c2[0] == '.' || c2[0] == 'g') ? 'R' : c2[0];
        // fill at the end of a phrase
        bool fill = false;
        if (phraseEnd && steps >= 16 && bar + 1 <= S.bars && rng.chance (std::clamp ((double) st.density * 0.45 * (0.4 + busy), 0.0, 0.8) * (rv.valid ? 1.0 - S.refAmount * 0.9 : 1.0)))
        {
            fill = true;
            for (int i = 12; i < steps; ++i) c2[(size_t) i] = '.';
        }

        // ---- walking bass: four chord-linked quarter notes
        std::vector<Note> bar_notes;
        const ChordSpan& nextBarChord = at ((bar + 1) * m.barBeats >= S.bars * m.barBeats - 1e-6 ? 0.0 : (bar + 1) * m.barBeats);
        auto mk = [&] (double start, double len, int pitch, int role, int vel, int art)
        {
            Note n; n.start = start; n.length = len; n.pitch = std::clamp (pitch, lo, hi); n.vel = std::clamp (vel, 1, 127); n.art = art; n.role = role; n.track = 0; n.voice = 0;
            const int sidx = std::clamp ((int) std::lround ((start - b0) / stepLen), 0, steps - 1);
            n.weight = m.weight[(size_t) sidx];
            return n;
        };
        const int baseVel = (int) std::lround (st.vel * (0.72 + 0.4 * S.intensity));
        auto velFor = [&] (double w, int role)
        {
            double v = baseVel + (w >= 0.8 ? 9 : (w >= 0.5 ? 4 : (w >= 0.3 ? -2 : -8)));
            v += rng.gauss (2.0 + st.dyn * 5.0);
            if (role == BR_Ghost) v = 26 + rng.i (0, 14);
            return (int) std::lround (v);
        };

        if (walking && m.simpleFour && ! fromRef)
        {
            const int nb = m.num;
            int p = prevPitch;
            for (int k = 0; k < nb; ++k)
            {
                const double t = b0 + k;
                const ChordSpan& c = at (t + 0.01);
                const ChordSpan& nxt = (k + 1 < nb) ? at (t + 1.01) : nextBarChord;
                int role = BR_Other; int pitch;
                if (k == 0 || (c.start > t - 1e-6 && c.start < t + 1e-6))
                {
                    role = BR_Root; pitch = pitchForRole (BR_Root, c, nxt, p, lo, hi, rng, st.step);
                }
                else if (k == nb - 1 || (nxt.rootPc != c.rootPc && k + 1 < nb))
                {
                    role = BR_Approach; pitch = pitchForRole (BR_Approach, c, nxt, p, lo, hi, rng, 1.f);
                }
                else
                {
                    const bool chordTone = rng.chance (0.4 + st.leap * 0.4);
                    if (chordTone) { role = (rng.chance (0.5) ? BR_Fifth : BR_Third); pitch = pitchForRole (role, c, nxt, p, lo, hi, rng, 1.f); }
                    else
                    {
                        const int dir = rng.chance (0.5) ? 1 : -1;
                        role = BR_Other; pitch = nearIn (c.scaleMask, p, dir, lo, hi);
                    }
                }
                // avoid a fixed repeated note: step to the other side if identical
                if (k > 0 && pitch == p && role != BR_Root) pitch = nearIn (c.scaleMask, p, rng.chance (0.5) ? 1 : -1, lo, hi);
                double len = 0.88 + 0.1 * st.legato;
                // busy: a passing eighth after the beat; very calm: hold the third beat over the second
                const bool passing = (k + 1 < nb) && role != BR_Approach && busy > 0.55 && rng.chance (std::min (0.9, (busy - 0.55) * 1.5));
                if (passing) len = 0.46;
                if (k == 2 && nb == 4 && busy < 0.3 && rng.chance ((0.3 - busy) * 2.5)) { if (! bar_notes.empty()) bar_notes.back().length = 1.9; p = pitch; continue; }
                bar_notes.push_back (mk (t, len, pitch, role, velFor (m.weight[(size_t) std::min (steps - 1, (int) (k * 4))], role), A_Sustain));
                p = pitch;
                if (passing)
                {
                    const int pp = nearIn (c.scaleMask, p, nxt.rootPc != c.rootPc ? (rootPitchOf (nxt.rootPc, lo) >= p ? 1 : -1) : (rng.chance (0.5) ? 1 : -1), lo, hi);
                    bar_notes.push_back (mk (t + 0.5, 0.42, pp, BR_Other, velFor (0.3, BR_Other) - 4, A_Sustain));
                    p = pp;
                }
            }
        }
        else
        {
            // ---- pattern bass
            struct H { int step; char ch; };
            std::vector<H> hits;
            for (int i = 0; i < steps; ++i) if (c2[(size_t) i] != '.') hits.push_back ({ i, c2[(size_t) i] });
            // Latin anticipation: the hit on the "and of 4" plays the root of the NEXT chord, a beat-and-a-half before it arrives (the tumbao way)
            if ((S.genre == G_Salsa || S.genre == G_AfroCuban || S.genre == G_Caribbean || S.genre == G_AfroBrazilian) && steps >= 16 && nextBarChord.rootPc != at (b0 + m.barBeats - 0.01).rootPc && rng.chance (0.8))
                for (auto& hh : hits) if (hh.step >= 13 && hh.step <= 14 && hh.ch != 'g') hh.ch = 'n';
            // tiny rhythm safety: never an empty bar
            if (hits.empty()) hits.push_back ({ 0, 'R' });
            int p = prevPitch;
            for (size_t h = 0; h < hits.size(); ++h)
            {
                const int i = hits[h].step;
                double t = b0 + i * stepLen;
                const ChordSpan& c = at (t + 0.001);
                const double nextT = (h + 1 < hits.size()) ? b0 + hits[h + 1].step * stepLen : b0 + m.barBeats;
                const ChordSpan& nxt = (h + 1 < hits.size()) ? at (nextT + 0.001) : nextBarChord;
                int role = BR_Root;
                switch (hits[h].ch)
                {
                    case 'R': role = BR_Root; break;   case 'f': role = BR_Fifth; break;   case 'o': role = (rng.chance (0.5 + st.octave * 0.5) ? BR_Octave : BR_Root); break;
                    case 't': role = BR_Third; break;  case 'b': role = BR_Seventh; break; case 'a': role = BR_Approach; break; case 'n': role = BR_Approach; break;
                    case 'g': role = BR_Ghost; break;  default: role = BR_Other; break;
                }
                // style: extra octave jumps on plain roots
                if (role == BR_Root && i != 0 && m.weight[(size_t) i] < 0.55 && rng.chance (st.octave * 0.35)) role = BR_Octave;
                // an approach note on a step that isn't the end of the bar is only an approach when the chord changes next
                if (role == BR_Approach && i < steps - 4 && nxt.rootPc == c.rootPc) role = rng.chance (0.5) ? BR_Fifth : BR_Other;
                int pitch = pitchForRole (role, c, nxt, p, lo, hi, rng, st.step);
                if (hits[h].ch == 'n') pitch = std::clamp (rootPitchOf (nextBarChord.rootPc, lo), lo, hi);
                if (role == BR_Ghost) pitch = p >= lo && p <= hi && std::abs (p - rootPitchOf (c.rootPc, lo)) <= 12 ? p : pitch;
                // swing: off-beat eighths are pushed late
                if (swingAmt > 0.01 && (i % 4 == 2)) t += swingAmt * (m.barBeats / m.num) / 6.0;
                const double gap = nextT - (b0 + i * stepLen);
                double len;
                if (role == BR_Ghost) len = std::min (0.14, gap * 0.9);
                else len = gap * (0.45 + 0.53 * (double) st.legato);
                len = std::max (0.08, std::min (len, gap * 0.99));
                int art = (role == BR_Ghost) ? A_Ghost : A_Sustain;
                bar_notes.push_back (mk (t, len, pitch, role, velFor (m.weight[(size_t) i], role), art));
                if (role != BR_Ghost) p = pitch;
            }
            // fill on the last beat: a scale run down into the next root
            if (fill)
            {
                const ChordSpan& c = at (b0 + m.barBeats - 0.01);
                const int T = rootPitchOf (nextBarChord.rootPc, lo);
                int cur = std::min (hi, T + (rng.chance (0.5) ? 7 : 5));
                const bool ghosty = rng.chance (st.ghost * 2.0);
                for (int i = 12; i < steps; ++i)
                {
                    const double t = b0 + i * stepLen;
                    int pitch;
                    if (i == steps - 1) pitch = rng.chance (0.6) ? T - 1 : T + 1;
                    else { pitch = nearIn (c.scaleMask, cur, -1, lo, hi); if (pitch == cur) pitch = std::max (lo, cur - 2); cur = pitch; }
                    const bool gh = ghosty && (i % 2 == 1) && i != steps - 1;
                    bar_notes.push_back (mk (t, stepLen * 0.8, pitch, gh ? BR_Ghost : BR_Other, velFor (0.2, gh ? BR_Ghost : BR_Other) + 6, gh ? A_Ghost : A_Staccato));
                }
            }
        }
        std::stable_sort (bar_notes.begin(), bar_notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        for (size_t k = 0; k + 1 < bar_notes.size(); ++k)       // a bass note never runs into the next one
        {
            const double room = bar_notes[k + 1].start - bar_notes[k].start;
            if (room > 1e-6 && bar_notes[k].length > room - 0.01) bar_notes[k].length = std::max (0.05, room * 0.97);
        }

        for (auto& n : bar_notes) if (n.start + n.length > b0 + m.barBeats - 0.01) n.length = std::max (0.05, b0 + m.barBeats - 0.01 - n.start);

        // ---- articulations, from the style
        for (size_t k = 0; k < bar_notes.size(); ++k)
        {
            Note& n = bar_notes[k];
            if (n.art == A_Ghost) continue;
            const double gapPrev = (k > 0) ? n.start - bar_notes[k - 1].start : 4.0;
            const int iv = (k > 0) ? n.pitch - bar_notes[k - 1].pitch : 0;
            const int aiv = std::abs (iv);
            const bool strong = n.weight >= 0.55;
            if (n.role == BR_Harmonic || (n.length >= 1.0 && rng.chance (st.harm * 0.5) && n.pitch + 12 <= hi))
            { if (n.role != BR_Harmonic) n.pitch += 12; n.role = BR_Harmonic; n.art = A_Harmonic; continue; }
            if (st.slap > 0.05f && (n.role == BR_Root || n.role == BR_Octave || n.role == BR_Fifth) && (strong || n.role == BR_Octave) && rng.chance (st.slap))
            { n.art = A_Slap; n.vel = std::min (127, n.vel + 8); n.length = std::min (n.length, 0.4); continue; }
            if (k > 0 && aiv >= 1 && aiv <= 5 && gapPrev <= 1.01 && rng.chance (st.slide * (n.role == BR_Root && bar_notes[k - 1].role == BR_Approach ? 1.6 : 1.0)))
            { n.art = A_Slide; continue; }
            if (k > 0 && aiv >= 1 && aiv <= 3 && gapPrev <= 0.55)
            {
                if (iv > 0 && rng.chance (st.hammer)) { n.art = A_HammerOn; continue; }
                if (iv < 0 && rng.chance (st.pull)) { n.art = A_PullOff; continue; }
            }
            if (st.palm > 0.05f && n.length <= 0.55 && rng.chance (st.palm)) { n.art = A_PalmMute; n.length *= 0.7; continue; }
            if (n.length <= 0.3 && rng.chance (st.stacc)) { n.art = A_Staccato; continue; }
            if (strong && n.weight >= 0.8 && rng.chance (0.15 + st.dyn * 0.2)) { n.art = A_Marcato; n.vel = std::min (127, n.vel + 8); }
        }
        for (auto& n : bar_notes) { prevPitch = n.pitch; out.push_back (n); }
    }
    return out;
}

/** after the chord of some bars changed: every note keeps its job (root, fifth, approach...) on the new chord */
inline void bassReharmonize (Pattern& p, const std::vector<ChordSpan>& newChords, const std::vector<char>& changed, const Settings& s)
{
    using namespace bassdetail;
    const Range R = instRange (I_Bass, 0);
    Rng rng (s.seed * 7919ULL + 17);
    auto at = [&] (double beat) -> const ChordSpan& { for (auto& c : newChords) if (beat + 1e-6 >= c.start && beat < c.end - 1e-6) return c; return newChords.back(); };
    int prev = rootPitchOf (newChords.front().rootPc, R.lo);
    for (auto& n : p.raw)
    {
        if (n.track != 0 || n.voice != 0) continue;
        const int bar = p.barOf (n.start);
        if (! changed[(size_t) bar]) { prev = n.pitch; continue; }
        const ChordSpan& c = at (n.start + 0.001);
        const double nb = (bar + 1) * p.barBeats;
        const ChordSpan& nxt = (nb >= p.lengthBeats - 1e-6) ? newChords.front() : at (nb + 0.001);
        int role = n.role;
        if (role == BR_None)       // notes saved before roles existed: guess from the old pitch
            role = (n.weight >= 0.55) ? BR_Root : BR_Other;
        const bool had12 = (n.art == A_Harmonic);
        int pitch = pitchForRole (role, c, nxt, prev, R.lo, R.hi, rng, 1.f);
        if (had12) pitch = std::min (R.hi, rootPitchOf (c.rootPc, R.lo) + 12);
        n.pitch = std::clamp (pitch, 0, 127);
        prev = n.pitch;
    }
}
} // namespace detail
} // namespace enochsolo
