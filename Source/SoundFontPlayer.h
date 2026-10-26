#pragma once
// SoundFont (.sf2) preview for the built-in test player, using the vendored TinySoundFont. Loading happens on the message thread;
// the audio thread only ever try-locks, so a swap can never block or crash the sound.
#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>
#include <algorithm>
#include "ThirdParty/tsf.h"

class SoundFontPlayer
{
public:
    ~SoundFontPlayer() { if (font) tsf_close (font); }

    bool load (const juce::File& f, double sampleRate, juce::String& message)
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb) || mb.getSize() < 64) { message = "Could not read that file."; return false; }
        tsf* t = tsf_load_memory (mb.getData(), (int) mb.getSize());
        if (t == nullptr || tsf_get_presetcount (t) < 1) { if (t) tsf_close (t); message = "That is not a valid SoundFont (.sf2) file."; return false; }
        tsf_set_output (t, TSF_STEREO_UNWEAVED, (int) sampleRate, 0.0f);
        tsf_set_max_voices (t, 48);
        tsf* old = nullptr;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            old = font; font = t; programApplied = -1; sr = sampleRate;
        }
        if (old) tsf_close (old);
        name = f.getFileName(); loaded = true;
        message = juce::String();
        return true;
    }

    void unload()
    {
        tsf* old = nullptr;
        { const juce::SpinLock::ScopedLockType sl (lock); old = font; font = nullptr; }
        loaded = false;
        if (old) tsf_close (old);
        name = juce::String();
    }

    bool isLoaded() const { return loaded.load(); }
    juce::String getName() const { return name; }

    void prepare (double sampleRate)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        sr = sampleRate;
        if (font) tsf_set_output (font, TSF_STEREO_UNWEAVED, (int) sampleRate, 0.0f);
        scratch.assign (4096 * 2, 0.f);
    }

    /** General MIDI program for the instrument (applied on the audio thread the next time it is needed) */
    void setProgram (int gm) { wantProgram = gm; }
    void setBendRange (float semis) { bendRange = semis; }

    // ---- audio thread
    void noteOn (int ch, int key, int vel)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || ! font) return;
        applyProgram();
        tsf_channel_note_on (font, ch, key, (float) vel / 127.0f);
    }
    void noteOff (int ch, int key)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || ! font) return;
        tsf_channel_note_off (font, ch, key);
    }
    void pitchWheel (int ch, int value14)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || ! font) return;
        tsf_channel_set_pitchrange (font, ch, bendRange);
        tsf_channel_set_pitchwheel (font, ch, value14);
    }
    void controller (int ch, int cc, int value)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || ! font) return;
        tsf_channel_midi_control (font, ch, cc, value);
    }
    void allNotesOff()
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || ! font) return;
        tsf_note_off_all (font);
    }
    void reset()
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || ! font) return;
        tsf_reset (font); programApplied = -1;
    }

    /** adds the SoundFont's sound to L / R (R may be null = mono) */
    void renderAdd (float* L, float* R, int num, float gain)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || ! font) return;
        applyProgram();
        int done = 0;
        while (done < num)
        {
            const int n = std::min (4096, num - done);
            if ((int) scratch.size() < 2 * n) scratch.assign ((size_t) (2 * n), 0.f);
            tsf_render_float (font, scratch.data(), n, 0);
            for (int i = 0; i < n; ++i)
            {
                L[done + i] += scratch[(size_t) i] * gain;
                if (R) R[done + i] += scratch[(size_t) (n + i)] * gain;
                else L[done + i] += scratch[(size_t) (n + i)] * gain * 0.0f;
            }
            done += n;
        }
    }

private:
    void applyProgram()      // lock held
    {
        const int gm = wantProgram.load();
        if (gm == programApplied) return;
        for (int c = 0; c < 16; ++c)
            if (! tsf_channel_set_presetnumber (font, c, gm, 0)) tsf_channel_set_presetindex (font, c, 0);   // not in this file: use its first instrument
        programApplied = gm;
    }

    juce::SpinLock lock;
    tsf* font = nullptr;
    std::atomic<bool> loaded { false };
    std::atomic<int> wantProgram { 0 };
    std::atomic<float> bendRange { 2.f };
    int programApplied = -1;
    double sr = 44100;
    std::vector<float> scratch = std::vector<float> (8192, 0.f);
    juce::String name;
};
