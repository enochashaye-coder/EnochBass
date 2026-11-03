#pragma once
// v3.6 output section of the built-in sound (identical in Enoch Solo and Enoch Bass):
// master volume -> optional compressor (soft knee) -> Safe Output limiter (1.5 ms look-ahead, hard ceiling) -> meter.
// Everything is allocated in prepare(); process() runs on the audio thread without allocating or locking.
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace enochsolo
{
class OutputStage
{
public:
    struct Settings
    {
        float masterDb = -3.f;
        bool compOn = false;
        float threshDb = -18.f, ratio = 3.f, attackMs = 10.f, releaseMs = 150.f, kneeDb = 6.f, makeupDb = 0.f;
        bool limOn = true;                       // "Safe Output"
        float ceilingDb = -1.f, limReleaseMs = 80.f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0 ? sampleRate : 44100.0;
        look = std::max (1, (int) std::lround (sr * 0.0015));
        for (auto& d : delay) d.assign ((size_t) look, 0.f);
        wpos = 0; hold = 0; limGain = 1.f; compEnv = 0.f; compGainDb = 0.f; gainSmoothed = -1.f;
    }

    /** n samples of the first one or two channels */
    void process (juce::AudioBuffer<float>& b, int n, const Settings& s)
    {
        const int chans = std::min (2, b.getNumChannels());
        if (chans == 0 || n <= 0 || delay[0].empty()) return;
        float* L = b.getWritePointer (0);
        float* R = chans > 1 ? b.getWritePointer (1) : nullptr;
        const float target = juce::Decibels::decibelsToGain (s.masterDb, -100.f);
        if (gainSmoothed < 0.f) gainSmoothed = target;
        const float gStep = std::exp (-1.f / (float) (0.02 * sr));          // 20 ms volume smoothing
        const float cAtt = std::exp (-1.f / (float) (std::max (0.05f, s.attackMs) * 0.001 * sr));
        const float cRel = std::exp (-1.f / (float) (std::max (1.f, s.releaseMs) * 0.001 * sr));
        const float lRel = std::exp (-1.f / (float) (std::max (1.f, s.limReleaseMs) * 0.001 * sr));
        const float ceiling = juce::Decibels::decibelsToGain (std::min (0.f, s.ceilingDb));
        const float makeup = s.compOn ? juce::Decibels::decibelsToGain (s.makeupDb) : 1.f;
        float pk[2] = { 0.f, 0.f }, worstGr = 0.f; bool over = false;
        for (int i = 0; i < n; ++i)
        {
            gainSmoothed = target + (gainSmoothed - target) * gStep;
            float l = L[i] * gainSmoothed, r = R ? R[i] * gainSmoothed : l;
            // ---- compressor (feed-forward, peak detector, soft knee)
            float grDb = 0.f;
            if (s.compOn)
            {
                const float in = std::max (std::abs (l), std::abs (r));
                compEnv = in > compEnv ? in + (compEnv - in) * cAtt : in + (compEnv - in) * cRel;
                const float lvl = juce::Decibels::gainToDecibels (compEnv, -120.f);
                const float over2 = lvl - s.threshDb, knee = std::max (0.f, s.kneeDb), slope = 1.f / std::max (1.f, s.ratio) - 1.f;
                float red = 0.f;
                if (knee > 0.f && std::abs (over2) <= knee * 0.5f) red = slope * (over2 + knee * 0.5f) * (over2 + knee * 0.5f) / (2.f * knee);
                else if (over2 > 0.f) red = slope * over2;
                compGainDb = red;
                const float g = juce::Decibels::decibelsToGain (compGainDb) * makeup;
                l *= g; r *= g; grDb = compGainDb;
            }
            // ---- Safe Output: look-ahead limiter. The gain drops as a peak enters the delay line and holds until it has left it.
            float outL = l, outR = r;
            if (s.limOn)
            {
                const float peak = std::max (std::abs (l), std::abs (r));
                const float want = peak > ceiling ? ceiling / peak : 1.f;
                if (want < limGain) { limGain = want; hold = look; }
                else if (hold > 0) --hold;
                else limGain = want + (limGain - want) * lRel;
                outL = delay[0][(size_t) wpos]; outR = delay[1][(size_t) wpos];
                delay[0][(size_t) wpos] = l; delay[1][(size_t) wpos] = r;
                wpos = (wpos + 1) % look;
                outL *= limGain; outR *= limGain;
                outL = juce::jlimit (-ceiling, ceiling, outL); outR = juce::jlimit (-ceiling, ceiling, outR);   // nothing ever passes the ceiling
                grDb += juce::Decibels::gainToDecibels (limGain, -60.f);
            }
            if (std::abs (outL) >= 1.f || std::abs (outR) >= 1.f) over = true;
            pk[0] = std::max (pk[0], std::abs (outL)); pk[1] = std::max (pk[1], std::abs (outR));
            worstGr = std::min (worstGr, grDb);
            L[i] = outL; if (R) R[i] = outR;
        }
        peak[0] = pk[0]; peak[1] = R ? pk[1] : pk[0]; gainReductionDb = worstGr; clipped = over;
    }

    float peak[2] = { 0.f, 0.f };      // of the last block, linear
    float gainReductionDb = 0.f;        // of the last block (<= 0)
    bool clipped = false;               // the last block reached 0 dBFS

private:
    double sr = 44100.0;
    int look = 66, wpos = 0, hold = 0;
    std::vector<float> delay[2];
    float limGain = 1.f, compEnv = 0.f, compGainDb = 0.f, gainSmoothed = -1.f;
};
} // namespace enochsolo
