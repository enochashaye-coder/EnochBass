// Accuracy test for the bass transcriber: synthetic bass recordings made from known notes, transcribed, and scored against the truth.
#include "../Source/Engine/MidiWriter.h"
#include "../Source/Engine/Transcribe.h"
#include <cstdio>
#include <random>
#include <cstdlib>
using namespace enochsolo;

struct SN { double t0, t1; int pitch; int art; int vel; };      // seconds
enum Timbre { T_Pluck = 0, T_Synth, T_WeakFund, T_Bright, T_Count };
static const char* timbreName[] = { "finger pluck", "sub synth", "weak fundamental", "bright / slap-like" };

struct Cfg { Timbre timbre = T_Pluck; double detuneCents = 0; double noiseDb = -60; double jitterMs = 0; bool reverb = false; bool drums = false; double sr = 44100; };

static std::vector<float> render (const std::vector<SN>& notes, double lengthSec, const Cfg& c, uint64_t seed)
{
    std::mt19937_64 rng (seed); std::normal_distribution<double> nd (0, 1); std::uniform_real_distribution<double> ud (0, 1);
    const double sr = c.sr; const size_t N = (size_t) (lengthSec * sr);
    std::vector<float> y (N, 0.f);
    const double detune = std::pow (2.0, c.detuneCents / 1200.0);
    double phase[16] = { 0 };
    double curF = 0, envState = 0;
    double lastT1 = -1;
    for (size_t ni = 0; ni < notes.size(); ++ni)
    {
        const SN& n = notes[ni];
        const double t0 = n.t0 + nd (rng) * c.jitterMs * 0.001, t1 = n.t1 + nd (rng) * c.jitterMs * 0.001;
        const double amp = 0.15 + 0.7 * ((double) n.vel / 127.0);
        const size_t i0 = (size_t) std::max (0.0, t0 * sr), i1 = std::min (N, (size_t) std::max (0.0, t1 * sr));
        if (n.art == A_Ghost)
        {
            // dead note: low-passed noise thump, no pitch
            double lp = 0, lp2 = 0;
            for (size_t i = i0; i < std::min (N, i0 + (size_t) (0.09 * sr)); ++i)
            {
                const double t = (double) (i - i0) / sr;
                lp += (nd (rng) - lp) * 0.04; lp2 += (lp - lp2) * 0.2;
                y[i] += (float) (lp2 * 9.0 * 0.5 * std::exp (-t * 38.0) * amp);
            }
            continue;
        }
        const bool legato = (n.art == A_HammerOn || n.art == A_PullOff || n.art == A_Slide) && lastT1 > 0 && t0 - lastT1 < 0.03;
        const double f1 = 440.0 * std::pow (2.0, (n.pitch - 69) / 12.0) * detune;
        if (! legato) { envState = 0; for (auto& p : phase) p = 0; curF = f1; }
        const double glide = (n.art == A_Slide && legato) ? 0.14 : 0.0;
        const double fStart = curF;
        for (size_t i = i0; i < i1; ++i)
        {
            const double t = (double) (i - i0) / sr;
            double f = f1;
            if (glide > 0 && t < glide) { const double u = t / glide; f = fStart * std::pow (f1 / fStart, u * u * (3 - 2 * u)); }
            curF = f;
            double s = 0;
            const double te = legato ? envState + t : t;      // legato notes continue the envelope of the string
            for (int h = 1; h <= 14; ++h)
            {
                if (f * h > sr * 0.45) break;
                double a, dec;
                switch (c.timbre)
                {
                    case T_Pluck:     a = 1.0 / std::pow (h, 1.1); dec = 1.2 + 0.9 * h; break;
                    case T_Synth:     a = (h == 1 ? 1.0 : (h == 2 ? 0.8 : 0.5 / h)); dec = 0.5 + 0.1 * h; break;
                    case T_WeakFund:  a = (h == 1 ? 0.22 : (h == 2 ? 1.0 : (h == 3 ? 0.8 : 0.6 / h))); dec = 1.0 + 0.6 * h; break;
                    default:          a = 1.0 / std::pow (h, 0.7); dec = 2.5 + 1.2 * h; break;
                }
                phase[h] += 2.0 * M_PI * f * h / sr;
                s += a * std::sin (phase[h]) * std::exp (-te * dec);
            }
            double att = std::min (1.0, t / 0.004);
            if (legato && n.art != A_Slide) att = 1.0;
            double rel = 1.0; const double tr = (double) (i1 - i) / sr; if (tr < 0.02) rel = tr / 0.02;
            y[i] += (float) (s * 0.5 * amp * att * rel);
            if (! legato && t < 0.012) y[i] += (float) (nd (rng) * 0.05 * amp * std::exp (-t * 300.0));     // pick noise
        }
        envState = legato ? envState + (double) (i1 - i0) / sr : (double) (i1 - i0) / sr;
        lastT1 = t1;
    }
    if (c.drums)
        for (double t = 0; t < lengthSec; t += 0.5)       // kick thumps at 55 Hz under the bass, -12 dB
        {
            const size_t i0 = (size_t) (t * sr);
            for (size_t i = i0; i < std::min (N, i0 + (size_t) (0.15 * sr)); ++i) { const double tt = (double) (i - i0) / sr; y[i] += (float) (0.25 * std::sin (2 * M_PI * (55.0 + 70.0 * std::exp (-tt * 40.0)) * tt) * std::exp (-tt * 22.0)); }
        }
    if (c.reverb)
    {
        std::vector<float> r (y);
        const int taps[] = { 1103, 1721, 2653, 3989 }; const float g[] = { 0.35f, 0.25f, 0.18f, 0.12f };
        for (int k = 0; k < 4; ++k) for (size_t i = taps[k]; i < N; ++i) r[i] += g[k] * y[i - (size_t) taps[k]];
        y = r;
    }
    const double nz = std::pow (10.0, c.noiseDb / 20.0);
    for (auto& v : y) v += (float) (nd (rng) * nz);
    return y;
}

struct Score { int truth = 0, found = 0, hit = 0, pitchOk = 0, octave = 0; double errMs = 0; int artTruth[40] = { 0 }, artOk[40] = { 0 }; };

int main (int argc, char** argv)
{
    (void) argc; (void) argv;
    struct Scenario { const char* name; Cfg cfg; };
    std::vector<Scenario> sc;
    for (int t = 0; t < T_Count; ++t) { Cfg c; c.timbre = (Timbre) t; sc.push_back ({ timbreName[t], c }); }
    { Cfg c; c.detuneCents = 28; c.jitterMs = 9; c.noiseDb = -50; sc.push_back ({ "pluck, +28 cents detuned, loose timing, noise", c }); }
    { Cfg c; c.reverb = true; c.noiseDb = -45; sc.push_back ({ "pluck with room reverb and noise", c }); }
    { Cfg c; c.drums = true; c.noiseDb = -50; sc.push_back ({ "pluck with kick drum bleed", c }); }
    { Cfg c; c.timbre = T_WeakFund; c.detuneCents = -18; c.jitterMs = 7; c.noiseDb = -48; c.sr = 48000; sc.push_back ({ "weak fundamental, 48 kHz, detuned, noise", c }); }

    if (std::getenv ("TONE")) { const int k = std::getenv ("TSC") ? std::atoi (std::getenv ("TSC")) : 0; sc = { sc[(size_t) k] }; }
    int worstIdx = -1; double worstF1 = 2;
    for (size_t si = 0; si < sc.size(); ++si)
    {
        Score S; double beatExact = 0, beatTot = 0;
        const int nSeeds = std::getenv ("TFAST") ? 3 : 8;
        for (int seed = 1; seed <= nSeeds; ++seed)
        {
            Settings s; s.instrument = I_Bass; s.artist = (seed * 5) % 24; s.genre = (seed * 3) % 12; s.seed = (uint64_t) seed * 17; s.bars = 8; s.keyRoot = (seed * 7) % 12; s.scale = seed % 2 ? SC_Minor : SC_Major; s.humanize = false; s.harmony = 1;
            const Pattern p = Generator::generate (s);
            const double bpm = 84.0 + 9.0 * seed;
            std::vector<SN> truth;
            for (auto& n : p.raw)
            {
                SN t; t.t0 = n.start * 60.0 / bpm; t.t1 = (n.start + n.length) * 60.0 / bpm; t.pitch = n.pitch; t.vel = n.vel;
                t.art = (n.art == A_Ghost) ? A_Ghost : (n.art == A_Slide || n.art == A_HammerOn || n.art == A_PullOff) ? n.art : A_Sustain;
                truth.push_back (t);
            }
            // slides / hammer-ons only make sense joined to a previous note
            for (size_t i = 0; i < truth.size(); ++i)
                if (truth[i].art == A_Slide || truth[i].art == A_HammerOn || truth[i].art == A_PullOff)
                {
                    if (i == 0 || truth[i - 1].art == A_Ghost || std::abs (truth[i].pitch - truth[i - 1].pitch) < 1 || truth[i].t0 - truth[i - 1].t1 > 0.03 || truth[i].t0 - truth[i - 1].t0 > 0.7) truth[i].art = A_Sustain;
                    else { truth[i].art = (truth[i].art == A_Slide) ? A_Slide : (truth[i].pitch > truth[i - 1].pitch ? A_HammerOn : A_PullOff); truth[i - 1].t1 = truth[i].t0; }
                }
            const double len = p.lengthBeats * 60.0 / bpm + 1.0;
            const auto audio = render (truth, len, sc[si].cfg, 1000 + (uint64_t) seed);
            TranscribeOptions o; o.bpm = bpm; o.tidy = 0;
            const auto r = transcribeBass (audio, sc[si].cfg.sr, o);
            if (! r.ok) { std::printf ("  seed %d failed: %s\n", seed, r.error.c_str()); S.truth += (int) truth.size(); continue; }
            if (std::getenv ("TDBG") && (si == 0 && std::getenv ("TONE") ? true : si == 0) && seed == (std::getenv ("TSEED") ? std::atoi (std::getenv ("TSEED")) : 1))
            {
                std::printf ("TRUTH\n"); for (auto& t : truth) std::printf ("  %.3f-%.3f p%d art%d\n", t.t0, t.t1, t.pitch, t.art);
                std::printf ("FOUND\n"); for (auto& m : r.notes) std::printf ("  %.3f-%.3f p%d art%d v%d c%.2f\n", m.t0, m.t1, m.pitch, m.art, m.vel, m.conf);
            }
            std::vector<char> used (r.notes.size(), 0);
            for (auto& t : truth)
            {
                ++S.truth; ++S.artTruth[t.art];
                int best = -1; double bd = 0.065;
                for (size_t k = 0; k < r.notes.size(); ++k) { if (used[k]) continue; const double d = std::abs (r.notes[k].t0 - t.t0); if (d < bd) { bd = d; best = (int) k; } }
                if (best < 0) continue;
                used[(size_t) best] = 1; ++S.hit; S.errMs += bd * 1000.0;
                const auto& m = r.notes[(size_t) best];
                if (t.art == A_Ghost) { ++S.pitchOk; if (m.art == A_Ghost) ++S.artOk[A_Ghost]; continue; }
                if (m.pitch == t.pitch) ++S.pitchOk; else if (std::abs (m.pitch - t.pitch) % 12 == 0) ++S.octave;
                if (t.art == A_Slide || t.art == A_HammerOn || t.art == A_PullOff) { if (m.art == t.art || ((t.art == A_HammerOn || t.art == A_PullOff) && (m.art == A_HammerOn || m.art == A_PullOff))) ++S.artOk[t.art]; }
            }
            S.found += (int) r.notes.size();
            // beat-exact check with the tidy option: the source notes sit on the 16th grid
            TranscribeOptions o2 = o; o2.tidy = 2;
            const auto r2 = transcribeBass (audio, sc[si].cfg.sr, o2);
            if (r2.ok)
                for (auto& t : truth)
                {
                    const double tb = t.t0 * bpm / 60.0; beatTot += 1;
                    for (auto& m : r2.notes) if (std::abs (m.beat - tb) < 0.02) { beatExact += 1; break; }
                }
        }
        const double prec = S.found ? (double) S.hit / S.found : 0, rec = S.truth ? (double) S.hit / S.truth : 0, f1 = (prec + rec) > 0 ? 2 * prec * rec / (prec + rec) : 0;
        std::printf ("%-52s  notes %4d  found %4d  F1 %.3f  (precision %.3f recall %.3f)  pitch right %.1f%%  octave slips %d  onset error %.1f ms  on-grid after tidy %.1f%%\n",
                     sc[si].name, S.truth, S.found, f1, prec, rec, S.hit ? 100.0 * S.pitchOk / S.hit : 0.0, S.octave, S.hit ? S.errMs / S.hit : 0.0, beatTot ? 100.0 * beatExact / beatTot : 0.0);
        std::printf ("%-52s  ghost %d/%d  slide %d/%d  hammer %d/%d  pull %d/%d\n", "", S.artOk[A_Ghost], S.artTruth[A_Ghost], S.artOk[A_Slide], S.artTruth[A_Slide], S.artOk[A_HammerOn], S.artTruth[A_HammerOn], S.artOk[A_PullOff], S.artTruth[A_PullOff]);
        if (f1 < worstF1) { worstF1 = f1; worstIdx = (int) si; }
    }
    std::printf ("worst scenario: %s (F1 %.3f)\n", worstIdx >= 0 ? sc[(size_t) worstIdx].name : "-", worstF1);
    return 0;
}
