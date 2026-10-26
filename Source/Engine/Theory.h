// Enoch Bass - music theory helpers (header only, no JUCE dependency)
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <random>

namespace enochsolo
{

inline const char* const kNoteNames[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

//==============================================================================
struct Rng
{
    std::mt19937_64 g;
    explicit Rng (uint64_t seed = 1) : g (seed) {}
    double f()                      { return std::uniform_real_distribution<double> (0.0, 1.0) (g); }
    double f (double a, double b)   { return a + (b - a) * f(); }
    int i (int a, int b)            { if (b <= a) return a; return std::uniform_int_distribution<int> (a, b) (g); }
    bool chance (double p)          { return f() < p; }
    double gauss (double sigma)     { return std::normal_distribution<double> (0.0, sigma) (g); }
    template <class T> const T& pick (const std::vector<T>& v) { return v[(size_t) i (0, (int) v.size() - 1)]; }
    int weighted (const std::vector<double>& w)
    {
        double s = 0; for (double x : w) s += std::max (0.0, x);
        if (s <= 0) return i (0, (int) w.size() - 1);
        double r = f() * s;
        for (size_t k = 0; k < w.size(); ++k) { r -= std::max (0.0, w[k]); if (r <= 0) return (int) k; }
        return (int) w.size() - 1;
    }
};

//==============================================================================
enum class Quality { Maj7, Dom7, Min7, Min7b5, Dim7, Maj6, Min6, Sus4, MinMaj7,
                     Maj, Min, Maj9, Dom9, Min9, Dom13, Min11, Aug7, Dom7alt, Sus2, Count };

/** four colour intervals per chord: [0] root, [1] "third", [2] "fifth", [3] "seventh" slot (triads use the 9th there) */
inline std::vector<int> chordIntervals (Quality q)
{
    switch (q)
    {
        case Quality::Maj7:    return { 0, 4, 7, 11 };
        case Quality::Dom7:    return { 0, 4, 7, 10 };
        case Quality::Min7:    return { 0, 3, 7, 10 };
        case Quality::Min7b5:  return { 0, 3, 6, 10 };
        case Quality::Dim7:    return { 0, 3, 6, 9 };
        case Quality::Maj6:    return { 0, 4, 7, 9 };
        case Quality::Min6:    return { 0, 3, 7, 9 };
        case Quality::Sus4:    return { 0, 5, 7, 10 };
        case Quality::MinMaj7: return { 0, 3, 7, 11 };
        case Quality::Maj:     return { 0, 4, 7, 2 };
        case Quality::Min:     return { 0, 3, 7, 2 };
        case Quality::Maj9:    return { 0, 4, 11, 2 };
        case Quality::Dom9:    return { 0, 4, 10, 2 };
        case Quality::Min9:    return { 0, 3, 10, 2 };
        case Quality::Dom13:   return { 0, 4, 10, 9 };
        case Quality::Min11:   return { 0, 3, 10, 5 };
        case Quality::Aug7:    return { 0, 4, 8, 10 };
        case Quality::Dom7alt: return { 0, 4, 10, 8 };
        case Quality::Sus2:    return { 0, 2, 7, 10 };
        default: break;
    }
    return { 0, 4, 7, 10 };
}

inline const char* qualitySuffix (Quality q)
{
    switch (q)
    {
        case Quality::Maj7: return "maj7";   case Quality::Dom7: return "7";
        case Quality::Min7: return "m7";     case Quality::Min7b5: return "m7b5";
        case Quality::Dim7: return "dim7";   case Quality::Maj6: return "6";
        case Quality::Min6: return "m6";     case Quality::Sus4: return "7sus4";
        case Quality::MinMaj7: return "m(maj7)";
        case Quality::Maj: return "";        case Quality::Min: return "m";
        case Quality::Maj9: return "maj9";   case Quality::Dom9: return "9";
        case Quality::Min9: return "m9";     case Quality::Dom13: return "13";
        case Quality::Min11: return "m11";   case Quality::Aug7: return "7#5";
        case Quality::Dom7alt: return "7alt"; case Quality::Sus2: return "sus2";
        default: break;
    }
    return "";
}

/** order used by the chord drop-down menu (most useful first) */
inline const std::vector<Quality>& qualityMenuOrder()
{
    static const std::vector<Quality> v = {
        Quality::Maj, Quality::Min, Quality::Dom7, Quality::Maj7, Quality::Min7, Quality::Dom9, Quality::Maj9, Quality::Min9,
        Quality::Maj6, Quality::Min6, Quality::Sus4, Quality::Sus2, Quality::Min7b5, Quality::Dim7, Quality::Dom13, Quality::Min11,
        Quality::Aug7, Quality::Dom7alt, Quality::MinMaj7 };
    return v;
}

inline bool qualityIsMinor (Quality q)
{
    return q == Quality::Min7 || q == Quality::Min7b5 || q == Quality::Min6 || q == Quality::MinMaj7 || q == Quality::Dim7
        || q == Quality::Min || q == Quality::Min9 || q == Quality::Min11;
}

/** a chord the user picked for one bar (degree = semitones above the key root, so it follows key changes) */
struct BarChord { bool set = false; int degree = 0; Quality q = Quality::Min7; int mode = -1; };   // mode >= 0: play this bar in that scale / mode (see Modes.h)

inline uint16_t maskOf (const std::vector<int>& pcs)
{
    uint16_t m = 0; for (int p : pcs) m |= (uint16_t) (1u << (((p % 12) + 12) % 12)); return m;
}
inline bool inMask (uint16_t m, int pc) { return (m >> (((pc % 12) + 12) % 12)) & 1; }

//==============================================================================
enum Scale { SC_Major = 0, SC_Minor, SC_Dorian, SC_Mixolydian, SC_Blues, SC_HarmonicMinor, SC_PhrygianDom, SC_MajPent, SC_MinPent, SC_Count };
inline const char* scaleName (int s)
{
    static const char* n[] = { "Major", "Minor", "Dorian", "Mixolydian", "Blues (minor)", "Harmonic Minor",
                               "Spanish (Phrygian dominant)", "Major Pentatonic", "Minor Pentatonic" };
    return n[std::clamp (s, 0, (int) SC_Count - 1)];
}

/** progression family: 0 = major, 1 = minor, 2 = dominant (mixolydian / phrygian dominant) */
inline int scaleFamily (int s)
{
    switch (s) { case SC_Major: case SC_MajPent: return 0; case SC_Mixolydian: case SC_PhrygianDom: return 2; default: return 1; }
}

//==============================================================================
struct ChordSpan
{
    double start = 0, end = 0;
    int rootPc = 0;           // absolute pitch class
    int degree = 0;           // semitones above key root
    Quality q = Quality::Min7;
    uint16_t scaleMask = 0;   // chord-scale pitch classes (absolute)
    uint16_t toneMask = 0;    // chord tones (absolute)
    std::string name;
};

/** Pick the chord-scale (absolute pc mask) for a chord inside a key / scale choice. */
inline uint16_t chordScaleFor (int keyRoot, int scale, int degree, Quality q)
{
    std::vector<int> iv;
    auto minorScale = [&]
    {
        if (scale == SC_PhrygianDom && degree == 0)        return std::vector<int> { 0, 1, 3, 5, 7, 8, 10 };   // phrygian
        if (degree == 0 && scale == SC_Minor)              return std::vector<int> { 0, 2, 3, 5, 7, 8, 10 };   // aeolian
        if (degree == 0 && scale == SC_HarmonicMinor)      return std::vector<int> { 0, 2, 3, 5, 7, 8, 11 };   // harmonic minor
        return std::vector<int> { 0, 2, 3, 5, 7, 9, 10 };                                                       // dorian
    };
    auto majorScale = [&]
    {
        if (degree == 5 || degree == 8 || degree == 3)     return std::vector<int> { 0, 2, 4, 6, 7, 9, 11 };   // lydian on IV / bVI / bIII
        if (degree == 10 || (degree == 0 && scale == SC_Mixolydian)) return std::vector<int> { 0, 2, 4, 5, 7, 9, 10 };   // mixolydian
        return std::vector<int> { 0, 2, 4, 5, 7, 9, 11 };
    };
    auto domScale = [&]
    {
        if (scale == SC_PhrygianDom && degree == 0)        return std::vector<int> { 0, 1, 4, 5, 7, 8, 10 };   // phrygian dominant
        if (scale == SC_HarmonicMinor && degree == 7)      return std::vector<int> { 0, 1, 4, 5, 7, 8, 10 };
        return std::vector<int> { 0, 2, 4, 5, 7, 9, 10 };                                                       // mixolydian
    };
    switch (q)
    {
        case Quality::Min7: case Quality::Min: case Quality::Min9: case Quality::Min11: iv = minorScale(); break;
        case Quality::Dom7: case Quality::Dom9: case Quality::Dom13: iv = domScale(); break;
        case Quality::Maj7: case Quality::Maj9: iv = majorScale(); break;
        case Quality::Maj:
            if (scale == SC_PhrygianDom && degree == 0)       iv = { 0, 1, 4, 5, 7, 8, 10 };
            else if (degree == 7 && scaleFamily (scale) != 0) iv = { 0, 2, 4, 5, 7, 8, 10 };                    // V in a minor key (mixolydian b6)
            else                                              iv = majorScale();
            break;
        case Quality::Maj6:    iv = { 0, 2, 4, 5, 7, 9, 11 }; break;
        case Quality::Min7b5:  iv = { 0, 2, 3, 5, 6, 8, 10 }; break;   // locrian #2
        case Quality::Dim7:    iv = { 0, 2, 3, 5, 6, 8, 9, 11 }; break;
        case Quality::Min6:    iv = { 0, 2, 3, 5, 7, 9, 11 }; break;   // melodic minor
        case Quality::Sus4: case Quality::Sus2: iv = { 0, 2, 4, 5, 7, 9, 10 }; break;
        case Quality::MinMaj7: iv = { 0, 2, 3, 5, 7, 9, 11 }; break;
        case Quality::Aug7:    iv = { 0, 2, 4, 6, 8, 10 }; break;      // whole tone
        case Quality::Dom7alt: iv = { 0, 1, 3, 4, 6, 8, 10 }; break;   // altered
        default:               iv = { 0, 2, 4, 5, 7, 9, 10 }; break;
    }
    const int root = (keyRoot + degree) % 12;
    if (scale == SC_MajPent || scale == SC_MinPent)
    {
        // pentatonic scale choice: pentatonic of the chord, plus the chord's own tones so the line can still outline the changes
        const std::vector<int> pent = qualityIsMinor (q) ? std::vector<int> { 0, 3, 5, 7, 10 } : std::vector<int> { 0, 2, 4, 7, 9 };
        std::vector<int> a; for (int x : pent) a.push_back (root + x);
        for (int x : chordIntervals (q)) a.push_back (root + x);
        return maskOf (a);
    }
    std::vector<int> abs; for (int x : iv) abs.push_back (root + x);
    return maskOf (abs);
}

inline uint16_t chordToneMaskFor (int rootPc, Quality q)
{
    std::vector<int> abs; for (int x : chordIntervals (q)) abs.push_back (rootPc + x);
    return maskOf (abs);
}

/** Pentatonic subset (absolute pc mask) for a chord */
inline uint16_t pentSubsetFor (int rootPc, Quality q)
{
    std::vector<int> iv;
    if (qualityIsMinor (q))        iv = { 0, 3, 5, 7, 10 };
    else if (q == Quality::Dom7 || q == Quality::Sus4) iv = { 0, 2, 4, 7, 9 };
    else                           iv = { 0, 2, 4, 7, 9 };
    if (q == Quality::Min7b5)      iv = { 0, 3, 5, 6, 8, 10 };
    std::vector<int> abs; for (int x : iv) abs.push_back (rootPc + x);
    return maskOf (abs);
}

/** Blue notes (absolute pc mask) for a chord */
inline uint16_t blueNotesFor (int rootPc, Quality q)
{
    if (qualityIsMinor (q)) return maskOf ({ rootPc + 6 });                 // b5
    return maskOf ({ rootPc + 3, rootPc + 6 });                              // b3 / b5 against dominant chords
}

//==============================================================================
enum Genre { G_Afrobeat = 0, G_AfroJazz, G_Highlife, G_AfroFusion, G_Swing, G_Ballad,
             G_Amapiano, G_Juju, G_Soukous, G_EthioJazz, G_Township, G_Gospel, G_Pop, G_Rock, G_Bossa, G_Samba, G_Flamenco,
             G_Salsa, G_AfroCuban, G_AfroBrazilian, G_Caribbean, G_Reggae, G_Funk, G_HipHop, G_AfroHouse, G_Count };

struct GenreInfo
{
    const char* name;
    double fixedSwing;   // <0 -> use the artist's own swing
    double riffy;        // how strongly motifs / riffs repeat
    double densMul;
    double restMul;
    int    phraseBars;   // 0 = free
    bool   afro;
    const char* desc;
};

inline const GenreInfo& genreInfo (int g)
{
    static const GenreInfo t[G_Count] = {
        { "Afrobeat",            0.00, 0.85, 1.00, 0.9, 2, true,  "Fela-style horn riffs, syncopated 16ths, call and response" },
        { "Afro-Jazz",           0.20, 0.50, 1.00, 1.0, 0, true,  "Afrobeat groove with jazz language" },
        { "Highlife",            0.10, 0.60, 1.00, 0.9, 2, true,  "Bright West African highlife, jaunty and melodic" },
        { "Afro-Fusion",         0.00, 0.40, 0.75, 1.2, 2, true,  "Modern Afrobeats / afro-fusion, laid back and melodic" },
        { "Swing Jazz",         -1.00, 0.20, 1.00, 1.0, 0, false, "Classic swing / bebop feel" },
        { "Smooth / Ballad",     0.30, 0.20, 0.50, 1.3, 0, false, "Slow, lyrical, expressive" },
        { "Amapiano",            0.18, 0.85, 0.80, 1.2, 2, true,  "South African amapiano: log-drum bounce, jazzy piano riffs, laid-back house groove" },
        { "Juju / Fuji",         0.05, 0.70, 1.00, 0.9, 2, true,  "Nigerian juju: interlocking singing lines over talking-drum rhythm" },
        { "Soukous",             0.00, 0.70, 1.15, 0.8, 2, true,  "Congolese rumba: bright cascading sebene guitar lines" },
        { "Ethio-Jazz",          0.10, 0.65, 0.80, 1.2, 0, true,  "Hypnotic modal-pentatonic Ethiopian jazz" },
        { "Township Jive",       0.15, 0.70, 1.00, 0.9, 2, true,  "Mbaqanga / Masekela-style: joyous, bouncing, call and response" },
        { "Gospel / Soul",       0.20, 0.40, 0.90, 1.0, 0, false, "Soulful, blues-gospel melodic lines with call and response" },
        { "Pop",                 0.00, 0.60, 0.70, 1.2, 2, false, "Hooky, singable melodic lines over the changes" },
        { "Rock",                0.00, 0.60, 0.85, 1.0, 2, false, "Pentatonic and blues-based riffs and licks with attitude" },
        { "Bossa Nova",          0.00, 0.40, 0.80, 1.2, 0, false, "Brazilian bossa: gentle syncopation, lyrical, sophisticated harmony" },
        { "Samba / Brazilian",   0.00, 0.50, 1.00, 1.0, 0, false, "Lively Brazilian samba / choro: running 16ths and syncopation" },
        { "Flamenco / Spanish",  0.00, 0.50, 1.10, 0.9, 0, false, "Phrygian colour, fast picado runs, dramatic ornaments" },
        { "Salsa",               0.00, 0.85, 1.00, 0.9, 2, false, "Salsa tumbao: anticipated bass that dances around the clave and the conga" },
        { "Afro-Cuban", 0.00, 0.80, 1.00, 0.9, 2, true, "Son, mambo, rumba, cha-cha-cha and bolero bass: the roots of salsa, close cousins of Afrobeat" },
        { "Afro-Brazilian", 0.00, 0.80, 1.00, 0.9, 2, true, "Baiao, maracatu, afoxe / ijexa, samba-reggae, axe and forro: Yoruba and Bantu rhythm in Brazil" },
        { "Caribbean", 0.00, 0.80, 1.00, 0.9, 2, false, "Cumbia, merengue, bachata, zouk and compas pulses" },
        { "Reggae & Dub",        0.00, 0.85, 0.75, 1.3, 2, false, "One-drop and rockers: space, deep roots and fifths, dub weight" },
        { "Funk & Disco",        0.00, 0.60, 1.10, 0.8, 2, false, "Pocket sixteenths, ghost notes, slap and disco octaves" },
        { "Hip-Hop & Trap",      0.00, 0.80, 0.65, 1.4, 2, false, "Boom-bap and 808: long sub notes, sparse hits and slides" },
        { "Afro-House",          0.00, 0.85, 0.85, 1.1, 2, true,  "Rolling offbeat house bass with African swing" }
    };
    return t[std::clamp (g, 0, (int) G_Count - 1)];
}

//==============================================================================
struct ProgChord { int degree; Quality q; double bars; };
struct Prog { int genre; int family; const char* name; std::vector<ProgChord> chords; };

inline const std::vector<Prog>& allProgressions()
{
    using Q = Quality;
    static const std::vector<Prog> p = {
        // ---------------- Afrobeat
        { G_Afrobeat, 1, "i7 | IV7",                { {0,Q::Min7,2}, {5,Q::Dom7,2} } },
        { G_Afrobeat, 1, "i7 | IV7 | i7 | bVII7",   { {0,Q::Min7,1}, {5,Q::Dom7,1}, {0,Q::Min7,1}, {10,Q::Dom7,1} } },
        { G_Afrobeat, 1, "i7 vamp",                 { {0,Q::Min7,4} } },
        { G_Afrobeat, 1, "i7 | bVII7 | IV7",        { {0,Q::Min7,2}, {10,Q::Dom7,1}, {5,Q::Dom7,1} } },
        { G_Afrobeat, 1, "i7 | bVImaj7 | bVII7 | i7", { {0,Q::Min7,1}, {8,Q::Maj7,1}, {10,Q::Dom7,1}, {0,Q::Min7,1} } },
        { G_Afrobeat, 0, "I7 | IV7",                { {0,Q::Dom7,2}, {5,Q::Dom7,2} } },
        { G_Afrobeat, 0, "I7 | IV7 | I7 | V7",      { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        { G_Afrobeat, 0, "I7 vamp",                 { {0,Q::Dom7,4} } },
        { G_Afrobeat, 0, "I7 | bVII7 | IV7 | I7",   { {0,Q::Dom7,1}, {10,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1} } },
        { G_Afrobeat, 2, "I7 | IV7",                { {0,Q::Dom7,2}, {5,Q::Dom7,2} } },
        { G_Afrobeat, 2, "I7 | bVII7",              { {0,Q::Dom7,2}, {10,Q::Dom7,2} } },
        { G_Afrobeat, 2, "I7 vamp",                 { {0,Q::Dom7,4} } },
        // ---------------- Afro-Jazz
        { G_AfroJazz, 1, "iim7b5 | V7 | i7",        { {2,Q::Min7b5,1}, {7,Q::Dom7,1}, {0,Q::Min7,2} } },
        { G_AfroJazz, 1, "i7 | IV7 | iim7b5 | V7",  { {0,Q::Min7,1}, {5,Q::Dom7,1}, {2,Q::Min7b5,1}, {7,Q::Dom7,1} } },
        { G_AfroJazz, 1, "i7 | bVII7 | bVImaj7 | V7", { {0,Q::Min7,1}, {10,Q::Dom7,1}, {8,Q::Maj7,1}, {7,Q::Dom7,1} } },
        { G_AfroJazz, 0, "iim7 | V7 | Imaj7",       { {2,Q::Min7,1}, {7,Q::Dom7,1}, {0,Q::Maj7,2} } },
        { G_AfroJazz, 0, "Imaj7 | vi7 | ii7 | V7",  { {0,Q::Maj7,1}, {9,Q::Min7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_AfroJazz, 0, "Imaj7 | IVmaj7",          { {0,Q::Maj7,2}, {5,Q::Maj7,2} } },
        { G_AfroJazz, 2, "I7 | IV7 | I7 | V7",      { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        // ---------------- Highlife
        { G_Highlife, 0, "I6 | IVmaj7 | V7 | IVmaj7", { {0,Q::Maj6,1}, {5,Q::Maj7,1}, {7,Q::Dom7,1}, {5,Q::Maj7,1} } },
        { G_Highlife, 0, "I | vi | IV | V7",        { {0,Q::Maj7,1}, {9,Q::Min7,1}, {5,Q::Maj7,1}, {7,Q::Dom7,1} } },
        { G_Highlife, 2, "I7 | IV7 | I7 | V7",      { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        { G_Highlife, 1, "i7 | iv7 | V7 | i7",      { {0,Q::Min7,1}, {5,Q::Min7,1}, {7,Q::Dom7,1}, {0,Q::Min7,1} } },
        { G_Highlife, 1, "i7 | bVII7 | bVImaj7 | V7", { {0,Q::Min7,1}, {10,Q::Dom7,1}, {8,Q::Maj7,1}, {7,Q::Dom7,1} } },
        // ---------------- Afro-Fusion
        { G_AfroFusion, 1, "i7 | bVImaj7 | bIIImaj7 | bVII7", { {0,Q::Min7,1}, {8,Q::Maj7,1}, {3,Q::Maj7,1}, {10,Q::Dom7,1} } },
        { G_AfroFusion, 1, "i7 | iv7 | bVImaj7 | bVII7",      { {0,Q::Min7,1}, {5,Q::Min7,1}, {8,Q::Maj7,1}, {10,Q::Dom7,1} } },
        { G_AfroFusion, 1, "i7 (2) | bVII | bVI",              { {0,Q::Min7,2}, {10,Q::Dom7,1}, {8,Q::Maj7,1} } },
        { G_AfroFusion, 0, "Imaj7 | V7 | vi7 | IVmaj7",       { {0,Q::Maj7,1}, {7,Q::Dom7,1}, {9,Q::Min7,1}, {5,Q::Maj7,1} } },
        { G_AfroFusion, 0, "vi7 | IVmaj7 | Imaj7 | V7",       { {9,Q::Min7,1}, {5,Q::Maj7,1}, {0,Q::Maj7,1}, {7,Q::Dom7,1} } },
        { G_AfroFusion, 2, "I7 | bVII7 | IV7 | I7",           { {0,Q::Dom7,1}, {10,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1} } },
        // ---------------- Swing Jazz
        { G_Swing, 0, "ii-V-I",                     { {2,Q::Min7,1}, {7,Q::Dom7,1}, {0,Q::Maj7,2} } },
        { G_Swing, 0, "I-vi-ii-V",                  { {0,Q::Maj7,1}, {9,Q::Min7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Swing, 0, "I | VI7 | ii | V7",          { {0,Q::Maj7,1}, {9,Q::Dom7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Swing, 2, "Jazz blues (12)",            { {0,Q::Dom7,4}, {5,Q::Dom7,2}, {0,Q::Dom7,2}, {7,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        { G_Swing, 0, "Jazz blues (12)",            { {0,Q::Dom7,4}, {5,Q::Dom7,2}, {0,Q::Dom7,2}, {7,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        { G_Swing, 1, "iim7b5 | V7 | i7 (2)",       { {2,Q::Min7b5,1}, {7,Q::Dom7,1}, {0,Q::Min7,2} } },
        { G_Swing, 1, "i7 | iv7 | iim7b5 | V7",     { {0,Q::Min7,1}, {5,Q::Min7,1}, {2,Q::Min7b5,1}, {7,Q::Dom7,1} } },
        { G_Swing, 1, "Minor blues (12)",           { {0,Q::Min7,4}, {5,Q::Min7,2}, {0,Q::Min7,2}, {8,Q::Dom7,1}, {7,Q::Dom7,1}, {0,Q::Min7,1}, {7,Q::Dom7,1} } },
        // ---------------- Ballad
        { G_Ballad, 0, "Imaj7 | iii7 | IVmaj7 | V7", { {0,Q::Maj7,1}, {4,Q::Min7,1}, {5,Q::Maj7,1}, {7,Q::Dom7,1} } },
        { G_Ballad, 0, "Imaj7 (2) | IVmaj7 (2)",     { {0,Q::Maj7,2}, {5,Q::Maj7,2} } },
        { G_Ballad, 0, "I-vi-ii-V",                  { {0,Q::Maj7,1}, {9,Q::Min7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Ballad, 2, "I7 | IV7",                   { {0,Q::Dom7,2}, {5,Q::Dom7,2} } },
        { G_Ballad, 1, "i7 | bVImaj7 | iv7 | V7",    { {0,Q::Min7,1}, {8,Q::Maj7,1}, {5,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Ballad, 1, "i7 (2) | iv7 | V7",          { {0,Q::Min7,2}, {5,Q::Min7,1}, {7,Q::Dom7,1} } },
        // ---------------- Amapiano
        { G_Amapiano, 1, "i9 | iv9 | bVImaj9 | bVII9",   { {0,Q::Min9,1}, {5,Q::Min9,1}, {8,Q::Maj9,1}, {10,Q::Dom9,1} } },
        { G_Amapiano, 1, "i9 (2) | bIIImaj7 | bVII7",    { {0,Q::Min9,2}, {3,Q::Maj7,1}, {10,Q::Dom7,1} } },
        { G_Amapiano, 1, "i9 | bVImaj7 | iv9 | V7alt",   { {0,Q::Min9,1}, {8,Q::Maj7,1}, {5,Q::Min9,1}, {7,Q::Dom7alt,1} } },
        { G_Amapiano, 0, "Imaj9 | vi9 | IVmaj9 | V9",    { {0,Q::Maj9,1}, {9,Q::Min9,1}, {5,Q::Maj9,1}, {7,Q::Dom9,1} } },
        { G_Amapiano, 0, "ii9 | V9 | Imaj9 (2)",         { {2,Q::Min9,1}, {7,Q::Dom9,1}, {0,Q::Maj9,2} } },
        { G_Amapiano, 2, "I9 | IV9",                     { {0,Q::Dom9,2}, {5,Q::Dom9,2} } },
        // ---------------- Juju
        { G_Juju, 0, "I | IV | I | V7",                  { {0,Q::Maj,1}, {5,Q::Maj,1}, {0,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Juju, 0, "I (2) | IV | V7",                  { {0,Q::Maj,2}, {5,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Juju, 1, "i7 | IV7 | i7 | V7",               { {0,Q::Min7,1}, {5,Q::Dom7,1}, {0,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Juju, 2, "I7 (2) | IV7 | V7",                { {0,Q::Dom7,2}, {5,Q::Dom7,1}, {7,Q::Dom7,1} } },
        // ---------------- Soukous
        { G_Soukous, 0, "I | IV | V7 | IV",              { {0,Q::Maj,1}, {5,Q::Maj,1}, {7,Q::Dom7,1}, {5,Q::Maj,1} } },
        { G_Soukous, 0, "I | vi | IV | V7",              { {0,Q::Maj,1}, {9,Q::Min,1}, {5,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Soukous, 1, "i | bVII | bVI | V7",           { {0,Q::Min,1}, {10,Q::Maj,1}, {8,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Soukous, 1, "i7 (2) | iv7 | V7",             { {0,Q::Min7,2}, {5,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Soukous, 2, "I7 | IV7 | I7 | V7",            { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        // ---------------- Ethio-Jazz
        { G_EthioJazz, 1, "i7 vamp",                     { {0,Q::Min7,4} } },
        { G_EthioJazz, 1, "i7 (2) | bIImaj7 (2)",        { {0,Q::Min7,2}, {1,Q::Maj7,2} } },
        { G_EthioJazz, 1, "i7 (2) | bVII7 (2)",          { {0,Q::Min7,2}, {10,Q::Dom7,2} } },
        { G_EthioJazz, 0, "Imaj7 (2) | IVmaj7 (2)",      { {0,Q::Maj7,2}, {5,Q::Maj7,2} } },
        { G_EthioJazz, 2, "I7 (2) | bVII7 (2)",          { {0,Q::Dom7,2}, {10,Q::Dom7,2} } },
        // ---------------- Township Jive
        { G_Township, 0, "I | IV | I | V7",              { {0,Q::Maj,1}, {5,Q::Maj,1}, {0,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Township, 0, "I | vi | ii | V7",             { {0,Q::Maj,1}, {9,Q::Min7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Township, 1, "i | iv | i | V7",              { {0,Q::Min,1}, {5,Q::Min,1}, {0,Q::Min,1}, {7,Q::Dom7,1} } },
        { G_Township, 2, "I7 | IV7 | I7 | V7",           { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        // ---------------- Gospel / Soul
        { G_Gospel, 0, "Imaj7 | vi7 | IVmaj7 | V7",      { {0,Q::Maj7,1}, {9,Q::Min7,1}, {5,Q::Maj7,1}, {7,Q::Dom7,1} } },
        { G_Gospel, 0, "IV | iii7 | ii7 | I",            { {5,Q::Maj,1}, {4,Q::Min7,1}, {2,Q::Min7,1}, {0,Q::Maj,1} } },
        { G_Gospel, 0, "I | III7 | IV | V7",             { {0,Q::Maj,1}, {4,Q::Dom7,1}, {5,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Gospel, 1, "i7 | iv7 | bVImaj7 | V7",        { {0,Q::Min7,1}, {5,Q::Min7,1}, {8,Q::Maj7,1}, {7,Q::Dom7,1} } },
        { G_Gospel, 2, "I7 | IV7 | I7 | V7",             { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        // ---------------- Pop
        { G_Pop, 0, "I | V | vi | IV",                   { {0,Q::Maj,1}, {7,Q::Maj,1}, {9,Q::Min,1}, {5,Q::Maj,1} } },
        { G_Pop, 0, "vi | IV | I | V",                   { {9,Q::Min,1}, {5,Q::Maj,1}, {0,Q::Maj,1}, {7,Q::Maj,1} } },
        { G_Pop, 0, "I | vi | IV | V",                   { {0,Q::Maj,1}, {9,Q::Min,1}, {5,Q::Maj,1}, {7,Q::Maj,1} } },
        { G_Pop, 1, "i | bVI | bIII | bVII",             { {0,Q::Min,1}, {8,Q::Maj,1}, {3,Q::Maj,1}, {10,Q::Maj,1} } },
        { G_Pop, 1, "i | iv | bVI | V7",                 { {0,Q::Min,1}, {5,Q::Min,1}, {8,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Pop, 2, "I7 | bVII | IV | I7",               { {0,Q::Dom7,1}, {10,Q::Maj,1}, {5,Q::Maj,1}, {0,Q::Dom7,1} } },
        // ---------------- Rock
        { G_Rock, 0, "I | IV | V | IV",                  { {0,Q::Maj,1}, {5,Q::Maj,1}, {7,Q::Maj,1}, {5,Q::Maj,1} } },
        { G_Rock, 0, "I | bVII | IV | I",                { {0,Q::Maj,1}, {10,Q::Maj,1}, {5,Q::Maj,1}, {0,Q::Maj,1} } },
        { G_Rock, 1, "i | bVII | bVI | V",               { {0,Q::Min,1}, {10,Q::Maj,1}, {8,Q::Maj,1}, {7,Q::Maj,1} } },
        { G_Rock, 1, "i | bVI | bVII | i",               { {0,Q::Min,1}, {8,Q::Maj,1}, {10,Q::Maj,1}, {0,Q::Min,1} } },
        { G_Rock, 2, "Blues (12)",                       { {0,Q::Dom7,4}, {5,Q::Dom7,2}, {0,Q::Dom7,2}, {7,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        { G_Rock, 2, "I7 (2) | IV7 (2)",                 { {0,Q::Dom7,2}, {5,Q::Dom7,2} } },
        // ---------------- Bossa Nova
        { G_Bossa, 0, "Imaj7 | VI7 | ii7 | V7",          { {0,Q::Maj7,1}, {9,Q::Dom7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Bossa, 0, "ii7 | V7 | Imaj7 (2)",            { {2,Q::Min7,1}, {7,Q::Dom7,1}, {0,Q::Maj7,2} } },
        { G_Bossa, 0, "Imaj7 | ii7 | iii7 | IVmaj7",     { {0,Q::Maj7,1}, {2,Q::Min7,1}, {4,Q::Min7,1}, {5,Q::Maj7,1} } },
        { G_Bossa, 1, "iim7b5 | V7alt | i7 (2)",         { {2,Q::Min7b5,1}, {7,Q::Dom7alt,1}, {0,Q::Min7,2} } },
        { G_Bossa, 1, "i7 | iv7 | iim7b5 | V7",          { {0,Q::Min7,1}, {5,Q::Min7,1}, {2,Q::Min7b5,1}, {7,Q::Dom7,1} } },
        { G_Bossa, 2, "I7 | IV7 | ii7 | V7",             { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        // ---------------- Samba
        { G_Samba, 0, "Imaj7 | VI7 | ii7 | V7",          { {0,Q::Maj7,1}, {9,Q::Dom7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Samba, 0, "I6 | V7 | I6 | IVmaj7",           { {0,Q::Maj6,1}, {7,Q::Dom7,1}, {0,Q::Maj6,1}, {5,Q::Maj7,1} } },
        { G_Samba, 1, "i7 | iim7b5 | V7 | i7",           { {0,Q::Min7,1}, {2,Q::Min7b5,1}, {7,Q::Dom7,1}, {0,Q::Min7,1} } },
        { G_Samba, 1, "i7 | V7 | i7 | V7",               { {0,Q::Min7,1}, {7,Q::Dom7,1}, {0,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Samba, 2, "I7 | IV7 | I7 | V7",              { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        // ---------------- Flamenco / Spanish
        { G_Flamenco, 1, "i | bVII | bVI | V7 (Andalusian)", { {0,Q::Min,1}, {10,Q::Maj,1}, {8,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Flamenco, 1, "i | bVII | bVI | V7 (2 bars each)", { {0,Q::Min,2}, {10,Q::Maj,2}, {8,Q::Maj,2}, {7,Q::Dom7,2} } },
        { G_Flamenco, 2, "I | bII | I (Phrygian)",       { {0,Q::Maj,2}, {1,Q::Maj,1}, {0,Q::Maj,1} } },
        { G_Flamenco, 2, "I7 | bIImaj7 | bIII | iv",     { {0,Q::Dom7,1}, {1,Q::Maj7,1}, {3,Q::Maj,1}, {5,Q::Min,1} } },
        { G_Flamenco, 0, "I | V7 | I | IV",              { {0,Q::Maj,1}, {7,Q::Dom7,1}, {0,Q::Maj,1}, {5,Q::Maj,1} } },
        // ---------------- Salsa
        { G_Salsa, 1, "i | iv | V7 | i",                 { {0,Q::Min,1}, {5,Q::Min,1}, {7,Q::Dom7,1}, {0,Q::Min,1} } },
        { G_Salsa, 1, "i | bVII | bVI | V7",             { {0,Q::Min,1}, {10,Q::Maj,1}, {8,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Salsa, 1, "i7 (2) | iv7 | V7", { {0,Q::Min7,2}, {5,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Salsa, 0, "I | IV | V7 | IV",                { {0,Q::Maj,1}, {5,Q::Maj,1}, {7,Q::Dom7,1}, {5,Q::Maj,1} } },
        { G_Salsa, 0, "I | vi | ii | V7",                { {0,Q::Maj,1}, {9,Q::Min,1}, {2,Q::Min,1}, {7,Q::Dom7,1} } },
        { G_Salsa, 0, "I (2) | IV | V7",                 { {0,Q::Maj,2}, {5,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Salsa, 2, "I7 | IV7 | I7 | V7",              { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        // ---------------- Afro-Cuban
        { G_AfroCuban, 0, "I | IV | I | V7 (son montuno)", { {0,Q::Maj,1}, {5,Q::Maj,1}, {0,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_AfroCuban, 0, "I (2) | V7 (2) (rumba)",      { {0,Q::Maj,2}, {7,Q::Dom7,2} } },
        { G_AfroCuban, 0, "Imaj7 | VI7 | iim7 | V7 (bolero)", { {0,Q::Maj7,1}, {9,Q::Dom7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_AfroCuban, 1, "i | iv | V7 | i",             { {0,Q::Min,1}, {5,Q::Min,1}, {7,Q::Dom7,1}, {0,Q::Min,1} } },
        { G_AfroCuban, 1, "i7 | V7 (mambo vamp)",        { {0,Q::Min7,2}, {7,Q::Dom7,2} } },
        { G_AfroCuban, 1, "i | bVII | bVI | V7",         { {0,Q::Min,1}, {10,Q::Maj,1}, {8,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_AfroCuban, 2, "I7 | IV7 | I7 | V7",          { {0,Q::Dom7,1}, {5,Q::Dom7,1}, {0,Q::Dom7,1}, {7,Q::Dom7,1} } },
        // ---------------- Afro-Brazilian
        { G_AfroBrazilian, 2, "I7 | bVII | IV | I7 (baiao)", { {0,Q::Dom7,1}, {10,Q::Maj,1}, {5,Q::Maj,1}, {0,Q::Dom7,1} } },
        { G_AfroBrazilian, 2, "I7 (2) | IV7 (2)",        { {0,Q::Dom7,2}, {5,Q::Dom7,2} } },
        { G_AfroBrazilian, 0, "I | IV | V7 | I",         { {0,Q::Maj,1}, {5,Q::Maj,1}, {7,Q::Dom7,1}, {0,Q::Maj,1} } },
        { G_AfroBrazilian, 0, "I | IV | V | IV (axe)",   { {0,Q::Maj,1}, {5,Q::Maj,1}, {7,Q::Maj,1}, {5,Q::Maj,1} } },
        { G_AfroBrazilian, 1, "i | bVII | bVI | bVII (maracatu)", { {0,Q::Min,1}, {10,Q::Maj,1}, {8,Q::Maj,1}, {10,Q::Maj,1} } },
        { G_AfroBrazilian, 1, "i7 (2) | IV7 (2) (dorian)", { {0,Q::Min7,2}, {5,Q::Dom7,2} } },
        { G_AfroBrazilian, 1, "i | iv | V7 | i",         { {0,Q::Min,1}, {5,Q::Min,1}, {7,Q::Dom7,1}, {0,Q::Min,1} } },
        // ---------------- Caribbean
        { G_Caribbean, 1, "i (2) | V7 (2) (cumbia)",     { {0,Q::Min,2}, {7,Q::Dom7,2} } },
        { G_Caribbean, 1, "i | VI | III | VII",          { {0,Q::Min,1}, {8,Q::Maj,1}, {3,Q::Maj,1}, {10,Q::Maj,1} } },
        { G_Caribbean, 1, "i7 | iv7 | bVII | bIIImaj7 (zouk)", { {0,Q::Min7,1}, {5,Q::Min7,1}, {10,Q::Maj,1}, {3,Q::Maj7,1} } },
        { G_Caribbean, 0, "I | V7 | I | V7 (merengue)",  { {0,Q::Maj,1}, {7,Q::Dom7,1}, {0,Q::Maj,1}, {7,Q::Dom7,1} } },
        { G_Caribbean, 0, "I | IV | V7 | I",             { {0,Q::Maj,1}, {5,Q::Maj,1}, {7,Q::Dom7,1}, {0,Q::Maj,1} } },
        { G_Caribbean, 0, "vi | IV | I | V (bachata)",   { {9,Q::Min,1}, {5,Q::Maj,1}, {0,Q::Maj,1}, {7,Q::Maj,1} } },
        { G_Caribbean, 2, "I7 | IV7",                    { {0,Q::Dom7,2}, {5,Q::Dom7,2} } },
        // ---------------- Reggae & Dub
        { G_Reggae, 0, "I | IV | I | V",                 { {0,Q::Maj,1}, {5,Q::Maj,1}, {0,Q::Maj,1}, {7,Q::Maj,1} } },
        { G_Reggae, 0, "I | V | vi | IV",                { {0,Q::Maj,1}, {7,Q::Maj,1}, {9,Q::Min,1}, {5,Q::Maj,1} } },
        { G_Reggae, 0, "I (2) | IV (2)",                 { {0,Q::Maj,2}, {5,Q::Maj,2} } },
        { G_Reggae, 1, "i | iv | i | V",                 { {0,Q::Min,1}, {5,Q::Min,1}, {0,Q::Min,1}, {7,Q::Min,1} } },
        { G_Reggae, 1, "i | bVII | bVI | bVII",          { {0,Q::Min,1}, {10,Q::Maj,1}, {8,Q::Maj,1}, {10,Q::Maj,1} } },
        { G_Reggae, 1, "i7 vamp (dub)",                  { {0,Q::Min7,4} } },
        { G_Reggae, 2, "I7 | IV7",                       { {0,Q::Dom7,2}, {5,Q::Dom7,2} } },
        // ---------------- Funk & Disco
        { G_Funk, 1, "i7 (2) | IV7 (2)",                 { {0,Q::Min7,2}, {5,Q::Dom7,2} } },
        { G_Funk, 1, "i7 | iv7 | bVII7 | bIIImaj7",      { {0,Q::Min7,1}, {5,Q::Min7,1}, {10,Q::Dom7,1}, {3,Q::Maj7,1} } },
        { G_Funk, 0, "vi7 | ii7 | V7 | Imaj7 (disco)",   { {9,Q::Min7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1}, {0,Q::Maj7,1} } },
        { G_Funk, 0, "Imaj7 | vi7 | ii7 | V7",           { {0,Q::Maj7,1}, {9,Q::Min7,1}, {2,Q::Min7,1}, {7,Q::Dom7,1} } },
        { G_Funk, 2, "I7 (2) | IV7 (1) | V7 (1)",        { {0,Q::Dom9,2}, {5,Q::Dom9,1}, {7,Q::Dom9,1} } },
        { G_Funk, 2, "I9 vamp",                          { {0,Q::Dom9,4} } },
        // ---------------- Hip-Hop & Trap
        { G_HipHop, 1, "i | bVI | bIII | bVII",          { {0,Q::Min,1}, {8,Q::Maj,1}, {3,Q::Maj,1}, {10,Q::Maj,1} } },
        { G_HipHop, 1, "i (2) | bVI (1) | V (1)",        { {0,Q::Min,2}, {8,Q::Maj,1}, {7,Q::Maj,1} } },
        { G_HipHop, 1, "i7 (2) | bVImaj7 (2)",           { {0,Q::Min7,2}, {8,Q::Maj7,2} } },
        { G_HipHop, 1, "i | iv | bVI | V",               { {0,Q::Min,1}, {5,Q::Min,1}, {8,Q::Maj,1}, {7,Q::Maj,1} } },
        { G_HipHop, 0, "Imaj7 (2) | vi7 (2)",            { {0,Q::Maj7,2}, {9,Q::Min7,2} } },
        { G_HipHop, 2, "I9 vamp",                        { {0,Q::Dom9,4} } },
        // ---------------- Afro-House
        { G_AfroHouse, 1, "i7 | bVI | bVII | i7",        { {0,Q::Min7,1}, {8,Q::Maj,1}, {10,Q::Maj,1}, {0,Q::Min7,1} } },
        { G_AfroHouse, 1, "i9 (2) | iv9 (2)",            { {0,Q::Min9,2}, {5,Q::Min9,2} } },
        { G_AfroHouse, 1, "i7 vamp",                     { {0,Q::Min7,4} } },
        { G_AfroHouse, 0, "vi | IV | I | V",             { {9,Q::Min,1}, {5,Q::Maj,1}, {0,Q::Maj,1}, {7,Q::Maj,1} } },
        { G_AfroHouse, 0, "Imaj9 | vi9 | IVmaj9 | V9",   { {0,Q::Maj9,1}, {9,Q::Min9,1}, {5,Q::Maj9,1}, {7,Q::Dom9,1} } },
        { G_AfroHouse, 2, "I9 | IV9",                    { {0,Q::Dom9,2}, {5,Q::Dom9,2} } },
    };
    return p;
}

//==============================================================================
struct Meter
{
    int num = 4, den = 4;
    double barBeats = 4.0;       // in quarter notes
    int stepsPerBar = 16;        // 16th-note steps
    bool compound = false;
    bool simpleFour = false;     // exactly 4/4 (rhythm cell banks apply)
    std::vector<int> groups;     // beat group lengths in steps
    std::vector<double> weight;  // metric weight per step
};

inline Meter makeMeter (int num, int den)
{
    Meter m; m.num = num; m.den = den;
    m.barBeats = num * 4.0 / den;
    m.stepsPerBar = (int) std::lround (m.barBeats * 4.0);
    m.simpleFour = (num == 4 && den == 4);
    if (den == 8)
    {
        if (num % 3 == 0 && num >= 6) { m.compound = true; for (int k = 0; k < num / 3; ++k) m.groups.push_back (6); }
        else if (num == 5) m.groups = { 6, 4 };
        else if (num == 7) m.groups = { 4, 4, 6 };
        else for (int k = 0; k < num; ++k) m.groups.push_back (2);
    }
    else if (den == 2) { for (int k = 0; k < num; ++k) m.groups.push_back (8); }
    else
    {
        if (num == 5) m.groups = { 12, 8 };
        else if (num == 7) m.groups = { 16, 12 };
        else for (int k = 0; k < num; ++k) m.groups.push_back (4);
    }
    m.weight.assign ((size_t) m.stepsPerBar, 0.15);
    int s = 0, gi = 0;
    for (int g : m.groups)
    {
        for (int off = 0; off < g && s < m.stepsPerBar; ++off, ++s)
        {
            double w;
            if (off == 0)
            {
                if (gi == 0) w = 1.0;
                else if (m.simpleFour && gi == 2) w = 0.8;
                else w = 0.62;
            }
            else if (m.compound) w = (off % 2 == 0) ? 0.35 : 0.15;
            else                 w = (off % 4 == 0) ? 0.55 : (off % 2 == 0 ? 0.35 : 0.15);
            m.weight[(size_t) s] = w;
        }
        ++gi;
    }
    return m;
}


//==============================================================================
struct ParsedChord { int rootPc = 0; Quality q = Quality::Min7; double bars = 1.0; std::string text; };

/** Parse text such as "Am7 | D7 | Gmaj7 | Cmaj7:2" (one chord per bar, ":n" = n bars). Returns true if at least one chord was read. */
inline bool parseChordText (const std::string& text, std::vector<ParsedChord>& out, std::string& err)
{
    out.clear(); err.clear();
    std::vector<std::string> toks; std::string cur;
    for (char ch : text)
    {
        if (ch == ' ' || ch == '|' || ch == ',' || ch == '\t' || ch == '\n' || ch == '\r' || ch == ';') { if (! cur.empty()) { toks.push_back (cur); cur.clear(); } }
        else cur += ch;
    }
    if (! cur.empty()) toks.push_back (cur);

    for (std::string t : toks)
    {
        const std::string original = t;
        double bars = 1.0;
        const auto colon = t.find (':');
        if (colon != std::string::npos) { bars = std::atof (t.substr (colon + 1).c_str()); t = t.substr (0, colon); if (bars <= 0.0 || bars > 16.0) bars = 1.0; }
        const auto slash = t.find ('/');
        if (slash != std::string::npos) t = t.substr (0, slash);
        if (t.empty()) continue;
        char L = t[0]; if (L >= 'a' && L <= 'g') L = (char) (L - 'a' + 'A');
        if (L < 'A' || L > 'G') { if (err.empty()) err = "Could not read chord \"" + original + "\""; continue; }
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        int root = base[L - 'A'];
        size_t pos = 1;
        if (pos < t.size() && (t[pos] == '#')) { ++root; ++pos; }
        else if (pos < t.size() && t[pos] == 'b' && (pos + 1 >= t.size() || ! (t[pos + 1] == 'a'))) { --root; ++pos; }
        root = ((root % 12) + 12) % 12;
        const std::string sfx = t.substr (pos);
        std::string ls = sfx; for (auto& c : ls) c = (char) std::tolower ((unsigned char) c);
        auto starts = [&] (const char* p) { return ls.rfind (p, 0) == 0; };
        Quality q = Quality::Maj; bool ok = true;
        auto has = [&] (const char* x) { return ls.find (x) != std::string::npos; };
        if (sfx.empty()) q = Quality::Maj;
        else if (starts ("m7b5") || starts ("min7b5") || starts ("-7b5") || starts ("halfdim") || starts ("\xc3\xb8")) q = Quality::Min7b5;
        else if (starts ("dim") || sfx[0] == 'o') q = Quality::Dim7;
        else if (starts ("mmaj") || starts ("minmaj") || starts ("m(maj") || sfx.rfind ("mM", 0) == 0) q = Quality::MinMaj7;
        else if (sfx[0] == 'M' || starts ("maj") || starts ("ma"))
        {
            if (has ("13") || has ("9") || has ("11")) q = Quality::Maj9;
            else if (has ("7")) q = Quality::Maj7;
            else if (has ("6")) q = Quality::Maj6;
            else q = (ls == "maj" || ls == "ma" || ls == "major") ? Quality::Maj : Quality::Maj7;
        }
        else if (starts ("min") || ls[0] == 'm' || ls[0] == '-')
        {
            if (has ("11") || has ("13")) q = Quality::Min11;
            else if (has ("9")) q = Quality::Min9;
            else if (has ("7")) q = Quality::Min7;
            else if (has ("6")) q = Quality::Min6;
            else q = Quality::Min;
        }
        else if (starts ("sus2")) q = Quality::Sus2;
        else if (starts ("sus")) q = Quality::Sus4;
        else if (ls[0] == '6') q = Quality::Maj6;
        else if (starts ("alt") || starts ("7alt") || starts ("7b9") || starts ("7#9") || starts ("7b5") || starts ("7#11") || starts ("7b13")) q = Quality::Dom7alt;
        else if (starts ("aug") || ls[0] == '+' || starts ("7#5") || starts ("7+5")) q = Quality::Aug7;
        else if (starts ("13")) q = Quality::Dom13;
        else if (starts ("9")) q = Quality::Dom9;
        else if (ls[0] == '7' || starts ("11") || ls[0] == '5') q = Quality::Dom7;
        else if (starts ("add") || ls[0] == '2' || ls[0] == '4') q = Quality::Maj;
        else ok = false;
        if (! ok) { if (err.empty()) err = "Could not read chord \"" + original + "\""; continue; }
        ParsedChord pc; pc.rootPc = root; pc.q = q; pc.bars = bars; pc.text = original;
        out.push_back (pc);
    }
    if (out.empty() && err.empty()) err = "No chords found";
    return ! out.empty();
}

//==============================================================================
inline std::string chordName (int rootPc, Quality q)
{
    return std::string (kNoteNames[((rootPc % 12) + 12) % 12]) + qualitySuffix (q);
}

inline int nearestPitchWithPc (int pitch, int pc)
{
    int best = pitch, bd = 99;
    for (int d = -6; d <= 6; ++d)
        if ((((pitch + d) % 12) + 12) % 12 == pc && std::abs (d) < bd) { bd = std::abs (d); best = pitch + d; }
    return best;
}

inline std::string noteName (int midi)
{
    return std::string (kNoteNames[((midi % 12) + 12) % 12]) + std::to_string (midi / 12 - 1);   // C4 = 60 (middle C), so EZbass key-switch names (B-1, G#0) read as Toontrack writes them
}

} // namespace enochsolo
