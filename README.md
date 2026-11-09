# Enoch Bass v2.14.1  (Windows VST3 instrument)
## v2.14.1: live timing for monitored instruments, VHorns, undo for splits, split bars
- **Live timing for monitored instruments (Studio One with Monitor on).** Each routing row has a **Timing** button that opens its TIMING page:
  - **Live Monitor Offset** (per route, -1000 to +200 ms): what Enoch sends live to that instrument goes out this much early, so a monitored EZbass / Kontakt track that Studio One plays late lines up with the project. It never moves the take or the MIDI you drag.
  - **Recording Offset** (per route): used instead while Studio One records, so MIDI recorded from Enoch's output lands on the grid (keep it at 0 for that).
  - **MEASURE**: route the instrument's audio into Enoch's side-chain input; Enoch plays 8 short notes on that route and measures how late they come back (median and spread), then **ACCEPT** sets the offset. Without a side-chain, **BY EAR** plays a click and the instrument's note alternately while you move the offset until they are one sound.
  - The alignment ruler shows where the instrument sounds against the beat, and the diagnostics show host, sample rate, block size, the reported latency (0), the offsets in use, whether Studio One is recording, side-chain audio, and notes sent late after pressing Play, with warnings when the delay is large or unsteady.
  - **MIDI TIMING** (bottom row, all routes, -100 to +100 ms) is for feel; it moves dragged MIDI only with "Apply the MIDI Timing Offset to dragged MIDI too".
  - Old sessions: a negative MIDI DELAY becomes the Live Monitor Offset of each external route, a positive one becomes the MIDI Timing Offset.
  - What plays live is now exactly what you drag: the MIDI check also runs on the live output.
  - Why it was late: Studio One plays its timeline early to make up for latency and plug-in delay, but a track you monitor live is not; Dropout Protection and look-ahead plug-ins add to that, which is why -200 ms was not enough. Lower Dropout Protection, a smaller block size, or Monitor off when you only play back all shorten it.
- **Undo for splits; split bars.** Ctrl+Z / Redo now undo phrase and bar splits, joins and removals. The split tool on the chord lane splits a bar on the grid; right-click a part: Re-roll only this part, Lock this part, Remove the splits in this bar.
- **VHorns Saxophones**, from the AcousticSamples user guide: no articulation keys (staccato, legato and double-tonguing come from the air flow and overlap). Enoch shapes the air flow on the mod wheel per phrase, plays legato by overlapping notes, presses the effect keys inside their notes (fall C#1, rise D#1, bend up F#1, scoop F1), plays fast repeats on E1 (static tonguing) and a soft breath (A1) before each phrase. No vibrato controller is sent: leave VHorns' vibrato on Auto (in Manual mode that controller bends the pitch). Keys assume UVI numbering (middle C = C3).

## v2.14: Ilya Efimov FX glissando, bar copy, phrase splits, royal-blue scale, problem reports
- **Ilya Efimov basses: G#0 FX glissando, played the way the library wants it.** G#0 is a mode key: Enoch presses it just before each glissando note and the next note's own key-switch always goes down before that note, so the effect never spills onto the following notes; when nothing follows within a beat, the plain-note key (F#4) goes down right after the effect ends. The effect notes are kept on the keys that carry an effect (B0 to D4), and each key gives its own glissando. Enoch uses it on about half of the exposed section endings it would otherwise slide out of (*Never* on the SLIDE family turns it off); you can also put *FX glissando* on any note from the right-click Articulation menu. The technique demo for the Efimov bass now ends with three FX glissandos and a plain note.
- **A bar can play exactly like another bar.** Right-click a bar's chord (or select several bars and right-click) > *Play this bar like bar...* > pick the bar. Its notes, articulations and hand-drawn key-switches are copied in, selected, ready for small changes. One Ctrl+Z puts the bar back. A long note from the bar before no longer rings into the copy.
- **Split a phrase on the grid and re-roll just that part.** Pick the split tool (key 4) and click the phrase strip above the notes: the phrase is cut on the grid line nearest the click (the Grid setting, e.g. 1/8). A main-line note across the cut is split too. Then right-click the part you want new > *Re-roll only this phrase*: only that part changes. Click the white split mark again (or right-click > *Join with the next phrase*) to join the parts; right-click > *Remove all my phrase splits* clears them. Splits can also be made from the phrase menu (*Split this phrase here*).
- **Show scale lights the scale in royal blue.** With *Show scale* on, every row of the selected bar's scale is filled royal blue across that bar (the root strongest), and the scale's piano keys are blue too, so you can move notes onto the scale at a glance. With no bar selected, all bars show their scale, a little softer.
- **Report a problem** (button under SOUND & FEEL): write in a few words what went wrong and press *Save report*. Enoch saves one zip in Documents\Enoch Reports with this take (as exported and before the MIDI check), the whole plugin state (it reloads the take exactly), the key-switch profiles in use, the MIDI check, a note-by-note list with each articulation and why, your host and version. *Open folder* shows it in Explorer. Attach that zip when you send a bug, so it can be reproduced and fixed straight away.
- **Profile files: Import... / Export...** in the routing profile editor. Export saves the profile you see as an .enochlib file (keep a copy or share it); Import loads one into the editor to check and Save.
- **ARTICULATION page: WHY and PHRASES.** WHY lists the most common reasons behind this take's articulations (hover one for the full sentence); PHRASES shows how articulated each phrase is (the same phrases as the phrase strip; hover for the phrase's bar, count and main reason).
- **Deeper MIDI check** (runs on every drag and save): key-switch velocities are set to the profile's own (some libraries choose the sound by velocity), a joined note whose held legato key went missing gets it back, and a legato jump wider than an octave is reported. The check now repeats until the file is clean (one repair can need another).

## Known limits and how to report a problem
- Enoch can only check what it writes: if a library's own patch is set differently (another key-switch bank, a mode switched on inside Kontakt, legato off in the instrument), the MIDI can be right and still sound wrong. Load the library's default patch first.
- Key-switch profiles for libraries you named are built from the key-switch pages you sent; anything not on those pages is a guess and is marked so in the profile notes.
- The phrase strip's automatic phrases can shift a little after you re-roll part of the song; your own splits always stay where you put them (until the song is replaced by a reference or a new length).
- Something wrong? Press **Report a problem**, write a line about what you heard and where (bar, instrument, library), save, and send the zip. It holds everything needed to reproduce your exact take, so the fix can be targeted and tested before the next update.

## v2.13: Editable key-switches, and bass lines with phrases and motifs
- **Key-switches are fully editable in the piano roll.** Grab, stretch or delete one of the orange key-switches Enoch wrote (or right-click > Key-switches > Edit key-switches as notes): every key-switch of the take becomes an ordinary orange note on its key row - move it, stretch it, delete it, copy it, change its velocity, draw new ones. What is dragged out is exactly what you see. The notes keep their articulations (legato overlaps and note lengths still follow them), but Enoch stops writing key-switches of its own for them, including string, repetition and mode keys. Right-click > Key-switches > Back to automatic removes the key-switch notes and lets Enoch write them again. Saved with the project, in favourites and in Undo.
- Checked on real takes for each Ilya Efimov / EZbass profile: turning the key-switches into notes gives an identical MIDI file; deleting, moving or re-velocity changes the file; back to automatic gives the original again.
- **Bass lines rebuilt around phrases and motifs** (stage 2). The genre's rhythm cells stay (they are its groove), but:
  - **A motif:** every rhythm cell gets a melodic shape, a function for each hit on the chord (root, fifth, third, seventh, octave, a passing step, a neighbour, an approach into the next chord), drawn from the genre's own vocabulary (funk: octaves, b7, passing; gospel / jazz / soul: thirds and sevenths as guide tones, walking steps; Afrobeat: melodic fifths and passing; amapiano: root / fifth / octave with space; reggae: root-fifth-octave and melodic fills; rock and disco keep pumping one note on purpose). The shape is kept, so the hook comes back, and is mapped onto each chord, so it follows the harmony.
  - **A phrase plan:** bar 1 states the motif, bar 2 answers it (its last note turns toward the next chord, or answers an octave away in call-and-response genres), bar 3 develops it (one note moves an octave; sometimes the variant cell), the last bar releases it (a fragment and space, or a fill).
  - **Fills from the genre:** scale runs down or up, a chromatic walk-up, an enclosure, an octave pop, a dead-note pickup, each landing on the next chord's root.
  - **No root pumping:** a weak beat never repeats the root just played (except rock, disco, merengue eighths).
  - **Gospel, jazz, soul walk:** passing 8ths between far-apart hits, guide tones (3rd, 7th) on the beats.
  - **Bar Notes Density is musical:** low keeps the anchors and holds them long (space); high adds pickups into the beats; never more than 5 + 7 x density hits a bar, and every half bar keeps a rest.
- Measured over 195 takes (every style, 8 bars), before / after: roots 59% / 37%, moves that repeat the same pitch 48% / 23%, moves by step 13% / 33%, three-note stepwise runs 1.7 / 3.7 per 8 bars, bars that bring the motif back (of each 4-bar phrase) random / about half. Gospel genre: thirds 17%, sevenths 12%, steps 40%. Legato / hammer-ons / pull-offs 9% / 13% of notes, because the lines now move by step.
## v2.12: Articulations as complete performance techniques; EZbass rebuilt from your test
- **Articulations are techniques, not labels.** Every key-switch in a profile now says *when* it goes down:
  - **before note**: a pre-roll before the note (20 ms by default, set per profile; never on the note's own tick), held through the note when it is a hold key. Several notes in a row with the same held key keep it down through the whole run.
  - **during note**: pressed while the note is held (EZbass F0 / G0 slide the held note down / up).
  - **is the sound**: the key plays the sound itself and no pitched note is sent (EZbass E0 ghost tap).
- **Legato as a complete performance:** the first note is plucked normally; the legato key goes down a pre-roll before the next note; the next note starts while the previous one still sounds (by the profile's overlap); the previous note lets go after the overlap; the key stays down through the run and comes up when it ends. A legato, hammer-on, pull-off or slide with no sounding note before it is played plain (nothing to join).
- **Instruments that play any overlap as legato** (EZbass, Ilya Efimov basses with auto legato): plain notes always end a moment before the next one, so only the notes meant to be joined are joined. Notes that start together keep ringing with the instrument's "no legato" key held (EZbass A-1).
- **Mapping editor:** each key has a WHEN setting (before note / during note / is the sound); the profile has PRE-ROLL, "Overlapping notes play legato" and a "no legato" key. Test plays each key the way its technique works (a joined pair for legato and slides, a held note for during-note keys, the key alone for is-the-sound keys). Saved in .enochlib files (`during`, `replace`, `press=`, `at=`, `preroll_ms`, `overlap_is_legato`, `detach_key`).
- **MIDI check** also repairs: a joined note whose overlap is too short for the instrument, a plain note that would be played legato by accident, a pitched note sent where the key itself is the sound. Keys pressed during a note are left inside it.
- **EZbass, key for key as you tested and described it:** G#0 legato held through hammer-on / pull-off runs; F#0 held to slide between two overlapping notes; F0 / G0 pressed while a note is held to slide it down / up (slide-outs); E0 ghost tap on its own (no pitched note); A#-1 harmonic and B-1 slap / pop held before and through their note; A-1 polyphony held for notes that ring together. **D0 repeater is no longer used for repeated notes** (it is a stutter; it is in the Articulation menu to place by hand). C0 palm mute stays from Toontrack's release notes. The D#0 and C#0 keys from the online summary are removed.
- On the bass the slide-outs are now called Slide Out Down (F0) and Slide Out Up (G0).
- Checked event by event on 65 takes per library: every EZbass legato note has G#0 down 20 ms before it and the previous note overlapping, every run's first note is plucked, every slide holds F#0 with the notes overlapping, every slide-out presses F0 / G0 inside its note, every ghost is the E0 tap alone, plain notes never overlap, no D0 stutters. The same for the Ilya Efimov basses (G4 held for hammer-ons / pull-offs, slurs by auto legato overlap).
## v2.11: The articulation planner, the ARTICULATION page, the MIDI check
- **A real plan instead of dice.** v3.10 / v2.10 rolled a separate chance for every rule on every note, and the groove generator added ghost fills in every style that nothing ever trimmed. That is why takes leaned on ghost notes and palm mutes. The articulation engine is rebuilt as a planner:
  1. It reads the line: previous and next note (interval, direction, gap, whether it still rings), place in the bar and phrase (start, peak, longest note, ending, section end), chord tone or not, chord change, speed.
  2. Every 4-bar phrase gets a budget per articulation family from the instrument, genre and player style. Budgets are ceilings, never quotas.
  3. Every possible articulation gets a score for how well it fits the note, with a reason. The best-fitting ones win until the budget is used; weak fits are never used.
  4. Hand checks: a hammer-on, pull-off or slur needs the previous note still ringing and a small step (up = hammer-on, down = pull-off); never across a leap or onto a repeated note; a slide never follows a slide; a bend lands on a chord tone; a violin slur is at most one bow; horns breathe in long runs.
  5. Palm mute and slap come as passages; a repeated riff repeats its articulations (only where the notes are reached the same way).
- **ARTICULATION page** (SOUND & FEEL drawer, third tab): INTENSITY (the ARTICULATION knob), VARIATION, and one row per articulation family your instrument has: an amount (100% = what the style naturally uses) and a lock (Auto / Never / Always where the hands allow it). Never also removes the style's own licks of that kind. PROFESSIONAL BALANCE puts everything back to the natural, balanced setting. THIS TAKE shows the share of each family in the current take.
- **Why this articulation?** Hover a note in the piano roll: after its articulation it says why the plan chose it ("Quick small step up from a ringing note: hammer-on", "Exposed phrase ending: slides out", "The riff repeats"...).
- **String-aware:** the string chooser keeps a hammer-on, pull-off or slide on the same string as the note before whenever the string can reach it, so the articulation survives (before, many slides were turned into plain legato because the next note was put on another string).
- **Key-switches, cleaner:** a held key-switch (EZbass G#0 legato) stays down through a run instead of being pressed again; two different held keys never overlap; a double-stop presses its key-switch once; a mode key the first note presses anyway is not sent twice at the start; a note never rings into the next note of the same pitch.
- **MIDI check on every file you drag or save** (Engine/MidiCheck.h): it checks and repairs, before the file is written, that every key-switch exists in the chosen profile, is down before its note and is released; no two held key-switches overlap; no note is on a key-switch key or outside the instrument's range; every note-on has its note-off (no stuck notes, no re-struck held notes); nothing plays after the clip end; sustain pedal and pitch bend end at rest; controller data is not denser than an instrument can use. DRAG MIDI shows "checked: OK" (or "checked: repaired", with the details in its tooltip). In the automatic tests no generated file needed a repair (104 files over every profile); injected faults are all repaired and a second check finds nothing.
- Repetition keys follow the REPEAT row: Never, Auto (every second fast repeat), Always (every repeat within a beat).
- Old projects open with every family at 100% / Auto, which is the Professional Balance setting.
- Ghost fills are now part of the plan: they stay only where the groove calls for them (funk, neo-soul, gospel, Afrobeats; none in ballads), off the main beats, and the ones that lose are taken out of the line before anything else is decided.
- Measured over all styles (8 takes each): ghost notes 21% -> 8%, palm mute 7.6% -> 4.4% (only in rock and muted styles), legato / hammer-ons / pull-offs 4.6% -> 9%, slides about 6% (as before, but now landing on chord changes and position shifts), plain notes 51% -> 60%. Ballads: no ghost notes, no palm mute. Slides are now chosen for chord changes, position shifts and phrase entries, and stay on one string so EZbass and the Efimov basses really receive them.
- Funk and neo-soul bass lines move mostly in roots and octaves, so legato there stays around 7% (it is used where the hands allow it, never forced); set LEGATO to Always for more.
## v2.10: The articulation brain
- **Articulations chosen like a bass player would.** Before, a take was mostly plain notes plus ghost notes and palm mutes. Every take now goes through a new articulation brain that reads the line before it picks an articulation: the gap to the next note, the interval, chord changes, downbeats, phrase starts and ends, section ends, fast runs. Then:
  - **Slides** into the chord change and the "one", and across leaps.
  - **Hammer-ons and pull-offs** on quick steps up and down.
  - **Ghost notes** as pickups before the beat.
  - **Slide out** (new) on exposed phrase ends and section ends.
  - **Slap and pop** (Pop is new) in slap passages: slap on the low notes and the beat, pop on the octaves and high notes.
  - **Palm-muted passages**, **dead mutes** (new, short percussive notes), **harmonics** on the occasional held high note, plus legato, staccato and marcato accents.
- **Whole phrases, not single notes.** Palm mute and slap come in passages, the way a player commits to a sound for a few bars. When a riff comes back, it comes back with the same articulations.
- **The style decides the flavour.** Funk, disco and neo-soul slap and pop with ghost notes; gospel slides and plays legato; rock palm-mutes; reggae plays short and muted; jazz plays legato with slide-outs; Afrobeat and Amapiano slide into the one with ghost pickups; ballads and fretless lines play legato. No articulation takes over a take: each has its own limit, and a take with almost no colour gets a slide and a slide-out.
- **ARTICULATION knob** (SOUND & FEEL): PLAIN = almost every note plain, EXPRESSIVE = the full vocabulary. Default 60 %.
- **On EZbass and the Ilya Efimov basses**, every one of these is written as the instrument's key-switch. Measured over 260 takes across every style:
  - EZbass gets 9 different key-switches: ghost, legato (hammer / pull), palm mute, slap, slide, glissando, mute, harmonic, repetition.
  - The Efimov Modern Bass gets 19 different keys: hammer / pull G4, slide B4, mute A#4, slap G#4, Pop A1, gliss A#0, X-notes F0, fingers, strings, repetition and modes.
  - Pop is sent as the profile's own Pop key when it has one (Efimov A1), as slap on EZbass, and as a plain note elsewhere.
- Fixed: a crash that could happen when the instrument changed while a project was playing (a sound-source setting that Enoch Bass does not have was read as if it existed). Any missing setting now reads as 0.
## v2.9: Key-switches you can see, held for the whole note
- **Key-switches written for your instrument.** When the main part plays on an external instrument (ROUTING: External VST / External + built-in / Hardware, or the EXTERNAL sound source), Enoch writes that instrument's key-switches from its mapping profile into the MIDI: live to the instrument, and into the MIDI you drag or save. Each key goes down about 20 ms before its note (at any tempo), is held until that note ends (EZbass: hold G0 while the note plays) and is let go before the same key is used again. On the built-in sound, a one-shot or a SoundFont there are no key-switches: every note is a note.
- **You see them in the piano roll.** The instrument's key-switch rows are tinted and named (e.g. "G0  Slide"), and every key-switch Enoch wrote sits on its key, held under the note it plays: exactly what goes into the MIDI. Right-click one to change the note's articulation or play it plainly; drag it up or down onto another key to give the note that articulation. Draw a note on a key-switch row to add a key-switch by hand: it is sent at that key, with the profile's velocity, exactly where you put it (never humanized, never moved).
- **Note start edge:** drag the left edge of a note to make it start earlier or later; its end stays put (the right edge works as before).
- **Double-click the ruler** (bar numbers) to play, double-click again to stop.
- **The view is remembered** with the project: zoom, scroll position, lane and lane height, tool, and the open drawer come back when you reopen Enoch.
- **Grid lines:** 16th lines are bright cyan and 32nd lines violet, so they stand out while you edit (the Grid slider still sets how strong all lines are). Beat and 8th lines are a little brighter too.
- Checked automatically on every build: the MIDI file of every profile, note by note (each key-switch present, 20 ms before its note, right velocity, released in time, never pressed while still held, no played note on a key); held key-switches; plain notes on the built-in sound and a one-shot; the drawn key-switch; start-edge resize; ruler double-click; the saved view; plus all the earlier freeze, removal and silent-note checks.
## v2.8: Exact Ilya Efimov basses and EZbass, piano-roll controls, mapping builder
- **Ilya Efimov basses** (Modern, Classic, Fretless, Retro, Rock), key for key from the Kontakt key-switch page: hammer-on / pull-off G4 is now its own articulation, slap G#4 on every bass; Pop, FX glissando, repetition mute, Hit 1 / Hit 2, shift fret position -1/-3 and +1/+3, set fret position 0-9 and the mode keys are in the Articulation menu under the profile's name. Five-string: Modern, Fretless, Rock; four-string: Classic, Retro.
- **Toontrack EZbass** follows the key-switch list you supplied: legato G#0 (held), slide F#0 (held), slide up G0, slide down F0, ghost E0, loud mute D#0, palm mute C0 (held), harmonic A#-1 (held), slap / pop B-1 (held), repeater D0; polyphony A-1 and middle finger / down stroke C#0 are in the Articulation menu. (Not yet checked against the Toontrack manual.)
- **Follow** (bar row above the piano roll): while it plays, the roll turns the page with the playhead when you are zoomed in. Off = the view stays where you put it. Saved with the project.
- **Arrow keys** (click the roll first): Up / Down move the selected notes one step; in a scale-locked bar (or with Snap scale on) that is the next note of the scale. Shift + Up / Down = an octave. Left / Right select the previous / next note (Shift adds to the selection, the view follows). Alt + Left / Right move the notes in time by the grid (what Left / Right did before).
- **Dragging past an edge** of the roll (selection box, moving or resizing notes) scrolls the view that way, faster the further you go. The selection box stays anchored to where you started, so you can select far more than one page.
- **Ghost** (bar row): the other Enoch plugin's notes show faintly behind these (Enoch Bass in Enoch Solo and the other way round), to line the parts up. Notes outside the view show as ticks on the top or bottom edge. Saved with the project.
- **Loop sel.** (bar row, or Ctrl+L, or the Edit menu): PLAY loops the selected notes or bars. Notes still sounding at the end of the loop are released there. Click again to stop looping. (The DAW's own transport is not affected.)
- **Humanize** the selected notes (H, or Edit > Humanize light / medium / strong): small random changes to timing and velocity. Undo takes it back.
- **Favourite takes** (the small "v" next to TAP): Keep this take as a favourite, load it again later in any project, or delete it. They are kept in %APPDATA%\Enoch.
- **?** (end of the first toolbar row, or the ? key / F1 with the roll clicked): every keyboard shortcut of the piano roll.
- **Mapping builder** (ROUTING > Edit): New profile starts an empty profile for any instrument. + Custom key adds one of the instrument's own keys with its own name (Pop, Down stroke, Set fret position 3 ...); custom keys appear in the piano roll's Articulation menu, so you can place them on notes by hand. MODE KEYS are sent once when playback starts (e.g. G0@120, F#0@40). Paste list... reads a key-switch list as the manual or Kontakt shows it ("Slide up 1 slow/fast (velo)  G#0 (32)"): Enoch fills in the articulations it recognises ("A / B (velo)" = low velocity A, high velocity B; "on/off" = 120 on, 40 off) and adds the rest as custom keys. Test all plays every key in turn with a test note. The editor uses the full height of the roll.
- **Safety, silent notes:** a played note never lands on a key that the chosen profile uses as a key-switch (the instrument would switch instead of sounding). Such a note moves by an octave to the nearest free key. The same holds for Enoch's standard layout when the Key-switch Base Note is moved into the playing range.
- **Safety, routing:** a part routed to an instrument with its own mapping profile now gets that profile's key-switches, even when the main panel's Instrument of choice says something else.
- **Safety, freezes:** nothing new is kept in static memory, test notes and Test all run on the panel's own timer (nothing stays scheduled when a window closes), favourites need no pop-up window, and a removed instance deletes its ghost-notes file. Every build passes the automatic checks for these, plus the stress test (600 random changes while playing, with removal while notes sound).
## v2.7: Silent root note fix, output section, tap rhythm and more
- Fixed: in some keys the root note (and a few notes near it) made no sound with the built-in sound, a one-shot or a SoundFont. Those notes sit on the key-switch keys of the chosen instrument layout (EZbass: F1 = 29, G1 = 31, G#1 = 32, ...), and the preview player treated them as articulation switches. Notes of the take now always sound; only notes you play in can act as key-switches.
- Notes start again when a new take arrives while playing (key change, TRY ANOTHER ...), so a held root note is not lost.
- MIDI DELAY (ROUTING page) now goes both ways: -200 to +200 ms. Left of 0 sends the MIDI early, which lines up an instrument the DAW plays late while you monitor it live (EZbass with Monitor on).
- OUTPUT drawer (footer button): master volume (default -3 dB), part levels, compressor (off by default: threshold, ratio, attack, release, knee, makeup) and SAFE OUTPUT, a limiter that keeps every note below the ceiling (on by default, -1 dB). Meter with peak hold, gain reduction and a clip light; the OUTPUT button shows CLIP if the sound reached 0 dB. Built-in sound only.
- The window opens at the size you last used (per project, and as the default for new windows).
- New instances start in C major. Saved projects keep their own key.
- The piano roll no longer redraws itself completely 30 times a second while playing (only the playhead strip), which keeps scrolling smooth.
- TAP (next to TRY ANOTHER): tap a rhythm with the mouse, computer keys or your MIDI keyboard, then USE RHYTHM. The take is written on that rhythm, and TRY ANOTHER keeps it.
- The small "v" next to TAP: TRY ANOTHER can keep the rhythm, the notes or the articulations of the current take, and step to earlier or later takes.
## v2.6: Removal freeze fix
- Removing the plugin from a project (or closing the project) froze the DAW on Windows, in Studio One and Cubase. The panel textures were kept in static memory until Windows unloaded the plugin, after JUCE had shut down its Direct2D graphics; freeing them then could hang the DAW. They now belong to the open plugin windows and are freed when the last Enoch window closes.
- A short log is written to %APPDATA%\Enoch\enoch-log.txt: instance created, window opened / closed, textures freed, each step of removal, plugin unloaded. If the DAW ever stops responding, the last line shows where.
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
