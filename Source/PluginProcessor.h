#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>   // v3.0: direct hardware MIDI port
#include <juce_events/juce_events.h>
#include <atomic>
#include <memory>
#include <vector>
#include "Engine/MidiWriter.h"
#include "Engine/Phrases.h"
#include "Engine/BarLocks.h"
#include "EnochLog.h"   // v3.6: open / close / removal log   // v3.4 bar locks
#include "TestPlayer.h"

class EnochSoloProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    enochlog::Mark partsMark { "instance: every part destroyed (removal finished)" };   // v3.6: first member = destroyed last
    EnochSoloProcessor();
    ~EnochSoloProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;                       // the host stops or removes us: every note and the port are released first
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void requestPanic() { panicRequest.store (true); }     // All Notes Off + controller reset on the next block (Panic button, auto triggers)
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Enoch Bass"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return numPresets(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override { return presetName (i); }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    /** A generated pattern plus its time-sorted playback list. Immutable once published. */
    struct Playback { double beat; juce::MidiMessage msg; };
    struct Snapshot
    {
        enochsolo::Pattern pattern;
        std::vector<Playback> events;       // what is exported / sent to the host (follows the export mode)
        std::vector<Playback> testEvents;   // what the built-in test player plays (always the full articulated version)
        double lengthBeats = 0;
        uint64_t seed = 0;
        int number = 0;     // increments each time a new snapshot is published (lets the UI know to repaint)
    };

    std::shared_ptr<const Snapshot> getSnapshot() const { const juce::SpinLock::ScopedLockType l (snapLock); return snapshot; }

    juce::AudioProcessorValueTreeState apvts;

    // actions (message thread)
    void tryAnother();                 // new seed
    void historyBack();
    void historyForward();
    void rehumanize();                 // new humanisation roll, same notes
    bool canGoBack() const    { return histIndex > 0; }
    bool canGoForward() const { return histIndex + 1 < (int) history.size(); }
    int  historyPosition() const { return histIndex + 1; }
    int  historyCount() const { return (int) history.size(); }

    //==========================================================================
    // editing (all on the message thread)
    void pushUndo();                       // call once before a change the user may want to undo
    void applyBarLocks (enochsolo::Pattern& p, bool restore);   // v3.4: restore = put locked notes back first (regeneration only)
    void refreshLockedBars();                                    // v3.4: a lock follows the notes its bar has now
    void fitToLocks();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo();
    void redo();
    /** replace every note (piano-roll edits). Re-humanises and publishes; does not touch the undo stack. */
    void commitRaw (std::vector<enochsolo::Note> raw);
    int allocNoteId() { return master.nextId++; }
    /** pick the chord of one bar (set=false returns it to the song's own chord). rootPc is absolute (0 = C). */
    void setBarChord (int bar, bool set, int rootPc, enochsolo::Quality q);
    void setBarChordRange (int bar0, int bar1, bool set, int rootPc, enochsolo::Quality q);
    void clearBarChords();
    bool hasBarChords() const;
    void rerollBars (int bar0, int bar1);
    /** make bars bar0..bar1 busier / calmer (0..1; negative = follow the global Busy slider) and write them again with a fixed seed */
    void applyBusy (int bar0, int bar1, float busy, uint64_t seed);
    const std::vector<float>& getBarBusy() const { return barBusy; }
    /** controller lanes drawn in the piano roll: replace the data of one lane between t0 and t1 (beats) */
    void commitCtrl (int lane, double t0, double t1, std::vector<enochsolo::CtrlPoint> pts);
    /** lane < 0 = every lane */
    void clearCtrl (int lane, double t0, double t1);
    const std::vector<enochsolo::BarChord>& getBarChords() const { return barChords; }

    /** reference recording (audio or MIDI): the solo is written with its rhythm / feel / shape. Message thread; analysing takes about a second. */
    bool loadReference (const juce::File& f, juce::String& message);
    void clearReference();
    bool hasReference() const { return refProfile != nullptr; }
    juce::String referenceName() const;
    juce::String referenceSummary() const;

    /** groove loop (drums, bass or keys of your project; audio or MIDI): the solo locks to its hits, leaves room where it is busy and swings like it. Message thread. */
    bool loadGroove (const juce::File& f, juce::String& message);
    void clearGroove();
    bool hasGroove() const { return grooveProfile != nullptr; }
    juce::String grooveName() const;
    juce::String grooveSummary() const;

    /** read the chords, key and length from a MIDI clip or an audio loop of your song (one chord per bar; audio assumes the loop starts on bar 1 at the project tempo) */
    bool detectChords (const juce::File& f, juce::String& message);

    /** bass audio to MIDI: listens to a recording of a bass (one note at a time works best), writes the notes, key and chords into the piano roll. Message thread. */
    bool transcribeAudio (const juce::File& f, juce::String& message);
    bool installTranscription (juce::String& message);      // (re)writes the notes of the last transcription at the current CLEANUP setting
    bool hasTranscription() const { return lastTr != nullptr; }
    juce::String lastTrMessage; int trVersion = 0;
    int trTempoMode = 0;       // 0 project tempo, 1 detect from the recording
    int trTidy = 1;            // 0 as played, 1 tidy, 2 snap 1/16, 3 snap 1/8
    int trAnchor = 0;          // 0 start of the audio is bar 1, 1 the first note is bar 1

    /** instrument of choice: the sample libraries / virtual instruments Enoch Bass can write key-switches and controllers for */
    const std::vector<std::shared_ptr<const enochsolo::Library>>& getLibraries (int inst) const { return libLists[(size_t) juce::jlimit (0, (int) enochsolo::I_Count - 1, inst)]; }
    std::shared_ptr<const enochsolo::Library> currentLibrary() const;
    static juce::File librariesFolder();
    juce::String libraryLoadReport() const { return libReport; }

    /** SoundFont (.sf2) for the built-in test player: the preview plays through it instead of the built-in sounds */
    bool loadSoundFont (const juce::File& f, juce::String& message);
    // v3.5: files are read and decoded on a worker thread; done (ok, message) runs on the message thread once the sound is in
    // place. The previous sound keeps playing while a file loads, and stays if the file cannot be used.
    void loadSoundFontAsync (const juce::File& f, std::function<void (bool, juce::String)> done);
    void loadOneShotAsync (const juce::File& f, std::function<void (bool, juce::String)> done);
    bool isLoadingFile() const { return loadingJobs.load() > 0; }
    int engineFaultCount() const { return engineFaults.load(); }
    void clearSoundFont();
    /** one-shot bass sound for the built-in preview: any single bass note recording (WAV / AIFF / FLAC / MP3) */
    bool loadBassSample (const juce::File& f, juce::String& message);
    void clearBassSample();
    bool hasBassSample() const;
    juce::String bassSampleInfo() const;
    bool hasSoundFont() const;
    juce::StringArray soundFontPresets();
    int soundFontPreset() const;
    void setSoundFontPreset (int index);                        // -1 = automatic (General MIDI program)
    /** what the audio engine is really playing (read back from the audio thread) */
    juce::String nowPlayingText (bool& matches, bool& warning) const;
    // ---- v3.0 external instrument driver: one route per part, per instrument
    enum RouteDest { RD_Builtin = 0, RD_External, RD_Both, RD_Hardware, RD_Off };
    struct PartRoute { int dest = RD_Builtin; int channel = 1; juce::String profile; int program = -1; int transpose = 0; juce::String port; };
    static constexpr int kParts = 3;
    using RouteTable = std::array<std::array<PartRoute, kParts>, TestPlayer::kSlots>;
    static bool routedIn (const RouteTable& rs, int slot);      // v3.5: routing on for an instrument (no lock: works on a copy)
    PartRoute getRoute (int slot, int part) const;
    void setRoute (int slot, int part, const PartRoute& r);
    static juce::StringArray partNames();
    std::shared_ptr<const enochsolo::Library> libraryByName (int inst, const juce::String& name) const;
    bool anyRouted (int slot) const;
    juce::String routeText (int slot) const;                        // "Kontakt on ch 2 (Ilya Efimov Nylon Guitar)" for NOW PLAYING
    void sendTest (const std::vector<juce::MidiMessage>& msgs);     // profile editor: test a key-switch / note on the part's channel
    std::atomic<int> lastInNote { -1 }, lastInVel { 0 }; std::atomic<uint32_t> inNoteCount { 0 };   // Learn: last note played into the plugin
    bool saveProfile (const enochsolo::Library& l, juce::String& message);   // writes an .enochlib file and reloads the libraries
    juce::ValueTree routesToTree() const;
    void routesFromTree (const juce::ValueTree& t);
    void setParamFromUI (const char* id, float v) { setParam (id, v); }
    /** v2.7: per-bar speed (-1 follow the global Speed, 0 normal, 1 double time); the bars are written again */
    void applyBarSpeed (int bar0, int bar1, int speed, uint64_t seed);
    int barSpeedOf (int bar) const;
    int previewSlot() const { return curSlot(); }                // the instrument slot the preview / source controls act on
    // same names as Enoch Solo so the shared SOUND SOURCE row works in both plugins
    bool loadOneShot (const juce::File& f, juce::String& m) { return loadBassSample (f, m); }
    void clearOneShot() { clearBassSample(); }
    bool hasOneShot() const { return hasBassSample(); }
    juce::String oneShotInfo() const { return bassSampleInfo(); }
    TestPlayer& testPlayer() { return player; }               // tests
    juce::String soundFontName() const;

    /** pick a mode (scale) for bars bar0..bar1 on top of their chord; -1 = the song's own scale */
    void setBarMode (int bar0, int bar1, int mode);

    /** phrase locking: locked phrases keep their notes when the solo is generated again (Try another, parameter changes, ...) */
    const enochsolo::LockRanges& getPhraseLocks() const { return phraseLocks; }
    // v3.4 bar locks (Engine/BarLocks.h): scale lock keeps a bar's notes in its chord scale, bar lock keeps them through regeneration
    bool isBarLocked (int bar) const   { return lockedBars.count (bar) > 0; }
    bool isScaleLocked (int bar) const { return scaleLockAll() || scaleLockBars.count (bar) > 0; }
    bool scaleLockAll() const          { return apvts.getRawParameterValue ("lockScaleAll")->load() >= 0.5f; }
    bool scaleLockAllPending() const   { return scaleLockAll() != appliedScaleAll; }   // the project-wide switch changed: fit the song
    bool hasBarLocks() const           { return ! lockedBars.empty() || ! scaleLockBars.empty() || scaleLockAll(); }
    void setBarLocks (int bar0, int bar1, bool lock);
    void setScaleLocks (int bar0, int bar1, bool lock);
    void applyLocksNow();
    void setPhraseLocked (const enochsolo::Phrase& ph, bool lock);
    void setAllPhrasesLocked (bool lock);
    void rerollPhrase (const enochsolo::Phrase& ph);            // new notes for just this phrase's region
    int  numLockedPhrases() const;

    /** my styles: named settings (instrument, artist, genre, feel sliders, humanise and the style reference) kept in the user's app-data folder */
    static juce::File stylesFolder();
    static juce::StringArray listMyStyles();
    bool saveMyStyle (const juce::String& name, juce::String& message);
    bool loadMyStyle (const juce::String& name, juce::String& message);
    bool deleteMyStyle (const juce::String& name);
    /** turn the solo on screen (rhythm, note lengths, accents, contour) into the style reference */
    bool learnFromSolo (juce::String& message);

    // factory presets
    static int numPresets();
    static juce::String presetName (int i);
    void applyPreset (int i);

    // chords typed by the user (message thread)
    void setChordText (const juce::String&);
    juce::String getChordText() const { return chordText; }
    juce::String getChordStatus() const { return chordStatus; }

    juce::File writeMidiToTemp();                       // used for drag & drop
    bool saveMidiTo (const juce::File& f);
    juce::String suggestedFileName() const;

    // hearing a single note while editing (message thread) and jumping the test player to a position
    void audition (int pitch, int velocity, bool hold = false);   // hold = keeps sounding until auditionRelease() (mouse button held)
    void auditionRelease();
    std::atomic<float> outLevel { 0.f };    // RMS of the preview output, for the VU meter
    float uiLaneH = 96.f;                  // height of the velocity / controller lane, remembered with the project
    int uiRollGrow = 0;                    // how far the person pulled the piano roll up (pixels), remembered with the project
    void seekInternal (double beat) { seekBeat = beat; }

    // transport / test player
    void setInternalPlaying (bool p);
    bool isInternalPlaying() const { return internalPlaying.load(); }
    double getPlayBeat() const { return playBeat.load(); }   // < 0 when stopped
    double getHostBpm() const { return hostBpm.load(); }
    int getHostNum() const { return hostNum.load(); }
    int getHostDen() const { return hostDen.load(); }
    bool hostIsPlaying() const { return hostPlaying.load(); }

    enochsolo::Settings currentSettings() const;
    static int tsNumFor (int choice, int hostN);
    static int tsDenFor (int choice, int hostD);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float) override;
    void timerCallback() override;

    void regenerate (bool newSeed, bool pushHistory);
    void refinalize();
    void publishMaster();
    enochsolo::Settings editSettings() const;      // structural settings of the current pattern + live humanise settings
    void setParam (const char* id, float value);
    static std::shared_ptr<Snapshot> makeSnapshot (enochsolo::Pattern&& p);

    // state
    mutable juce::SpinLock snapLock;
    std::vector<std::shared_ptr<const Snapshot>> retiredSnaps;   // v3.5: old patterns are freed on the message thread, not the audio thread
    juce::MidiBuffer rtAudition, rtInternal, rtAll, rtTimed, rtDaw, rtHw, rtPanic;   // v3.5: reused by the audio thread (no allocation while playing)
    std::atomic<int> engineFaults { 0 };                         // v3.5: blocks a sound engine produced NaN / infinity in (silenced)
    std::atomic<bool> clearBarChordsPending { false }, libraryResetPending { false };   // v3.5: work deferred from parameterChanged
    std::atomic<int> loadingJobs { 0 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);   // false once this instance is being destroyed
    juce::ThreadPool loader { 1 };                               // v3.5: file loading worker
    std::shared_ptr<const Snapshot> snapshot;       // guarded by snapLock
    std::shared_ptr<const Snapshot> audioSnap;      // audio-thread copy
    int snapCounter = 0;

    // the pattern being edited (message thread only) and its undo / redo history
    struct EditState { enochsolo::Pattern p; std::vector<enochsolo::BarChord> bc; std::vector<float> busy; };
    enochsolo::Pattern master;
    std::vector<enochsolo::BarChord> barChords;
    std::vector<float> barBusy;
    std::shared_ptr<const enochsolo::RefProfile> refProfile;
    std::shared_ptr<const enochsolo::RefProfile> grooveProfile;
    enochsolo::LockRanges phraseLocks;
    std::vector<EditState> undoStack, redoStack;
    int currentPreset = 0;

    juce::String chordText, chordStatus;
    juce::String soundFontPath, bassSamplePath;                 // (pre-2.7 single paths, kept for reading old projects)
    std::array<juce::String, TestPlayer::kSlots> smpPaths, sfPaths;
    std::vector<int> barSpeed;                                 // v2.7 per-bar speed override
    std::set<int> scaleLockBars;                 // v3.4 bars with a scale lock
    enochsolo::barlock::Saved lockedBars;        // v3.4 bars with a bar lock, with their notes (bar-relative)
    bool appliedScaleAll = false;                // the project-wide scale lock the song was last fitted to
    std::array<std::array<PartRoute, kParts>, TestPlayer::kSlots> routes;
    mutable juce::SpinLock routeLock;
    std::atomic<bool> routesChanged { false };
    RouteTable audioRoutes {};                                   // v3.5: the audio thread's own copy of the routes
    int audioRouteVersion = -1;
    std::atomic<int> routeVersion { 0 };
    std::shared_ptr<juce::MidiOutput> hwOut; juce::String hwOpenId; juce::SpinLock hwLock;
    juce::MidiBuffer testBuf; juce::SpinLock testLock;
    std::atomic<bool> panicRequest { false };
    juce::MidiBuffer panicBuf;                                  // panic messages still to send in this block
    std::vector<std::pair<int64_t, juce::MidiMessage>> delayQ; // external MIDI held back by the MIDI Delay setting
    int64_t sampleClock = 0;                                    // running sample count for the delay queue
    std::vector<std::pair<int64_t, juce::MidiMessage>> intQ;   // built-in sound events held back by the Built-in Delay setting
    int64_t intClock = 0;
    void buildPanic (juce::MidiBuffer& pb);
    static void applyDelay (std::vector<std::pair<int64_t, juce::MidiMessage>>& q, int64_t& clock, const juce::MidiBuffer& in, juce::MidiBuffer& out, int n, int64_t delaySamples);
    void defaultRoutes();
    void openHardwareFor (int slot);
    void fillRoutes (enochsolo::Settings& s) const;
    std::array<std::atomic<int>, TestPlayer::kSlots> sfPresets {};
    std::array<std::atomic<uint32_t>, TestPlayer::kSlots> smpGen {}, sfGen {};
    std::array<std::atomic<float>*, TestPlayer::kSlots> soundParams {}, srcParams {};
    int curSlot() const;
    bool loadSoundFontInto (int slot, const juce::File& f, juce::String& message);
    void restoreSoundSlots (const juce::ValueTree& vt);
    std::shared_ptr<enochsolo::TranscribeResult> lastTr; float lastTrClean = 0.5f; bool lastTrGhosts = false;
    std::vector<std::shared_ptr<const enochsolo::Library>> libLists[enochsolo::I_Count];
    juce::String libReport;
    void loadLibraries();
    const enochsolo::Library* lastLibPtr = nullptr; int lastLibInst = -1, lastLibBase = -1;
    uint64_t seed = 12345, hSeed = 1;
    std::vector<std::pair<uint64_t, uint64_t>> history;   // (seed, hSeed)
    int histIndex = -1;

    std::atomic<bool> needRegen { true }, needFinalize { false };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<int> hostNum { 4 }, hostDen { 4 };
    std::atomic<bool> hostPlaying { false }, internalPlaying { false };
    std::atomic<double> playBeat { -1.0 };
    std::atomic<double> seekBeat { -1.0 };
    juce::MidiKeyboardState auditionState;
    int auditionNote = -1; juce::uint32 auditionOffAt = 0, auditionStart = 0; bool auditionHeld = false;
    double lastBpmUsed = 120.0;
    int lastTsNum = 4, lastTsDen = 4;
    bool loadingState = false;

    // audio thread
    TestPlayer player;
    double sr = 44100.0;
    double internalBeat = 0.0, expectedNext = -1.0;
    bool wasRunning = false;
    const void* lastPlayedSnap = nullptr;          // audio thread: which pattern the player last played
    juce::MidiBuffer genBuffer, playBuffer;
    int lastInstrument = -1;
    bool heldOut[16][128] = {};              // notes currently sounding on the MIDI output (so they can be released when the host stops)
    bool wasSendingOut = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnochSoloProcessor)
};
