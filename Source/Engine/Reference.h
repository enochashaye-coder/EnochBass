#pragma once
// Reference analysis: turns a recording of ONE melodic instrument (or a MIDI file) into a playing-style profile
// (where the notes fall, how long and how loud they are, the swing, the shape of the line).
// The generator then writes NEW notes that follow that rhythm and feel. No JUCE dependency.
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace enochsolo
{
//------------------------------------------------------------------------------ stored profile (seconds)
struct RefRaw { double t = 0, dur = 0.2; float loud = 0.5f; int pitch = -1; };   // loud 0..1 relative to the file, pitch = MIDI note or -1

struct RefProfile
{
    std::string name;
    double bpm = 0;          // detected (or read from the MIDI file)
    double phase = 0;        // seconds of a beat in the recording (beat grid anchor)
    double lengthSec = 0;
    bool fromMidi = false;
    std::vector<RefRaw> notes;
    bool valid() const { return notes.size() >= 3 && bpm > 20.0; }
};

//------------------------------------------------------------------------------ derived view in beats (bar-aligned)
struct RefNote
{
    double beat = 0, len = 0.25, pos = 0;   // pos = beat inside its bar
    int bar = 0;
    float loud = 0.5f;
    int pitch = -1;
    double rel = 0;                         // pitch relative to the reference's median pitch (semitones)
    bool hasPitch = false;
    float lenFrac = 0.9f;                   // length / distance to the next note  (short = staccato, ~1 = legato)
};

struct RefView
{
    bool valid = false;
    std::string name;
    double bpm = 0, barBeats = 4;
    int bars = 0;
    std::vector<RefNote> notes;
    std::vector<std::vector<int>> byBar;    // indices into notes, in time order
    std::vector<double> barRel;             // median relative pitch of each bar
    double swing = 0; bool swingKnown = false;      // 0 straight .. 1 full triplet swing
    double legato = 0.8;                    // mean length fraction
    double notesPerBar = 0;
    double pitchSpan = 12;                  // 10..90 percentile spread in semitones
    bool pitched = false;
};

//------------------------------------------------------------------------------ helpers
namespace refdetail
{
inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

inline double median (std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    const size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}
inline double percentile (std::vector<double> v, double p)
{
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    const double x = std::clamp (p, 0.0, 1.0) * (double) (v.size() - 1);
    const size_t i = (size_t) x; const double f = x - (double) i;
    return i + 1 < v.size() ? v[i] * (1.0 - f) + v[i + 1] * f : v[i];
}

/** YIN pitch estimate for one frame. Returns MIDI pitch (float) or -1 when unvoiced. */
inline double yin (const float* x, int W, int minTau, int maxTau, double sr, std::vector<double>& d)
{
    const int N = W - maxTau;
    if (N < 64) return -1.0;
    d.assign ((size_t) maxTau + 1, 0.0);
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double s = 0;
        for (int j = 0; j < N; ++j) { const double dd = (double) x[j] - (double) x[j + tau]; s += dd * dd; }
        d[(size_t) tau] = s;
    }
    double run = 0; d[0] = 1.0;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        run += d[(size_t) tau];
        d[(size_t) tau] = run > 1e-12 ? d[(size_t) tau] * tau / run : 1.0;
    }
    int best = -1;
    for (int tau = std::max (2, minTau); tau < maxTau; ++tau)
    {
        if (d[(size_t) tau] < 0.15)
        {
            while (tau + 1 < maxTau && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
            best = tau; break;
        }
    }
    if (best < 0)
    {
        double mn = 1e9; int mi = -1;
        for (int tau = std::max (2, minTau); tau < maxTau; ++tau) if (d[(size_t) tau] < mn) { mn = d[(size_t) tau]; mi = tau; }
        if (mi < 0 || mn > 0.32) return -1.0;
        best = mi;
    }
    double tau = best;
    if (best > 1 && best < maxTau)
    {
        const double a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-12) tau += 0.5 * (a - c) / den;
    }
    const double f = sr / tau;
    if (f < 40.0 || f > 4200.0) return -1.0;
    return 69.0 + 12.0 * std::log2 (f / 440.0);
}
} // namespace refdetail

//------------------------------------------------------------------------------ analysis of a mono recording
inline RefProfile analyseAudio (const std::vector<float>& mono, double sr, const std::string& name)
{
    using namespace refdetail;
    RefProfile prof; prof.name = name;
    if (mono.size() < (size_t) (sr * 1.0) || sr < 8000.0) return prof;

    // 1. decimate to ~11 kHz (box filter) and normalise
    const int D = std::max (1, (int) std::lround (sr / 11025.0));
    const double sr2 = sr / D;
    std::vector<float> x;
    x.reserve (mono.size() / (size_t) D + 1);
    double mean = 0;
    for (size_t i = 0; i + (size_t) D <= mono.size(); i += (size_t) D)
    {
        double s = 0; for (int k = 0; k < D; ++k) s += mono[i + (size_t) k];
        x.push_back ((float) (s / D)); mean += s / D;
    }
    mean /= (double) std::max<size_t> (1, x.size());
    double peak = 1e-9;
    for (auto& v : x) { v = (float) (v - mean); peak = std::max (peak, (double) std::abs (v)); }
    for (auto& v : x) v = (float) (v / peak);
    prof.lengthSec = (double) mono.size() / sr;

    // 2. spectral flux (log-magnitude) + frame rms
    const int N = 512, H = std::max (32, (int) std::lround (sr2 * 0.008));
    if ((int) x.size() < N * 4) return prof;
    const int F = ((int) x.size() - N) / H + 1;
    std::vector<double> win ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / (N - 1));
    const int K0 = 3, K1 = 200;
    std::vector<double> prevL ((size_t) K1, 0.0), flux ((size_t) F, 0.0), rms ((size_t) F, 0.0);
    std::vector<std::complex<double>> buf ((size_t) N);
    for (int f = 0; f < F; ++f)
    {
        const float* p = x.data() + (size_t) f * (size_t) H;
        for (int i = 0; i < N; ++i) buf[(size_t) i] = { p[i] * win[(size_t) i], 0.0 };
        fft (buf);
        double fl = 0;
        for (int k = K0; k < K1; ++k)
        {
            const double l = std::log (1.0 + 400.0 * std::abs (buf[(size_t) k]) / (N * 0.25));
            if (f > 0) fl += std::max (0.0, l - prevL[(size_t) k]);
            prevL[(size_t) k] = l;
        }
        flux[(size_t) f] = fl;
        double e = 0; const int c0 = N / 2 - H, c1 = N / 2 + H;
        for (int i = c0; i < c1; ++i) e += (double) p[i] * p[i];
        rms[(size_t) f] = std::sqrt (e / (c1 - c0));
    }
    // onset envelope: flux minus a local mean
    std::vector<double> oe ((size_t) F, 0.0);
    {
        const int w = std::max (4, (int) std::lround (0.18 / (H / sr2)));
        for (int f = 0; f < F; ++f)
        {
            double s = 0; int c = 0;
            for (int k = std::max (0, f - w); k <= std::min (F - 1, f + w); ++k) { s += flux[(size_t) k]; ++c; }
            oe[(size_t) f] = std::max (0.0, flux[(size_t) f] - s / c);
        }
    }
    double oMean = 0, oMax = 1e-9;
    for (double v : oe) { oMean += v; oMax = std::max (oMax, v); }
    oMean /= F;
    double oVar = 0; for (double v : oe) oVar += (v - oMean) * (v - oMean);
    const double oStd = std::sqrt (oVar / F);
    if (oMax < 20.0) return prof;                                    // no clear attacks at all (silence, steady tone, hiss)
    const double thr = std::max ({ oMean + 0.7 * oStd, 0.07 * oMax, 6.0 });
    const double fr = sr2 / H;                       // flux frames per second
    const int sep = std::max (2, (int) std::lround (0.055 * fr));

    std::vector<int> peaks;
    for (int f = 2; f < F - 2; ++f)
    {
        if (oe[(size_t) f] < thr) continue;
        bool isMax = true;
        for (int k = std::max (0, f - 3); k <= std::min (F - 1, f + 3); ++k) if (oe[(size_t) k] > oe[(size_t) f]) { isMax = false; break; }
        if (! isMax) continue;
        if (! peaks.empty() && f - peaks.back() < sep) { if (oe[(size_t) f] > oe[(size_t) peaks.back()]) peaks.back() = f; continue; }
        peaks.push_back (f);
    }
    auto frameTime = [&] (int f) { return ((double) f * H + N * 0.5) / sr2 - 0.012; };

    // 3. pitch track
    const int W = 640, PH = H * 2;
    const int maxTau = (int) (sr2 / 55.0), minTau = std::max (3, (int) (sr2 / 2200.0));
    const int PF = ((int) x.size() - W) / PH + 1;
    std::vector<double> pitch ((size_t) std::max (0, PF), -1.0);
    {
        std::vector<double> dbuf;
        double rmsMax = 1e-9; for (double v : rms) rmsMax = std::max (rmsMax, v);
        for (int j = 0; j < PF; ++j)
        {
            const float* p = x.data() + (size_t) j * (size_t) PH;
            const int rf = std::min (F - 1, (j * PH + W / 2 - N / 2) / H);
            if (rf >= 0 && rms[(size_t) rf] < 0.02 * rmsMax) continue;
            pitch[(size_t) j] = yin (p, W, minTau, maxTau, sr2, dbuf);
        }
    }
    auto pitchTime = [&] (int j) { return ((double) j * PH + W * 0.5) / sr2; };
    auto pitchIdx = [&] (double t) { return (int) std::lround ((t * sr2 - W * 0.5) / PH); };

    // 4. split legato notes where the pitch clearly changes without a new attack
    std::vector<double> onsets;
    for (int f : peaks) onsets.push_back (frameTime (f));
    {
        std::vector<double> extra;
        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const double a = onsets[i] + 0.05, b = i + 1 < onsets.size() ? onsets[i + 1] - 0.05 : onsets[i] + 1.2;
            int ja = std::max (0, pitchIdx (a)), jb = std::min (PF - 1, pitchIdx (b));
            std::vector<double> cur;
            for (int j = ja; j <= jb; ++j)
            {
                const double p = pitch[(size_t) j];
                if (p < 0) { continue; }
                if (cur.size() < 2) { cur.push_back (p); continue; }
                const double med = median (cur);
                if (std::abs (p - med) >= 1.3 && j + 3 <= jb)
                {
                    const double p1 = pitch[(size_t) j + 1], p2 = pitch[(size_t) j + 2], p3 = pitch[(size_t) j + 3];
                    if (p1 > 0 && p2 > 0 && p3 > 0 && std::abs (p1 - p) < 0.55 && std::abs (p2 - p) < 0.55 && std::abs (p3 - p) < 0.55
                        && std::abs (median ({ p, p1, p2, p3 }) - med) >= 1.3)
                    { extra.push_back (pitchTime (j) - 0.02); cur.assign (1, p); j += 3; continue; }
                }
                if (std::abs (p - med) < 1.0) cur.push_back (p);
            }
        }
        onsets.insert (onsets.end(), extra.begin(), extra.end());
        std::sort (onsets.begin(), onsets.end());
    }
    if (onsets.size() < 3) return prof;

    // 5. build notes: length, loudness, pitch
    auto rmsAt = [&] (double t) { const int f = std::clamp ((int) std::lround ((t * sr2 - N * 0.5) / H), 0, F - 1); return rms[(size_t) f]; };
    double rmsMax = 1e-9; for (double v : rms) rmsMax = std::max (rmsMax, v);
    std::vector<RefRaw> notes; std::vector<double> dbs;
    for (size_t i = 0; i < onsets.size(); ++i)
    {
        const double t0 = onsets[i];
        const double bound = i + 1 < onsets.size() ? onsets[i + 1] : std::min (prof.lengthSec, t0 + 2.0);
        double pk = 0;
        for (double t = t0; t < std::min (bound, t0 + 0.14); t += H / sr2) pk = std::max (pk, rmsAt (t + 0.012));
        if (pk < 0.03 * rmsMax) continue;                                    // noise / reverb tail
        double end = bound;
        for (double t = t0 + 0.05; t < bound; t += H / sr2)
            if (rmsAt (t + 0.012) < 0.14 * pk) { end = t; break; }
        RefRaw n; n.t = t0; n.dur = std::max (0.04, end - t0);
        std::vector<double> ps;
        const int ja = std::max (0, pitchIdx (t0 + 0.03)), jb = std::min (PF - 1, pitchIdx (t0 + 0.03 + std::max (0.04, 0.6 * n.dur)));
        for (int j = ja; j <= jb; ++j) if (pitch[(size_t) j] > 0) ps.push_back (pitch[(size_t) j]);
        if (ps.size() >= 2) n.pitch = (int) std::lround (median (ps));
        else
        {
            const int j = std::clamp (pitchIdx (t0 + 0.05), 0, std::max (0, PF - 1));
            if (PF > 0 && pitch[(size_t) j] > 0) n.pitch = (int) std::lround (pitch[(size_t) j]);
        }
        n.loud = (float) (20.0 * std::log10 (pk + 1e-9));                    // dB for now
        dbs.push_back (n.loud);
        notes.push_back (n);
    }
    if (notes.size() < 3) return prof;
    {
        // drop quiet, pitchless blips (clicks, breaths, note tails) when most notes are pitched
        size_t pitched = 0; for (auto& n : notes) if (n.pitch >= 0) ++pitched;
        if (pitched * 2 > notes.size())
        {
            const double lo0 = percentile (dbs, 0.1), hi0 = percentile (dbs, 0.9);
            std::vector<RefRaw> keep;
            for (auto& n : notes)
            {
                const double rel = (n.loud - lo0) / std::max (3.0, hi0 - lo0);
                if (n.pitch < 0 && rel < 0.3) continue;
                keep.push_back (n);
            }
            if (keep.size() >= 3) { notes = keep; dbs.clear(); for (auto& n : notes) dbs.push_back (n.loud); }
        }
    }
    {
        const double lo = percentile (dbs, 0.1), hi = percentile (dbs, 0.9);
        for (auto& n : notes) n.loud = (float) std::clamp ((n.loud - lo) / std::max (3.0, hi - lo), 0.0, 1.0);
    }

    // 6. tempo + bar line from how well the onsets sit on a 16th-note grid (works on rhythm alone)
    {
        const double TWO_PI = 2.0 * 3.14159265358979323846;
        std::vector<double> w; double wsum = 0;
        for (auto& n : notes) { const double ww = 0.3 + n.loud + std::min (1.0, n.dur); w.push_back (ww); wsum += ww; }
        double bestScore = -1, bestG = 0.15, bestPhase = 0;
        for (double g = 0.06; g <= 0.27; g *= 1.001)
        {
            std::complex<double> acc (0, 0);
            for (size_t i = 0; i < notes.size(); ++i) acc += w[i] * std::polar (1.0, -TWO_PI * notes[i].t / g);
            const double fit = std::abs (acc) / wsum;                         // 1 = every onset exactly on the grid
            const double bpmG = 15.0 / g;                                     // beat = 4 grid steps
            const double oct = std::log2 (bpmG / 105.0);
            const double score = fit * std::pow (g / 0.19, 0.35) * std::exp (-oct * oct / (2.0 * 0.7 * 0.7));
            if (score > bestScore) { bestScore = score; bestG = g; bestPhase = -std::arg (acc) / TWO_PI; }
        }
        // refine g around the best value (finer steps)
        {
            double bf = -1;
            for (double g = bestG * 0.997; g <= bestG * 1.003; g *= 1.00005)
            {
                std::complex<double> acc (0, 0);
                for (size_t i = 0; i < notes.size(); ++i) acc += w[i] * std::polar (1.0, -TWO_PI * notes[i].t / g);
                const double fit = std::abs (acc) / wsum;
                if (fit > bf) { bf = fit; bestG = g; bestPhase = -std::arg (acc) / TWO_PI; }
            }
        }
        if (bestPhase < 0) bestPhase += 1.0;
        const double g = bestG;
        const double gridT0 = bestPhase * g;                                  // a grid point (seconds)
        // which grid point is the bar line? weigh onset strength by metric position
        double A[16] = {};
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const long idx = std::lround ((notes[i].t - gridT0) / g);
            A[((idx % 16) + 16) % 16] += w[i];
        }
        static const double M[16] = { 1.0, 0.05, 0.15, 0.05, 0.35, 0.05, 0.15, 0.05, 0.55, 0.05, 0.15, 0.05, 0.35, 0.05, 0.15, 0.05 };
        int bestK = 0; double bk = -1;
        for (int k = 0; k < 16; ++k)
        {
            double sc = 0; for (int j = 0; j < 16; ++j) sc += A[(k + j) % 16] * M[j];
            if (sc > bk) { bk = sc; bestK = k; }
        }
        prof.bpm = 15.0 / g;
        prof.phase = gridT0 + bestK * g;                                      // a bar line in the recording
    }
    prof.notes = std::move (notes);
    return prof;
}

//------------------------------------------------------------------------------ profile -> bar-aligned view
/** tempoMode: 0 as detected, 1 twice as fast, 2 half as fast.  shift: beats to move the bar line later. */
inline RefView makeRefView (const RefProfile& p, double barBeats, int tempoMode, double shift)
{
    using namespace refdetail;
    RefView v; v.name = p.name; v.barBeats = std::max (0.5, barBeats);
    if (! p.valid()) return v;
    const double bpm = p.bpm * (tempoMode == 1 ? 2.0 : (tempoMode == 2 ? 0.5 : 1.0));
    v.bpm = bpm;
    const double bps = bpm / 60.0;
    std::vector<RefNote> ns;
    double maxB = 0;
    for (auto& r : p.notes)
    {
        RefNote n;
        n.beat = (r.t - p.phase) * bps - shift;
        n.len = std::max (0.05, r.dur * bps);
        n.loud = r.loud; n.pitch = r.pitch; n.hasPitch = r.pitch >= 0;
        ns.push_back (n);
    }
    std::sort (ns.begin(), ns.end(), [] (const RefNote& a, const RefNote& b) { return a.beat < b.beat; });
    // anything before the first bar line is a pick-up: move whole bars forward so that it sits at the end of a bar
    for (auto& n : ns) if (n.beat < 0 && n.beat > -0.12) n.beat = 0;      // tiny timing error before the bar line is not a pick-up
    double minB = ns.front().beat;
    if (minB < -1e-6)
    {
        const double k = std::ceil (-minB / v.barBeats);
        for (auto& n : ns) n.beat += k * v.barBeats;
    }
    for (auto& n : ns) maxB = std::max (maxB, n.beat);
    v.bars = std::max (1, (int) std::ceil ((maxB + 0.01) / v.barBeats));
    // a pick-up at the very start belongs to the previous bar (wraps to the end), so shift the start of the loop
    v.byBar.assign ((size_t) v.bars, {});
    std::vector<double> pitches;
    for (size_t i = 0; i < ns.size(); ++i)
    {
        auto& n = ns[i];
        n.bar = std::clamp ((int) std::floor (n.beat / v.barBeats + 1e-6), 0, v.bars - 1);
        n.pos = n.beat - n.bar * v.barBeats;
        const double nextGap = i + 1 < ns.size() ? ns[i + 1].beat - n.beat : 2.0;
        const double ioi = std::clamp (nextGap, 0.12, 2.0);
        n.lenFrac = (float) std::clamp (n.len / ioi, 0.12, 1.1);
        if (n.hasPitch) pitches.push_back (n.pitch);
        v.byBar[(size_t) n.bar].push_back ((int) i);
    }
    v.pitched = pitches.size() >= std::max<size_t> (3, ns.size() / 3);
    const double med = median (pitches);
    v.pitchSpan = pitches.empty() ? 12.0 : std::max (6.0, percentile (pitches, 0.9) - percentile (pitches, 0.1));
    double lf = 0;
    for (auto& n : ns) { n.rel = n.hasPitch ? n.pitch - med : 0.0; lf += n.lenFrac; }
    v.legato = lf / (double) ns.size();
    v.barRel.assign ((size_t) v.bars, 0.0);
    for (int b = 0; b < v.bars; ++b)
    {
        std::vector<double> rs;
        for (int i : v.byBar[(size_t) b]) if (ns[(size_t) i].hasPitch) rs.push_back (ns[(size_t) i].rel);
        v.barRel[(size_t) b] = rs.empty() ? 0.0 : median (rs);
    }
    v.notes = std::move (ns);
    v.notesPerBar = (double) v.notes.size() / v.bars;
    // swing: how late do off-beat eighths fall (0.5 = straight, 0.667 = triplet)?
    std::vector<double> off;
    for (auto& n : v.notes)
    {
        const double fr = n.beat - std::floor (n.beat);
        if (fr > 0.42 && fr < 0.72) off.push_back (fr);
    }
    if (off.size() >= 6) { v.swing = std::clamp ((median (off) - 0.5) * 6.0, 0.0, 1.0); v.swingKnown = true; }
    v.valid = true;
    return v;
}

inline std::string describeRef (const RefProfile& p, int tempoMode, double shift)
{
    if (! p.valid()) return "Nothing loaded";
    const auto v = makeRefView (p, 4.0, tempoMode, shift);
    char b[256];
    std::snprintf (b, sizeof (b), "%.0f BPM  |  %.1f notes/bar  |  %s  |  %d%% legato%s",
                   v.bpm, v.notesPerBar, v.swingKnown ? (v.swing < 0.2 ? "straight" : (v.swing < 0.6 ? "light swing" : "swung")) : "feel n/a",
                   (int) std::lround (v.legato * 100.0), v.pitched ? "" : "  |  rhythm only");
    return b;
}

} // namespace enochsolo
