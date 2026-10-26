#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <memory>
#include <vector>
#include "Engine/MidiWriter.h"
#include "Engine/Phrases.h"
#include "TestPlayer.h"

class EnochSoloProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    EnochSoloProcessor();
    ~EnochSoloProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
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
    void clearSoundFont();
    /** one-shot bass sound for the built-in preview: any single bass note recording (WAV / AIFF / FLAC / MP3) */
    bool loadBassSample (const juce::File& f, juce::String& message);
    void clearBassSample();
    bool hasBassSample() const;
    juce::String bassSampleInfo() const;
    bool hasSoundFont() const { return player.sf.isLoaded(); }
    juce::String soundFontName() const;

    /** pick a mode (scale) for bars bar0..bar1 on top of their chord; -1 = the song's own scale */
    void setBarMode (int bar0, int bar1, int mode);

    /** phrase locking: locked phrases keep their notes when the solo is generated again (Try another, parameter changes, ...) */
    const enochsolo::LockRanges& getPhraseLocks() const { return phraseLocks; }
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
    juce::String soundFontPath, bassSamplePath;
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
    juce::MidiBuffer genBuffer, playBuffer;
    int lastInstrument = -1;
    bool heldOut[16][128] = {};              // notes currently sounding on the MIDI output (so they can be released when the host stops)
    bool wasSendingOut = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnochSoloProcessor)
};
