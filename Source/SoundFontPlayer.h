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
            old = font; font = t; programApplied = -1; appliedPreset = -2; sr = sampleRate;
        }
        if (old) tsf_close (old);
        name = f.getFileName(); path = f.getFullPathName(); loaded = true;
        message = juce::String();
        return true;
    }

    /** v3.5: read and parse a SoundFont without touching the player (any thread, used by the loading worker) */
    static tsf* parse (const juce::File& f, double sampleRate, juce::String& message)
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb) || mb.getSize() < 64) { message = "Could not read that file."; return nullptr; }
        tsf* t = tsf_load_memory (mb.getData(), (int) mb.getSize());
        if (t == nullptr || tsf_get_presetcount (t) < 1) { if (t) tsf_close (t); message = "That is not a valid SoundFont (.sf2) file."; return nullptr; }
        tsf_set_output (t, TSF_STEREO_UNWEAVED, (int) sampleRate, 0.0f);
        tsf_set_max_voices (t, 48);
        return t;
    }
    static void discard (tsf* t) { if (t) tsf_close (t); }
    /** v3.5: put a parsed SoundFont in place (message thread); the audio thread only ever sees the old or the new one */
    void install (tsf* t, const juce::File& f, double sampleRate)
    {
        tsf_set_output (t, TSF_STEREO_UNWEAVED, (int) sampleRate, 0.0f);
        tsf* old = nullptr;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            old = font; font = t; programApplied = -1; appliedPreset = -2; sr = sampleRate;
        }
        if (old) tsf_close (old);
        name = f.getFileName(); path = f.getFullPathName(); loaded = true;
    }

    /** shares the sample data of a SoundFont another instrument already loaded (no second copy in memory) */
    bool loadCopyOf (SoundFontPlayer& src, double sampleRate)
    {
        tsf* t = nullptr;
        {
            const juce::SpinLock::ScopedLockType sl (src.lock);
            if (src.font == nullptr) return false;
            t = tsf_copy (src.font);
        }
        if (t == nullptr) return false;
        tsf_set_output (t, TSF_STEREO_UNWEAVED, (int) sampleRate, 0.0f);
        tsf_set_max_voices (t, 48);
        tsf* old = nullptr;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            old = font; font = t; programApplied = -1; appliedPreset = -2; sr = sampleRate;
        }
        if (old) tsf_close (old);
        name = src.name; path = src.path; loaded = true;
        return true;
    }
    juce::String path;                                      // file it came from (set by the owner)

    /** the presets inside the file, for the preset menu (message thread) */
    juce::StringArray presetNames()
    {
        juce::StringArray r;
        const juce::SpinLock::ScopedLockType sl (lock);
        if (font) for (int i = 0; i < tsf_get_presetcount (font); ++i) r.add (juce::String (i + 1) + ": " + juce::String (tsf_get_presetname (font, i)));
        return r;
    }
    /** -1 = automatic (the General MIDI program of the instrument), otherwise the preset index to use */
    void setPreset (int index) { wantPreset = index; }
    int getPreset() const { return wantPreset.load(); }
    /** true when "automatic" is chosen and the file has no preset for the instrument's General MIDI program */
    bool programMissing() const { return missing.load(); }
    /** worked out on any thread: would the current settings find a preset? */
    bool canPlay()
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked()) return ! missing.load();
        if (! font) return false;
        const int pr = wantPreset.load();
        if (pr >= 0) return pr < tsf_get_presetcount (font);
        const bool ok = tsf_get_presetindex (font, 0, wantProgram.load()) >= 0;
        missing = ! ok;
        return ok;
    }
    void soundsOff()
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || ! font) return;
        for (int c = 0; c < 16; ++c) tsf_channel_sounds_off_all (font, c);
        tsf_reset (font); programApplied = -1; appliedPreset = -2;
    }

    void unload()
    {
        tsf* old = nullptr;
        { const juce::SpinLock::ScopedLockType sl (lock); old = font; font = nullptr; }
        loaded = false;
        if (old) tsf_close (old);
        name = juce::String(); path = juce::String(); missing = false;
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
        if (missing.load()) return;
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
        const int gm = wantProgram.load(), pr = wantPreset.load();
        if (gm == programApplied && pr == appliedPreset) return;
        bool ok = true;
        if (pr >= 0 && pr < tsf_get_presetcount (font))
            for (int c = 0; c < 16; ++c) tsf_channel_set_presetindex (font, c, pr);
        else
        {
            // never fall back to the file's first instrument: a missing program is reported and the built-in engine plays instead
            ok = tsf_get_presetindex (font, 0, gm) >= 0;
            if (ok) for (int c = 0; c < 16; ++c) tsf_channel_set_presetnumber (font, c, gm, 0);
        }
        missing = ! ok;
        programApplied = gm; appliedPreset = pr;
    }
    std::atomic<int> wantPreset { -1 };
    std::atomic<bool> missing { false };
    int appliedPreset = -2;

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
