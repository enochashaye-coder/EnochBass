#pragma once
// Chord detection (JUCE-free): one chord per bar from the notes of a MIDI clip or from the audio of a chord / backing loop, plus the key.
#include "Theory.h"
#include "Reference.h"
#include <array>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

namespace enochsolo
{
struct ChordNote { double beat = 0, len = 1; int pitch = 60; float vel = 0.7f; };

struct DetectResult
{
    bool ok = false;
    std::string error;
    int keyRoot = 0; bool minor = false; double keyConfidence = 0;
    int bars = 0;
    std::vector<BarChord> chords;         // one per bar, set = true
    std::vector<int> rootPc;              // absolute roots
    std::string text;                     // e.g. "Am7 | D7 | Gmaj7 | Cmaj7"
};

namespace chorddetect
{
struct Tpl { Quality q; std::vector<std::pair<int, float>> tones; float prior; };

inline const std::vector<Tpl>& templates()
{
    static const std::vector<Tpl> t = {
        { Quality::Maj,    { { 0, 1.3f }, { 4, 1.f }, { 7, 0.8f } }, 0.f },
        { Quality::Min,    { { 0, 1.3f }, { 3, 1.f }, { 7, 0.8f } }, 0.f },
        { Quality::Dom7,   { { 0, 1.3f }, { 4, 1.f }, { 7, 0.7f }, { 10, 0.95f } }, -0.01f },
        { Quality::Maj7,   { { 0, 1.3f }, { 4, 1.f }, { 7, 0.7f }, { 11, 0.95f } }, -0.01f },
        { Quality::Min7,   { { 0, 1.3f }, { 3, 1.f }, { 7, 0.7f }, { 10, 0.95f } }, -0.01f },
        { Quality::Min7b5, { { 0, 1.3f }, { 3, 1.f }, { 6, 1.f }, { 10, 0.95f } }, -0.02f },
        { Quality::Dim7,   { { 0, 1.3f }, { 3, 1.f }, { 6, 1.f }, { 9, 0.95f } }, -0.03f },
        { Quality::Sus4,   { { 0, 1.3f }, { 5, 1.f }, { 7, 0.8f }, { 10, 0.6f } }, -0.03f },
        { Quality::Maj6,   { { 0, 1.3f }, { 4, 1.f }, { 7, 0.7f }, { 9, 0.95f } }, -0.025f },
        { Quality::Min6,   { { 0, 1.3f }, { 3, 1.f }, { 7, 0.7f }, { 9, 0.95f } }, -0.025f },
        { Quality::Dom9,   { { 0, 1.3f }, { 4, 1.f }, { 7, 0.6f }, { 10, 0.95f }, { 2, 0.8f } }, -0.04f },
        { Quality::Min9,   { { 0, 1.3f }, { 3, 1.f }, { 7, 0.6f }, { 10, 0.95f }, { 2, 0.8f } }, -0.04f },
        { Quality::Maj9,   { { 0, 1.3f }, { 4, 1.f }, { 7, 0.6f }, { 11, 0.95f }, { 2, 0.8f } }, -0.04f },
    };
    return t;
}

inline void bestChord (const std::array<double, 12>& chroma, const std::array<double, 12>& bass, int& rootOut, Quality& qOut, double& scoreOut)
{
    double norm = 0, bmax = 1e-9;
    for (int i = 0; i < 12; ++i) { norm += chroma[(size_t) i] * chroma[(size_t) i]; bmax = std::max (bmax, bass[(size_t) i]); }
    norm = std::sqrt (std::max (1e-12, norm));
    double best = -1e9; int br = 0; Quality bq = Quality::Maj;
    for (int r = 0; r < 12; ++r)
        for (auto& t : templates())
        {
            double dot = 0, tn = 0;
            for (auto& tone : t.tones) { dot += chroma[(size_t) ((r + tone.first) % 12)] * tone.second; tn += tone.second * tone.second; }
            double sc = dot / (norm * std::sqrt (tn)) + t.prior + 0.18 * bass[(size_t) r] / bmax;
            if (sc > best) { best = sc; br = r; bq = t.q; }
        }
    rootOut = br; qOut = bq; scoreOut = best;
}

/** Krumhansl-Schmuckler key finding on the summed chroma */
inline void findKey (const std::array<double, 12>& total, int& root, bool& minor, double& conf)
{
    static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    double best = -2, second = -2; int br = 0; bool bm = false;
    double mean = 0; for (double v : total) mean += v / 12.0;
    for (int r = 0; r < 12; ++r)
        for (int m = 0; m < 2; ++m)
        {
            const double* prof = m ? mnr : maj;
            double pm = 0; for (int i = 0; i < 12; ++i) pm += prof[i] / 12.0;
            double num = 0, da = 0, db = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double a = total[(size_t) ((r + i) % 12)] - mean, b = prof[i] - pm;
                num += a * b; da += a * a; db += b * b;
            }
            const double c = num / std::sqrt (std::max (1e-12, da * db));
            if (c > best) { second = best; best = c; br = r; bm = (m == 1); } else if (c > second) second = c;
        }
    root = br; minor = bm; conf = best - second;
}

inline DetectResult finish (const std::vector<std::array<double, 12>>& barChroma, const std::vector<std::array<double, 12>>& barBass, const std::vector<char>& barUsed)
{
    DetectResult res;
    const int n = (int) barChroma.size();
    std::array<double, 12> total {}; double energy = 0;
    for (int b = 0; b < n; ++b) for (int i = 0; i < 12; ++i) { total[(size_t) i] += barChroma[(size_t) b][(size_t) i]; energy += barChroma[(size_t) b][(size_t) i]; }
    if (n < 1 || energy < 1e-9) { res.error = "I could not find any notes or tonal sound in that file."; return res; }
    findKey (total, res.keyRoot, res.minor, res.keyConfidence);
    // trailing empty bars are not part of the loop
    int last = n - 1; while (last > 0 && ! barUsed[(size_t) last]) --last;
    for (int b = 0; b <= last; ++b)
    {
        BarChord bc; bc.set = true;
        int r = 0; Quality q = Quality::Maj; double sc = 0;
        if (barUsed[(size_t) b]) bestChord (barChroma[(size_t) b], barBass[(size_t) b], r, q, sc);
        else if (b > 0) { r = (res.keyRoot + res.chords.back().degree) % 12; q = res.chords.back().q; }      // an empty bar repeats the one before
        else { r = res.keyRoot; q = res.minor ? Quality::Min7 : Quality::Maj7; }
        bc.degree = ((r - res.keyRoot) % 12 + 12) % 12; bc.q = q;
        res.chords.push_back (bc); res.rootPc.push_back (r);
        if (b) res.text += " | ";
        res.text += chordName (r, q);
    }
    res.bars = (int) res.chords.size();
    res.ok = true;
    return res;
}
} // namespace chorddetect

/** notes in beats (quarter notes from the start of the clip); bar 1 starts at beat 0 */
inline DetectResult detectChordsFromNotes (const std::vector<ChordNote>& notes, double barBeats, int maxBars = 32)
{
    DetectResult bad;
    if (notes.size() < 3) { bad.error = "That clip has too few notes to find chords."; return bad; }
    barBeats = std::max (0.5, barBeats);
    double endB = 0; for (auto& n : notes) endB = std::max (endB, n.beat + std::max (0.05, n.len));
    const int nb = std::clamp ((int) std::ceil (endB / barBeats - 0.02), 1, maxBars);
    std::vector<std::array<double, 12>> chroma ((size_t) nb), bass ((size_t) nb);
    std::vector<char> used ((size_t) nb, 0);
    for (auto& a : chroma) a.fill (0.0);
    for (auto& a : bass) a.fill (0.0);
    for (auto& n : notes)
    {
        const double a0 = n.beat, a1 = n.beat + std::max (0.05, n.len);
        for (int b = (int) std::floor (a0 / barBeats); b <= (int) std::floor ((a1 - 1e-6) / barBeats) && b < nb; ++b)
        {
            if (b < 0) continue;
            const double ov = std::min (a1, (b + 1) * barBeats) - std::max (a0, b * barBeats);
            if (ov <= 0) continue;
            const double w = ov * (0.4 + 0.6 * n.vel) * (n.pitch < 55 ? 1.25 : 1.0);
            chroma[(size_t) b][(size_t) (n.pitch % 12)] += w;
            if (n.pitch < 60) bass[(size_t) b][(size_t) (n.pitch % 12)] += w;
            used[(size_t) b] = 1;
        }
    }
    return chorddetect::finish (chroma, bass, used);
}

/** mono audio, bar 1 = the start of the file, at the given tempo */
inline DetectResult detectChordsFromAudio (const std::vector<float>& mono, double sr, double bpm, double barBeats, int maxBars = 32)
{
    using refdetail::fft;
    DetectResult bad;
    if (mono.size() < (size_t) (sr * 1.0)) { bad.error = "That recording is too short."; return bad; }
    barBeats = std::max (0.5, barBeats);
    const double barSec = barBeats * 60.0 / std::max (20.0, bpm);
    // decimate to about 11 kHz by averaging
    const int dec = std::max (1, (int) std::lround (sr / 11025.0));
    const double fs = sr / dec;
    std::vector<float> x; x.reserve (mono.size() / (size_t) dec + 1);
    for (size_t i = 0; i + (size_t) dec <= mono.size(); i += (size_t) dec) { float a = 0; for (int k = 0; k < dec; ++k) a += mono[i + (size_t) k]; x.push_back (a / dec); }
    const int N = 8192, hop = 2048;
    if ((int) x.size() < N) { bad.error = "That recording is too short."; return bad; }
    const double totalSec = (double) x.size() / fs;
    const int nb = std::clamp ((int) std::ceil (totalSec / barSec - 0.05), 1, maxBars);
    std::vector<std::array<double, 12>> chroma ((size_t) nb), bass ((size_t) nb);
    std::vector<double> bw ((size_t) nb, 0.0);
    for (auto& a : chroma) a.fill (0.0);
    for (auto& a : bass) a.fill (0.0);
    std::vector<double> win ((size_t) N); for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5 - 0.5 * std::cos (6.283185307179586 * i / (N - 1));
    std::vector<std::complex<double>> buf ((size_t) N);
    // pitch class of every FFT bin
    const double binHz = fs / N;
    std::vector<int> pcOf ((size_t) N / 2, -1); std::vector<float> wOf ((size_t) N / 2, 0.f); std::vector<char> isBass ((size_t) N / 2, 0);
    for (int k = 1; k < N / 2; ++k)
    {
        const double f = k * binHz;
        if (f < 62.0 || f > 2100.0) continue;
        const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
        pcOf[(size_t) k] = ((int) std::lround (midi) % 12 + 12) % 12;
        wOf[(size_t) k] = (float) (1.0 / std::sqrt (std::max (1.0, f / 200.0)));      // gentle tilt: the low harmonics carry the chord
        isBass[(size_t) k] = f < 230.0;
    }
    for (int pos = 0; pos + N <= (int) x.size(); pos += hop)
    {
        const double tc = (pos + N * 0.5) / fs;
        const int b = std::min (nb - 1, (int) std::floor (tc / barSec));
        for (int i = 0; i < N; ++i) buf[(size_t) i] = x[(size_t) (pos + i)] * win[(size_t) i];
        fft (buf);
        std::array<double, 12> fr {}, fb {};
        for (int k = 1; k < N / 2; ++k)
        {
            if (pcOf[(size_t) k] < 0) continue;
            const double m = std::sqrt (std::abs (buf[(size_t) k])) * wOf[(size_t) k];      // compressed magnitude
            fr[(size_t) pcOf[(size_t) k]] += m;
            if (isBass[(size_t) k]) fb[(size_t) pcOf[(size_t) k]] += m;
        }
        double e = 0; for (double v : fr) e += v;
        if (e <= 0) continue;
        for (int i = 0; i < 12; ++i) { chroma[(size_t) b][(size_t) i] += fr[(size_t) i] / e; bass[(size_t) b][(size_t) i] += fb[(size_t) i] / e; }
        bw[(size_t) b] += 1.0;
    }
    std::vector<char> used ((size_t) nb, 0);
    for (int b = 0; b < nb; ++b) used[(size_t) b] = bw[(size_t) b] > 0.5 ? 1 : 0;
    return chorddetect::finish (chroma, bass, used);
}
} // namespace enochsolo
