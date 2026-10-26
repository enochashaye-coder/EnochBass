#pragma once
// Enoch Bass - audio to MIDI for an isolated bass recording (JUCE-free, header only).
//
// Pipeline: decimate to ~5.5 kHz -> YIN pitch track (36 Hz .. 400 Hz) + loudness envelope -> note starts from the envelope (refined to ~1 ms)
// -> pitch runs inside each note (hammer-ons / pull-offs / slides are told apart) -> tuning correction -> octave check on every note
// -> ghost (dead) notes from loud-but-unpitched attacks -> tempo / grid fit -> optional tidy (quantise) -> notes in beats.
// It is made for ONE bass line at a time (no chords, no drums). Mixed audio gives poor results.
#include "Styles.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace enochsolo
{
struct TranscribeOptions
{
    double bpm = 120.0;           // project tempo
    bool detectTempo = false;     // guess the tempo from the recording instead
    int tidy = 1;                 // 0 as played, 1 tidy (75 percent towards the 1/16 grid), 2 snap to 1/16, 3 snap to 1/8
    bool firstNoteIsBarOne = false;
    double maxSeconds = 900.0;
};

struct TNote
{
    double t0 = 0, t1 = 0;        // seconds in the recording
    double beat = 0, lenBeats = 0;// after tempo + tidy
    int pitch = 40, vel = 90, art = A_Sustain;
    float conf = 1.f;
};

struct TranscribeResult
{
    bool ok = false;
    std::string error, text;
    double bpm = 120.0, tuningCents = 0, gridFit = 0, lengthBeats = 0, quality = 0;
    int ghosts = 0, slides = 0, legatos = 0, octaveFixes = 0;
    bool tidyApplied = false;
    std::vector<TNote> notes;
};

namespace tdetail
{
inline double median (std::vector<double> v)
{
    if (v.empty()) return 0;
    std::nth_element (v.begin(), v.begin() + (long) (v.size() / 2), v.end());
    return v[v.size() / 2];
}
inline double percentile (std::vector<double> v, double p)
{
    if (v.empty()) return 0;
    std::sort (v.begin(), v.end());
    return v[(size_t) std::clamp ((int) std::lround (p * (double) (v.size() - 1)), 0, (int) v.size() - 1)];
}

/** windowed-sinc low-pass + decimation */
inline std::vector<float> decimate (const std::vector<float>& x, int D)
{
    if (D <= 1) return x;
    const int half = 6 * D;
    std::vector<double> h ((size_t) (2 * half + 1));
    const double fc = 0.42 / (double) D;                      // cycles per input sample
    double sum = 0;
    for (int i = -half; i <= half; ++i)
    {
        const double a = (i == 0) ? 2.0 * fc : std::sin (2.0 * M_PI * fc * i) / (M_PI * i);
        const double w = 0.5 + 0.5 * std::cos (M_PI * i / (half + 1));
        h[(size_t) (i + half)] = a * w; sum += a * w;
    }
    for (auto& v : h) v /= sum;
    std::vector<float> y;
    const long n = (long) x.size();
    y.reserve ((size_t) (n / D + 1));
    for (long c = 0; c < n; c += D)
    {
        double acc = 0;
        for (int i = -half; i <= half; ++i) { const long j = c + i; if (j >= 0 && j < n) acc += (double) x[(size_t) j] * h[(size_t) (i + half)]; }
        y.push_back ((float) acc);
    }
    return y;
}

struct Frames
{
    int W = 560, H = 32, minTau = 12, maxTau = 190;
    double fs = 5512.5;
    int n = 0;
    std::vector<double> t, db, midi, conf;
    std::vector<char> voiced;
    double timeOf (int k) const { return ((double) k * H + W * 0.5) / fs; }
};

/** YIN on one frame; returns f0 in Hz (0 = none) and confidence (1 - normalised difference) */
inline double yinFrame (const float* x, int W, int minTau, int maxTau, double fs, std::vector<double>& d, std::vector<double>& cm, double& conf)
{
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double s = 0;
        for (int j = 0; j < W; ++j) { const double e = (double) x[j] - (double) x[j + tau]; s += e * e; }
        d[(size_t) tau] = s;
    }
    cm[0] = 1.0; double run = 0;
    for (int tau = 1; tau <= maxTau; ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = (run > 1e-12) ? d[(size_t) tau] * tau / run : 1.0; }
    int best = -1; const double thr = 0.15;
    for (int tau = minTau; tau < maxTau; ++tau)
        if (cm[(size_t) tau] < thr && cm[(size_t) tau] <= cm[(size_t) (tau + 1)] && cm[(size_t) tau] <= cm[(size_t) (tau - 1)]) { best = tau; break; }
    if (best < 0)
    {
        double mn = 1e9;
        for (int tau = minTau; tau < maxTau; ++tau) if (cm[(size_t) tau] < mn) { mn = cm[(size_t) tau]; best = tau; }
    }
    if (best < 0) { conf = 0; return 0; }
    // sub-octave guard: when twice the lag is nearly as good, the lower note is the real fundamental
    if (2 * best < maxTau && cm[(size_t) (2 * best)] < 0.9 * cm[(size_t) best] && cm[(size_t) (2 * best)] < 0.12) best = 2 * best;
    conf = std::clamp (1.0 - cm[(size_t) best], 0.0, 1.0);
    double tauF = best;
    if (best > 1 && best < maxTau)
    {
        const double a = cm[(size_t) (best - 1)], b = cm[(size_t) best], c = cm[(size_t) (best + 1)];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-12) tauF = best + 0.5 * (a - c) / den;
    }
    return fs / tauF;
}

/** magnitude of a Hann-windowed tone at f Hz */
inline double toneMag (const std::vector<float>& x, long i0, long i1, double fs, double f)
{
    if (i1 - i0 < 64) return 0;
    double re = 0, im = 0; const long n = i1 - i0;
    for (long i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * (double) i / (double) (n - 1));
        const double ph = 2.0 * M_PI * f * (double) i / fs;
        const double v = (double) x[(size_t) (i0 + i)] * w;
        re += v * std::cos (ph); im -= v * std::sin (ph);
    }
    return std::sqrt (re * re + im * im) / (double) n;
}
inline double bestMag (const std::vector<float>& x, long i0, long i1, double fs, double f)
{
    double m = 0;
    for (double r : { 0.97, 0.985, 1.0, 1.015, 1.03 }) m = std::max (m, toneMag (x, i0, i1, fs, f * r));
    return m;
}
} // namespace tdetail

inline double midiToHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
inline double hzToMidi (double f) { return 69.0 + 12.0 * std::log2 (f / 440.0); }

inline TranscribeResult transcribeBass (const std::vector<float>& monoIn, double sr, const TranscribeOptions& opt)
{
    using namespace tdetail;
    TranscribeResult R;
    if (monoIn.size() < (size_t) (sr * 1.0) || sr < 8000) { R.error = "That recording is too short to transcribe."; return R; }
    const size_t maxN = (size_t) (opt.maxSeconds * sr);
    std::vector<float> mono (monoIn.begin(), monoIn.begin() + (long) std::min (monoIn.size(), maxN));
    // clean: remove DC and normalise
    double mean = 0; for (float v : mono) mean += v; mean /= (double) mono.size();
    float peak = 0; for (auto& v : mono) { v = (float) (v - mean); peak = std::max (peak, std::abs (v)); }
    if (peak < 1e-5f) { R.error = "That recording is silent."; return R; }
    for (auto& v : mono) v /= peak;

    const int D = std::max (1, (int) std::lround (sr / 5512.5));
    const double fs = sr / D;
    const std::vector<float> x = decimate (mono, D);

    Frames F; F.fs = fs; F.W = 560; F.H = 32;
    F.minTau = std::max (6, (int) std::floor (fs / 420.0));
    F.maxTau = (int) std::ceil (fs / 30.0);
    const long N2 = (long) x.size();
    F.n = (int) ((N2 - F.W - F.maxTau - 2) / F.H);
    if (F.n < 20) { R.error = "That recording is too short to transcribe."; return R; }
    F.t.assign ((size_t) F.n, 0); F.db.assign ((size_t) F.n, -120); F.midi.assign ((size_t) F.n, 0); F.conf.assign ((size_t) F.n, 0); F.voiced.assign ((size_t) F.n, 0);

    // ---- loudness per frame (window 160 samples ~ 29 ms) centred on the frame
    const int EW = 160;
    for (int k = 0; k < F.n; ++k)
    {
        const long c = (long) k * F.H + F.W / 2;
        double s = 0; int cnt = 0;
        for (long i = std::max (0L, c - EW / 2); i < std::min (N2, c + EW / 2); ++i) { s += (double) x[(size_t) i] * x[(size_t) i]; ++cnt; }
        F.db[(size_t) k] = 10.0 * std::log10 (s / std::max (1, cnt) + 1e-12);
        F.t[(size_t) k] = F.timeOf (k);
    }
    std::vector<double> dbSorted (F.db);
    const double top = percentile (dbSorted, 0.97);
    const double gate = top - 42.0;            // quieter than this is silence
    if (top < -60.0) { R.error = "That recording is too quiet to transcribe."; return R; }

    // ---- note starts from the loudness
    std::vector<int> onsets; bool insertedFirst = false;
    {
        std::vector<double> sc ((size_t) F.n, 0.0);
        for (int k = 1; k < F.n; ++k)
        {
            double mn = 1e9; for (int j = std::max (0, k - 6); j < k; ++j) mn = std::min (mn, F.db[(size_t) j]);
            sc[(size_t) k] = (F.db[(size_t) k] > gate) ? F.db[(size_t) k] - mn : 0.0;
        }
        for (int k = 1; k < F.n; ++k)
        {
            if (sc[(size_t) k] < 5.5) continue;
            bool localMax = true;
            for (int j = std::max (1, k - 4); j <= std::min (F.n - 1, k + 4); ++j) if (sc[(size_t) j] > sc[(size_t) k] + 1e-9 || (sc[(size_t) j] == sc[(size_t) k] && j < k)) { localMax = false; break; }
            if (! localMax) continue;
            if (! onsets.empty() && F.t[(size_t) k] - F.t[(size_t) onsets.back()] < 0.05)
            { if (sc[(size_t) k] > sc[(size_t) onsets.back()]) onsets.back() = k; continue; }
            onsets.push_back (k);
        }
        // a recording that starts right on a note has no rise to find
        int first = -1; for (int k = 0; k < F.n; ++k) if (F.db[(size_t) k] > gate) { first = k; break; }
        if (first >= 0 && (onsets.empty() || onsets.front() - first > 6) && F.t[(size_t) first] < 0.5) { onsets.insert (onsets.begin(), first); insertedFirst = true; }
    }
    if (onsets.empty()) { R.error = "I could not find any bass notes in that recording."; return R; }

    // refine each start to the sample level using a short envelope of the decimated signal
    auto refine = [&] (int k) -> double
    {
        const double tc = F.t[(size_t) k];
        const long a = std::max (0L, (long) ((tc - 0.075) * fs)), b = std::min (N2 - 1, (long) ((tc + 0.02) * fs));
        const int w = 14;
        auto envAt = [&] (long i) { double s = 0; for (long j = i; j < i + w && j < N2; ++j) s += (double) x[(size_t) j] * x[(size_t) j]; return std::sqrt (s / w); };
        double m0 = 1e9; long im0 = a;
        for (long i = a; i < b; i += 2) { const double e = envAt (i); if (e < m0) { m0 = e; im0 = i; } }
        double pk = 0; for (long i = im0; i < std::min (N2 - 1, im0 + (long) (0.06 * fs)); i += 2) pk = std::max (pk, envAt (i));
        const double lvl = m0 + 0.22 * (pk - m0);
        for (long i = im0; i < N2; i += 1) if (envAt (i) >= lvl) return std::max (0.0, ((double) i + w * 0.3) / fs);
        return tc;
    };

    struct Seg { int k0 = 0, k1 = 0, pitch = 0; };
    struct Region
    {
        int a = 0, b = 0; double onsetT = 0, endT = 0; std::vector<Seg> segs; double peakDb = -120; bool ghost = false;
        std::vector<double> ft, fm, fc;        // pitch frames of this note only: time, MIDI (float), confidence
        double audible = 0;                    // seconds until the note is 20 dB below its peak
        double periodicity = 0;                // how tonal it is (0 noise .. 1 pure tone)
    };
    std::vector<Region> regions;
    for (size_t i = 0; i < onsets.size(); ++i)
    {
        Region g; g.a = onsets[i]; g.b = (i + 1 < onsets.size()) ? onsets[i + 1] : F.n; g.onsetT = refine (g.a);
        if (g.onsetT > F.t[(size_t) g.a] + 0.03) g.onsetT = F.t[(size_t) g.a];
        for (int k = g.a; k < std::min (g.b, g.a + 12); ++k) g.peakDb = std::max (g.peakDb, F.db[(size_t) k]);
        if (i == 0 && insertedFirst)
        {
            double pk = 0; for (long j = 0; j < std::min (N2, (long) (0.12 * fs)); ++j) pk = std::max (pk, (double) std::abs (x[(size_t) j]));
            for (long j = 0; j < N2; ++j) if (std::abs (x[(size_t) j]) > 0.15 * pk) { g.onsetT = std::max (0.0, (double) j / fs); break; }
        }
        regions.push_back (g);
    }
    for (size_t i = 0; i < regions.size(); ++i)
    {
        auto& g = regions[i];
        g.endT = (i + 1 < regions.size()) ? regions[i + 1].onsetT - 0.004 : (double) N2 / fs;
        g.audible = g.endT - g.onsetT;
        for (int k = g.a; k < g.b; ++k) if (F.db[(size_t) k] < g.peakDb - 20.0 && F.t[(size_t) k] > g.onsetT + 0.02) { g.audible = std::min (g.audible, F.t[(size_t) k] - g.onsetT); break; }
    }

    // ---- pitch inside each note: only the samples of that note are looked at, so a neighbouring note can never bend the reading
    {
        std::vector<double> d ((size_t) F.maxTau + 2), cm ((size_t) F.maxTau + 2);
        for (auto& g : regions)
        {
            const long sa = (long) ((g.onsetT + 0.018) * fs), se = std::min (N2, (long) (g.endT * fs));
            const long len = se - sa;
            if (len < 170) continue;
            int maxTau = F.maxTau;
            int Wr = (int) std::clamp (len - maxTau - 4, 0L, (long) F.W);
            if (Wr < 220) { maxTau = (int) std::clamp (len - 224, (long) F.minTau + 10, (long) F.maxTau); Wr = (int) std::clamp (len - maxTau - 4, 100L, (long) F.W); }
            if (Wr + maxTau + 2 > len) continue;
            std::vector<long> starts;
            for (long s = sa; s + Wr + maxTau + 2 <= se; s += F.H) starts.push_back (s);
            if (starts.empty() || starts.back() + F.H < se - Wr - maxTau - 2) starts.push_back (se - Wr - maxTau - 2);
            for (long s : starts)
            {
                double conf = 0;
                const double f0 = yinFrame (&x[(size_t) s], Wr, F.minTau, maxTau, fs, d, cm, conf);
                g.ft.push_back (((double) s + Wr * 0.5 + 40.0) / fs);
                g.fm.push_back (f0 > 25.0 && f0 < 450.0 ? hzToMidi (f0) : 0.0);
                g.fc.push_back (f0 > 25.0 && f0 < 450.0 ? conf : 0.0);
            }
            // how periodic is the attack? (a real note repeats itself, a dead-string thump does not)
            std::vector<double> good; for (size_t j = 0; j < g.fm.size(); ++j) if (g.fc[j] > 0.6) good.push_back (g.fm[j]);
            if (! good.empty())
            {
                const double f0 = midiToHz (median (good)); const long tau = (long) std::lround (fs / f0);
                const long M = std::min<long> (330, len - 2 * tau - 2);
                if (M >= 120)
                {
                    double xx = 0, yy = 0, zz = 0, c1 = 0, c2 = 0;
                    for (long j = 0; j < M; ++j) { const double a0 = x[(size_t) (sa + j)], a1 = x[(size_t) (sa + j + tau)], a2 = x[(size_t) (sa + j + 2 * tau)]; xx += a0 * a0; yy += a1 * a1; zz += a2 * a2; c1 += a0 * a1; c2 += a0 * a2; }
                    const double r1 = c1 / std::sqrt (xx * yy + 1e-18), r2 = c2 / std::sqrt (xx * zz + 1e-18);
                    g.periodicity = std::clamp (0.5 * (r1 + std::max (0.0, r2)), 0.0, 1.0);
                }
                else g.periodicity = 0.7;
            }
        }
    }

    // ---- tuning of the recording (circular mean of the fractional part of stable pitches)
    double tuning = 0;
    {
        double sx = 0, sy = 0, wsum = 0;
        for (auto& g : regions)
            for (size_t k = 2; k + 2 < g.fm.size(); ++k)
            {
                if (g.fc[k] < 0.85 || g.fm[k] <= 0) continue;
                if (std::abs (g.fm[k] - g.fm[k - 2]) > 0.2 || std::abs (g.fm[k] - g.fm[k + 2]) > 0.2) continue;
                const double fr = g.fm[k] - std::round (g.fm[k]);
                sx += g.fc[k] * std::cos (2.0 * M_PI * fr); sy += g.fc[k] * std::sin (2.0 * M_PI * fr); wsum += g.fc[k];
            }
        if (wsum > 20)
        {
            const double Rlen = std::sqrt (sx * sx + sy * sy) / wsum;
            if (Rlen > 0.55) tuning = std::atan2 (sy, sx) / (2.0 * M_PI);
        }
        if (std::abs (tuning) < 0.05) tuning = 0;     // within 5 cents: leave it
    }
    R.tuningCents = tuning * 100.0;

    // ---- pitch runs inside each region
    for (auto& g : regions)
    {
        const int nfr = (int) g.fm.size();
        std::vector<int> q ((size_t) nfr, INT32_MIN);
        for (int i = 0; i < nfr; ++i) if (g.fc[(size_t) i] >= 0.62 && g.fm[(size_t) i] > 0) q[(size_t) i] = (int) std::lround (g.fm[(size_t) i] - tuning);
        std::vector<int> qm = q;                                  // median of 5 removes single-frame glitches
        for (int i = 0; i < nfr; ++i)
        {
            if (q[(size_t) i] == INT32_MIN) continue;
            std::vector<int> w; for (int j = std::max (0, i - 2); j <= std::min (nfr - 1, i + 2); ++j) if (q[(size_t) j] != INT32_MIN) w.push_back (q[(size_t) j]);
            std::sort (w.begin(), w.end()); qm[(size_t) i] = w[w.size() / 2];
        }
        struct Run { int i0, i1, p; };
        std::vector<Run> runs;
        for (int i = 0; i < nfr; ++i)
        {
            if (qm[(size_t) i] == INT32_MIN) continue;
            if (! runs.empty() && runs.back().p == qm[(size_t) i] && i - runs.back().i1 <= 3) { runs.back().i1 = i; continue; }
            runs.push_back ({ i, i, qm[(size_t) i] });
        }
        std::vector<Run> st;
        for (auto& r : runs) if (r.i1 - r.i0 + 1 >= 3 || (st.empty() && nfr < 12 && r.i1 - r.i0 + 1 >= 2)) st.push_back (r);
        // octave glitches: a short run exactly an octave away is the same note
        for (size_t i = 1; i + 1 < st.size();)
        {
            const int len = st[i].i1 - st[i].i0 + 1;
            const int d1 = std::abs (st[i].p - st[i - 1].p);
            if (len < 20 && (d1 == 12 || d1 == 24) && st[i - 1].p == st[i + 1].p) { st[i - 1].i1 = st[i + 1].i1; st.erase (st.begin() + (long) i, st.begin() + (long) i + 2); continue; }
            ++i;
        }
        for (size_t i = 1; i < st.size();) { if (st[i].p == st[i - 1].p) { st[i - 1].i1 = st[i].i1; st.erase (st.begin() + (long) i); } else ++i; }
        // a run too short to trust at the end of a note is dropped (the neighbour's tail is bending the reading)
        if (st.size() >= 2 && (st.back().i1 - st.back().i0 + 1) < 5 && std::abs (st.back().p - st[st.size() - 2].p) >= 5) st.pop_back();
        // a very short note only has one or two readings: trust them when they are confident
        if (st.empty())
        {
            std::vector<int> pp; int i0 = nfr, i1 = -1;
            for (int i = 0; i < nfr; ++i) if (g.fc[(size_t) i] >= 0.85 && g.fm[(size_t) i] > 0) { pp.push_back ((int) std::lround (g.fm[(size_t) i] - tuning)); i0 = std::min (i0, i); i1 = std::max (i1, i); }
            if (! pp.empty() && (int) pp.size() * 2 >= nfr && nfr <= 3) { std::sort (pp.begin(), pp.end()); st.push_back ({ i0, i1, pp[pp.size() / 2] }); }
        }
        // convert frame indices to the global frame grid used below (times are stored per region, so use pseudo frame ids)
        for (auto& r : st) g.segs.push_back ({ r.i0, r.i1, r.p });
        if (g.segs.empty()) g.ghost = true;
        // dead-string thump: short, unpitched or not periodic
        if (! g.segs.empty() && g.audible < 0.13 && g.periodicity < 0.62) g.ghost = true;
        if (! g.segs.empty() && g.periodicity < 0.35 && g.audible < 0.25) g.ghost = true;
    }

    // ---- build notes (in seconds)
    struct Raw { double t0, t1; int pitch; int art; double peakDb; double conf; bool fromOnset; int region; };
    std::vector<Raw> raw;
    for (size_t ri = 0; ri < regions.size(); ++ri)
    {
        auto& g = regions[ri];
        const double endLimit = g.endT;
        if (g.ghost)
        {
            const double len = std::clamp (g.audible, 0.04, 0.16);
            raw.push_back ({ g.onsetT, std::min (g.onsetT + len, endLimit), -1, A_Ghost, g.peakDb, 0.5, true, (int) ri });
            continue;
        }
        for (size_t si = 0; si < g.segs.size(); ++si)
        {
            const Seg& sg = g.segs[si];
            Raw n; n.pitch = sg.pitch; n.art = A_Sustain; n.peakDb = g.peakDb; n.fromOnset = (si == 0); n.region = (int) ri;
            double cs = 0; int cc = 0; for (int k = sg.k0; k <= sg.k1; ++k) { cs += g.fc[(size_t) k]; ++cc; }
            n.conf = cc ? cs / cc : 0.5;
            if (si == 0) n.t0 = g.onsetT;
            else
            {
                const Seg& pv = g.segs[si - 1];
                const int ramp = sg.k0 - pv.k1 - 1;
                const int interval = sg.pitch - pv.pitch;
                // a slide moves through the notes in between without a break in the sound
                int voicedRamp = 0; double lo = 1e9, hi = -1e9;
                for (int k = pv.k1 + 1; k < sg.k0; ++k) if (g.fc[(size_t) k] >= 0.5 && g.fm[(size_t) k] > 0) { ++voicedRamp; lo = std::min (lo, g.fm[(size_t) k] - tuning); hi = std::max (hi, g.fm[(size_t) k] - tuning); }
                const bool between = ramp >= 3 && voicedRamp >= (ramp * 3 + 3) / 5 && lo > std::min (pv.pitch, sg.pitch) - 1.2 && hi < std::max (pv.pitch, sg.pitch) + 1.2;
                const bool moving = (std::abs (interval) >= 2) ? (hi - lo > 0.8) : (hi - lo > 0.35);
                if (between && moving && ramp <= 70)
                {
                    n.art = A_Slide; n.t0 = g.ft[(size_t) (pv.k1 + 1)] - 0.01;
                    ++R.slides;
                }
                else if (si + 1 == g.segs.size() && sg.k1 - sg.k0 < 12
                         && [&] { // still moving when the note ends: a slide into the next note, so lean the target the way it is going
                                  const int nf = (int) g.fm.size(); if (nf < 6 || sg.k1 < nf - 2) return false;
                                  const double last = g.fm[(size_t) (nf - 1)] - tuning, prev = g.fm[(size_t) (nf - 5)] - tuning;
                                  if (g.fc[(size_t) (nf - 1)] < 0.5 || g.fc[(size_t) (nf - 5)] < 0.5) return false;
                                  const double drift = last - prev;
                                  if (std::abs (drift) < 0.5 || drift * interval <= 0 || std::abs (last - pv.pitch) < 0.9) return false;
                                  const double target = last + std::clamp (drift * 1.5, -2.0, 2.0);
                                  n.pitch = (int) std::lround (target); return true; }())
                {
                    n.art = A_Slide; n.t0 = g.ft[(size_t) (pv.k1 + 1)] - 0.01; ++R.slides;
                }
                else
                {
                    n.art = interval > 0 ? A_HammerOn : A_PullOff; n.t0 = 0.5 * (g.ft[(size_t) pv.k1] + g.ft[(size_t) sg.k0]);
                    ++R.legatos;
                }
                raw.back().t1 = n.t0;               // legato: the previous note ends where this one starts
            }
            if (si + 1 < g.segs.size()) n.t1 = g.ft[(size_t) g.segs[si + 1].k0];
            else
            {
                // until the sound dies away (30 dB below its peak) or the next note starts
                const double floorDb = std::max (gate + 3.0, g.peakDb - 30.0);
                double e = endLimit;
                for (int k = g.a; k < g.b; ++k) if (F.t[(size_t) k] > n.t0 + 0.05 && F.db[(size_t) k] < floorDb) { e = F.t[(size_t) k]; break; }
                n.t1 = std::min (e, endLimit);
            }
            if (n.t1 < n.t0 + 0.03) n.t1 = n.t0 + 0.03;
            raw.push_back (n);
        }
    }

    // ---- octave check on every pitched note: strong energy half an octave... an octave below means the real note is lower
    for (auto& n : raw)
    {
        if (n.pitch < 0) continue;
        const long i0 = (long) ((n.t0 + 0.03) * fs), i1 = (long) (std::min (n.t1, n.t0 + 0.4) * fs);
        if (i1 - i0 < 400) continue;
        const double f = midiToHz ((double) n.pitch + tuning);
        if (n.pitch - 12 >= 23)
        {
            const double lo = bestMag (x, i0, std::min (i1, N2), fs, f * 0.5), hi = bestMag (x, i0, std::min (i1, N2), fs, f);
            if (lo > 0.7 * hi && lo > 1e-4) { n.pitch -= 12; ++R.octaveFixes; }
        }
    }

    // ---- ghost notes take the pitch of their neighbour; very short voiced blips with a weak pitch become ghosts too
    for (size_t i = 0; i < raw.size(); ++i)
    {
        auto& n = raw[i];
        if (n.pitch >= 0 && n.conf < 0.72 && (n.t1 - n.t0) < 0.12 && n.fromOnset) { n.art = A_Ghost; }
        if (n.art == A_Ghost && n.pitch < 0)
        {
            int p = -1;
            for (long j = (long) i - 1; j >= 0 && p < 0; --j) if (raw[(size_t) j].pitch >= 0 && raw[(size_t) j].art != A_Ghost) p = raw[(size_t) j].pitch;
            for (size_t j = i + 1; j < raw.size() && p < 0; ++j) if (raw[j].pitch >= 0) p = raw[j].pitch;
            n.pitch = p >= 0 ? p : 40;
        }
        if (n.art == A_Ghost) ++R.ghosts;
    }
    // drop crumbs: pitched notes shorter than 35 ms that are not onsets
    raw.erase (std::remove_if (raw.begin(), raw.end(), [] (const Raw& n) { return n.pitch < 0 || (! n.fromOnset && n.t1 - n.t0 < 0.035); }), raw.end());
    if (raw.empty()) { R.error = "I could not find any bass notes in that recording."; return R; }
    std::stable_sort (raw.begin(), raw.end(), [] (const Raw& a, const Raw& b) { return a.t0 < b.t0; });
    for (size_t i = 0; i + 1 < raw.size(); ++i) if (raw[i].t1 > raw[i + 1].t0) raw[i].t1 = std::max (raw[i].t0 + 0.03, raw[i + 1].t0 - 0.002);

    // ---- velocity from the attack loudness
    {
        std::vector<double> pk; for (auto& n : raw) pk.push_back (n.peakDb);
        const double ref = percentile (pk, 0.9);
        for (auto& n : raw)
        {
            const double rel = n.peakDb - ref;
            int v = (int) std::lround (104.0 + 2.1 * rel);
            if (! n.fromOnset) v = std::max (40, v - 8);
            if (n.art == A_Ghost) v = std::min (v, 52);
            TNote t; t.t0 = n.t0; t.t1 = n.t1; t.pitch = std::clamp (n.pitch, 12, 100); t.vel = std::clamp (v, 20, 127); t.art = n.art; t.conf = (float) n.conf;
            R.notes.push_back (t);
        }
    }
    // pitched notes shorter than a 32nd that follow closely look like staccato: mark short plucks
    for (size_t i = 0; i < R.notes.size(); ++i)
    {
        auto& n = R.notes[i];
        if (n.art != A_Sustain) continue;
        const double nextT = (i + 1 < R.notes.size()) ? R.notes[i + 1].t0 : n.t1 + 1.0;
        if (nextT - n.t0 > 0.2 && n.t1 - n.t0 < 0.45 * (nextT - n.t0) && n.t1 - n.t0 < 0.25) n.art = A_Staccato;
    }

    // ---- tempo and grid
    std::vector<double> ons; for (auto& n : R.notes) ons.push_back (n.t0);
    auto gridFit = [&] (double bpm, double& phase)
    {
        const double g = 60.0 / bpm / 4.0; double sx = 0, sy = 0;
        for (double t : ons) { const double a = 2.0 * M_PI * t / g; sx += std::cos (a); sy += std::sin (a); }
        const double len = std::sqrt (sx * sx + sy * sy) / (double) std::max<size_t> (1, ons.size());
        phase = std::atan2 (sy, sx) / (2.0 * M_PI) * g;       // seconds: onsets sit at phase + k * g
        return len;
    };
    double bpm = std::clamp (opt.bpm, 40.0, 240.0);
    if (opt.detectTempo && ons.size() >= 6)
    {
        double bestScore = -1, bestB = bpm;
        for (double b = 70.0; b <= 170.0; b += 0.25)
        {
            double ph; const double s = gridFit (b, ph);
            const double score = s - 0.0004 * std::abs (b - 110.0);      // gentle preference for common tempi when two fit equally well
            if (score > bestScore) { bestScore = score; bestB = b; }
        }
        bpm = bestB;
    }
    double phase = 0; const double fit = gridFit (bpm, phase);
    R.gridFit = fit; R.bpm = bpm;
    const double g16 = 60.0 / bpm / 4.0;
    // keep bar 1 at the start of the recording, but let the grid sit where the notes actually are (within half a sixteenth)
    double shift = phase; while (shift > g16 * 0.5) shift -= g16; while (shift < -g16 * 0.5) shift += g16;
    double t0 = (fit >= 0.55 && ! opt.firstNoteIsBarOne) ? shift : 0.0;
    if (opt.firstNoteIsBarOne) t0 = R.notes.front().t0;
    int tidy = opt.tidy;
    if (tidy > 0 && fit < 0.5) tidy = 0;           // swing / triplets / wrong tempo: do not force a straight grid
    R.tidyApplied = tidy > 0;
    const double grid = (tidy == 3) ? 0.5 : 0.25;
    const double strength = (tidy == 1) ? 0.75 : (tidy >= 2 ? 1.0 : 0.0);
    auto toBeat = [&] (double t) { return (t - t0) * bpm / 60.0; };
    auto snap = [&] (double b) { return std::round (b / grid) * grid; };
    for (auto& n : R.notes)
    {
        const double b0 = toBeat (n.t0), b1 = toBeat (n.t1);
        n.beat = b0 + strength * (snap (b0) - b0);
        n.lenBeats = std::max (0.0625, b1 - b0);
        n.lenBeats = (strength > 0) ? std::max (grid * 0.5, (b1 + strength * (snap (b1) - b1)) - n.beat) : n.lenBeats;
        n.beat = std::max (0.0, n.beat);
    }
    // legato joins stay joined; notes never overlap
    for (size_t i = 0; i + 1 < R.notes.size(); ++i)
    {
        auto& a = R.notes[i]; auto& b = R.notes[i + 1];
        const bool joined = std::abs (R.notes[i].t1 - R.notes[i + 1].t0) < 0.02;
        if (joined) a.lenBeats = std::max (0.0625, b.beat - a.beat);
        if (a.beat + a.lenBeats > b.beat) a.lenBeats = std::max (0.0625, b.beat - a.beat - 0.01);
        if (b.beat < a.beat) b.beat = a.beat;
    }
    double endB = 0; for (auto& n : R.notes) endB = std::max (endB, n.beat + n.lenBeats);
    R.lengthBeats = endB;
    double qs = 0; for (auto& n : R.notes) qs += n.conf; R.quality = qs / (double) R.notes.size();
    R.ok = true;
    return R;
}

/** one line for the user */
inline std::string describeTranscription (const TranscribeResult& r)
{
    char b[400];
    std::snprintf (b, sizeof (b), "%d notes (%d ghost, %d slides, %d hammer-ons / pull-offs) at %.0f BPM. %s%s",
                   (int) r.notes.size(), r.ghosts, r.slides, r.legatos, r.bpm,
                   r.tuningCents != 0 ? ("Tuning corrected by " + std::to_string ((int) std::lround (-r.tuningCents)) + " cents. ").c_str() : "",
                   r.gridFit >= 0.7 ? (r.tidyApplied ? "Timing sits well on a 1/16 grid." : "Timing sits on a 1/16 grid.")
                                    : (r.gridFit >= 0.5 ? "Timing is loose against the grid." : "Timing does not sit on a straight grid (swing, or the tempo is off): left as played."));
    return b;
}
} // namespace enochsolo
