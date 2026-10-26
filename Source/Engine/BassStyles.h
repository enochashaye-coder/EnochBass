#pragma once
// Enoch Bass - bass "player" styles. Each one is a set of tendencies (see Style): how busy, how syncopated, how much ghosting, sliding, slapping, octave jumping.
// They are broad descriptions of a way of playing, not copies of anyone's recordings.
#include "Styles.h"

namespace enochsolo
{
inline std::vector<Style> bassStyles()
{
    const int B = I_Bass;
    const char* AF = "African & Afrocentric"; const char* PS = "Pop & Soul"; const char* RB = "Rock & Blues";
    const char* JZ = "Jazz"; const char* BR = "Brazilian"; const char* SP = "Spanish & Latin";
    const char* CR = "Caribbean & Reggae"; const char* FD = "Funk, Hip-Hop & Dance";
    std::vector<Style> v;
    // density = how many of the pattern's hits are played; sync = push hits a 16th early; ghost = ghost-note fills; octave = octave jumps;
    // slide / hammer / pull = glides and legato links; slap / palm = slap & pop weight / palm-muted notes; legato = note length; step = passing-note use
    v.push_back (mkS ("Afrobeat Classic", B, 0, AF, "Afrobeat, Afro-Jazz", "Tight repeating ostinato: root, fifth and octave pushed off the beat, short and dry",
        "density .8, rest .25, sync .45, motif .85, legato .55, stacc .35, laid 4, vel 88, ghost .22, octave .22, slide .04, hammer .05, pull .05, palm .15, step .2, dyn .35"));
    v.push_back (mkS ("Afrobeats Laid-back", B, 0, AF, "Afro-Fusion, Pop", "Modern Afrobeats: roomy, round notes that sit behind the beat, a slide into the changes",
        "density .55, rest .45, sync .35, motif .8, legato .85, stacc .1, laid 9, vel 80, ghost .1, octave .12, slide .22, hammer .05, pull .05, step .25, dyn .3"));
    v.push_back (mkS ("Amapiano Log-Drum Bass", B, 0, AF, "Amapiano", "Deep bouncing sub notes that answer the log drum: long gliding tones with syncopated pickups",
        "density .65, rest .4, sync .5, motif .85, legato .92, stacc .05, laid 8, vel 84, ghost .04, octave .1, slide .45, hammer .02, pull .02, step .15, dyn .25"));
    v.push_back (mkS ("Amapiano Private-School Glide", B, 0, AF, "Amapiano", "Smoother, jazzier amapiano bass: sustained roots and fifths with long slides between chords",
        "density .5, rest .45, sync .4, motif .8, legato .97, stacc .03, laid 7, vel 78, ghost .03, octave .08, slide .6, hammer .02, pull .02, step .3, dyn .2"));
    v.push_back (mkS ("Highlife Bounce", B, 0, AF, "Highlife, Juju / Fuji", "Bright walking bounce between root and fifth, lively and melodic",
        "density .8, rest .2, sync .3, motif .7, legato .75, stacc .2, laid 3, vel 84, ghost .08, octave .25, slide .06, hammer .08, pull .08, step .5, dyn .4"));
    v.push_back (mkS ("Juju Interlock", B, 0, AF, "Juju / Fuji", "Interlocking low pulse that rolls under the talking drums",
        "density .85, rest .2, sync .4, motif .8, legato .65, stacc .25, laid 4, vel 82, ghost .18, octave .3, slide .05, hammer .08, pull .08, step .3, dyn .4"));
    v.push_back (mkS ("Soukous Runner", B, 0, AF, "Soukous", "Running octave-and-fifth bass that drives the sebene",
        "density .95, rest .1, sync .35, motif .75, legato .7, stacc .2, laid 2, vel 86, ghost .12, octave .5, slide .04, hammer .1, pull .1, step .3, dyn .4"));
    v.push_back (mkS ("Ethio-Jazz Modal Pulse", B, 0, AF, "Ethio-Jazz, Afro-Jazz", "Hypnotic modal pedal: wide-spaced notes with minor-pentatonic colour",
        "density .6, rest .35, sync .3, motif .9, legato .8, stacc .1, laid 5, vel 80, ghost .06, octave .2, slide .1, hammer .06, pull .06, step .35, dyn .3"));
    v.push_back (mkS ("Township Jive Walk", B, 0, AF, "Township Jive, Highlife", "Joyous bouncing walk, steady and singing",
        "density .85, rest .15, sync .25, motif .7, legato .8, stacc .15, laid 2, vel 84, ghost .05, octave .2, slide .08, hammer .06, pull .06, step .6, dyn .4"));
    v.push_back (mkS ("Bakithi Kumalo Style", B, 0, AF, "Township Jive, Afro-Fusion, Pop", "Fretless, singing lines: slides into notes and little melodic fills",
        "density .7, rest .3, sync .4, motif .6, legato .9, stacc .05, laid 4, vel 82, ghost .08, octave .15, slide .4, hammer .1, pull .1, step .55, harm .06, dyn .5"));
    v.push_back (mkS ("Richard Bona Style", B, 0, AF, "Afro-Jazz, Afro-Fusion", "Vocal, melodic bass: flowing lines, harmonics and gentle slides",
        "density .65, rest .3, sync .45, motif .55, legato .9, stacc .05, laid 3, vel 78, ghost .1, octave .15, slide .3, hammer .12, pull .12, step .6, harm .1, dyn .5"));
    v.push_back (mkS ("James Jamerson Style", B, 0, PS, "Gospel / Soul, Pop", "Busy, syncopated Motown pocket with chromatic approaches and ghosted fills",
        "density .9, rest .12, sync .5, motif .5, legato .8, stacc .1, laid 2, vel 86, ghost .25, octave .15, slide .1, hammer .12, pull .12, step .7, dyn .5"));
    v.push_back (mkS ("Larry Graham Style", B, 0, PS, "Gospel / Soul, Pop, Afrobeat", "Slap-and-pop funk: thumb on the roots, pops on the octaves, lots of ghosts",
        "density .85, rest .15, sync .5, motif .75, legato .45, stacc .4, laid 0, vel 96, ghost .3, octave .45, slide .03, hammer .04, pull .04, slap .9, palm .05, step .2, dyn .5"));
    v.push_back (mkS ("Marcus Miller Style", B, 0, PS, "Pop, Gospel / Soul, Afro-Fusion", "Modern groove: fingerstyle roots with slap accents and tasteful runs",
        "density .8, rest .15, sync .5, motif .65, legato .6, stacc .25, laid 1, vel 94, ghost .25, octave .35, slide .08, hammer .08, pull .08, slap .5, step .4, dyn .55"));
    v.push_back (mkS ("Pino Palladino Style", B, 0, PS, "Afro-Fusion, Gospel / Soul, Pop", "Deep fretless pocket: sparse, warm notes, behind the beat with little slides",
        "density .45, rest .5, sync .35, motif .8, legato .92, stacc .05, laid 10, vel 72, ghost .06, octave .08, slide .3, hammer .04, pull .04, step .3, dyn .3"));
    v.push_back (mkS ("Pop Driving Eighths", B, 0, PS, "Pop", "Clean, steady eighth-note roots that lock to the kick",
        "density .75, rest .15, sync .15, motif .9, legato .65, stacc .15, laid 0, vel 84, ghost .02, octave .15, slide .02, palm .25, step .15, dyn .25"));
    v.push_back (mkS ("Gospel Walk-ups", B, 0, PS, "Gospel / Soul, Smooth / Ballad", "Soulful roots with stepwise walk-ups and slides into the next chord",
        "density .6, rest .3, sync .3, motif .6, legato .9, stacc .05, laid 5, vel 80, ghost .08, octave .1, slide .25, hammer .1, pull .1, step .75, dyn .45"));
    v.push_back (mkS ("Rock Eighths (Palm-muted)", B, 0, RB, "Rock", "Hard-driving palm-muted eighth notes on the roots",
        "density .9, rest .08, sync .1, motif .9, legato .55, stacc .3, laid 0, vel 92, ghost .02, octave .15, palm .6, step .1, dyn .25"));
    v.push_back (mkS ("Blues Shuffle", B, 0, RB, "Rock, Swing Jazz", "Walking shuffle: root, third, fifth, sixth climbing and returning",
        "density .85, rest .1, sync .1, motif .8, legato .8, stacc .1, laid 2, vel 82, ghost .05, octave .1, step .65, swing .6, dyn .35"));
    v.push_back (mkS ("Ron Carter Style Walking", B, 0, JZ, "Swing Jazz, Bossa Nova, Afro-Jazz", "Melodic walking quarter notes with chromatic approach tones",
        "density .8, rest .08, sync .1, motif .3, legato .85, stacc .1, laid 3, vel 78, ghost .06, octave .05, step .85, swing .6, hammer .04, pull .04, dyn .45"));
    v.push_back (mkS ("Jaco Pastorius Style", B, 0, JZ, "Afro-Jazz, Samba / Brazilian, Pop", "Fluid fretless virtuoso: sixteenth runs, harmonics, singing slides",
        "density .9, rest .12, sync .5, motif .4, legato .85, stacc .1, laid 1, vel 88, ghost .25, octave .3, slide .3, hammer .15, pull .15, step .65, harm .15, dyn .6"));
    v.push_back (mkS ("Bossa Root-Fifth", B, 0, BR, "Bossa Nova", "Soft root-and-fifth bossa heartbeat",
        "density .65, rest .25, sync .3, motif .9, legato .85, stacc .05, laid 4, vel 70, ghost .02, octave .08, step .2, dyn .2"));
    v.push_back (mkS ("Samba Partido", B, 0, BR, "Samba / Brazilian", "Driving samba bass: pulsing roots and fifths on the surdo pattern",
        "density .85, rest .12, sync .45, motif .85, legato .6, stacc .25, laid 1, vel 84, ghost .06, octave .2, step .3, dyn .35"));
    v.push_back (mkS ("Latin Tumbao", B, 0, SP, "Salsa, Afro-Cuban, Flamenco / Spanish", "Afro-Cuban tumbao: anticipates the downbeat, leaves space, steady and round",
        "density .6, rest .35, sync .55, motif .9, legato .8, stacc .1, laid 3, vel 80, ghost .05, octave .25, step .25, dyn .3"));
    v.push_back (mkS ("Salsa Tumbao (Clave-Aware)", B, 0, SP, "Salsa, Afro-Cuban", "Classic salsa tumbao: lands on the and of 2 and beat 4, anticipates the next bar, alternates with the clave",
        "density .6, rest .4, sync .6, motif .9, legato .75, stacc .15, laid 3, vel 82, ghost .04, octave .12, slide .03, step .2, dyn .3"));
    v.push_back (mkS ("Son Montuno Anticipated Bass", B, 0, SP, "Afro-Cuban, Salsa", "Old-school son: anticipated bass that answers the bombo and the tres, short and woody",
        "density .55, rest .45, sync .6, motif .9, legato .55, stacc .3, laid 3, vel 80, ghost .03, octave .1, step .2, dyn .3"));
    v.push_back (mkS ("Mambo Driving Pulse", B, 0, SP, "Afro-Cuban, Salsa", "Hard-driving mambo: eighth-note drive with fifths and octave punches under the horns",
        "density .85, rest .15, sync .5, motif .8, legato .6, stacc .25, laid 1, vel 90, ghost .08, octave .35, step .3, dyn .4"));
    v.push_back (mkS ("Salsa Dura Busy", B, 0, SP, "Salsa", "Busy New York salsa dura: tumbao with fast passing tones and syncopated fills",
        "density .9, rest .12, sync .6, motif .6, legato .7, stacc .15, laid 2, vel 90, ghost .12, octave .2, slide .06, hammer .06, pull .06, step .5, dyn .5"));
    v.push_back (mkS ("Cachao Style Descarga", B, 0, SP, "Afro-Cuban, Salsa, Afro-Jazz", "Cuban jam-session bass (the mambo bassist's way): melodic ostinatos that walk and sing",
        "density .8, rest .2, sync .55, motif .65, legato .75, stacc .2, laid 2, vel 84, ghost .06, octave .2, step .6, dyn .5"));
    v.push_back (mkS ("Timba / Songo Funk-Latin", B, 0, SP, "Afro-Cuban, Funk & Disco", "Modern Cuban timba: slap pops, ghost notes and sixteenth runs on top of the clave",
        "density .95, rest .08, sync .65, motif .5, legato .55, stacc .3, laid 0, vel 94, ghost .28, octave .3, slap .35, hammer .1, pull .1, step .4, dyn .6"));
    v.push_back (mkS ("Rumba Guaguanco Pulse", B, 0, SP, "Afro-Cuban", "Rumba pulse: low drum-like hits that lock with quinto and clave",
        "density .8, rest .2, sync .55, motif .8, legato .5, stacc .35, laid 2, vel 86, ghost .12, octave .15, step .25, dyn .35"));
    v.push_back (mkS ("Cha-Cha-Cha Walk", B, 0, SP, "Afro-Cuban, Salsa", "Light, bouncy cha-cha-chá: steady quarters with the three-note tag",
        "density .6, rest .3, sync .3, motif .85, legato .6, stacc .3, laid 3, vel 76, ghost .02, octave .1, step .45, dyn .25"));
    v.push_back (mkS ("Bolero Slow Pulse", B, 0, SP, "Afro-Cuban, Smooth / Ballad", "Romantic bolero: slow, round, singing roots and fifths",
        "density .4, rest .55, sync .2, motif .85, legato .95, stacc .02, laid 6, vel 66, ghost 0, octave .06, slide .1, step .3, dyn .2"));
    v.push_back (mkS ("Latin Jazz Bajo Anticipado", B, 0, SP, "Afro-Jazz, Afro-Cuban", "Afro-Cuban jazz: anticipated bass with walking passages and modal colour",
        "density .75, rest .22, sync .55, motif .6, legato .8, stacc .15, laid 2, vel 82, ghost .08, octave .15, step .65, swing .15, dyn .5"));
    v.push_back (mkS ("Baiao Pulse", B, 0, BR, "Afro-Brazilian", "Northeast Brazil baiao: root and fifth in the zabumba pattern, dry and driving",
        "density .65, rest .3, sync .45, motif .9, legato .55, stacc .3, laid 2, vel 84, ghost .04, octave .12, step .2, dyn .3"));
    v.push_back (mkS ("Maracatu Alfaia Drive", B, 0, BR, "Afro-Brazilian", "Heavy maracatu: sixteenth pulse that answers the alfaia drums with big rolling weight",
        "density .9, rest .1, sync .35, motif .85, legato .5, stacc .35, laid 0, vel 94, ghost .1, octave .15, step .1, dyn .35"));
    v.push_back (mkS ("Afoxe / Ijexa Flow", B, 0, BR, "Afro-Brazilian, Afrobeat", "Candomble-rooted ijexa: gentle rolling bass that breathes with the agogo and the atabaque",
        "density .7, rest .3, sync .45, motif .85, legato .8, stacc .1, laid 4, vel 78, ghost .05, octave .1, slide .1, step .3, dyn .3"));
    v.push_back (mkS ("Samba-Reggae Olodum Punch", B, 0, BR, "Afro-Brazilian, Reggae & Dub", "Bahian samba-reggae: syncopated punches over a heavy surdo",
        "density .85, rest .15, sync .6, motif .8, legato .55, stacc .3, laid 1, vel 92, ghost .1, octave .25, step .15, dyn .4"));
    v.push_back (mkS ("Axe Bahia Bounce", B, 0, BR, "Afro-Brazilian, Funk & Disco", "Carnival axe bass: bouncing eighths with octave jumps and festival energy",
        "density .9, rest .1, sync .4, motif .8, legato .6, stacc .3, laid 0, vel 94, ghost .08, octave .4, slap .1, step .25, dyn .4"));
    v.push_back (mkS ("Forro Oom-Pah", B, 0, BR, "Afro-Brazilian", "Forro: the bouncy two-beat root-fifth step of the zabumba",
        "density .7, rest .25, sync .3, motif .9, legato .5, stacc .35, laid 1, vel 82, ghost .02, octave .1, step .2, dyn .3"));
    v.push_back (mkS ("Partido Alto Samba Groove", B, 0, BR, "Samba / Brazilian, Afro-Brazilian", "Rolling samba de roda bass: surdo patterns with melodic turnarounds",
        "density .8, rest .15, sync .5, motif .75, legato .65, stacc .2, laid 1, vel 84, ghost .06, octave .15, step .45, dyn .4"));
    v.push_back (mkS ("MPB Melodic Bass", B, 0, BR, "Samba / Brazilian, Bossa Nova, Pop", "Melodic Brazilian pop bass: flowing, sung lines with chromatic turns",
        "density .65, rest .3, sync .45, motif .6, legato .9, stacc .05, laid 3, vel 76, ghost .03, octave .1, slide .15, step .65, dyn .5"));
    v.push_back (mkS ("Cumbia Colombiana Roll", B, 0, CR, "Caribbean", "Rolling cumbia: root-fifth sway with off-beat pickups",
        "density .6, rest .35, sync .4, motif .9, legato .7, stacc .15, laid 3, vel 78, ghost .03, octave .1, step .25, dyn .3"));
    v.push_back (mkS ("Merengue Tambora Drive", B, 0, CR, "Caribbean", "Fast merengue: relentless root-fifth eighths that chase the tambora",
        "density 1, rest .05, sync .25, motif .9, legato .45, stacc .4, laid 0, vel 90, ghost .04, octave .15, step .3, dyn .3"));
    v.push_back (mkS ("Bachata Bounce", B, 0, CR, "Caribbean", "Bachata: syncopated bounce, sweet and sentimental, with a pickup into each change",
        "density .7, rest .25, sync .5, motif .85, legato .7, stacc .15, laid 3, vel 80, ghost .03, octave .1, slide .06, step .35, dyn .35"));
    v.push_back (mkS ("Zouk Smooth Glide", B, 0, CR, "Caribbean, Smooth / Ballad", "Zouk and compas: slow-burning, round and gliding",
        "density .5, rest .45, sync .35, motif .85, legato .9, stacc .05, laid 6, vel 72, ghost .02, octave .08, slide .3, step .3, dyn .25"));
    v.push_back (mkS ("Reggae One-Drop Roots", B, 0, CR, "Reggae & Dub", "Roots reggae: melodic, round bass that leaves the first beat empty and sings around the one-drop",
        "density .55, rest .45, sync .4, motif .9, legato .85, stacc .08, laid 5, vel 82, ghost .02, octave .1, slide .08, step .45, dyn .3"));
    v.push_back (mkS ("Dub Heavy Sub", B, 0, CR, "Reggae & Dub", "Dub weight: huge sustained sub notes with lots of space",
        "density .3, rest .65, sync .25, motif .95, legato 1, stacc 0, laid 6, vel 92, ghost 0, octave .1, slide .15, step .1, dyn .15"));
    v.push_back (mkS ("Dancehall Riddim", B, 0, CR, "Reggae & Dub, Hip-Hop & Trap", "Dancehall riddim: repeated hypnotic loop with sliding sub",
        "density .5, rest .45, sync .55, motif .98, legato .8, stacc .1, laid 3, vel 88, ghost .02, octave .1, slide .3, step .1, dyn .2"));
    v.push_back (mkS ("Ska Walking Upbeat", B, 0, CR, "Reggae & Dub, Swing Jazz", "Ska: fast walking quarters under the offbeat guitar",
        "density .9, rest .1, sync .15, motif .5, legato .6, stacc .3, laid 0, vel 84, ghost .02, octave .1, step .8, swing .25, dyn .3"));
    v.push_back (mkS ("Funk Pocket Sixteenths", B, 0, FD, "Funk & Disco, Afrobeat", "In-the-pocket funk: ghost-note sixteenths around a fat one",
        "density .9, rest .12, sync .6, motif .65, legato .5, stacc .35, laid 0, vel 90, ghost .3, octave .25, slap .2, hammer .08, pull .08, step .35, dyn .55"));
    v.push_back (mkS ("Disco Octaves", B, 0, FD, "Funk & Disco", "Disco: driving octave eighths, bright and bouncy",
        "density 1, rest .05, sync .15, motif .95, legato .6, stacc .25, laid 0, vel 90, ghost .03, octave .85, step .15, dyn .25"));
    v.push_back (mkS ("Boom-Bap Hip-Hop", B, 0, FD, "Hip-Hop & Trap", "Boom-bap: laid-back sampled-style bass that sits behind the kick",
        "density .45, rest .5, sync .5, motif .9, legato .75, stacc .15, laid 8, vel 88, ghost .05, octave .1, slide .1, step .2, dyn .2"));
    v.push_back (mkS ("Trap 808 Slides", B, 0, FD, "Hip-Hop & Trap, Amapiano", "808 sub: long tails, stuttering restarts and wide glides",
        "density .55, rest .4, sync .65, motif .8, legato 1, stacc 0, laid 2, vel 96, ghost .02, octave .25, slide .6, step .1, dyn .25"));
    v.push_back (mkS ("Afro-House Rolling Offbeat", B, 0, FD, "Afro-House, Amapiano", "Four-on-the-floor house bass: rolling offbeat pulse with a lazy African swing",
        "density .75, rest .25, sync .6, motif .9, legato .55, stacc .3, laid 2, vel 86, ghost .06, octave .3, slide .08, step .25, dyn .3"));
    v.push_back (mkS ("Gqom Dark Bounce", B, 0, FD, "Afro-House, Amapiano", "Gqom: dark, minimal, heavy kick-bass hits with big gaps",
        "density .4, rest .55, sync .6, motif .95, legato .7, stacc .2, laid 1, vel 96, ghost .03, octave .1, slide .2, step .1, dyn .15"));
    return v;
}
} // namespace enochsolo
