# Enoch Bass (v2.5)
## v2.5: Stability (Studio One freezes)
- Changing the key, scale, genre, artist, instrument or sound source no longer makes the plugin call back into the DAW, or rebuild the song, from inside the DAW's own call. The work happens on the plugin's own timer a moment later. (The Studio One crash dump showed it crashing in its own window code right after calling into Enoch.)
- SoundFonts and one-shots load on a background thread. The previous sound keeps playing while a file loads and stays if the file cannot be used; the status line shows "Loading ..." meanwhile.
- The audio thread no longer waits for any lock the editor holds, no longer allocates or frees memory while playing, and never reads the editor's copy of the song.
- Sound engines: a broken value can no longer trap the audio thread in an endless loop. A block that contains NaN or infinity is silenced and the voices are reset instead of reaching the DAW.
- Removing the plugin while a hardware MIDI port is closing can no longer unload code that is still running.
- Every build now writes a .pdb symbol file (a separate "Symbols" download in GitHub Actions), so a crash dump can name the exact function.
## v2.4: Bar locks, scale lock, grid visibility, dropdown stepping
- **LOCK BAR** (bar panel, selected bars): keeps their notes exactly as they are when the song is regenerated (genre, artist, key, instrument...). Editing a locked bar updates its lock. Locks are saved with the project.
- **LOCK SCALE** (bar panel): the notes of the selected bars stay in their chord scale. Dragging a note steps to the next scale tone in the direction of the drag, and out-of-scale notes are fitted at once (Undo reverts).
- **Lock all** (toolbar, second row): the scale lock for the whole song, including regenerated notes.
- A gold strip along the top of a bar in the piano roll = bar lock; an accent strip = scale lock.
- **Grid** slider (toolbar, second row): Grid Visibility. 50 = the look of earlier versions; it scales the bar, beat and grid lines together.
- Dropdowns: the up and down arrows on their right edge step one item (top half up, bottom half down). The Up and Down keys and the mouse wheel step too, and apply at once. Wheeling over a dropdown no longer scrolls the panel behind it.

## v2.3: Panic, safe removal, timing offsets
- **Panic** (toolbar, next to Clear all): releases every note on the DAW and hardware outputs, and resets sustain, expression (CC11 = 127), modulation (CC1 = 0) and pitch bend on all 16 channels.
- Panic also runs automatically on transport stop, on a change of instrument or sound source, on a routing change, and on bypass (when the host reports bypass to the plugin).
- Removing the plugin releases its notes first. The hardware MIDI port is closed on its own thread, so a stuck MIDI driver can no longer hold up the DAW (the host waits at most 300 ms).
- **MIDI DELAY** (ROUTING page, bottom row): sends external MIDI 0-200 ms later, in 0.1 ms steps. The *MIDI Delay (samples)* parameter adds whole samples (0-4000).
- **BUILT-IN DELAY**: holds the built-in sounds back 0-400 ms. Use it when an external instrument (EZbass, for example) plays late compared with the built-in sound: delay the built-in sound by the same amount.
- MIDI DELAY can only make external notes later. To make external notes earlier relative to the built-in sound, use BUILT-IN DELAY.

A Windows-only VST3 that writes bass-guitar MIDI for you. Choose the key and the chord of each bar, click **TRY ANOTHER** (generate), and drag the MIDI into **Toontrack EZbass** (or any bass instrument). It is built from the Enoch Solo engine and keeps its piano roll, humanize, chord-per-bar editing, groove lock and style reference. Afrobeat, Afrobeats, Amapiano and other African styles come first, then Pop, Gospel, Rock, Swing, Bossa, Samba and Latin.

## Build and install (no software to install)
1. Make a **new** GitHub repo for Enoch Bass. Upload everything in this folder, **including the hidden `.github` folder** (it holds the build recipe).
2. Open **Actions**, wait for the green tick (or run *Build Enoch Bass (Windows VST3)*).
3. Download the `EnochBass-Windows-VST3` zip from the finished run and extract it.
4. Close your DAW, right-click `Install-EnochBass.bat` and choose *Run as administrator*, then rescan plugins. It shows up under Instruments as **Enoch Bass**.




## v2.2: session presets, quick routing, more synth basses
- **13 new presets**, one per EZbass-style genre: Funk, Gospel, Neo-soul, R&B, Afrobeats, Amapiano, Reggae, Dancehall, Rock, Pop, Jazz, Kizomba and Zouk. Each uses a Session Player Profile plus the genre's Timing Feel and Note Density. A ninth bass profile, *Jazz Walking Session Bass*, is new.
- **ROUTING > Quick setup...**: one click for EZbass (ch 4), EZbass + log drum VST (ch 4 / ch 5), Ilya Efimov Modern Bass + log drum, *Bass line to my VST*, or back to *All built-in*.
- **6 more synth basses** ("Synth basses (v3.2)"): Moog ladder bass, Acid 303-style, Dancehall square sub, Amapiano warm saw sub, 808 short, Kizomba smooth sub.
- **Fix:** with nothing routed, v2.0-v2.1.1 sent and exported the bass on MIDI channel 4 instead of channel 1. It is channel 1 again, as in v1.x.

## v2.1: clear buttons, Note Density, new subdivisions, timing feels, session players

- **Clear all** (piano-roll toolbar, next to Delete): removes every note, so the roll is blank and nothing plays. A cleared project stays blank when you reopen it. Undo brings the notes back; TRY ANOTHER writes a new take.
- **Clear bar** (bar panel, next to Re-roll bars): click a bar number (drag for several), then Clear bar. Those bars go silent, and a note held into them stops at the bar line. Undo brings them back.
- **Note Density** (was BUSY): 0-30 % sparse (few, long notes, lots of space), 30-70 % normal phrasing, 70-100 % dense lines and runs. Notes are added on the strongest beats and off-beats of the groove, never at random. The per-bar BAR NOTES DENSITY works the same way.
- **Subdivision** (SOUND & FEEL): new **1/4, 1/8 and 1/8 triplets**. The line is re-phrased to move in that rhythm; 1/16T, 1/32 and 1/32T still add fast runs.
- **Timing Feel** (SOUND & FEEL): Tight studio grid, Laid-back pocket, Pushed, Afrobeats pocket, Amapiano swing, Gospel behind the beat, Neo-soul drunk, Jazz swing, Live and loose. It works with the HUMANIZE knobs.
- **Session Player Profiles**: new players described by how they play (no artist names), listed first in the STYLE OF menu. The older styles are all still there and old projects keep theirs.
- **Bass articulations follow the neck** (v2.1.1): hammer-ons, pull-offs and slides never jump strings; a hammer-on wider than 4 frets becomes a slide.
- **The log drum part has its own pattern**: two-bar amapiano phrases that interlock with the bass line instead of copying it, with pitches from the chords, rolls and held drop notes at phrase ends, following Note Density. Route the *Log drum* row (ROUTING) to your log drum VST. A bar where the bass is cleared is silent for the log drum too.
- Bass profiles (8): Afrobeats, Amapiano, Gospel, Neo-Soul & R&B, Funk, Reggae & Dancehall, Kizomba & Zouk, Pop & Rock.

## v2.0: drive your own instruments (ROUTING) and the new log drums
**ROUTING** (bottom bar) sends the parts to your own VSTs or hardware: **Bass line** (EZbass, Ilya Efimov basses, any bass),
**Log drum** (plays the bass line on a second channel for a log drum VST - its own pattern arrives in v2.1) and **Chord pad**.
Per part: destination (built-in / external VST / both / hardware / off), MIDI channel, mapping profile, program change and octave.
In Studio One set the instrument track's Input to *Enoch Bass* and the channel shown. Dragged MIDI keeps the same channels and key-switches.

Profiles: **Toontrack EZbass** (notes an octave up, legato G#0, slide G0, slide down F0, palm mute C0 - check ghost / harmonic / slap with Test),
**Ilya Efimov Modern / Classic / Fretless** (string keys, fingered / legato / slap / palm mute / harmonics / slides, auto-legato off at start,
repetition key E4), **Retro / Rock** (same layout assumed - check with Test), **Plain bass**, and **Log drum VST: DX10 style**.
**Edit** opens the profile editor (Learn, Test, Save profile).

**Log drums rebuilt.** Eight new types built on the original *mda DX10 "Log Drum"* patch that Amapiano log drums come from
(2-operator FM, fast modulation decay, ~0.5 s ring): ORIGINAL, CLASSIC, DEEP, PUNCHY, SUB-HEAVY, KNOCK, BACARDI, SGIJA.
The older six types are still in the list so old projects sound the same.

## Writing a bass line
1. **Key / Scale / Genre / Bars / Time sig**: time signature "Auto" follows your project.
2. **Style of**: 25 bass styles, grouped. African first: Afrobeat Classic, Afrobeats Laid-back, Amapiano Log-Drum Bass, Amapiano Private-School Glide, Highlife Bounce, Juju Interlock, Soukous Runner, Ethio-Jazz Modal Pulse, Township Jive Walk, Bakithi Kumalo Style, Richard Bona Style. Then Pop & Soul (Jamerson, Larry Graham, Marcus Miller, Pino Palladino, Pop Driving Eighths, Gospel Walk-ups), Rock, Jazz (Ron Carter walking, Jaco), Brazilian and Latin. The styles describe a way of playing (how busy, how syncopated, how much ghosting, sliding, slapping, octave jumping); they are not copies of any recording.
3. **Chords**: click the chord above any bar to change it (right-click for the menu), or type chords such as `Am7 | D7 | Gmaj7 | Cmaj7`. The bass re-pitches that bar: roots stay roots, fifths stay fifths, approach notes aim at the next chord.
4. **Busy / Space / Repeat / Intensity / Octave**: how many notes, how much silence, how much the riff repeats, how hard it plays, and which octave. Select bars and use "Busy of selected bars" for per-bar control.
5. **TRY ANOTHER** writes a new line. Edit in the piano roll, then **Save .mid** or drag **DRAG MIDI** into the EZbass track.
6. **FROM YOUR SONG**: detect chords from a MIDI or audio loop, or load a **groove** (your drum loop). With groove lock the bass lands on the drum hits and leaves room where the loop is busy. **LOAD AUDIO / MIDI REFERENCE** borrows the rhythm of a bass recording (the notes are always new).
7. **LIVE MIDI OUT** sends the line live to a track you route the plugin's MIDI output to (routing differs per DAW).

## Instrument of choice: EZbass
Pick **Toontrack EZbass** (the default). Enoch Bass then writes EZbass's own key-switch notes **below the bass range**, so they never play a note.
Note names use Toontrack's numbering (middle C = C3 = note 60). v2.0 corrects the octave: v1.x wrote these an octave too low.

| Articulation | EZbass key-switch |
|---|---|
| Legato (hammer-on / pull-off) | G#0 = 32 (hold) |
| Slide up | G0 = 31 (hold) |
| Glissando / slide down | F0 = 29 (hold) |
| Palm mute | C0 = 24 (hold) |
| Ghost / dead note | E-1 = 16 (hold, unconfirmed) |
| Harmonic | A#-1 = 22 (hold, unconfirmed) |
| Slap / pop | B-1 = 23 (hold, unconfirmed) |

The bass notes themselves are sent one octave up, which is where EZbass expects them.

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
- This Windows build is made by GitHub Actions. Where it was written, the plugin code was compiled with the same JUCE version (on Linux), its screen was rendered and its audio was measured, but nobody listened to it there: judge the sounds by ear.
- Musical quality is by design, not judged by ear: listen and use TRY ANOTHER, busy and style to steer.
- Audio chord detection is approximate; the built-in bass sounds are models for auditioning; EZbass or your own library gives the final sound.


## What is new in v1.3
- **Clean transcription.** After you drop in a bass recording, the **CLEANUP** slider (song row) decides how strict the note filter is: low keeps everything heard, high keeps only definite, clear notes. It moves live, no need to transcribe again. The **Ghosts** switch keeps or drops the muted/ghost notes (off by default). The message shows how many notes were kept and how many weak ones were left out.
- **Click to hear.** Pressing a piano key or a note in the roll plays it and keeps sounding while the mouse button is held. Slide down the keys to play each one. Also sent to your instrument when "Send MIDI to host" is on.
- **Mouse wheel** inside the roll scrolls up and down. Ctrl+wheel = make the roll taller or shorter (pitch zoom), Alt+wheel = time zoom, Shift+wheel = sideways. The ruler still scrolls sideways.
- **Taller piano roll.** Move the cursor to the top edge of the roll: it turns into the up/down arrow. Drag up to make the roll taller, down to shrink it, double-click to reset. Remembered with the project.
- **Remembers folders.** Every file picker (one-shots, SoundFonts, recordings, MIDI export) opens in the last folder you used.

## What is new in v1.4: real basses and a new look
- **PREVIEW SOUND** (next to Instrument of choice): P-Bass (fingers, flatwound), Jazz Bass (bridge + neck), pick bass, slap bass (thumb + pop), upright bass, fretless (mwah), Moog-style synth bass, 808 sub, amapiano log drum, or Classic (the old preview sound).
  - The electric basses are modelled strings with pluck position, pickups, pickup tone and amp; ghost, palm-mute, hammer-on, pull-off, slide and slap articulations change how the string is played. They respond to pitch-bend, mod wheel and expression.
  - These are models, not samples: much more natural than before, but EZbass or your own library is still the final word.
- **Fixed: notes that kept ringing** after the pattern changed while playing (a lost sustain pedal or note-off). Changing a knob, TRY ANOTHER, editing while playing, stopping or jumping now lifts the pedal and releases old notes; live MIDI out gets the note-offs and a pedal-up too.
- **New interface.** Glass cards, glowing knobs and sliders, switch toggles, a real ivory-and-ebony keyboard on the piano roll, gradient notes, and the Inter typeface built in so it looks the same on every PC. Advanced opens as a drawer over the roll instead of squashing it. "Reset chords" moved next to Copy chord / Paste chord.
- Inter typeface: SIL Open Font License 1.1, (c) The Inter Project Authors (https://rsms.me/inter).

## What is new in v1.5: studio hardware and many more basses
- **A new hardware-style interface** with lighting from the top left everywhere. It has a brushed-gunmetal faceplate with walnut end cheeks and screwed-on plates. The main controls are machined knobs (knurled grips, spun-aluminium caps, LED pointer, backlit value arc, printed scale legends, a value display under each knob). Drag up/down, hold **Shift** for fine adjustment, **double-click** to reset. Menus are recessed LCD windows, on/off settings are chrome toggle switches with status LEDs, and the piano roll sits in a bezelled screen.
- **VU meter** at the bottom left shows the preview output.
- **Resizable velocity / controller lane.** Drag the thin line above the velocity lane (the cursor turns into the up/down arrow); double-click to reset. Remembered with the project.
- **Many more bass sounds** (PREVIEW SOUND, grouped):
  - **Electric:** P-Bass flatwound, Precision roundwound, Jazz Bass, Jazz Bass bridge pickup, Modern active 5-string, StingRay-style humbucker, Rickenbacker 4003 style (pick, growl), Gospel active bass, Afrobeat bass, Motown flats with foam mute, Hofner violin bass, pick bass, slap bass, fretless.
  - **Upright and acoustic:** upright, upright jazz pizzicato, upright rockabilly slap, acoustic bass guitar.
  - **Synth:** Minimoog bass with glide, Moog synth bass, Reese, Juno square, Acid 303, clean sine sub, 808, 808 long (slides), and three log drums (classic, deep, short).
  - All of these are models built from the character of each instrument. They are not samples and do not come from any commercial sample library (Scarbee or any other). For final tracks, EZbass or your own library is still the reference.


## What is new in v1.6 (upgrade batch 1: critical usability + Amapiano log drums)
- **Sound switching fixed**: changing sound or source clears every voice, effect tail, filter, sample voice, SoundFont channel and sustain pedal with a short fade.
- **SOUND SOURCE row** on the main panel: BUILT-IN / ONE-SHOT / SOUNDFONT / EXTERNAL, with Load / Clear, the SoundFont preset list and the one-shot root note.
  A SoundFont without a bass program (GM 33) is **not** used silently: the built-in bass plays and NOW PLAYING shows an amber warning until you pick a preset by hand.
- **NOW PLAYING** display with a lamp (green = the engine matches the panel, amber = a fallback is in use).
- **Amapiano log drums rebuilt**: six new types in PREVIEW SOUND > *Amapiano log drums*: Classic Amapiano, Punchy Club, Deep Soulful, Sub-Heavy, Percussive, Hybrid Log / Bass.
  Pitch lands exactly on the note (measured within 1 cent), body and sub share one oscillator so they never cancel, the low end is mono.
  The three older log drums are kept under *Legacy log drums (v2.6)*.
- **LOG DRUM panel** (button in the footer): type buttons and 17 knobs - Tune, Fine, Pitch Drop, Drop Time, Attack, Punch, Decay, Sub, Body, Tone, Drive, Width,
  Velocity, Glide, Human, Room, Output - plus *Mono below* (off / 80 / 120 / 160 Hz) and *Sidechain-friendly output* (soft ceiling at -1 dBFS).
  The knobs are relative: 100% = the type's own voicing. All are automatable.
- **Log drum glide**: overlap two notes or use the Slide articulation and the drum bends from the ringing note to the new one.
- **Amapiano phrasing**: the Amapiano genre now writes call-and-response log drum phrases with a fill at the end of every fourth bar.

## What is new in v1.7 (upgrade batch 2: realistic sound and performance)

- **Physical models** (new sounds, appended so older sound numbers do not move):
  - *Saxophones (physical model)*: tenor, alto, soprano, baritone, breathy ballad tenor. A reed and a conical bore waveguide (after Cook's Saxofony), with the horn formants, breath and key noise on top.
  - *Strings (bowed string model)*: violin, viola, cello, fiddle. A real bowed string: neck and bridge delay lines, the bow's friction, bow changes on long notes. Notes above E6 use the classic engine, because a very short string does not speak reliably.
  - *Brass (lip + bore model)*: trumpet, flugelhorn, muted trumpet. A hybrid: band-limited lip pulses drive a waveguide bore with brassy steepening and a bell.
  - Measured: every note of every range speaks at every velocity, within about ±7 cents.
- **Upgrades to the existing models** (pianos, e-pianos, guitars, basses):
  - realism-dependent damper, key, finger and pedal noises;
  - soundboard and pedal resonance on the pianos;
  - fretboard logic on guitar and bass;
  - analog drift on the synths.
- **SOUND & FEEL drawer** (footer button):
  - knobs: Realism, Humanize, Dynamics, Expression response, Brightness, Mechanical noise, Mic (close <-> far), Glide time;
  - menus: Room (Classic / Studio / Small room / Club / Hall / Plate / Church), Glide mode, Output (stereo / mono), Speed, Fastest notes;
  - Human Play on/off.
  - 50% (and "Classic") reproduce the previous behaviour where it makes sense.
- **Glide modes**: Off, Legato glide (default, as before), Always glide, Overlap glide, Portamento. Glide time runs from fast and subtle to slow. Articulations are never changed, and only the chosen situations glide.
- **Human Play phrasing engine**:
  - phrase arcs;
  - accented phrase openings;
  - lighter fast runs;
  - breath at phrase starts (winds);
  - down / up bow alternation with bow-change noise (strings);
  - string and fret choice with position shifts and squeaks (guitar, bass).
  - Seeded, so a take plays the same way every time.
- **Bar Notes Density** (the old "Busy of selected bars"): same parameter and saved values, new name. The added notes follow the chord and scale, and runs scale with it.
- **Speed and subdivisions**:
  - Speed: Normal / Double time;
  - Fastest notes: Auto / 1/16 / 1/16 triplets / 1/32 / 1/32 triplets;
  - a per-bar Speed box in the bar panel.
  - Runs walk through the chord scale into the next note. Winds keep a breath every two bars, and bass uses runs sparingly.
- **More synths**:
  - 5 new lead synths with every Enoch Solo instrument;
  - 6 new synth basses in Enoch Bass (Amapiano soft sub, Funk Minimoog, deep house organ, Reese growl, Trap 808 glide, Afro-house pluck).

## What is new in v1.8 (upgrade batch 3: expansion and interface)

- **12 new genres** (appended: older genre numbers do not move):
  - jazz styles: *Jazz - Bebop, Hard Bop, Modal, Cool / West Coast, Latin Jazz, Jazz-Funk / Fusion, Smooth Jazz*;
  - *Kizomba*, *Zouk*;
  - the Amapiano family: *Amapiano - Private School, Bacardi, Sgija*.
  Each has its own feel settings, chord progressions and rhythm cells (Enoch Bass: its own bass cells; the Amapiano sub-genres answer with their own call-and-response).
- **Starter MIDI library** in *Examples/*-Starter-Library.zip* (one zip, so the repository upload stays small; unzip it anywhere): one folder per genre with an intro, a verse, a solo and an outro, each as a sparse, medium and hot take.
  - Files are written without key-switches, so they play on any instrument, at the genre's tempo, in A minor or C major.
  - Enoch Solo has a sub-folder per instrument.
- **MODULATION & FX** (second page of SOUND & FEEL):
  - a 4-slot modulation matrix. Sources: LFO 1, LFO 2, velocity, mod wheel, aftertouch, expression, key, note envelope, random per note. Destinations: pitch, brightness, level, pan, vibrato, drive.
  - two LFO rates;
  - a creative effect: tape wobble, lo-fi, wide, telephone / radio, warm saturation.
- **SoundFonts and one-shots**: drop an .sf2 or an audio file on the SOUND SOURCE row and it loads for the selected instrument.
- **Scroll bar on the right**: full height and always visible. When the window is short, the interface scrolls instead of shrinking. The mouse wheel over empty space also scrolls.
- **Premium redesign**:
  - deep charcoal / midnight base;
  - colour-coded sections: GENERATE in the instrument colour, the SOUND row in teal, EDIT in azure, PLAY & EXPORT in gold;
  - colour families: guitar amber, sax rose, violin violet, piano / synth ice blue, trumpet emerald, bass ember, and log drum magenta (Enoch Bass switches to it when a log drum is the sound).
