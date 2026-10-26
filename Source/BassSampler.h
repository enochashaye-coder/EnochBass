#pragma once
// Enoch Bass - one-shot bass sampler for the built-in test player.
// Load any single bass note (WAV / AIFF / FLAC / MP3). The root pitch is found automatically (or set by hand), and the note is replayed
// at every pitch the bass line needs, with velocity, short / ghost / palm-muted notes, slides and hammer-ons played legato.
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>
#include "Engine/Transcribe.h"

namespace enochsolo
{
struct BassSample
{
    std::vector<float> data;      // mono
    double rate = 44100.0;
    int rootNote = 40;            // MIDI note the recording sounds at
    bool autoRoot = true;
    juce::String name;
    double seconds() const { return rate > 0 ? (double) data.size() / rate : 0.0; }
};

/** reads a one-shot from disk and works out its pitch */
inline std::shared_ptr<BassSample> loadBassSample (const juce::File& f, juce::String& message)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (rd == nullptr) { message = "Could not open that audio file (use WAV, AIFF, FLAC or MP3)."; return nullptr; }
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 20.0));
    if (n < 400) { message = "That sample is too short."; return nullptr; }
    const int chans = (int) std::min<unsigned> (2u, rd->numChannels);
    juce::AudioBuffer<float> buf (chans, n);
    rd->read (&buf, 0, n, 0, true, chans > 1);
    auto s = std::make_shared<BassSample>();
    s->rate = rd->sampleRate; s->name = f.getFileNameWithoutExtension();
    s->data.assign ((size_t) n, 0.f);
    for (int c = 0; c < chans; ++c) { const float* p = buf.getReadPointer (c); for (int i = 0; i < n; ++i) s->data[(size_t) i] += p[i] / (float) chans; }
    // trim leading silence so the note speaks at once
    float pk = 0; for (float v : s->data) pk = std::max (pk, std::abs (v));
    if (pk < 1.0e-4f) { message = "That sample is silent."; return nullptr; }
    size_t start = 0; while (start + 1 < s->data.size() && std::abs (s->data[start]) < 0.02f * pk) ++start;
    start = start > 40 ? start - 40 : 0;
    if (start > 0) s->data.erase (s->data.begin(), s->data.begin() + (long) start);
    // root pitch: reuse the bass transcriber on a padded copy of the sample
    std::vector<float> pad ((size_t) (s->rate * 0.6), 0.f); pad.insert (pad.end(), s->data.begin(), s->data.end());
    pad.resize (pad.size() + (size_t) (s->rate * 0.3), 0.f);
    TranscribeOptions o; o.bpm = 120.0; o.tidy = 0; o.maxSeconds = 25.0;
    const TranscribeResult r = transcribeBass (pad, s->rate, o);
    int root = 40;
    if (r.ok && ! r.notes.empty())
    {
        std::vector<int> ps; for (auto& t : r.notes) if (t.art != A_Ghost) ps.push_back (t.pitch);
        if (! ps.empty()) { std::sort (ps.begin(), ps.end()); root = ps[ps.size() / 2]; }
    }
    s->rootNote = std::clamp (root, 12, 84);
    message = "Loaded " + s->name + " (" + juce::String (s->seconds(), 1) + " s), root note " + juce::String (s->rootNote);
    return s;
}

class BassSampler
{
public:
    void prepare (double sr) { sampleRate = sr; allNotesOff(); }
    void setSample (std::shared_ptr<const BassSample> s) { std::atomic_store (&sample, std::move (s)); }
    std::shared_ptr<const BassSample> getSample() const { return std::atomic_load (&sample); }
    bool isLoaded() const { return std::atomic_load (&sample) != nullptr; }
    void setRootOverride (int note) { rootOverride.store (note); }       // 0 = automatic
    void setBendRange (float semis) { bendRange = semis; }

    void noteOn (int ch, int note, int vel, int art)
    {
        auto smp = std::atomic_load (&sample);
        if (! smp) return;
        const bool legato = (art == A_Legato || art == A_HammerOn || art == A_PullOff || art == A_Slide);
        if (legato)
            for (auto& v : voices)
                if (v.active && v.ch == ch && v.held && ! v.releasing)
                {
                    v.note = note; v.vel = vel;
                    v.glideCoef = 1.0f - std::exp (-1.0f / ((art == A_Slide ? 0.07f : 0.012f) * (float) sampleRate));
                    return;
                }
        for (auto& v : voices) if (v.active && v.ch == ch && v.held) { v.held = false; startRelease (v, 0.03f); }    // a bass plays one note at a time
        Voice* slot = nullptr;
        for (auto& v : voices) if (! v.active) { slot = &v; break; }
        if (! slot) { long long oldest = -1; for (auto& v : voices) if (v.age > oldest) { oldest = v.age; slot = &v; } }
        Voice& v = *slot;
        v = Voice();
        v.active = true; v.held = true; v.ch = ch; v.note = note; v.vel = vel; v.art = art; v.smp = smp; v.curSemis = (float) note;
        v.gain = std::pow ((float) vel / 127.0f, 1.2f) * (art == A_Ghost ? 0.55f : art == A_PalmMute ? 0.85f : 1.0f);
        v.lp = (art == A_Ghost) ? 0.12f : (art == A_PalmMute ? 0.25f : 1.0f);          // dull, short notes
        if (art == A_Ghost) v.maxAge = (long long) (0.09 * sampleRate);
        else if (art == A_Staccato) v.maxAge = (long long) (0.16 * sampleRate);
        else if (art == A_PalmMute) v.maxAge = (long long) (0.22 * sampleRate);
        else v.maxAge = -1;
        v.pos = 0;
    }

    void noteOff (int ch, int note)
    {
        for (auto& v : voices)
            if (v.active && v.ch == ch && v.note == note && v.held) { v.held = false; startRelease (v, 0.09f); }
    }
    void setBend (int ch, float semis) { if (ch >= 0 && ch < 16) bend[(size_t) ch] = semis; }
    void allNotesOff() { for (auto& v : voices) if (v.active) { v.held = false; startRelease (v, 0.02f); } }
    void reset() { for (auto& v : voices) v = Voice(); bend.fill (0.f); }

    void renderAdd (float* L, float* R, int num, float gain)
    {
        const float sr = (float) sampleRate;
        for (auto& v : voices)
        {
            if (! v.active || ! v.smp) continue;
            const auto& d = v.smp->data; const long long n = (long long) d.size();
            const int rootN = rootOverride.load() > 0 ? rootOverride.load() : v.smp->rootNote;
            for (int i = 0; i < num; ++i)
            {
                v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
                const float semis = v.curSemis + bend[(size_t) v.ch];
                const double ratio = std::exp2 ((double) (semis - (float) rootN) / 12.0) * v.smp->rate / (double) sr;
                const long long i0 = (long long) v.pos;
                if (i0 + 2 >= n) { v.active = false; break; }
                const float fr = (float) (v.pos - (double) i0);
                const float y0 = i0 > 0 ? d[(size_t) (i0 - 1)] : d[(size_t) i0], y1 = d[(size_t) i0], y2 = d[(size_t) (i0 + 1)], y3 = d[(size_t) (i0 + 2)];
                const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                float x = ((c3 * fr + c2) * fr + c1) * fr + y1;
                v.z += (x - v.z) * v.lp; x = v.lp < 0.99f ? v.z : x;
                v.pos += ratio;
                if (v.releasing) { v.env *= v.relCoef; if (v.env < 0.0003f) { v.active = false; break; } }
                else if (v.maxAge > 0 && v.age >= v.maxAge) startRelease (v, 0.02f);
                ++v.age;
                const float out = x * v.gain * v.env * gain;
                L[i] += out; if (R) R[i] += out;
            }
        }
    }

private:
    struct Voice
    {
        bool active = false, held = false, releasing = false;
        int ch = 0, note = 40, vel = 90, art = 0;
        long long age = 0, maxAge = -1;
        double pos = 0; float curSemis = 40.f, glideCoef = 1.f, env = 1.f, relCoef = 0.999f, gain = 1.f, lp = 1.f, z = 0.f;
        std::shared_ptr<const BassSample> smp;
    };
    void startRelease (Voice& v, float seconds)
    {
        v.releasing = true; v.relCoef = std::exp (-1.0f / (seconds * 0.25f * (float) sampleRate));
    }
    double sampleRate = 44100.0;
    float bendRange = 2.f;
    std::array<Voice, 12> voices;
    std::array<float, 16> bend {};
    std::shared_ptr<const BassSample> sample;
    std::atomic<int> rootOverride { 0 };
};
}
