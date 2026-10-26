// Renders generated patterns through the TestPlayer (JUCE audio_basics only) and sanity-checks the audio.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdio>
#include <cmath>
#include <fstream>
#include "../Source/TestPlayer.h"
#include "../Source/Engine/MidiWriter.h"
using namespace enochsolo;

static void writeWav (const char* fn, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    std::ofstream f (fn, std::ios::binary); const uint32_t n = (uint32_t) l.size(), dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); }; auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVEfmt ", 8); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i) { int16_t a = (int16_t) std::lround (std::clamp (l[i], -1.f, 1.f) * 32000), b = (int16_t) std::lround (std::clamp (r[i], -1.f, 1.f) * 32000); f.write ((char*) &a, 2); f.write ((char*) &b, 2); }
}

int main (int argc, char** argv)
{
    const double sr = 44100; int bad = 0;
    for (int inst = 0; inst < I_Count; ++inst)
    {
        Settings s; s.instrument = inst; s.artist = 0; s.bars = 4; s.genre = G_Afrobeat; s.keyRoot = 9; s.scale = SC_Minor; s.seed = 77 + inst; s.bpm = 110; s.mode = 0;
        Pattern p = Generator::generate (s);
        TestPlayer tp; tp.prepare (sr); tp.setInstrument (inst, styleFor (inst, 0).variant); tp.setKeySwitches (s.ksBase, s.bendRange); tp.setLevel (0.75f, 0.25f);
        const double secPerBeat = 60.0 / s.bpm; const int total = (int) ((p.lengthBeats * secPerBeat + 2.0) * sr);
        std::vector<float> L ((size_t) total), R ((size_t) total);
        juce::AudioBuffer<float> buf (2, 512);
        size_t ei = 0; int pos = 0;
        while (pos < total)
        {
            const int n = std::min (512, total - pos);
            buf.clear();
            int done = 0;
            while (done < n)
            {
                int next = n;
                std::vector<juce::MidiMessage> now;
                while (ei < p.events.size())
                {
                    const int sp = (int) std::lround (p.events[ei].t * secPerBeat * sr) - pos;
                    if (sp >= n) break;
                    if (sp > done) { next = std::min (next, sp); break; }
                    const auto& e = p.events[ei]; const int ch = e.track + 1;
                    switch (e.type) { case EV_ON: tp.handle (juce::MidiMessage::noteOn (ch, e.a, (juce::uint8) e.b)); break;
                                      case EV_OFF: tp.handle (juce::MidiMessage::noteOff (ch, e.a)); break;
                                      case EV_CC: tp.handle (juce::MidiMessage::controllerEvent (ch, e.a, e.b)); break;
                                      default: tp.handle (juce::MidiMessage::pitchWheel (ch, e.a)); }
                    ++ei;
                }
                tp.render (buf, done, next - done); done = next;
            }
            for (int i = 0; i < n; ++i) { L[(size_t) (pos + i)] = buf.getSample (0, i); R[(size_t) (pos + i)] = buf.getSample (1, i); }
            pos += n;
        }
        double peak = 0, rms = 0; bool nan = false; int silentSecs = 0;
        for (int i = 0; i < total; ++i) { const double v = L[(size_t) i]; if (! std::isfinite (v)) nan = true; peak = std::max (peak, std::abs (v)); rms += v * v; }
        rms = std::sqrt (rms / total);
        const double musicSecs = p.lengthBeats * secPerBeat;
        for (int sec = 0; sec < (int) musicSecs; ++sec) { double e = 0; for (int i = sec * (int) sr; i < (sec + 1) * (int) sr && i < total; ++i) e += L[(size_t) i] * L[(size_t) i]; if (e < 1e-6) ++silentSecs; }
        std::printf ("%-8s peak %.3f rms %.4f nan=%d silentSecs=%d/%d notes=%zu\n", instName (inst), peak, rms, (int) nan, silentSecs, (int) musicSecs, p.notes.size());
        if (nan || peak < 0.02 || peak > 1.0) ++bad;
        if (argc > 1) { char fn[512]; std::snprintf (fn, sizeof fn, "%s/%s.wav", argv[1], instName (inst)); writeWav (fn, L, R, (int) sr); }
    }
    std::printf (bad ? "AUDIO CHECK FAILED\n" : "audio check ok\n");
    return bad;
}
