# Enoch Bass (v1.3)

A Windows-only VST3 that writes bass-guitar MIDI for you. Choose the key and the chord of each bar, click **TRY ANOTHER** (generate), and drag the MIDI into **Toontrack EZbass** (or any bass instrument). It is built from the Enoch Solo engine and keeps its piano roll, humanize, chord-per-bar editing, groove lock and style reference. Afrobeat, Afrobeats, Amapiano and other African styles come first, then Pop, Gospel, Rock, Swing, Bossa, Samba and Latin.

## Build and install (no software to install)
1. Make a **new** GitHub repo for Enoch Bass. Upload everything in this folder, **including the hidden `.github` folder** (it holds the build recipe).
2. Open **Actions**, wait for the green tick (or run *Build Enoch Bass (Windows VST3)*).
3. Download the `EnochBass-Windows-VST3` zip from the finished run and extract it.
4. Close your DAW, right-click `Install-EnochBass.bat` and choose *Run as administrator*, then rescan plugins. It shows up under Instruments as **Enoch Bass**.

## Writing a bass line
1. **Key / Scale / Genre / Bars / Time sig**: time signature "Auto" follows your project.
2. **Style of**: 25 bass styles, grouped. African first: Afrobeat Classic, Afrobeats Laid-back, Amapiano Log-Drum Bass, Amapiano Private-School Glide, Highlife Bounce, Juju Interlock, Soukous Runner, Ethio-Jazz Modal Pulse, Township Jive Walk, Bakithi Kumalo Style, Richard Bona Style. Then Pop & Soul (Jamerson, Larry Graham, Marcus Miller, Pino Palladino, Pop Driving Eighths, Gospel Walk-ups), Rock, Jazz (Ron Carter walking, Jaco), Brazilian and Latin. The styles describe a way of playing (how busy, how syncopated, how much ghosting, sliding, slapping, octave jumping); they are not copies of any recording.
3. **Chords**: click the chord above any bar to change it (right-click for the menu), or type chords such as `Am7 | D7 | Gmaj7 | Cmaj7`. The bass re-pitches that bar: roots stay roots, fifths stay fifths, approach notes aim at the next chord.
4. **Busy / Space / Repeat / Intensity / Octave**: how many notes, how much silence, how much the riff repeats, how hard it plays, and which octave. Select bars and use "Busy of selected bars" for per-bar control.
5. **TRY ANOTHER** writes a new line. Edit in the piano roll, then **Save .mid** or drag **DRAG MIDI** into the EZbass track.
6. **FROM YOUR SONG**: detect chords from a MIDI or audio loop, or load a **groove** (your drum loop). With groove lock the bass lands on the drum hits and leaves room where the loop is busy. **LOAD AUDIO / MIDI REFERENCE** borrows the rhythm of a bass recording (the notes are always new).
7. **LIVE MIDI OUT** sends the line live to a track you route the plugin's MIDI output to (routing differs per DAW).

## Instrument of choice: EZbass
Pick **Toontrack EZbass** (the default). Enoch Bass then writes EZbass's own key-switch notes **below the bass range**, so they never play a note:

| Articulation | EZbass key-switch (C-1 = note 0) |
|---|---|
| Legato (hammer-on / pull-off) | G#0 (hold) |
| Slide | F#0 (hold) |
| Ghost / dead note | E-1 (hold) |
| Palm mute | C0 (hold; EZbass libraries that contain recorded palm mutes) |
| Harmonic | A#-1 (hold) |
| Slap / pop | B-1 (hold) |

**Honesty note:** Toontrack does not publish one clean table of these, so the map was put together from Toontrack's release notes, its help text and review pages. They agree, but I could not open the EZbass program itself. Test one bar first. If a key is wrong in your version, copy `Examples/Instruments/Example-MyBassLibrary.enochlib` to `%APPDATA%\Enoch Bass\Instruments\`, correct the note numbers and restart: your version then appears in the dropdown. EZbass also chooses index / middle finger and fret positions by itself. "Plain bass" writes no key-switches at all (notes only), and "Standard layout" uses Enoch's own order from the Key-switch Base Note (Advanced).

Export mode (next to Save .mid): *Full* (key-switches + expression), *no key-switches* (articulations baked into note length and velocity), *plain notes*, or one track per articulation.

## Bass audio to MIDI (new in v1.1)
Drag a recording of a bass onto the plugin and choose **Turn this bass into MIDI**, or use **FROM YOUR SONG > Bass audio to MIDI**. It writes the notes into the piano roll, sets the key, and fills in the chord of every bar, so you can edit, regenerate around it, or export it to EZbass.
- Under the same menu you can pick the tempo (project tempo or detected from the recording), the timing (as played, tidy, snap 1/16 or 1/8) and whether the start of the audio or the first note is bar 1.
- It hears pitch, note starts and lengths, dead (ghost) notes, hammer-ons, pull-offs and slides.
- Tested on synthetic basses in 8 conditions: on a clean bass line it finds about 95% of the notes with almost no false ones, about 98% of pitches are right, and note starts are within a few milliseconds.
- Works best on a **solo bass track** playing one note at a time. A DI or a clean bass stem is ideal. Drum bleed (kick) adds false notes, and reverb or room noise lowers accuracy. Chords, slap pops, palm mutes and harmonics are not detected. Very fast slides can land a semitone or two off. Swing and triplet timing is kept as played (set timing to "As played").
- Load a **whole bass stem**: up to 15 minutes of audio and 256 bars are read in one go (a 4-minute song at 120 BPM is about 120 bars). The plugin screen can pause for a few seconds while it listens to a long file. The bass length is set to fit automatically.

## Genres and styles (v1.2)
Besides Afrobeat, Afrobeats, Amapiano, Highlife, Juju, Soukous, Ethio-Jazz, Township Jive, Gospel / Soul, Pop, Rock, Swing, Bossa Nova and Samba, there are now eight more genres, each with its own chord progressions and rhythm patterns:
- **Salsa**: tumbao, salsa dura, mambo-style drive. The bass anticipates the next chord on the "and of 4" the way a tumbao does.
- **Afro-Cuban**: son montuno, mambo, rumba (guaguanco), cha-cha-cha, bolero, timba / songo, Latin jazz.
- **Afro-Brazilian**: baiao, maracatu, afoxe / ijexa, samba-reggae (Olodum), axe, forro, partido alto.
- **Caribbean**: cumbia, merengue, bachata, zouk / compas.
- **Reggae & Dub**: one-drop roots, dub sub, dancehall riddim, ska.
- **Funk & Disco**, **Hip-Hop & Trap** (boom-bap, 808 slides), **Afro-House** (rolling offbeat, gqom).
That makes 56 bass styles and 47 one-click presets.

**Clave:** for Salsa and Afro-Cuban, the CLAVE box (3-2 or 2-3, or Off) decides which bars answer the three-hit side and which the two-hit side of the clave, so the bass lines up with your percussion. The patterns are my own descriptions of how these styles are played, not copies of any recordings: listen and use TRY ANOTHER, BUSY and the style to steer them.

## Bass sound for the preview (v1.2)
Press Advanced. The built-in player has three sounds: the built-in bass, a **SoundFont** (.sf2) of your choice, or your own **bass one-shot**. Load any single bass note recording (WAV, AIFF, FLAC or MP3) with "Load bass one-shot". Its pitch is found automatically (or set it by hand in the box next to it), and the preview plays your lines with it at every pitch, with ghost notes, short notes and palm mutes shortened, and slides and hammer-ons played legato. A one-shot plays once, so a long note stops when the sample ends. Only the preview changes; the MIDI you export is the same.

## What is the same as Enoch Solo
Piano roll with tools, grid, snap, quantize, velocity / bend / mod lanes, undo, phrase locking, My Styles (save your own and learn from a line), humanize, SoundFont preview in Advanced, resizable window, and project save/restore.

## Limits
- This Windows build is made by GitHub Actions; it could not be compiled or heard in the environment where it was written (logic, MIDI and the screen were tested there, not the sound).
- Musical quality is by design, not judged by ear: listen and use TRY ANOTHER, busy and style to steer.
- Audio chord detection is approximate; the bass sound in the built-in test player is only a stand-in for EZbass.


## What is new in v1.3
- **Clean transcription.** After you drop in a bass recording, the **CLEANUP** slider (song row) decides how strict the note filter is: low keeps everything heard, high keeps only definite, clear notes. It moves live, no need to transcribe again. The **Ghosts** switch keeps or drops the muted/ghost notes (off by default). The message shows how many notes were kept and how many weak ones were left out.
- **Click to hear.** Pressing a piano key or a note in the roll plays it and keeps sounding while the mouse button is held. Slide down the keys to play each one. Also sent to your instrument when "Send MIDI to host" is on.
- **Mouse wheel** inside the roll scrolls up and down. Ctrl+wheel = time zoom, Alt+wheel = pitch zoom, Shift+wheel = sideways. The ruler still scrolls sideways.
- **Taller piano roll.** Move the cursor to the top edge of the roll: it turns into the up/down arrow. Drag up to make the roll taller, down to shrink it, double-click to reset. Remembered with the project.
- **Remembers folders.** Every file picker (one-shots, SoundFonts, recordings, MIDI export) opens in the last folder you used.
