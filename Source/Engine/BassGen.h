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
#include <map>

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
        // ---- v2.7 batch 3 genres
        case G_Kizomba:       { static const std::vector<std::string> v = { "R.....R.R.......", "R..f..R.........", "R.....f...R.....", "R.......o..f..a.", "R..R............", "R.....R.o...f..." }; return v; }
        case G_Zouk:          { static const std::vector<std::string> v = { "R..f..R.R..f..R.", "R..R..f.R..f..a.", "R..f....R..o..f." }; static const auto m = [&] { auto r = v; r.insert (r.end(), zouk.begin(), zouk.end()); return r; }(); return m; }
        case G_PrivateSchool: { static const std::vector<std::string> v = { "R.....R...f.....", "..R...R.....f...", "R..R......o.f...", "R.......t..f....", "....R.R.....f.a." }; return v; }
        case G_Bacardi:       { static const std::vector<std::string> v = { "R.RR.R.RR.Rf.R.o", "R..R.RR.R.Rf.R.f", "RR.R.R.RR.R.f.Ro", "R.R.RR.R.RR.f.R.", "R..RR.R..RR.f.R." }; return v; }
        case G_Sgija:         { static const std::vector<std::string> v = { "R..R..R...R.....", "R..R..R..R..f...", "R.....R.R..f....", "R..R......R..a..", "R..R..R.o...f..." }; return v; }
        case G_JazzFunk:      { static const std::vector<std::string> v = { "R..R..Ro.R.R..f.", "R.gR..R..g.R.oRa", "R..g.RR.g.R..f.a", "Rg.R.gR.R.gRo.f." }; static const auto m = [&] { auto r = v; r.insert (r.end(), funk.begin(), funk.end()); return r; }(); return m; }
        case G_SmoothJazz:    { static const std::vector<std::string> v = { "R.....R...f.R...", "R..f....R...t.a.", "R.....R.o...f...", "R...R...f...R.a." }; return v; }
        case G_Bebop: case G_HardBop: case G_ModalJazz: case G_CoolJazz: return jazz;
        case G_LatinJazz:     return bassCells (G_AfroCuban);
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
//------------------------------------------------------------------------------ v2.12 motif engine
// A motif is one bar of rhythm (the genre's cell) plus a melodic shape: every hit has a FUNCTION on the chord (root, fifth, third,
// seventh, octave, approach into the next chord, a passing step up / down, a neighbour). The functions are chosen once per motif, so
// the hook comes back bar after bar, and are mapped onto each bar's own chord, so it follows the harmony.
enum TF { TF_Rest = -1, TF_Root = 0, TF_Fifth, TF_Octave, TF_Third, TF_Seventh, TF_Approach, TF_PassUp, TF_PassDown, TF_Neighbor, TF_Ghost, TF_Any, TF_NextRoot };

struct Vocab
{
    double melodic = 0.4;           // how often a weak hit becomes a melodic function instead of the cell's root / fifth / octave
    double w[7] = { 1, 1, 1, 1, 1, 1, 1 };   // fifth, octave, third, seventh, pass up, pass down, neighbour
    bool pump = false;              // the style is MEANT to pump one note (rock eighths, disco octaves, merengue)
    double fillMelodic = 0.5;       // how melodic its fills are (runs, enclosures) vs. rhythmic (octave pops, dead-note pickups)
    double guide = 0.0;             // jazz / gospel: guide tones (3rd, 7th) on beat 3
};

inline Vocab vocabFor (int genre, const std::string& style)
{
    Vocab v;
    auto set = [&] (double mel, std::initializer_list<double> ws, double fillMel, double guide = 0.0, bool pump = false)
    { v.melodic = mel; int i = 0; for (double x : ws) v.w[i++] = x; v.fillMelodic = fillMel; v.guide = guide; v.pump = pump; };
    const int b = baseGenre (genre);
    switch (b)
    {
        case G_Funk:        set (0.45, { 1.0, 2.2, 0.6, 1.0, 0.9, 1.0, 0.6 }, 0.35); break;     // octave pops, b7, passing
        case G_Gospel:      set (0.62, { 1.0, 0.6, 1.4, 1.1, 1.6, 1.4, 0.6 }, 0.8, 0.5); break; // walking, thirds, runs
        case G_Swing:       set (0.7,  { 1.0, 0.4, 1.4, 1.3, 1.5, 1.5, 0.8 }, 0.8, 0.8); break;
        case G_Afrobeat: case G_Highlife: case G_Juju: case G_Soukous: case G_Township: case G_EthioJazz: case G_AfroJazz:
                            set (0.55, { 1.3, 1.0, 1.0, 0.6, 1.2, 1.2, 0.8 }, 0.6); break;    // melodic, syncopated
        case G_AfroFusion:  set (0.45, { 1.4, 0.9, 0.9, 0.6, 1.0, 1.0, 0.6 }, 0.5); break;
        case G_Amapiano:    set (0.35, { 1.4, 1.6, 0.6, 0.5, 0.8, 1.0, 0.4 }, 0.4); break;    // log drum: root / fifth / octave, space
        case G_Reggae:      set (0.4,  { 1.5, 1.2, 0.9, 0.5, 1.0, 1.1, 0.7 }, 0.65); break;   // root-fifth-octave, melodic fills
        case G_Pop:         set (0.35, { 1.3, 1.0, 0.8, 0.4, 1.0, 1.0, 0.5 }, 0.5); break;
        case G_Rock:        set (0.15, { 1.5, 1.2, 0.4, 0.3, 0.8, 0.8, 0.3 }, 0.4, 0.0, true); break;
        case G_Ballad:      set (0.45, { 1.2, 0.6, 1.2, 0.8, 1.3, 1.3, 0.6 }, 0.7, 0.3); break;
        case G_HipHop:      set (0.25, { 1.2, 1.4, 0.5, 0.6, 0.6, 0.9, 0.4 }, 0.3); break;
        case G_AfroHouse:   set (0.3,  { 1.4, 1.4, 0.6, 0.5, 0.7, 0.8, 0.4 }, 0.4); break;
        case G_Salsa: case G_AfroCuban: case G_Caribbean: case G_AfroBrazilian: case G_Bossa: case G_Samba: case G_Flamenco:
                            set (0.3,  { 1.8, 0.8, 0.7, 0.4, 0.8, 0.9, 0.4 }, 0.55); break;   // root-fifth tumbao, anticipations
        default:            set (0.4,  { 1.2, 1.0, 0.9, 0.6, 1.0, 1.0, 0.6 }, 0.5); break;
    }
    if (genre == G_Kizomba || genre == G_Zouk) { v.melodic = 0.38; v.w[0] = 1.8; v.w[1] = 0.6; v.w[4] = 1.1; v.w[5] = 1.1; v.fillMelodic = 0.7; }
    if (genre == G_JazzFunk || genre == G_SmoothJazz) { v.melodic = 0.6; v.w[2] = 1.3; v.w[3] = 1.3; v.guide = 0.4; v.fillMelodic = 0.75; }
    std::string n = style; for (auto& ch : n) ch = (char) std::tolower ((unsigned char) ch);
    if (n.find ("neo") != std::string::npos || n.find ("r&b") != std::string::npos || n.find ("soul") != std::string::npos)
    { v.melodic = std::max (v.melodic, 0.6); v.w[2] = 1.5; v.w[3] = 1.4; v.w[6] = 1.0; v.fillMelodic = 0.75; v.guide = std::max (v.guide, 0.35); }
    if (n.find ("disco") != std::string::npos || n.find ("merengue") != std::string::npos || n.find ("eighths") != std::string::npos) v.pump = true;
    return v;
}

/** the motif's functions for one rhythm cell (decided once, so the motif keeps its shape when it comes back) */
inline std::vector<int> makeFuncs (const std::string& cell, const std::vector<double>& weight, const Vocab& v, const Style& st, Rng& rng)
{
    const int steps = (int) cell.size();
    std::vector<int> f ((size_t) steps, TF_Rest);
    const double mel = std::clamp (v.melodic * (0.65 + 0.7 * (double) st.step), 0.0, 0.92);
    int prevF = TF_Rest;
    for (int i = 0; i < steps; ++i)
    {
        const char ch = cell[(size_t) i];
        if (ch == '.') continue;
        const double w = i < (int) weight.size() ? weight[(size_t) i] : 0.3;
        int fn;
        switch (ch)
        {
            case 'R': fn = TF_Root; break;   case 'f': fn = TF_Fifth; break;   case 'o': fn = TF_Octave; break;
            case 't': fn = TF_Third; break;  case 'b': fn = TF_Seventh; break; case 'a': fn = TF_Approach; break;
            case 'g': fn = TF_Ghost; break;  default: fn = TF_Any; break;
        }
        // weak hits become melodic: a function from the genre's vocabulary (beats and the bar's first hit stay the cell's)
        static const int vocabFn[7] = { TF_Fifth, TF_Octave, TF_Third, TF_Seventh, TF_PassUp, TF_PassDown, TF_Neighbor };
        if (i > 0 && w < 0.8 && fn != TF_Ghost && fn != TF_Approach && rng.chance (mel))
            fn = vocabFn[std::clamp ((int) rng.weighted (std::vector<double> { v.w[0], v.w[1], v.w[2], v.w[3], v.w[4], v.w[5], v.w[6] }), 0, 6)];
        // never the same root twice in a row on a weak hit (unless the style pumps on purpose)
        if (! v.pump && fn == TF_Root && prevF == TF_Root && i > 0 && w < 0.55)
            { static const int alt[4] = { TF_Octave, TF_Fifth, TF_PassUp, TF_PassDown };
              fn = alt[std::clamp ((int) rng.weighted (std::vector<double> { v.w[1] + 0.5, v.w[0], 0.6 * v.w[4], 0.6 * v.w[5] }), 0, 3)]; }
        // guide tones (jazz, gospel, soul): a 3rd or 7th on beat 3
        if (v.guide > 0 && steps == 16 && i == 8 && fn == TF_Root && rng.chance (v.guide)) fn = rng.chance (0.6) ? TF_Third : TF_Seventh;
        if (v.guide > 0 && i > 0 && i != steps / 2 && (fn == TF_Fifth || fn == TF_Octave || fn == TF_Root) && rng.chance (v.guide * (w < 0.8 ? 0.55 : 0.45))) fn = rng.chance (0.7) ? TF_Third : TF_Seventh;   // guide tones, beats 2 and 4 too
        if (fn == TF_Any) fn = rng.chance (0.5) ? TF_Fifth : (rng.chance (0.5) ? TF_Third : TF_Octave);
        f[(size_t) i] = fn; if (fn != TF_Ghost) prevF = fn;
    }
    return f;
}

inline int roleOfFunc (int f)
{
    switch (f)
    {
        case TF_Root: return BR_Root; case TF_Fifth: return BR_Fifth; case TF_Octave: return BR_Octave; case TF_Third: return BR_Third;
        case TF_Seventh: return BR_Seventh; case TF_Approach: case TF_NextRoot: return BR_Approach; case TF_Ghost: return BR_Ghost;
        default: return BR_Other;
    }
}

/** the pitch of a motif function on a chord; passing steps move by scale step from the previous note toward where the line is going */
inline int pitchForFunc (int f, const ChordSpan& c, const ChordSpan& next, int prev, int lo, int hi, Rng& rng, float step, double w, int toward)
{
    switch (f)
    {
        case TF_PassUp: case TF_PassDown: case TF_Neighbor:
        {
            int dir = f == TF_PassUp ? 1 : (f == TF_PassDown ? -1 : (toward >= prev ? 1 : -1));
            if (f != TF_Neighbor && toward >= 0 && std::abs (toward - prev) >= 2) dir = toward > prev ? 1 : -1;   // pass toward the target
            // on a strong beat a passing note must still be a chord tone
            const uint16_t mask = w >= 0.5 ? c.toneMask : c.scaleMask;
            int q = nearIn (mask, prev, dir, lo, hi);
            if (q == prev || q < lo || q > hi) q = nearIn (mask, prev, -dir, lo, hi);
            return std::clamp (q, lo, hi);
        }
        case TF_NextRoot: return std::clamp (rootPitchOf (next.rootPc, lo), lo, hi);
        default: return pitchForRole (roleOfFunc (f), c, next, prev, lo, hi, rng, step);
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
    const bool walking = (baseGenre (S.genre) == G_Swing) || st.name.rfind ("Ron Carter", 0) == 0 || st.name.rfind ("Blues Shuffle", 0) == 0;

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
    const bool claveOn = S.clave > 0 && (baseGenre (S.genre) == G_Salsa || baseGenre (S.genre) == G_AfroCuban) && m.simpleFour;
    auto pickSide = [&] (bool three) -> std::string { const auto& b = bassCellsSide (S.genre, three); return b[(size_t) rng.i (0, (int) b.size() - 1)]; };
    std::string mainCell = claveOn ? pickSide (true) : pickCell(), varCell = claveOn ? pickSide (true) : pickCell();
    std::string mainCell2 = claveOn ? pickSide (false) : std::string(), varCell2 = claveOn ? pickSide (false) : std::string();
    int prevPitch = rootPitchOf (chords.front().rootPc, lo);
    // v2.12: the genre's musical vocabulary, and each rhythm cell's motif (its melodic shape, decided once so the hook returns)
    const Vocab voc = vocabFor (S.genre, st.name);
    std::vector<double> wts ((size_t) steps); for (int i = 0; i < steps; ++i) wts[(size_t) i] = m.weight[(size_t) i];
    std::map<std::string, std::vector<int>> motifOf;
    auto funcsOf = [&] (const std::string& cell) -> const std::vector<int>& { auto it = motifOf.find (cell); if (it != motifOf.end()) return it->second; return motifOf[cell] = makeFuncs (cell, wts, voc, st, rng); };
    const bool callResponse = (baseGenre (S.genre) == G_Funk || baseGenre (S.genre) == G_Afrobeat || baseGenre (S.genre) == G_Amapiano || baseGenre (S.genre) == G_Highlife || baseGenre (S.genre) == G_Gospel);

    // ---- v2.7 Amapiano log-drum phrasing: two-bar call and response, a fill at the end of every 4-bar phrase, new pairs now and then
    const bool amapiano = isAmapianoFamily (S.genre) && m.simpleFour && steps == 16;
    static const std::vector<std::string> amaCall = {
        "R..R..R...R..o..", "R..R...R..R.f...", "R.....R.R..R..f.", "R..R..o...R..R..", "R...R..R..R.R...", "R..R..R..R....f." };
    static const std::vector<std::string> amaResp = {
        ".R..R..R..R.f...", "R..f..R...o..R..", "..R..R.R...R.f..", "R.....f.R..R.a..", "...R..R...R..o..", "R..R....R.R..f.." };
    static const std::vector<std::string> amaFill = {
        "R..R..R.R.R.fo.a", "R..R..RRo..R.f.a", "R..R.R..R.o.o.fa", "R.R..R..R..R.o.R" };
    int amaPair = rng.i (0, (int) amaCall.size() - 1);

    for (int bar = 0; bar < S.bars; ++bar)
    {
        const double b0 = bar * m.barBeats;
        const bool phraseStart = (bar % phrase == 0);
        const bool phraseEnd = (bar % phrase == phrase - 1);
        if (phraseStart && bar > 0 && ! rng.chance (std::clamp (0.35 + 0.6 * (double) st.motif * (0.5 + S.repeat), 0.0, 0.97))) { if (claveOn) { mainCell = pickSide (true); varCell = pickSide (true); mainCell2 = pickSide (false); varCell2 = pickSide (false); } else { mainCell = pickCell(); varCell = pickCell(); } }
        const bool threeSide = ((bar % 2 == 0) == (S.clave != 2));        // clave 1 = 3-2 (bar 1 is the 3 side), clave 2 = 2-3
        std::string cell = (claveOn && ! threeSide) ? mainCell2 : mainCell;
        // v2.12 the phrase plan: bar 1 states the motif, bar 2 answers it (its ending turns toward the next chord or answers in another
        // register), bar 3 develops it (the variant, one note moved an octave), the last bar releases it (a fragment and a fill)
        const int jp = bar % phrase;
        enum Intent { IN_Statement, IN_Answer, IN_Development, IN_Release };
        const Intent intent = (phrase >= 2 && jp == phrase - 1) ? IN_Release : (jp % 4 == 1 ? IN_Answer : (jp % 4 == 2 ? IN_Development : IN_Statement));
        if (intent == IN_Development && rng.chance (std::clamp (0.35 - (double) S.repeat * 0.25, 0.08, 0.4))) cell = (claveOn && ! threeSide) ? varCell2 : varCell;   // usually the motif itself, varied
        if (amapiano)
        {
            if (phraseStart && bar > 0 && rng.chance (std::clamp (0.6 - (double) S.repeat * 0.5, 0.1, 0.7))) amaPair = rng.i (0, (int) amaCall.size() - 1);
            // the sub-genres answer with their own cells: Private School fills rarely, Bacardi often, Sgija repeats its call
            const double fillP = S.genre == G_PrivateSchool ? 0.35 : S.genre == G_Bacardi ? 0.9 : 0.75;
            if (bar % 4 == 3 && rng.chance (fillP)) cell = amaFill[(size_t) rng.i (0, (int) amaFill.size() - 1)];
            else if (S.genre != G_Amapiano) cell = (bar % 2 == 0 || S.genre == G_Sgija) ? mainCell : varCell;
            else if (bar % 2 == 0) cell = amaCall[(size_t) amaPair];
            else cell = rng.chance (0.8) ? amaResp[(size_t) amaPair] : amaResp[(size_t) rng.i (0, (int) amaResp.size() - 1)];
        }

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

        // v2.12: the motif's own choices (which weak hits it keeps, its pickups, ghosts, the pitches of its steps) come from a seed of the
        // cell and the bar's place in the phrase plan, so a statement and its repeat really are the same figure; the development and
        // the release vary it, and every second phrase it evolves a little
        const int variant = (intent == IN_Development ? 1 : 0) + (intent == IN_Release ? 2 : 0) + 8 * ((bar / std::max (1, phrase)) / 2);
        Rng rr ((uint64_t) S.seed * 2654435761ULL ^ (uint64_t) std::hash<std::string>() (cell) ^ ((uint64_t) variant * 0x9E3779B97F4A7C15ULL));
        // busy slider: per-bar override or global
        // v2.14.1: a bar you did not set by hand follows its song section (sparser intro / outro, busier chorus)
        const double busy = (bar < (int) S.barBusy.size() && S.barBusy[(size_t) bar] >= 0.f) ? S.barBusy[(size_t) bar]
                          : std::clamp ((double) S.busy + (S.transAmount > 0.f && transitions::songHasSections (S) ? transitions::sectionBusy (transitions::sectionOf (S, bar)) : 0.0), 0.0, 1.0);
        const double keep = std::clamp ((double) st.density * (0.3 + busy * 1.1) * gi.densMul * (0.7 + 0.5 * S.intensity) - (S.space - 0.5) * 0.4, 0.1, 1.0);
        std::string c2 = cell;
        for (int i = 1; i < steps; ++i)
        {
            if (c2[(size_t) i] == '.' ) continue;
            const double w = m.weight[(size_t) i];
            if (! fromRef && w < 0.8 && ! rr.chance (std::min (1.0, keep + w * 0.35))) c2[(size_t) i] = '.';
        }
        // busy: extra hits on the empty eighths
        if (busy > 0.55 && ! fromRef)
            for (int i = 2; i < steps; i += 2)
                if (c2[(size_t) i] == '.' && rr.chance ((busy - 0.55) * 0.6 * (double) st.density)) c2[(size_t) i] = 'x';
        // anticipations: a hit on the beat is pushed one 16th early
        for (int i = 4; i < steps; i += 4)
            if (! fromRef && c2[(size_t) i] != '.' && c2[(size_t) (i - 1)] == '.' && rr.chance (st.sync * 0.28)) { c2[(size_t) (i - 1)] = c2[(size_t) i]; c2[(size_t) i] = '.'; }
        // v2.12 Bar Notes Density: low = space and the anchors held long; high = pickups into the beats, never every subdivision
        if (! fromRef && busy < 0.3)
            for (int i = 1; i < steps; ++i)
                if (c2[(size_t) i] != '.' && m.weight[(size_t) i] < 0.5 && c2[(size_t) i] != 'a' && rr.chance ((0.3 - busy) / 0.3 * 0.85)) c2[(size_t) i] = '.';
        if (! fromRef && busy > 0.65 && steps == 16)
            for (int i : { 3, 7, 11 })
                if (c2[(size_t) i] == '.' && c2[(size_t) (i + 1)] != '.' && rr.chance ((busy - 0.65) * 1.6 * (0.5 + (double) st.density) + 0.05)) c2[(size_t) i] = 'p';
        // gospel / jazz / soul: walking movement, passing 8ths between hits that are far apart (they step toward the next hit)
        if (! fromRef && voc.guide > 0 && steps == 16 && busy >= 0.25)
            for (int i = 2; i < steps; i += 2)
            {
                if (c2[(size_t) i] != '.') continue;
                int before = -1, after = -1;
                for (int k = i - 1; k >= 0; --k) if (c2[(size_t) k] != '.' && c2[(size_t) k] != 'g') { before = k; break; }
                for (int k = i + 1; k < steps; ++k) if (c2[(size_t) k] != '.' && c2[(size_t) k] != 'g') { after = k; break; }
                if (before >= 0 && i - before >= 2 && (after < 0 || after - i >= 2) && rr.chance ((0.3 + 0.5 * busy) * std::min (1.0, voc.guide * 1.6)))
                    c2[(size_t) i] = 'p';
            }
        // ghost-note fills on empty 16ths when busy
        for (int i = 1; i < steps; ++i)
            if (! fromRef && c2[(size_t) i] == '.' && (i % 2 == 1) && rr.chance (std::max (0.0, (busy - 0.3)) * 0.8 * (0.3 + (double) st.ghost * 3.0) * (0.5 + S.intensity)))
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
        // never every subdivision: at most 5 + 7 x density hits a bar, and each half bar keeps at least one empty 8th
        if (! fromRef && steps == 16)
        {
            const int cap = (int) std::lround (5 + 7 * busy);
            auto count = [&] { int c = 0; for (char ch : c2) if (ch != '.') ++c; return c; };
            while (count() > cap)
            {
                int worst = -1; double wv = 9;
                for (int i = 1; i < steps; ++i) if (c2[(size_t) i] != '.' && c2[(size_t) i] != 'a' && m.weight[(size_t) i] < wv) { wv = m.weight[(size_t) i]; worst = i; }
                if (worst < 0) break;
                c2[(size_t) worst] = '.';
            }
            for (int half = 0; half < 2; ++half)
            {
                bool room = false; for (int i = half * 8; i < half * 8 + 8; i += 2) if (c2[(size_t) i] == '.' && c2[(size_t) i + 1] == '.') room = true;
                if (! room) c2[(size_t) (half * 8 + 6)] = '.', c2[(size_t) (half * 8 + 7)] = c2[(size_t) (half * 8 + 7)] == 'p' ? '.' : c2[(size_t) (half * 8 + 7)];
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
        // the release bar without a fill: the motif's first half, then space (unless the density is high)
        if (S.transAmount <= 0.f && intent == IN_Release && ! fromRef && steps == 16 && busy < 0.6 && rr.chance (0.5))   // v2.14.1: the planner decides endings
            for (int i = 10; i < steps; ++i) if (c2[(size_t) i] != 'a' && c2[(size_t) i] != 'R') c2[(size_t) i] = '.';
        // fill at the end of a phrase
        bool fill = false;
        if (S.transAmount <= 0.f && phraseEnd && steps >= 16 && bar + 1 <= S.bars && rng.chance (std::clamp ((double) st.density * 0.55 * (0.4 + busy) + 0.12, 0.0, 0.85) * (rv.valid ? 1.0 - S.refAmount * 0.9 : 1.0)))
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
                    if (k == 2 && voc.guide > 0 && rng.chance (voc.guide)) { role = rng.chance (0.6) ? BR_Third : BR_Seventh; pitch = pitchForRole (role, c, nxt, p, lo, hi, rng, 1.f); }   // v2.12 guide tone
                    else if (chordTone) { role = (rng.chance (0.5) ? BR_Fifth : BR_Third); pitch = pitchForRole (role, c, nxt, p, lo, hi, rng, 1.f); }
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
            if ((baseGenre (S.genre) == G_Salsa || baseGenre (S.genre) == G_AfroCuban || baseGenre (S.genre) == G_Caribbean || baseGenre (S.genre) == G_AfroBrazilian) && steps >= 16 && nextBarChord.rootPc != at (b0 + m.barBeats - 0.01).rootPc && rng.chance (0.8))
                for (auto& hh : hits) if (hh.step >= 13 && hh.step <= 14 && hh.ch != 'g') hh.ch = 'n';
            // tiny rhythm safety: never an empty bar
            if (hits.empty()) hits.push_back ({ 0, 'R' });
            int p = prevPitch;
            const auto& motif = funcsOf (cell);
            // development: one weak note of the motif moves up an octave; answer: the last note turns toward the next chord
            int devHit = -1; if (intent == IN_Development) for (size_t h = 0; h < hits.size(); ++h) if (hits[h].step >= 4 && m.weight[(size_t) hits[h].step] < 0.55 && hits[h].ch != 'g') { devHit = hits[h].step; break; }
            int lastHit = -1; for (size_t h = 0; h < hits.size(); ++h) if (hits[h].ch != 'g' && hits[h].step >= 10) lastHit = hits[h].step;
            for (size_t h = 0; h < hits.size(); ++h)
            {
                const int i = hits[h].step;
                double t = b0 + i * stepLen;
                const ChordSpan& c = at (t + 0.001);
                const double nextT = (h + 1 < hits.size()) ? b0 + hits[h + 1].step * stepLen : b0 + m.barBeats;
                const ChordSpan& nxt = (h + 1 < hits.size()) ? at (nextT + 0.001) : nextBarChord;
                const char ch = hits[h].ch;
                // the motif's function where the hit is the cell's own, else from the letter
                int fn = (i < (int) motif.size() && cell.size() == (size_t) steps && cell[(size_t) i] == ch) ? motif[(size_t) i] : TF_Rest;
                if (fn == TF_Rest)
                    switch (ch)
                    {
                        case 'R': fn = TF_Root; break; case 'f': fn = TF_Fifth; break; case 'o': fn = TF_Octave; break; case 't': fn = TF_Third; break;
                        case 'b': fn = TF_Seventh; break; case 'a': fn = TF_Approach; break; case 'n': fn = TF_NextRoot; break; case 'g': fn = TF_Ghost; break;
                        case 'p': fn = TF_PassUp; break; default: fn = TF_Any; break;
                    }
                if (fn == TF_Any) fn = rr.chance (0.5) ? TF_Fifth : TF_Octave;
                // a new chord in the bar is always met by its root
                for (auto& chd : chords) if (std::abs (chd.start - t) < 1e-6 && fn != TF_Ghost) fn = TF_Root;
                // an approach only approaches a new chord; otherwise it walks toward the root
                if (fn == TF_Approach && i < steps - 4 && nxt.rootPc == c.rootPc) fn = TF_PassDown;
                if (intent == IN_Answer && i == lastHit && fn != TF_NextRoot)
                    fn = nextBarChord.rootPc != c.rootPc ? TF_Approach : (callResponse ? TF_Octave : TF_PassUp);
                // where the line is heading: the next chord's root nearest to here
                int toward = rootPitchOf (nxt.rootPc, lo); while (toward + 12 <= hi && std::abs (toward + 12 - p) < std::abs (toward - p)) toward += 12;
                int pitch = pitchForFunc (fn, c, nxt, p, lo, hi, rr, st.step, m.weight[(size_t) i], toward);
                if (i == devHit && pitch + 12 <= hi) pitch += 12;
                if (fn == TF_Ghost) pitch = p >= lo && p <= hi && std::abs (p - rootPitchOf (c.rootPc, lo)) <= 12 ? p : pitch;
                const int role = roleOfFunc (fn);
                // swing: off-beat eighths are pushed late
                if (swingAmt > 0.01 && (i % 4 == 2)) t += swingAmt * (m.barBeats / m.num) / 6.0;
                const double gap = nextT - (b0 + i * stepLen);
                double len;
                if (role == BR_Ghost) len = std::min (0.14, gap * 0.9);
                else len = gap * (busy < 0.3 ? 0.92 : (0.45 + 0.53 * (double) st.legato));      // sparse: held long
                len = std::max (0.08, std::min (len, gap * 0.99));
                int art = (role == BR_Ghost) ? A_Ghost : A_Sustain;
                bar_notes.push_back (mk (t, len, pitch, role, velFor (m.weight[(size_t) i], role), art));
                if (role != BR_Ghost) p = pitch;
            }
            // v2.12 fill devices on the last beat, chosen by the genre: melodic (run down, run up, chromatic walk-up, enclosure)
            // or rhythmic (octave pop, dead-note pickup); every one lands on the next chord's root
            if (fill)
            {
                const ChordSpan& c = at (b0 + m.barBeats - 0.01);
                int T = rootPitchOf (nextBarChord.rootPc, lo); while (T + 12 <= hi && std::abs (T + 12 - p) < std::abs (T - p)) T += 12;
                T = std::clamp (T, lo + 2, hi - 2);
                const int dev = (int) rng.weighted ({ voc.fillMelodic * 1.0, voc.fillMelodic * 0.9, voc.fillMelodic * 0.7, voc.fillMelodic * 0.6, (1.0 - voc.fillMelodic) * 1.2, (1.0 - voc.fillMelodic) * (0.4 + st.ghost * 4.0) });
                struct FN { int step; int pitch; bool ghost; };
                std::vector<FN> fn;
                auto scaleFrom = [&] (int from, int dir, int n) { std::vector<int> r; int q = from; for (int k = 0; k < n; ++k) { const int nq = nearIn (c.scaleMask, q, dir, lo, hi); q = nq == q ? std::clamp (q + dir * 2, lo, hi) : nq; r.push_back (q); } return r; };
                const int cur = rootPitchOf (c.rootPc, lo);
                switch (dev)
                {
                    case 0: { auto r = scaleFrom (std::min (hi, T + 7), -1, 3); fn = { { 12, r[0], false }, { 13, r[1], false }, { 14, r[2], false }, { 15, T + 1, false } }; break; }     // run down
                    case 1: { auto r = scaleFrom (std::max (lo, T - 6), 1, 3); fn = { { 12, std::max (lo, T - 7), false }, { 13, r[0], false }, { 14, r[1], false }, { 15, T - 1, false } }; break; } // run up
                    case 2: fn = { { 12, cur + fifthIv (c.toneMask, c.rootPc) <= hi ? cur + fifthIv (c.toneMask, c.rootPc) : cur, false }, { 13, T - 3, false }, { 14, T - 2, false }, { 15, T - 1, false } }; break;   // chromatic walk-up
                    case 3: fn = { { 12, cur, false }, { 14, nearIn (nextBarChord.scaleMask, T, 1, lo, hi), false }, { 15, T - 1, false } }; break;        // enclosure
                    case 4: fn = { { 12, cur, false }, { 13, cur, true }, { 14, std::min (hi, cur + 12), false }, { 15, T + (rng.chance (0.5) ? 1 : -1), false } }; break;   // octave pop
                    default: fn = { { 12, cur, false }, { 13, cur, true }, { 14, cur, true }, { 15, T - 1, false } }; break;   // dead-note pickup
                }
                for (auto& f : fn)
                {
                    const double t = b0 + f.step * stepLen;
                    const int pitch = std::clamp (f.pitch, lo, hi);
                    bar_notes.push_back (mk (t, f.ghost ? 0.12 : stepLen * 0.85, pitch, f.ghost ? BR_Ghost : (f.step == 15 ? BR_Approach : BR_Other), velFor (0.2, f.ghost ? BR_Ghost : BR_Other) + 6, f.ghost ? A_Ghost : A_Sustain));
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
            // v2.10: every other articulation is chosen by the articulation brain (Engine/ArtBrain.h), phrase by phrase
            (void) gapPrev; (void) aiv; (void) strong; (void) iv;
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
