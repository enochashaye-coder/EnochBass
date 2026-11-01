// Enoch Bass - built-in test player.
// A small physically-inspired synth so the generated MIDI can be auditioned straight away with a sound that
// resembles the chosen instrument (plucked string, hammered piano strings, reed, bowed string, brass lip-buzz)
// and a soft pad for the chord-pad track (MIDI channel 3).
// It is not meant to replace a real sample library: drag the MIDI into your own instrument.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include "Engine/Styles.h"
#include "SoundFontPlayer.h"
#include "BassSampler.h"
#include <array>
#include "Engine/Libraries.h"
#include "Sounds.h"

class TestPlayer
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& f : sfSlot) f->prepare (sr);
        smp.prepare (sr);
        tailL.assign (kFade, 0.f); tailR.assign (kFade, 0.f); tailLeft = 0;
        eng.prepare (sr); engKey = -1; syncEngine();
        reverb.setSampleRate (sr);
        juce::Reverb::Parameters rp; rp.roomSize = 0.45f; rp.damping = 0.5f; rp.wetLevel = 0.f; rp.dryLevel = 1.f; rp.width = 1.f;
        reverb.setParameters (rp);
        reset();
    }

    //------------------------------------------------------------------ sound sources (one set per instrument)
    // An instrument never inherits another instrument's one-shot or SoundFont: every instrument has its own slot, and an instrument
    // without a custom sound always plays the built-in engine.
    static constexpr int kSlots = 6, kFade = 288;
    enum Src { S_Builtin = 0, S_OneShot, S_SoundFont, S_External };
    enum Warn { W_None = 0, W_NoOneShot, W_NoSoundFont, W_NoProgram };
    struct Config
    {
        int inst = 0, sound = 0, src = S_Builtin, sfPreset = -1, sax = 0;
        uint32_t smpGen = 0, sfGen = 0;
        bool operator== (const Config& o) const { return inst == o.inst && sound == o.sound && src == o.src && sfPreset == o.sfPreset && sax == o.sax && smpGen == o.smpGen && sfGen == o.sfGen; }
        bool operator!= (const Config& o) const { return ! (*this == o); }
    };
    std::array<std::unique_ptr<SoundFontPlayer>, kSlots> sfSlot = [] { std::array<std::unique_ptr<SoundFontPlayer>, kSlots> a; for (auto& f : a) f = std::make_unique<SoundFontPlayer>(); return a; }();
    std::array<std::shared_ptr<const enochsolo::BassSample>, kSlots> smpSlot;           // written on the message thread with std::atomic_store
    enochsolo::BassSampler smp;                                                            // plays the active instrument's one-shot
    void setSlotSample (int slot, std::shared_ptr<const enochsolo::BassSample> sm) { std::atomic_store (&smpSlot[(size_t) juce::jlimit (0, kSlots - 1, slot)], std::move (sm)); }
    std::shared_ptr<const enochsolo::BassSample> slotSample (int slot) const { return std::atomic_load (&smpSlot[(size_t) juce::jlimit (0, kSlots - 1, slot)]); }
    SoundFontPlayer& slotSF (int slot) { return *sfSlot[(size_t) juce::jlimit (0, kSlots - 1, slot)]; }

    /** what the processor wants (audio thread, every block) */
    void setDesired (const Config& c) { desired = c; }
    /** audio thread, once per block before any MIDI: when the instrument / sound / source changed, the old sound fades out over 6 ms,
        every voice, effect tail, filter, sample voice, SoundFont channel and the pedal are flushed, and only then is the new sound set up */
    void update()
    {
        if (desired == active && effective == resolveSource (active)) return;
        // render a short faded tail of whatever was sounding, so the switch never clicks
        if (tailLeft <= 0 && configured)
        {
            std::fill (tailL.begin(), tailL.end(), 0.f); std::fill (tailR.begin(), tailR.end(), 0.f);
            renderInto (tailL.data(), tailR.data(), kFade);
            for (int i = 0; i < kFade; ++i) { const float g = 1.f - (float) i / (float) kFade; tailL[(size_t) i] *= g * g; tailR[(size_t) i] *= g * g; }
            tailLeft = kFade;
        }
        flush();
        apply (desired);
        switches.fetch_add (1);
    }
    // ---- read back by the editor's NOW PLAYING display (what the audio thread is really using)
    std::atomic<int> nowInst { -1 }, nowSound { -1 }, nowReqSrc { 0 }, nowEffSrc { 0 }, nowWarn { 0 }, nowSfPreset { -1 }, nowAlto { 0 };
    std::atomic<uint32_t> switches { 0 };
    int effectiveSource() const { return effective; }

    static int gmProgram (int inst, bool alto)
    {
        switch (inst) { case enochsolo::I_Bass: return 33; case enochsolo::I_Guitar: return 27; case enochsolo::I_Sax: return alto ? 65 : 66; case enochsolo::I_Violin: return 40;
                        case enochsolo::I_Piano: return 0; default: return 56; }
    }
    void setInstrument (int inst, int sax) { instrument = inst; saxAlto = (sax == 1); curSF().setProgram (gmProgram (inst, saxAlto)); syncEngine(); }
    /** which built-in sound (index into enochsolo::snd::patchList (instrument)) the test player uses */
    void setSound (int idx) { soundIdx = idx; syncEngine(); }
    enochsolo::snd::Engine eng;                   // the modelled instruments (grand pianos, e-pianos, B3, Prophet, Strat / Tele / Les Paul, saxes, basses ...)
    void setKeySwitches (int base, int bendRangeSemis) { ksBase = base; bendRange = bendRangeSemis; for (auto& f : sfSlot) f->setBendRange ((float) bendRangeSemis); }
    /** which notes are key-switches (and what they switch) for the chosen instrument of choice; call from the audio thread only */
    void configureKeySwitches (const enochsolo::Library* lib, int inst, int base)
    {
        ksArt.fill (-2);
        if (lib && lib->inst == inst && ! lib->standard)
        {
            if (lib->noKeySwitches) return;
            for (auto& a : lib->arts)
                for (size_t i = 0; i < a.press.size(); ++i)
                {
                    const int n = a.press[i].note; if (n < 0 || n > 127) continue;
                    ksArt[(size_t) n] = (i == 0 && ! a.oneShot && ! a.hold && ksArt[(size_t) n] < 0) ? a.art : std::max (ksArt[(size_t) n], -1);
                }
            return;
        }
        const auto& list = enochsolo::artList (inst);
        for (size_t i = 0; i < list.size(); ++i) if (base + (int) i < 128) ksArt[(size_t) (base + (int) i)] = list[i];
    }
    void setLevel (float gain, float reverbAmt)
    {
        level = gain;
        auto p = reverb.getParameters(); p.wetLevel = reverbAmt * 0.45f; p.dryLevel = 1.0f - reverbAmt * 0.2f;
        p.roomSize = roomSize; p.damping = roomDamp; p.width = roomWidth;
        reverb.setParameters (p);
    }
    /** v2.7 room type for the preview reverb: 0 classic (as before), 1 studio, 2 small room, 3 club, 4 hall, 5 plate, 6 church */
    void setRoom (int type)
    {
        static const float sz[] = { 0.45f, 0.30f, 0.22f, 0.55f, 0.82f, 0.62f, 0.95f }, dp[] = { 0.5f, 0.65f, 0.40f, 0.45f, 0.30f, 0.12f, 0.25f }, wd[] = { 1.f, 0.8f, 0.6f, 1.f, 1.f, 1.f, 1.f };
        type = juce::jlimit (0, 6, type);
        roomSize = sz[type]; roomDamp = dp[type]; roomWidth = wd[type];
    }
    void setMonoOut (bool m) { monoOut = m; }

    void reset()
    {
        for (auto& v : voices) v.clear();
        for (int c = 0; c < 16; ++c) { bend[(size_t) c] = 0; mod[(size_t) c] = 0; aftertouch[(size_t) c] = 0; expr[(size_t) c] = exprTarget[(size_t) c] = 100.f / 127.f; pedal[(size_t) c] = false; art[(size_t) c] = enochsolo::A_Sustain; }
        reverb.reset();
        for (auto& f : sfSlot) f->reset();
        smp.reset(); eng.reset();
    }

    void handle (const juce::MidiMessage& m)
    {
        const int ch = juce::jlimit (0, 15, m.getChannel() - 1);
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            if (n >= 0 && n < 128 && ksArt[(size_t) n] != -2) { if (ksArt[(size_t) n] >= 0) art[(size_t) ch] = ksArt[(size_t) n]; return; }   // key-switch
            if (effective == S_External) return;
            if (sampleActive()) smp.noteOn (ch, n, m.getVelocity(), art[(size_t) ch]); else if (sfActive()) curSF().noteOn (ch, n, m.getVelocity()); else if (engFor (ch)) eng.noteOn (ch, n, m.getVelocity(), art[(size_t) ch]); else noteOn (ch, n, m.getVelocity());
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            if (n >= 0 && n < 128 && ksArt[(size_t) n] != -2) return;
            if (sampleActive()) smp.noteOff (ch, n); else if (sfActive()) curSF().noteOff (ch, n); else { eng.noteOff (ch, n); noteOff (ch, n); }
        }
        else if (m.isPitchWheel())
            { bend[(size_t) ch] = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * (float) bendRange; smp.setBend (ch, bend[(size_t) ch]); eng.setBend (ch, bend[(size_t) ch]); if (sfActive()) curSF().pitchWheel (ch, m.getPitchWheelValue()); }
        else if (m.isChannelPressure())
            { aftertouch[(size_t) ch] = (float) m.getChannelPressureValue() / 127.0f; eng.setAftertouch (ch, aftertouch[(size_t) ch]); }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if (sfActive() && cc != 123 && cc != 120) curSF().controller (ch, cc, v);
            if (cc == 1)  { mod[(size_t) ch] = (float) v / 127.0f; eng.setMod (ch, mod[(size_t) ch]); }
            if (cc == 11) { exprTarget[(size_t) ch] = (float) v / 127.0f; eng.setExpression (ch, exprTarget[(size_t) ch]); }
            if (cc == 64)
            {
                const bool down = v >= 64;
                pedal[(size_t) ch] = down; eng.setPedal (ch, down);
                if (! down) for (auto& vc : voices) if (vc.active && vc.ch == ch && vc.heldByPedal) { vc.heldByPedal = false; release (vc); }
            }
            if (cc == 123 || cc == 120) allNotesOff();
        }
    }

    /** stops everything. The sustain pedal is lifted too: before v2.5 a pedal-down that lost its pedal-up (stop, loop jump or a
        re-generated pattern while playing) kept every later piano note ringing - the "unwanted sustain" bug. */
    void allNotesOff()
    {
        pedal.fill (false);
        for (auto& v : voices) if (v.active) { v.held = false; v.heldByPedal = false; release (v); }
        for (auto& f : sfSlot) f->allNotesOff();
        smp.allNotesOff(); eng.allNotesOff();
    }

    void render (juce::AudioBuffer<float>& buf, int start, int num)
    {
        auto* L = buf.getWritePointer (0, start);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1, start) : nullptr;
        renderInto (L, R, num);
        // the faded tail of the sound that was playing before a switch
        for (int i = 0; i < num && tailLeft > 0; ++i, --tailLeft)
        {
            const size_t k = (size_t) (kFade - tailLeft);
            L[i] += tailL[k]; if (R) R[i] += tailR[k];
        }
        if (monoOut && R) for (int i = 0; i < num; ++i) { const float m = 0.5f * (L[i] + R[i]); L[i] = R[i] = m; }
    }

    void renderInto (float* L, float* R, int num)
    {
        if (effective == S_External)
        {
            for (int i = 0; i < num; ++i) { L[i] = 0.f; if (R) R[i] = 0.f; }
            return;
        }
        if (sampleActive())
        {
            for (int i = 0; i < num; ++i) { L[i] = 0.f; if (R) R[i] = 0.f; }
            smp.renderAdd (L, R, num, level * 0.9f);
            if (R) reverb.processStereo (L, R, num);
            for (int i = 0; i < num; ++i) { L[i] = std::tanh (L[i]); if (R) R[i] = std::tanh (R[i]); }
            return;
        }
        if (sfActive())
        {
            for (int i = 0; i < num; ++i) { L[i] = 0.f; if (R) R[i] = 0.f; }
            curSF().renderAdd (L, R, num, level * 0.6f);
            if (R) reverb.processStereo (L, R, num);
            for (int i = 0; i < num; ++i) { L[i] = std::tanh (L[i]); if (R) R[i] = std::tanh (R[i]); }
            return;
        }
        const float sr = (float) sampleRate;
        const float exprSmooth = 1.0f - std::exp (-1.0f / (0.006f * sr));
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i < num; ++i)
        {
            for (int c = 0; c < 16; ++c) expr[(size_t) c] += (exprTarget[(size_t) c] - expr[(size_t) c]) * exprSmooth;
            float outL = 0, outR = 0;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                const int c = v.ch;
                const float tsec = (float) v.age / sr;

                // ---- envelope
                float env;
                if (v.releasing)
                {
                    v.env *= v.relCoef; env = v.env;
                    if (v.env < 0.0004f) { v.active = false; continue; }
                }
                else if (v.age < v.attackN) { v.env = (float) v.age / (float) v.attackN; env = v.env; }
                else { v.env = 1.0f; env = 1.0f; }

                // ---- pitch (not for piano strings, which can't bend)
                float semis = (float) v.note + bend[(size_t) c];
                v.glide += (0.f - v.glide) * v.glideCoef;
                semis += v.glide;
                v.lfo += 5.6f / sr; if (v.lfo > 1.f) v.lfo -= 1.f;
                const float lfoV = std::sin (twoPi * v.lfo);
                const float modAmt = std::min (1.f, mod[(size_t) c] + aftertouch[(size_t) c] * 0.8f);
                const float onset = juce::jlimit (0.f, 1.f, (tsec - v.vibDelay) / 0.35f);
                semis += lfoV * (modAmt * 0.6f + v.autoVib) * onset;
                if (v.art == enochsolo::A_Trill || v.art == enochsolo::A_Shake)
                {
                    const float rate = v.art == enochsolo::A_Shake ? 7.f : 11.f;
                    if (std::fmod (tsec * rate, 1.f) > 0.5f && tsec > 0.1f) semis += (v.art == enochsolo::A_Shake ? 2.f : 1.5f);
                }
                if (v.kind == K_Sax || v.kind == K_Trumpet) semis -= v.scoop * std::exp (-tsec / 0.03f);   // lip / reed attack scoop
                const float f0 = 440.0f * std::exp2 ((semis - 69.0f) / 12.0f);
                if (f0 > 0.45f * sr || f0 < 5.f) { v.active = false; continue; }
                const float dphi = f0 / sr;

                const float e = expr[(size_t) c];
                const float velF = (float) v.vel / 127.0f;
                float sL = 0, sR = 0; bool stereo = false;

                switch (v.kind)
                {
                    //--------------------------------------------------------------- plucked string (Karplus-Strong + body)
                    case K_Pluck:
                    {
                        float d = juce::jlimit (2.f, 4000.f, sr / f0 - 0.5f);
                        const float rp = (float) v.wp - d;
                        const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
                        const float y = v.buf[(size_t) (i0 & kMask)] * (1.f - fr) + v.buf[(size_t) ((i0 + 1) & kMask)] * fr;
                        const float filt = v.damp * y + (1.f - v.damp) * v.prevY; v.prevY = y;
                        v.buf[(size_t) (v.wp & kMask)] = filt * v.loopGain; ++v.wp;
                        float o = y * 0.75f + 1.0f * v.bq[0].process (y) + 0.9f * v.bq[1].process (y) + 0.2f * v.bq[2].process (y);
                        if (v.tick > 0.f) { o += v.tick * noise(); v.tick *= v.tickDecay; }
                        const float amp = v.gain * velF * (0.45f + 0.55f * velF);
                        sL = sR = o * amp; break;
                    }
                    //--------------------------------------------------------------- piano (inharmonic partials, two detuned strings, hammer thump)
                    case K_Piano:
                    {
                        float acc = 0;
                        for (int h = 0; h < v.nPart; ++h)
                        {
                            auto& p = v.part[(size_t) h];
                            const float ns = p.s * p.cw + p.c * p.sw, nc = p.c * p.cw - p.s * p.sw;
                            p.s = ns; p.c = nc; p.a *= p.dec;
                            acc += p.s * p.a;
                        }
                        if ((v.age & 1023) == 0) for (int h = 0; h < v.nPart; ++h) { auto& p = v.part[(size_t) h]; const float m = 1.f / std::sqrt (p.s * p.s + p.c * p.c); p.s *= m; p.c *= m; }
                        float o = acc;
                        if (v.tick > 0.0001f) { v.nz += (noise() - v.nz) * 0.18f; o += v.tick * v.nz; v.tick *= v.tickDecay; }
                        o = v.bq[0].process (o) * 0.0f + o;                       // (bq[0] unused for piano)
                        sL = sR = o * v.gain;
                        const float pan = juce::jlimit (0.15f, 0.85f, 0.15f + (float) (v.note - 30) / 70.f * 0.7f);
                        sL *= (1.f - pan) * 1.4f; sR *= pan * 1.4f; stereo = true;
                        break;
                    }
                    //--------------------------------------------------------------- saxophone (reed + bore formants + breath)
                    case K_Sax:
                    {
                        v.phase += dphi; if (v.phase >= 1.f) v.phase -= 1.f;
                        const float saw = 2.f * v.phase - 1.f - polyBlep (v.phase, dphi);
                        float pw = v.phase < 0.38f ? 1.f : -1.f; pw += polyBlep (v.phase, dphi); pw -= polyBlep (std::fmod (v.phase - 0.38f + 1.f, 1.f), dphi);
                        float src = 0.65f * saw + 0.5f * pw;
                        const float bright = juce::jlimit (0.f, 1.f, e * (0.45f + velF * 0.55f)) * (v.art == enochsolo::A_Subtone ? 0.45f : 1.f);
                        const float fc = juce::jlimit (350.f, 9000.f, f0 * (2.f + 5.f * bright));
                        v.lp1 += (src - v.lp1) * (1.f - std::exp (-twoPi * fc / sr));
                        float y = v.lp1 * 0.55f + 1.15f * v.bq[0].process (v.lp1) + 0.65f * v.bq[1].process (v.lp1);
                        const float breath = (v.art == enochsolo::A_Subtone ? 0.16f : 0.05f) + 0.10f * std::exp (-tsec / 0.04f);
                        y += breath * v.bq[2].process (noise()) * (0.4f + 0.6f * e);
                        if (v.art == enochsolo::A_Growl) { y *= 0.55f + 0.45f * std::sin (twoPi * 32.f * tsec); y = std::tanh (y * 2.6f) * 0.7f; }
                        if (v.art == enochsolo::A_Flutter) y *= (std::fmod (tsec * 24.f, 1.f) < 0.5f) ? 1.f : 0.25f;
                        y = std::tanh (1.3f * y);
                        sL = sR = y * v.gain * std::pow (e, 1.4f) * (0.35f + 0.65f * velF); break;
                    }
                    //--------------------------------------------------------------- violin (bowed saw + body resonances + bow noise)
                    case K_Bowed:
                    {
                        v.phase += dphi; if (v.phase >= 1.f) v.phase -= 1.f;
                        v.phase2 += dphi * 1.0017f; if (v.phase2 >= 1.f) v.phase2 -= 1.f;
                        const float saw = 0.6f * (2.f * v.phase - 1.f - polyBlep (v.phase, dphi))
                                        + 0.4f * (2.f * v.phase2 - 1.f - polyBlep (v.phase2, dphi * 1.0017f));
                        const bool harm = (v.art == enochsolo::A_Harmonic);
                        const float bright = juce::jlimit (0.f, 1.f, e * velF * 1.1f);
                        const float fc = harm ? f0 * 1.6f : juce::jlimit (400.f, 8000.f, f0 * (3.f + 7.f * bright));
                        v.lp1 += (saw - v.lp1) * (1.f - std::exp (-twoPi * fc / sr));
                        float y = v.lp1 * 0.5f + 0.5f * v.bq[0].process (v.lp1) + 0.38f * v.bq[1].process (v.lp1) + 0.4f * v.bq[2].process (v.lp1);
                        y += (0.035f + 0.14f * std::exp (-tsec / 0.05f)) * v.bq[3].process (noise());     // rosin / bow scratch
                        if (v.art == enochsolo::A_Tremolo) y *= (std::fmod (tsec * 13.f, 1.f) < 0.55f) ? 1.f : 0.2f;
                        sL = sR = y * v.gain * std::pow (e, 1.3f) * (0.35f + 0.65f * velF); break;
                    }
                    //--------------------------------------------------------------- trumpet (lip buzz, brightness grows with level, bell formants)
                    case K_Trumpet:
                    {
                        v.phase += dphi; if (v.phase >= 1.f) v.phase -= 1.f;
                        const float src = 2.f * v.phase - 1.f - polyBlep (v.phase, dphi);
                        const float lvl = juce::jlimit (0.f, 1.f, e * (0.4f + 0.6f * velF));
                        const float fc = juce::jlimit (400.f, 11000.f, f0 * (1.4f + 15.f * lvl * lvl));
                        v.lp1 += (src - v.lp1) * (1.f - std::exp (-twoPi * fc / sr));
                        const bool mute = (v.art == enochsolo::A_Mute);
                        float y = (mute ? 0.15f : 0.5f) * v.lp1 + 0.75f * v.bq[0].process (v.lp1) + 0.45f * v.bq[1].process (v.lp1);
                        if (mute) y += 1.3f * v.bq[2].process (v.lp1);
                        y += 0.10f * std::exp (-tsec / 0.02f) * noise();
                        if (v.art == enochsolo::A_Growl) y *= 0.6f + 0.4f * std::sin (twoPi * 34.f * tsec);
                        if (v.art == enochsolo::A_Flutter) y *= (std::fmod (tsec * 22.f, 1.f) < 0.5f) ? 1.f : 0.3f;
                        y = std::tanh (1.8f * y);
                        sL = sR = y * v.gain * std::pow (e, 1.3f) * (0.35f + 0.65f * velF) * (mute ? 0.8f : 1.f); break;
                    }
                    //--------------------------------------------------------------- soft detuned pad (chord track)
                    default:
                    {
                        float acc[3];
                        const float det[3] = { 0.9988f, 1.0f, 1.0013f };
                        for (int k = 0; k < 3; ++k)
                        {
                            float& ph = k == 0 ? v.phase : (k == 1 ? v.phase2 : v.phase3);
                            const float dp = dphi * det[k];
                            ph += dp; if (ph >= 1.f) ph -= 1.f;
                            acc[k] = 2.f * ph - 1.f - polyBlep (ph, dp);
                        }
                        const float fc = juce::jlimit (300.f, 2400.f, f0 * 4.f);
                        const float kk = 1.f - std::exp (-twoPi * fc / sr);
                        v.lp1 += (acc[0] - v.lp1) * kk; v.lp2 += (acc[1] - v.lp2) * kk; v.lp3 += (acc[2] - v.lp3) * kk;
                        sL = (v.lp1 * 0.65f + v.lp2 * 0.35f) * v.gain; sR = (v.lp3 * 0.65f + v.lp2 * 0.35f) * v.gain; stereo = true;
                        break;
                    }
                }
                (void) stereo;
                // other articulations that gate the amplitude
                float artMod = 1.f;
                if (v.art == enochsolo::A_Tremolo && v.kind == K_Piano) artMod = (std::fmod (tsec * 12.f, 1.f) < 0.5f) ? 1.f : 0.3f;
                outL += sL * env * artMod * level; outR += sR * env * artMod * level;
                ++v.age;
            }
            L[i] = outL; if (R) R[i] = outR;
        }
        eng.render (L, R, num, level);
        if (R) reverb.processStereo (L, R, num);
        for (int i = 0; i < num; ++i)
        {
            L[i] = std::tanh (L[i]); if (R) R[i] = std::tanh (R[i]);
        }
    }

private:
    std::array<int, 128> ksArt = [] { std::array<int, 128> a; a.fill (-2); return a; }();   // -2 = a normal note, -1 = silent key-switch, >= 0 = switches to that articulation
    bool sampleActive() const { return effective == S_OneShot && smp.isLoaded(); }
    bool sfActive() { return effective == S_SoundFont && curSF().isLoaded(); }
    SoundFontPlayer& curSF() { return *sfSlot[(size_t) juce::jlimit (0, kSlots - 1, instrument)]; }

    Config desired, active; bool configured = false;
    int effective = S_Builtin;
    std::vector<float> tailL, tailR; int tailLeft = 0;

    /** which source can really play for this configuration (never a silent fallback to someone else's sound) */
    int resolveSource (const Config& c)
    {
        int warn = W_None, eff = S_Builtin;
        auto& f = slotSF (c.inst);
        switch (c.src)
        {
            case S_OneShot:   if (slotSample (c.inst) != nullptr) eff = S_OneShot; else warn = W_NoOneShot; break;
            case S_SoundFont:
                if (! f.isLoaded()) warn = W_NoSoundFont;
                else { f.setProgram (gmProgram (c.inst, c.sax == 1)); f.setPreset (c.sfPreset); if (f.canPlay()) eff = S_SoundFont; else warn = W_NoProgram; }
                break;
            case S_External:  eff = S_External; break;
            default: break;
        }
        lastWarn = warn;
        return eff;
    }
    int lastWarn = W_None;
    void flush()
    {
        for (auto& v : voices) v.clear();
        for (auto& f : sfSlot) f->soundsOff();
        smp.reset(); eng.reset(); reverb.reset();
        for (int c = 0; c < 16; ++c) { bend[(size_t) c] = 0; mod[(size_t) c] = 0; aftertouch[(size_t) c] = 0; expr[(size_t) c] = exprTarget[(size_t) c] = 100.f / 127.f; pedal[(size_t) c] = false; }
    }
    void apply (const Config& c)
    {
        active = c; configured = true;
        instrument = c.inst; saxAlto = (c.sax == 1); soundIdx = c.sound;
        effective = resolveSource (c);
        smp.setSample (effective == S_OneShot ? slotSample (c.inst) : nullptr);
        syncEngine();
        nowInst = c.inst; nowSound = c.sound; nowReqSrc = c.src; nowEffSrc = effective; nowWarn = lastWarn; nowSfPreset = c.sfPreset; nowAlto = c.sax == 1 ? 1 : 0;
    }
    bool engFor (int ch) const { return ch != 2 && ! eng.usesClassic(); }      // MIDI channel 3 (the chord pad) keeps its soft pad
    int soundIdx = 0, engKey = -1;
    void syncEngine()
    {
        const int key = instrument * 1000 + soundIdx * 2 + (saxAlto ? 1 : 0);
        if (key == engKey) return;
        engKey = key;
        eng.setSound (instrument, soundIdx, saxAlto);
    }
    enum Kind { K_Pluck = 0, K_Piano, K_Sax, K_Bowed, K_Trumpet, K_Pad };
    static constexpr int kBuf = 4096, kMask = kBuf - 1, kMaxPart = 34;

    struct Biquad
    {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void bandpass (float f, float q, float sr)
        {
            const float w = juce::MathConstants<float>::twoPi * juce::jlimit (20.f, 0.45f * sr, f) / sr, al = std::sin (w) / (2.f * q), a0 = 1.f + al;
            b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2.f * std::cos (w) / a0; a2 = (1.f - al) / a0; z1 = z2 = 0;
        }
        float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    struct Partial { float c = 1, s = 0, cw = 1, sw = 0, a = 0, dec = 1; };

    struct Voice
    {
        bool active = false, releasing = false, held = false, heldByPedal = false;
        int kind = K_Pluck, ch = 0, note = 60, vel = 90, art = 0;
        long long age = 0; int attackN = 100;
        float phase = 0, phase2 = 0, phase3 = 0, lfo = 0, env = 0, glide = 0, glideCoef = 0.01f, relCoef = 0.999f;
        float gain = 0.5f, autoVib = 0, vibDelay = 0.2f, scoop = 0;
        float lp1 = 0, lp2 = 0, lp3 = 0, nz = 0;
        float tick = 0, tickDecay = 0.99f;
        // plucked string
        int wp = 0; float damp = 0.9f, loopGain = 0.999f, prevY = 0;
        std::array<float, kBuf> buf {};
        // piano
        int nPart = 0; std::array<Partial, kMaxPart> part {};
        std::array<Biquad, 4> bq {};
        void clear() { active = false; releasing = false; held = false; heldByPedal = false; }
    };

    double sampleRate = 44100.0;
    int instrument = 0, ksBase = 24, bendRange = 2; bool saxAlto = false;
    float level = 0.8f;
    std::array<Voice, 24> voices;
    std::array<float, 16> bend {}, mod {}, expr {}, exprTarget {}, aftertouch {};
    std::array<bool, 16> pedal {};
    std::array<int, 16> art {};
    juce::Reverb reverb;
    float roomSize = 0.45f, roomDamp = 0.5f, roomWidth = 1.f; bool monoOut = false;
    juce::Random rnd;

    float noise() { return rnd.nextFloat() * 2.f - 1.f; }
    static float polyBlep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }

    bool isMonoChannel (int ch) const { return ch != 2 && (instrument == enochsolo::I_Sax || instrument == enochsolo::I_Trumpet); }
    static bool isLegatoArt (int a)
    {
        return a == enochsolo::A_Legato || a == enochsolo::A_HammerOn || a == enochsolo::A_PullOff || a == enochsolo::A_Slide || a == enochsolo::A_Gliss;
    }

    void release (Voice& v)
    {
        v.releasing = true;
        if (v.kind == K_Pluck) v.loopGain = std::min (v.loopGain, 0.86f);      // finger damps the string
    }

    void setupPluck (Voice& v, int note, int vel, int a, bool violin)
    {
        const float sr = (float) sampleRate;
        const float velF = (float) vel / 127.f;
        const float f0 = 440.f * std::exp2 (((float) note - 69.f) / 12.f);
        const float N = juce::jlimit (4.f, 4000.f, sr / f0);
        v.kind = K_Pluck;
        float pickPos = 0.16f, bright = 0.35f + 0.6f * velF, tau = violin ? 0.45f : 2.4f;
        v.damp = 0.80f + 0.17f * velF;
        float exciteAmp = 1.f;
        if (a == enochsolo::A_PalmMute) { v.damp = 0.55f; tau = 0.16f; pickPos = 0.22f; }
        if (a == enochsolo::A_Harmonic) { pickPos = 0.5f; v.damp = 0.97f; tau = 2.0f; }
        if (a == enochsolo::A_Ghost)    { v.damp = 0.42f; tau = 0.06f; exciteAmp = 0.6f; }
        if (a == enochsolo::A_HammerOn || a == enochsolo::A_PullOff) { exciteAmp = 0.6f; bright *= 0.7f; }
        if (a == enochsolo::A_ColLegno) { tau = 0.09f; v.tick = 0.25f; v.tickDecay = std::exp (-1.f / (0.004f * sr)); }
        v.loopGain = std::exp (-1.f / (tau * f0)); v.loopGain = juce::jlimit (0.5f, 0.99995f, v.loopGain);
        // excitation: noise burst, one-pole lowpassed for brightness, with a pick-position comb
        const int n = (int) N; const int pk = std::max (1, (int) (N * pickPos));
        std::vector<float> ex ((size_t) n); float lpS = 0; const float k = 0.06f + 0.5f * bright;
        for (int i = 0; i < n; ++i) { lpS += (noise() - lpS) * k; ex[(size_t) i] = lpS; }
        float mean = 0; for (float x : ex) mean += x; mean /= (float) n;
        for (int i = 0; i < n; ++i) { float x = ex[(size_t) i] - mean; if (i >= pk) x -= (ex[(size_t) (i - pk)] - mean); v.buf[(size_t) i] = x * exciteAmp; }
        v.wp = n; v.prevY = 0;
        if (violin) { v.bq[0].bandpass (290.f, 7.f, sr); v.bq[1].bandpass (560.f, 6.f, sr); v.bq[2].bandpass (2600.f, 1.6f, sr); v.gain = 0.7f; }
        else        { v.bq[0].bandpass (105.f, 4.f, sr); v.bq[1].bandpass (225.f, 5.f, sr);  v.bq[2].bandpass (2300.f, 1.3f, sr); v.gain = 0.8f; }
        v.attackN = 3; v.autoVib = 0.f;
    }

    void setupPiano (Voice& v, int note, int vel)
    {
        const float sr = (float) sampleRate;
        const float velF = (float) vel / 127.f;
        const float f0 = 440.f * std::exp2 (((float) note - 69.f) / 12.f);
        const float B = 0.00012f * std::exp (((float) note - 60.f) * 0.03f);
        const float tauBase = 6.f * std::pow (55.f / (float) std::max (30, note), 1.6f);
        v.kind = K_Piano; v.nPart = 0;
        float norm = 0;
        for (int h = 1; h <= 26; ++h)
        {
            const float fh = f0 * (float) h * std::sqrt (1.f + B * (float) (h * h));
            if (fh > 0.45f * sr) break;
            float a = std::pow ((float) h, -0.85f) * std::exp (-(float) (h - 1) * (0.30f - 0.20f * velF));
            a *= 0.35f + 1.2f * std::abs (std::sin (juce::MathConstants<float>::pi * (float) h * 0.12f));        // hammer strike point
            const float tau = tauBase / (1.f + 0.55f * (float) (h - 1));
            const float dec = std::exp (-1.f / (tau * sr));
            const int strings = (h <= 5) ? 2 : 1;
            for (int sIdx = 0; sIdx < strings && v.nPart < kMaxPart; ++sIdx)
            {
                auto& p = v.part[(size_t) v.nPart++];
                const float ff = fh * (sIdx == 0 ? 1.f : 1.f + 0.00045f * (1.f + 0.3f * (float) h));
                const float w = juce::MathConstants<float>::twoPi * ff / sr;
                p.c = 1.f; p.s = 0.f; p.cw = std::cos (w); p.sw = std::sin (w); p.a = a / (float) strings; p.dec = dec;
                norm += p.a;
            }
        }
        v.gain = 0.5f * (0.25f + 0.75f * velF) / std::max (1.f, norm * 0.55f);
        v.tick = 0.07f * velF; v.tickDecay = std::exp (-1.f / (0.006f * sr)); v.nz = 0.f;
        v.attackN = 2;
    }

    void setupVoice (Voice& v, int ch, int note, int vel, int a)
    {
        v = Voice();
        v.active = true; v.held = true; v.ch = ch; v.note = note; v.vel = vel; v.art = a;
        const float sr = (float) sampleRate;
        float atk = 0.01f, rel = 0.08f;
        if (ch == 2)    // chord pad
        {
            v.kind = K_Pad; v.gain = 0.16f * (0.35f + 0.65f * (float) vel / 127.f); atk = 0.25f; rel = 0.7f;
        }
        else switch (instrument)
        {
            case enochsolo::I_Piano:
                setupPiano (v, note, vel); atk = 0.002f; rel = 0.22f; break;
            case enochsolo::I_Bass:
                setupPluck (v, note, vel, a, false);
                v.bq[0].bandpass (70.f, 3.f, (float) sampleRate); v.bq[1].bandpass (150.f, 3.f, (float) sampleRate); v.bq[2].bandpass (850.f, 1.1f, (float) sampleRate); v.gain = 1.15f;
                atk = 0.002f; rel = 0.12f; break;
            case enochsolo::I_Guitar:
                setupPluck (v, note, vel, a, false); atk = 0.002f; rel = 0.14f; break;
            case enochsolo::I_Sax:
            {
                v.kind = K_Sax; v.gain = 0.5f; v.autoVib = 0.16f; v.vibDelay = 0.3f; v.scoop = isLegatoArt (a) ? 0.f : 0.45f;
                atk = (a == enochsolo::A_Marcato || a == enochsolo::A_Staccato) ? 0.012f : (isLegatoArt (a) ? 0.03f : 0.022f); rel = 0.07f;
                if (saxAlto) { v.bq[0].bandpass (900.f, 3.f, sr); v.bq[1].bandpass (1800.f, 2.f, sr); }
                else         { v.bq[0].bandpass (680.f, 3.f, sr); v.bq[1].bandpass (1350.f, 2.f, sr); }
                v.bq[2].bandpass (2400.f, 0.8f, sr);
                break;
            }
            case enochsolo::I_Trumpet:
                v.kind = K_Trumpet; v.gain = 0.34f; v.autoVib = 0.10f; v.vibDelay = 0.4f; v.scoop = 0.4f;
                atk = (a == enochsolo::A_Marcato) ? 0.01f : 0.022f; rel = 0.06f;
                v.bq[0].bandpass (1150.f, 2.5f, sr); v.bq[1].bandpass (2400.f, 2.f, sr); v.bq[2].bandpass (1500.f, 5.f, sr);
                break;
            default:   // violin
                if (a == enochsolo::A_Pizz || a == enochsolo::A_ColLegno) { setupPluck (v, note, vel, a, true); atk = 0.002f; rel = 0.09f; }
                else
                {
                    v.kind = K_Bowed; v.gain = 0.75f; v.autoVib = 0.30f; v.vibDelay = 0.22f;
                    atk = isLegatoArt (a) ? 0.045f : 0.07f; rel = 0.09f;
                    v.bq[0].bandpass (290.f, 7.f, sr); v.bq[1].bandpass (560.f, 6.f, sr); v.bq[2].bandpass (2600.f, 1.6f, sr); v.bq[3].bandpass (4200.f, 0.7f, sr);
                }
                break;
        }
        if (a == enochsolo::A_Ghost && v.kind != K_Pluck) v.vel = std::max (10, vel / 2);
        v.attackN = std::max (3, (int) (atk * sr));
        v.relCoef = std::exp (-1.0f / (rel * sr * 0.25f));
        v.glideCoef = 1.0f - std::exp (-1.0f / (0.012f * sr));
    }

    void noteOn (int ch, int note, int vel)
    {
        const int a = art[(size_t) ch];
        // monophonic winds: legato re-uses the sounding voice
        if (isMonoChannel (ch))
        {
            for (auto& v : voices)
                if (v.active && v.ch == ch && v.held && ! v.releasing)
                {
                    if (isLegatoArt (a))
                    {
                        v.glide = (float) (v.note - note);
                        v.note = note; v.vel = vel; v.art = a;
                        v.glideCoef = 1.0f - std::exp (-1.0f / ((a == enochsolo::A_Slide || a == enochsolo::A_Gliss ? 0.07f : 0.012f) * (float) sampleRate));
                        return;
                    }
                    v.releasing = true; v.held = false;
                    v.relCoef = std::exp (-1.0f / (0.012f * (float) sampleRate * 0.25f));
                }
        }
        Voice* slot = nullptr;
        for (auto& v : voices) if (! v.active) { slot = &v; break; }
        if (slot == nullptr)
        {
            long long oldest = -1;
            for (auto& v : voices) if (v.age > oldest) { oldest = v.age; slot = &v; }
        }
        setupVoice (*slot, ch, note, vel, a);
    }

    void noteOff (int ch, int note)
    {
        for (auto& v : voices)
            if (v.active && v.ch == ch && v.note == note && v.held)
            {
                v.held = false;
                if (pedal[(size_t) ch] && v.kind == K_Piano) v.heldByPedal = true;
                else release (v);
            }
    }
};
