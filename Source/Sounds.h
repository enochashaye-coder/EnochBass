#pragma once
// Enoch - built-in sound engine for the test player (header only, no JUCE needed).
//
//  Pianos          modal piano: inharmonic strings, 2 strings per note (beating + two-stage decay), hammer position / hardness by velocity,
//                  damper release, hammer thump, soundboard EQ.  Concert / studio / upright / felt / honky-tonk.
//  Electric keys   Rhodes (tine + bell mode + pickup bark), Wurlitzer (reed), FM e-piano, Hammond B3 (9 drawbars, key click, leslie), clavinet.
//  Synths          Prophet-5 style poly: 2 detuned PolyBLEP saws, 4-pole ladder filter with its own envelope, analog drift, chorus.
//  Strings         digital waveguide strings (Karplus-Strong with loop damping, pick position, pickup position, pickup resonance, drive, cab):
//                  Stratocaster, Telecaster, Les Paul, hollow-body, steel, nylon, P / J / slap / fretless / upright bass.
//  Winds / bowed   reed (tenor, alto, soprano, baritone), brass (trumpet, flugel, muted), bowed (violin, viola, fiddle, cello), with
//                  breath / bow noise, formants, micro pitch and level jitter, legato transfer.
//  Bass synths     Moog-style bass, 808 sub, amapiano log drum.
//
// These are models, not samples. They give a far more natural audition than a plain synth, but they are not a replacement for your
// own sample libraries: drag the MIDI into those for the final sound.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <vector>
#include "Engine/Styles.h"

namespace enochsolo
{
namespace snd
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;
constexpr int kArtSlap = 27;              // Enoch Bass only (A_Slap)
constexpr int kInstGuitar = 0, kInstSax = 1, kInstViolin = 2, kInstPiano = 3, kInstTrumpet = 4, kInstBass = 5;

enum Kind { K_Piano = 0, K_Rhodes, K_Wurli, K_FMEP, K_Organ, K_Poly, K_Pluck, K_Sax, K_Brass, K_Bow, K_Sub, K_Log, K_Lead, K_LogDrum, K_Classic,
           K_BowPM, K_SaxPM, K_BrassPM,
           K_LogDX };   // v3.0: the Amapiano log drum built on the original DX10 'Log Drum' FM recipe      // v2.7 batch 2: physical models (bowed string, reed + bore, lip + bore)
constexpr int kNumP = 28;

struct Patch
{
    const char* name = "";
    const char* group = "";                // heading in the sound menu
    int kind = K_Classic;
    float p[kNumP] = {};
    int chorus = 0;                        // 0 none, 1 light, 2 wide
    float eqLow = 0, eqMid = 0, eqHigh = 0; // dB: low shelf 150 Hz, peak 2.5 kHz, high shelf 8 kHz
    float level = 1.f;                     // loudness match between sounds
};

//------------------------------------------------------------------------------------------------ the sound lists
// Parameter meaning per kind:
//  K_Piano : p0 brightness, p1 hammer position, p2 decay scale, p3 string detune (cents), p4 thump, p5 inharmonicity scale, p6 stereo width
//  K_Rhodes: p0 bell, p1 bark drive, p2 decay scale, p3 auto-pan depth, p4 second harmonic
//  K_Wurli : p0 drive, p1 decay scale, p2 tremolo depth, p3 brightness
//  K_FMEP  : p0 index scale, p1 tine amount, p2 decay scale, p3 tine modulator ratio
//  K_Organ : p0..p8 drawbars (16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1'), p9 percussion, p10 leslie (0 slow, 1 fast), p11 drive
//  K_Poly  : p0 detune cents, p1 cutoff Hz, p2 filter env (octaves), p3 resonance 0..3.6, p4 A, p5 D, p6 S, p7 R (amp), p8 fA, p9 fD, p10 fS, p11 fR,
//            p12 keytrack 0..1, p13 pulse width (0 = saw), p14 noise
//  K_Pluck : p0 T60 at 110 Hz (s), p1 loop damping, p2 pick position, p3 hardness, p4 pickup 1 pos, p5 pickup 2 pos, p6 pickup resonance Hz, p7 Q,
//            p8 drive, p9 cab low-pass Hz (0 = none), p10 body resonance, p11 pick click, p12 pickup blend, p13 high-pass Hz, p14 auto vibrato (semis)
//  K_Sax   : p0 formant set (0 tenor 1 alto 2 soprano 3 baritone), p1 brightness, p2 breath, p3 growl, p4 reed drive, p5 attack scale, p6 auto vibrato
//  K_Brass : p0 brightness, p1 formant scale, p2 mute, p3 attack scale, p4 buzz drive, p5 breath, p6 auto vibrato
//  K_Bow   : p0 body scale, p1 brightness, p2 bow noise, p3 attack scale, p4 auto vibrato, p5 detune
//  K_Sub   : p0 decay (s), p1 pitch drop, p2 drive, p3 filter Hz (0 = none)
//  K_Log   : p0 decay (s), p1 pitch drop, p2 body ratio
//  K_Lead  : mono lead / bass synth. p0 osc A wave (0 saw, 1 pulse, 2 triangle, 3 sine, 4 supersaw, 5 hard-sync saw), p1 osc B wave (-1 off), p2 osc B semitones,
//            p3 osc B cents, p4 osc B level, p5 sub (square, octave down), p6 noise, p7 pulse width, p8 PWM depth, p9 cutoff Hz, p10 resonance 0..3.6,
//            p11 filter env (octaves), p12 key-track, p13 A, p14 D, p15 S, p16 R, p17 filter D, p18 filter S, p19 glide (s), p20 auto vibrato (semis),
//            p21 drive, p22 echo mix, p23 echo time (s), p24 talkbox vowel amount, p25 sync sweep (semis), p26 echo feedback, p27 vibrato rate (Hz, 0 = 5.4)
inline bool isLogKind (int k) { return k == K_LogDrum || k == K_LogDX; }

inline const std::vector<Patch>& patchList (int inst)
{
    static const std::vector<std::vector<Patch>> all = []
    {
        std::vector<std::vector<Patch>> a (6);
        const char* grp = "";
        auto P = [&grp] (const char* n, int k, std::initializer_list<float> v, int ch = 0, float lo = 0, float mid = 0, float hi = 0, float lvl = 1.f)
        {
            Patch p; p.name = n; p.group = grp; p.kind = k; int i = 0; for (float f : v) if (i < kNumP) p.p[i++] = f;
            p.chorus = ch; p.eqLow = lo; p.eqMid = mid; p.eqHigh = hi; p.level = lvl; return p;
        };
        auto classic = [&] { grp = "Original"; return P ("Classic (original preview sound)", K_Classic, {}); };

        // ---------------- lead synths: offered with every Enoch Solo instrument (the solo keeps that instrument's phrasing)
        grp = "Lead synths";
        //                                       A    B    Bst  Bct  Blv  sub  nz   pw   pwm  cut     res   fenv  key  A      D     S     R     fD    fS    glide vib   drv   echo eT    vow  sync fb   vRate
        const std::vector<Patch> leads = {
            P ("Moog lead (fat, glide)",          K_Lead, { 0, 0, 0, 7, 1.f, .5f, 0, .5f, 0, 1500, .35f, 2.0f, .5f, .004f, .35f, .85f, .12f, .45f, .45f, .05f, .10f, 1.6f, .12f, .33f, 0, 0, .30f, 5.4f }, 0, 0, 0, 0, 1.0f),
            P ("Gospel Moog lead (singing)",      K_Lead, { 1, 0, 0, -6, .8f, .35f, 0, .5f, 0, 1900, .25f, 1.6f, .5f, .004f, .4f, .85f, .14f, .45f, .5f, .07f, .22f, 1.3f, .15f, .30f, 0, 0, .25f, 5.8f }, 0, 0, 0, 0, 1.0f),
            P ("Soul lead (Minimoog, wide vibrato)", K_Lead, { 0, 1, -12, 0, .6f, 0, 0, .5f, 0, 1300, .40f, 1.8f, .5f, .006f, .4f, .8f, .14f, .4f, .45f, .08f, .30f, 1.4f, .12f, .30f, 0, 0, .25f, 5.6f }, 0, 0, 0, 0, 1.05f),
            P ("G-funk whistle (sine, long glide)", K_Lead, { 3, 2, 12, 0, .15f, 0, 0, .5f, 0, 6000, 0, 0, .3f, .02f, .3f, 1.f, .2f, .3f, 1.f, .16f, .28f, .8f, .18f, .38f, 0, 0, .35f, 5.2f }, 0, 0, 0, 0, 1.0f),
            P ("Talkbox lead (vowel)",            K_Lead, { 0, 0, 0, 5, .6f, .2f, 0, .5f, 0, 4000, .1f, .5f, .4f, .008f, .4f, .9f, .12f, .5f, .5f, .06f, .12f, 1.2f, .10f, .30f, .9f, 0, .25f, 5.4f }, 0, 0, 0, 0, 1.3f),
            P ("Prophet sync lead",               K_Lead, { 5, 1, -12, 0, .3f, 0, 0, .5f, 0, 2600, .2f, 1.2f, .5f, .003f, .45f, .85f, .14f, .5f, .2f, .03f, .08f, 1.5f, .15f, .30f, 0, 19, .30f, 5.4f }, 0, 0, 0, 0, 0.9f),
            P ("Fuzz sync screamer",              K_Lead, { 5, 0, 0, 10, .5f, 0, 0, .5f, 0, 4000, .3f, 1.5f, .5f, .003f, .4f, .9f, .14f, .5f, .3f, .03f, .15f, 4.0f, .12f, .30f, 0, 24, .25f, 5.6f }, 0, 0, 0, 0, 0.75f),
            P ("Supersaw lead (trance, wide)",    K_Lead, { 4, 0, 12, 0, .25f, 0, 0, .5f, 0, 5000, .15f, .8f, .4f, .005f, .5f, .9f, .3f, .5f, .6f, .02f, .06f, 1.0f, .25f, .375f, 0, 0, .40f, 5.4f }, 2, 0, 0, 0, 0.9f),
            P ("Hard detuned saw (EDM / trap)",   K_Lead, { 0, 0, 0, 18, 1.f, .4f, 0, .5f, 0, 6000, .1f, .4f, .4f, .003f, .4f, .95f, .12f, .4f, .7f, .02f, .05f, 2.0f, .18f, .30f, 0, 0, .30f, 5.4f }, 1, 0, 0, 0, 0.85f),
            P ("80s square lead (PWM)",           K_Lead, { 1, 1, 0, 9, .5f, 0, 0, .5f, .35f, 2400, .2f, 1.0f, .5f, .004f, .4f, .85f, .14f, .45f, .5f, .03f, .10f, 1.0f, .15f, .30f, 0, 0, .30f, 5.4f }, 1, 0, 0, 0, 1.0f),
            P ("Brass lead (OB style)",           K_Lead, { 0, 0, 0, -8, 1.f, 0, 0, .5f, 0, 900, .15f, 2.6f, .5f, .03f, .4f, .75f, .18f, .4f, .45f, .03f, .12f, 1.2f, .10f, .30f, 0, 0, .25f, 5.4f }, 1, 0, 0, 0, 1.0f),
            P ("Soft R&B lead (sine + triangle)", K_Lead, { 3, 2, 0, 3, .4f, 0, 0, .5f, 0, 3000, 0, .3f, .4f, .015f, .5f, .9f, .25f, .5f, .7f, .045f, .14f, .7f, .20f, .33f, 0, 0, .30f, 5.4f }, 1, 0, 0, 0, 1.15f),
            P ("Afrobeats pluck lead",            K_Lead, { 1, 0, 12, 0, .35f, 0, 0, .5f, 0, 2200, .3f, 2.2f, .5f, .001f, .32f, .15f, .2f, .2f, .1f, 0, 0, 1.0f, .30f, .27f, 0, 0, .35f, 5.4f }, 1, 0, 0, 0, 1.6f),
            P ("Amapiano soft lead",              K_Lead, { 3, 2, 12, 0, .3f, 0, 0, .5f, 0, 2500, 0, .4f, .4f, .01f, .6f, .6f, .35f, .5f, .5f, .03f, .08f, .8f, .32f, .36f, 0, 0, .40f, 5.4f }, 2, 0, 0, 0, 1.2f),
            P ("Flute synth lead (breathy)",      K_Lead, { 2, 3, 12, 0, .2f, 0, .12f, .5f, 0, 3500, .1f, .4f, .4f, .04f, .3f, .9f, .15f, .4f, .7f, .04f, .18f, .8f, .20f, .30f, 0, 0, .30f, 5.2f }, 0, 0, 0, 0, 1.2f),
            P ("Acid 303 lead",                   K_Lead, { 0, -1, 0, 0, 0, 0, 0, .5f, 0, 500, 3.2f, 3.5f, .5f, .002f, .25f, .1f, .08f, .22f, 0, .05f, 0, 2.2f, .12f, .30f, 0, 0, .30f, 5.4f }, 0, 0, 0, 0, 2.0f),
            P ("Chiptune pulse lead",             K_Lead, { 1, -1, 0, 0, 0, 0, 0, .125f, 0, 12000, 0, 0, 0, .001f, .2f, .9f, .05f, .3f, 1.f, 0, .15f, .6f, .08f, .15f, 0, 0, .20f, 7.0f }, 0, 0, 0, 0, 0.8f),
            P ("Moog lead with dotted echo",      K_Lead, { 0, 0, 0, 7, 1.f, .5f, 0, .5f, 0, 1700, .35f, 2.0f, .5f, .004f, .35f, .85f, .14f, .45f, .45f, .05f, .10f, 1.6f, .35f, .375f, 0, 0, .45f, 5.4f }, 0, 0, 0, 0, 1.0f),
        };
        auto withLeads = [&] (std::vector<Patch> v) { v.insert (v.end(), leads.begin(), leads.end()); v.push_back (classic()); return v; };

        // ---------------- guitar
        grp = "Electric guitars";
        std::vector<Patch> gtr = {
            P ("Stratocaster (clean, neck + middle)", K_Pluck, { 5.6f, .30f, .20f, .55f, .24f, .16f, 4300.f, 1.6f, .25f, 5200.f, 0.f, .35f, .5f, 75.f }, 0, 0, 0, 0, 1.3f),
            P ("Stratocaster bridge (bright, hot)", K_Pluck, { 5.0f, .26f, .13f, .70f, .075f, 0.f, 4800.f, 1.9f, .45f, 5600.f, 0.f, .45f, 1.f, 80.f }, 0, 0, 1.f, 0, 1.0f),
            P ("Telecaster bridge (twang)", K_Pluck, { 4.6f, .24f, .10f, .78f, .06f, 0.f, 5400.f, 2.4f, .45f, 6200.f, 0.f, .55f, 1.f, 85.f }, 0, -1.f, 2.f, 0, .8f),
            P ("Les Paul neck (warm humbucker)", K_Pluck, { 6.4f, .42f, .22f, .45f, .20f, 0.f, 2700.f, 1.2f, .55f, 4300.f, 0.f, .25f, 1.f, 70.f }, 0, 1.5f, -1.f, 0, 1.1f),
            P ("Les Paul bridge (crunch)", K_Pluck, { 7.0f, .38f, .12f, .60f, .085f, 0.f, 3000.f, 1.5f, 3.2f, 4600.f, 0.f, .30f, 1.f, 90.f }, 0, 1.f, 1.f, 0, 0.85f),
            P ("Jazz hollow-body (ES-175 style)", K_Pluck, { 4.2f, .56f, .20f, .30f, .20f, 0.f, 2100.f, 1.1f, .20f, 3300.f, .25f, .20f, 1.f, 75.f }, 0, 2.f, -1.5f, 0, 1.5f),
            P ("Stratocaster fuzz lead", K_Pluck, { 8.0f, .30f, .13f, .60f, .075f, .16f, 3600.f, 1.4f, 9.f, 3800.f, 0.f, .25f, .5f, 110.f }, 0, 0, 0, 0, 0.7f),
        };
        grp = "Acoustic guitars";
        gtr.push_back (P ("Acoustic steel-string", K_Pluck, { 3.8f, .25f, .18f, .62f, 0.f, 0.f, 0.f, 0.f, .1f, 0.f, 1.f, .55f, 0.f, 60.f }, 0, 0, 0, 0, 1.8f));
        gtr.push_back (P ("Nylon classical", K_Pluck, { 3.0f, .55f, .20f, .30f, 0.f, 0.f, 0.f, 0.f, .1f, 0.f, 1.f, .30f, 0.f, 60.f }, 0, 0, 0, 0, 1.8f));
        a[kInstGuitar] = withLeads (gtr);

        // ---------------- sax
        grp = "Saxophones";
        a[kInstSax] = withLeads ({
            P ("Auto (tenor or alto to suit the artist)", K_Sax, { 0.f, 1.f, .06f, 0.f, 1.f, 1.f, .06f }),
            P ("Tenor sax", K_Sax, { 0.f, 1.f, .06f, 0.f, 1.f, 1.f, .05f }),
            P ("Alto sax", K_Sax, { 1.f, 1.1f, .06f, 0.f, 1.f, 1.f, .05f }),
            P ("Soprano sax", K_Sax, { 2.f, 1.25f, .05f, 0.f, 1.15f, .9f, .05f }),
            P ("Baritone sax", K_Sax, { 3.f, .8f, .08f, 0.f, 1.f, 1.2f, .04f }),
            P ("Smooth tenor (breathy, subtone)", K_Sax, { 0.f, .65f, .17f, 0.f, .8f, 1.3f, .06f }, 1),
            P ("R&B tenor (growly, hot reed)", K_Sax, { 0.f, 1.3f, .06f, .35f, 1.7f, .8f, .05f }),
        });
        // v2.7: physical models (reed + conical bore waveguide, then the body formants), appended after Classic: older sound numbers stay the same
        //  K_SaxPM: p0 formant set, p1 brightness, p2 breath, p3 reed stiffness, p4 reed opening, p5 attack scale, p6 auto vibrato, p7 blow position
        grp = "Saxophones (physical model)";
        for (auto& pt : std::vector<Patch> {
            P ("Tenor sax (reed + bore model)",    K_SaxPM, { 0.f, 1.f, .05f, .30f, .70f, 1.f, .05f, .20f }, 0, 0, 0, 0, 1.f),
            P ("Alto sax (reed + bore model)",     K_SaxPM, { 1.f, 1.1f, .05f, .32f, .68f, 1.f, .05f, .21f }, 0, 0, 0, 0, 1.f),
            P ("Soprano sax (reed + bore model)",  K_SaxPM, { 2.f, 1.2f, .04f, .34f, .66f, .9f, .05f, .22f }, 0, 0, 0, 0, 1.f),
            P ("Baritone sax (reed + bore model)", K_SaxPM, { 3.f, .85f, .06f, .28f, .72f, 1.2f, .04f, .19f }, 0, 0, 0, 0, 1.f),
            P ("Breathy ballad tenor (model)",     K_SaxPM, { 0.f, .7f, .14f, .26f, .74f, 1.3f, .06f, .20f }, 1, 0, 0, 0, 1.f) }) a[kInstSax].push_back (pt);

        // ---------------- violin family
        grp = "Strings";
        a[kInstViolin] = withLeads ({
            P ("Violin", K_Bow, { 1.f, 1.f, .05f, 1.f, .07f, 1.f }),
            P ("Viola", K_Bow, { .82f, .85f, .06f, 1.1f, .07f, 1.f }),
            P ("Fiddle (bright, edgy)", K_Bow, { 1.f, 1.35f, .09f, .7f, .05f, 1.3f }),
            P ("Cello", K_Bow, { .55f, .8f, .07f, 1.4f, .08f, 1.f }),
        });
        //  K_BowPM: p0 body scale, p1 brightness, p2 bow noise, p3 attack scale, p4 auto vibrato, p5 bow pressure 0..1, p6 bow position (fraction of the string)
        grp = "Strings (bowed string model)";
        for (auto& pt : std::vector<Patch> {
            P ("Violin (bowed string model)",      K_BowPM, { 1.f, 1.f, .04f, 1.f, .07f, .45f, .127f }, 0, 0, 0, 0, 1.f),
            P ("Viola (bowed string model)",       K_BowPM, { .82f, .9f, .05f, 1.1f, .07f, .50f, .135f }, 0, 0, 0, 0, 1.f),
            P ("Cello (bowed string model)",       K_BowPM, { .55f, .85f, .05f, 1.3f, .08f, .55f, .14f }, 0, 0, 0, 0, 1.f),
            P ("Fiddle (bowed string model)",      K_BowPM, { 1.f, 1.3f, .08f, .7f, .05f, .58f, .125f }, 0, 0, 0, 0, 1.f) }) a[kInstViolin].push_back (pt);

        // ---------------- piano / keys
        grp = "Acoustic pianos";
        std::vector<Patch> keys = {
            P ("Concert grand", K_Piano, { 1.0f, .125f, 1.f, .55f, .55f, 1.f, .18f }, 0, 1.5f, 0.f, -.5f, 1.6f),
            P ("Studio grand (bright, modern)", K_Piano, { 1.45f, .11f, .85f, .5f, .45f, 1.f, .22f }, 0, 1.f, 1.5f, 1.f, 1.5f),
            P ("Upright piano", K_Piano, { .8f, .14f, .65f, 1.4f, .9f, 1.8f, .12f }, 0, 2.f, 0.f, -2.f, 1.65f),
            P ("Felt piano (mellow, intimate)", K_Piano, { .32f, .15f, .9f, .6f, .35f, 1.f, .14f }, 0, 2.f, -1.f, -3.f, 1.8f),
            P ("Honky-tonk piano", K_Piano, { 1.1f, .12f, .55f, 8.f, .8f, 1.3f, .30f }, 0, 0.f, 2.f, 0.f, 1.55f),
        };
        grp = "Electric pianos and organs";
        for (auto& pt : std::vector<Patch> {
            P ("Rhodes suitcase (mellow)", K_Rhodes, { .6f, 1.6f, 1.f, .35f, .10f }, 1, 1.5f, -1.f, 0, 1.2f),
            P ("Rhodes stage (bark)", K_Rhodes, { 1.0f, 3.2f, 1.f, 0.f, .14f }, 0, 0.f, 1.f, 0, 1.1f),
            P ("Wurlitzer 200A", K_Wurli, { 2.2f, 1.f, .22f, 1.0f }, 0, 0.f, 1.5f, -1.f, 0.9f),
            P ("FM e-piano (DX style)", K_FMEP, { 1.f, .8f, 1.f, 14.f }, 1, 0.f, 0.f, 0.f, 0.9f),
            P ("Clavinet", K_Pluck, { 1.6f, .08f, .20f, .95f, .08f, .22f, 5500.f, 2.2f, 1.2f, 7000.f, 0.f, .8f, 1.f, 120.f }, 0, -1.f, 2.f, 0, 1.5f),
            P ("Hammond B3 organ (full, gospel)", K_Organ, { 8, 8, 8, 8, 0, 0, 0, 0, 0, 0.f, 1.f, 1.5f }, 0, 0, 0, 0, 0.75f),
            P ("Hammond B3 organ (jazz, percussion)", K_Organ, { 8, 8, 6, 0, 0, 0, 0, 0, 0, 1.f, 0.f, 1.2f }, 0, 0, 0, 0, 0.75f) }) keys.push_back (pt);
        grp = "Poly synths";
        for (auto& pt : std::vector<Patch> {
            P ("Prophet-5 brass", K_Poly, { 7.f, 700.f, 3.0f, .18f, .015f, .35f, .75f, .18f, .02f, .55f, .35f, .25f, .6f, 0.f, 0.f }, 1, 0, 0, 0, 1.1f),
            P ("Prophet-5 pad", K_Poly, { 11.f, 1300.f, 1.0f, .12f, .55f, 1.0f, .85f, 1.1f, .6f, 1.2f, .6f, .8f, .4f, 0.f, .0f }, 2, 0, 0, 0, 1.05f),
            P ("Prophet-5 strings", K_Poly, { 13.f, 2800.f, .35f, .05f, .30f, .8f, .9f, .6f, .3f, .8f, .8f, .5f, .3f, 0.f, 0.f }, 2, 0, 0, 0, 1.0f),
            P ("Prophet-5 poly lead", K_Poly, { 8.f, 1500.f, 2.2f, .38f, .008f, .3f, .95f, .14f, .01f, .45f, .55f, .15f, .7f, 0.f, 0.f }, 1, 0, 0, 0, 1.1f),
            P ("Prophet-5 pluck", K_Poly, { 8.f, 600.f, 3.8f, .30f, .002f, .65f, 0.f, .18f, .002f, .22f, 0.f, .15f, .8f, 0.f, 0.f }, 1, 0, 0, 0, 1.6f) }) keys.push_back (pt);
        a[kInstPiano] = withLeads (keys);

        // ---------------- trumpet family
        grp = "Brass";
        a[kInstTrumpet] = withLeads ({
            P ("Trumpet", K_Brass, { 1.f, 1.f, 0.f, 1.f, 1.f, .06f, .05f }),
            P ("Flugelhorn (dark, round)", K_Brass, { .6f, .78f, 0.f, 1.3f, .7f, .08f, .05f }),
            P ("Muted trumpet (Harmon)", K_Brass, { .9f, 1.f, 1.f, 1.f, 1.f, .06f, .04f }),
            P ("Lead trumpet (bright, brassy)", K_Brass, { 1.5f, 1.15f, 0.f, .8f, 1.5f, .05f, .04f }),
        });
        //  K_BrassPM: p0 brightness, p1 formant scale, p2 mute, p3 attack scale, p4 lip tension (cents), p5 breath, p6 auto vibrato
        grp = "Brass (lip + bore model)";
        for (auto& pt : std::vector<Patch> {
            P ("Trumpet (lip + bore model)",       K_BrassPM, { 1.f, 1.f, 0.f, 1.f, 0.f, .05f, .05f }, 0, 0, 0, 0, 1.f),
            P ("Flugelhorn (lip + bore model)",    K_BrassPM, { .6f, .8f, 0.f, 1.3f, 0.f, .07f, .05f }, 0, 0, 0, 0, 1.f),
            P ("Muted trumpet (lip + bore model)", K_BrassPM, { .9f, 1.f, 1.f, 1.f, 0.f, .05f, .04f }, 0, 0, 0, 0, 1.f) }) a[kInstTrumpet].push_back (pt);

        // ---------------- bass (Enoch Bass)
        std::vector<Patch> bs;
        grp = "Electric basses";
        for (auto& pt : std::vector<Patch> {
            P ("P-Bass (fingers, flatwound)", K_Pluck, { 4.0f, .55f, .40f, .22f, .23f, 0.f, 1700.f, 1.0f, .3f, 3500.f, 0.f, .06f, 1.f, 28.f }, 0, 1.f, 0.f, 0, 1.15f),
            P ("Precision roundwound (bright, punchy)", K_Pluck, { 4.6f, .30f, .30f, .45f, .23f, 0.f, 2600.f, 1.3f, .35f, 5200.f, 0.f, .18f, 1.f, 30.f }, 0, 1.f, 1.5f, .5f, 1.0f),
            P ("Jazz Bass (bridge + neck)", K_Pluck, { 4.6f, .40f, .30f, .35f, .11f, .24f, 3200.f, 1.3f, .35f, 5200.f, 0.f, .12f, .5f, 28.f }, 0, 0.f, 1.5f, 0, .9f),
            P ("Jazz Bass bridge pickup (growl)", K_Pluck, { 4.4f, .28f, .14f, .55f, .08f, 0.f, 3600.f, 1.6f, .6f, 5600.f, 0.f, .20f, 1.f, 32.f }, 0, 0.f, 2.f, 0, .9f),
            P ("Modern active 5-string (hi-fi)", K_Pluck, { 5.0f, .20f, .22f, .50f, .12f, .24f, 4800.f, 1.2f, .2f, 8000.f, 0.f, .25f, .5f, 25.f }, 0, 3.f, -1.f, 2.f, 1.5f),
            P ("StingRay-style humbucker (punchy)", K_Pluck, { 4.2f, .25f, .16f, .70f, .11f, 0.f, 3300.f, 1.8f, .45f, 6500.f, 0.f, .35f, 1.f, 32.f }, 0, 2.f, 2.f, 1.5f, .9f),
            P ("Rickenbacker 4003 style (pick, growl)", K_Pluck, { 4.8f, .22f, .12f, .85f, .09f, .22f, 4200.f, 2.2f, 1.4f, 6000.f, 0.f, .50f, .6f, 40.f }, 0, -.5f, 3.f, 1.f, .85f),
            P ("Gospel active bass (deep + bright)", K_Pluck, { 4.6f, .24f, .25f, .55f, .14f, .22f, 4000.f, 1.3f, .35f, 7000.f, 0.f, .25f, .5f, 25.f }, 0, 3.5f, 0.f, 2.f, 1.25f),
            P ("Afrobeat bass (warm fingers, mids)", K_Pluck, { 3.2f, .48f, .32f, .35f, .20f, .10f, 1900.f, 1.2f, .4f, 3800.f, 0.f, .08f, .7f, 32.f }, 0, 1.5f, 1.f, -1.f, 1.15f),
            P ("Motown flats with foam mute (thump)", K_Pluck, { .9f, .70f, .30f, .20f, .23f, 0.f, 1300.f, .9f, .25f, 2400.f, 0.f, .03f, 1.f, 32.f }, 0, 2.f, -1.f, -2.f, 1.4f),
            P ("Hofner violin bass (hollow, thuddy)", K_Pluck, { 1.6f, .62f, .40f, .30f, .30f, 0.f, 1200.f, 1.4f, .3f, 2600.f, .6f, .05f, 1.f, 40.f }, 0, 1.f, -1.f, -2.f, 1.3f),
            P ("Pick bass (punchy)", K_Pluck, { 3.8f, .42f, .22f, .75f, .17f, 0.f, 2600.f, 1.2f, .8f, 4800.f, 0.f, .45f, 1.f, 30.f }, 0, 0.f, 1.f, 0, .9f),
            P ("Slap bass (thumb + pop)", K_Pluck, { 3.0f, .20f, .30f, .98f, .09f, .20f, 3800.f, 1.7f, .9f, 6500.f, 0.f, .9f, .5f, 35.f }, 0, 1.5f, 2.f, 1.f, 1.3f),
            P ("Fretless bass (mwah)", K_Pluck, { 4.4f, .52f, .35f, .30f, .20f, .10f, 1500.f, 3.2f, .4f, 3600.f, 0.f, .08f, .4f, 28.f, .04f }, 0, 1.f, 0.f, 0, .75f) }) bs.push_back (pt);
        grp = "Upright and acoustic";
        for (auto& pt : std::vector<Patch> {
            P ("Upright bass (acoustic)", K_Pluck, { 1.9f, .62f, .30f, .45f, 0.f, 0.f, 0.f, 0.f, .05f, 2600.f, 1.0f, .25f, 0.f, 35.f, .06f }, 0, 2.f, 0.f, -1.f, 2.0f),
            P ("Upright jazz pizzicato (warm, long)", K_Pluck, { 2.6f, .58f, .28f, .40f, 0.f, 0.f, 0.f, 0.f, .05f, 2400.f, 1.2f, .18f, 0.f, 32.f, .04f }, 0, 2.f, -.5f, -2.f, 1.9f),
            P ("Upright rockabilly slap", K_Pluck, { 1.2f, .45f, .20f, .90f, 0.f, 0.f, 0.f, 0.f, .2f, 3500.f, 1.0f, .9f, 0.f, 40.f, 0.f }, 0, 1.f, 2.f, 0.f, 2.4f),
            P ("Acoustic bass guitar", K_Pluck, { 2.2f, .35f, .25f, .55f, 0.f, 0.f, 0.f, 0.f, .08f, 4500.f, 1.0f, .30f, 0.f, 40.f }, 0, 1.f, 1.f, 1.f, 1.7f) }) bs.push_back (pt);
        grp = "Synth basses";
        //                                       A    B    Bst  Bct  Blv  sub  nz   pw   pwm  cut     res   fenv  key  A      D     S     R     fD    fS    glide vib   drv   echo eT    vow  sync fb   vRate
        for (auto& pt : std::vector<Patch> {
            P ("Minimoog bass (fat, glide)",      K_Lead, { 0, 0, 0, 5, .8f, .6f, 0, .5f, 0, 500, .35f, 2.2f, .3f, .002f, .3f, .6f, .08f, .25f, .3f, .03f, 0, 1.8f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 0.85f),
            P ("Moog synth bass", K_Poly, { 4.f, 260.f, 3.4f, .45f, .004f, .25f, .8f, .08f, .002f, .22f, .15f, .10f, .5f, 0.f, 0.f }, 0, 0, 0, 0, 1.0f),
            P ("Reese bass (detuned, dark)",      K_Lead, { 0, 0, 0, 22, 1.f, .5f, 0, .5f, 0, 700, .2f, .4f, .3f, .01f, .5f, .9f, .12f, .4f, .8f, .02f, 0, 2.0f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 0.75f),
            P ("Juno square bass",                K_Lead, { 1, -1, 0, 0, 0, .7f, 0, .5f, 0, 900, .15f, 1.2f, .3f, .002f, .25f, .7f, .08f, .25f, .4f, 0, 0, 1.2f, 0, .3f, 0, 0, 0, 5.4f }, 1, 0, 0, 0, 0.8f),
            P ("Acid 303 bass",                   K_Lead, { 0, -1, 0, 0, 0, 0, 0, .5f, 0, 350, 3.0f, 3.0f, .3f, .002f, .2f, .05f, .06f, .2f, 0, .06f, 0, 2.0f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 2.6f),
            P ("Sine sub bass (clean)",           K_Lead, { 3, -1, 0, 0, 0, 0, 0, .5f, 0, 8000, 0, 0, 0, .003f, .3f, 1.f, .1f, .3f, 1.f, .02f, 0, .6f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 1.4f),
            P ("808 sub bass", K_Sub, { 1.5f, 1.4f, 1.8f, 0.f }, 0, 0, 0, 0, 1.0f),
            P ("808 long (slides)", K_Sub, { 2.8f, 1.0f, 2.2f, 0.f }, 0, 0, 0, 0, 1.0f) }) bs.push_back (pt);
        grp = "Legacy log drums (v2.6)";
        for (auto& pt : std::vector<Patch> {
            P ("Log drum (amapiano)", K_Log, { .55f, .9f, 1.58f }, 0, 0, 0, 0, 1.1f),
            P ("Log drum deep (long)", K_Log, { .9f, .6f, 1.5f }, 0, 0, 0, 0, 1.1f),
            P ("Log drum short (knock)", K_Log, { .3f, 1.3f, 1.62f }, 0, 0, 0, 0, 1.2f) }) bs.push_back (pt);
        bs.push_back (classic());
        // ---- v2.7: the rebuilt Amapiano log drums (appended after Classic so every older sound keeps its number)
        grp = "Amapiano log drums";
        //   K_LogDrum: p0 decay s, p1 drop st, p2 drop time s, p3 click, p4 punch, p5 sub, p6 body (wood modes), p7 tone Hz, p8 drive, p9 width,
        //              p10 hybrid sustain, p11 glide s, p12 attack s, p13 sub decay ratio, p14 mode-2 ratio, p15 mode-3 ratio
        for (auto& pt : std::vector<Patch> {
            P ("Log drum: Classic Amapiano",  K_LogDrum, { .50f,  9.f, .035f, .35f, .60f, .55f, .50f, 1800.f, .25f, .20f, 0.f,  .06f, .0005f, 1.3f, 2.f, 3.f }, 0, 0, 0, 0, 1.0f),
            P ("Log drum: Punchy Club",       K_LogDrum, { .32f, 12.f, .022f, .60f, 1.0f, .40f, .70f, 2600.f, .45f, .25f, 0.f,  .04f, .0003f, 1.0f, 2.f, 3.f }, 0, 0, 0, 0, 0.95f),
            P ("Log drum: Deep Soulful",      K_LogDrum, { .85f,  6.f, .060f, .15f, .30f, .70f, .35f, 1200.f, .15f, .15f, 0.f,  .09f, .0020f, 1.5f, 2.f, 3.f }, 0, 0, 0, 0, 1.05f),
            P ("Log drum: Sub-Heavy",         K_LogDrum, { .60f,  8.f, .040f, .30f, .55f, 1.0f, .30f, 1400.f, .30f, .10f, 0.f,  .07f, .0008f, 1.6f, 2.f, 3.f }, 0, 0, 0, 0, 0.95f),
            P ("Log drum: Percussive",        K_LogDrum, { .20f, 14.f, .018f, .70f, .90f, .25f, .80f, 3000.f, .35f, .30f, 0.f,  .03f, .0003f, 0.8f, 2.f, 3.f }, 0, 0, 0, 0, 1.05f),
            P ("Log drum: Hybrid Log / Bass", K_LogDrum, { .55f,  8.f, .035f, .35f, .55f, .70f, .45f, 1600.f, .30f, .15f, .70f, .08f, .0008f, 2.0f, 2.f, 3.f }, 0, 0, 0, 0, 0.95f) }) bs.push_back (pt);
        a[kInstBass] = bs;
        // ---- v2.7 batch 2: more lead synths for every Enoch Solo instrument (appended: older sound numbers do not move)
        grp = "Lead synths (v2.7)";
        //                                       A    B    Bst  Bct  Blv  sub  nz   pw   pwm  cut     res   fenv  key  A      D     S     R     fD    fS    glide vib   drv   echo eT    vow  sync fb   vRate
        const std::vector<Patch> leads2 = {
            P ("Analog mono lead (warm, drifting)", K_Lead, { 0, 0, 0, 4, .9f, .3f, 0, .5f, 0, 1800, .3f, 1.6f, .5f, .006f, .4f, .85f, .15f, .45f, .5f, .04f, .12f, 1.4f, .10f, .30f, 0, 0, .25f, 5.4f }, 0, 0, 0, 0, 1.0f),
            P ("Wah lead (filter sweep)",           K_Lead, { 0, 1, 0, 6, .6f, .2f, 0, .5f, 0, 600, 1.4f, 3.2f, .5f, .004f, .6f, .7f, .18f, .7f, .2f, .03f, .10f, 1.6f, .10f, .30f, 0, 0, .25f, 5.4f }, 0, 0, 0, 0, 1.1f),
            P ("Hollow square lead (woody)",        K_Lead, { 1, -1, 0, 0, 0, 0, .04f, .5f, 0, 2200, .2f, .6f, .5f, .02f, .3f, .9f, .15f, .4f, .7f, .04f, .14f, .8f, .15f, .33f, 0, 0, .25f, 5.0f }, 0, 0, 0, 0, 1.2f),
            P ("Vocal synth lead (aah)",            K_Lead, { 0, 2, 12, 0, .4f, 0, .02f, .5f, 0, 3500, .1f, .6f, .4f, .03f, .4f, .9f, .18f, .5f, .6f, .05f, .20f, 1.0f, .18f, .33f, .7f, 0, .25f, 5.6f }, 0, 0, 0, 0, 1.3f),
            P ("Highlife synth lead (bright pulse)", K_Lead, { 1, 1, 12, 5, .4f, 0, 0, .3f, .2f, 3200, .25f, 1.2f, .5f, .003f, .35f, .75f, .12f, .35f, .4f, .02f, .08f, 1.1f, .20f, .25f, 0, 0, .30f, 5.6f }, 1, 0, 0, 0, 1.1f),
        };
        for (int i = 0; i < 5; ++i) a[(size_t) i].insert (a[(size_t) i].end(), leads2.begin(), leads2.end());
        // ---- v2.7 batch 2: more synth basses (Enoch Bass)
        grp = "Synth basses (v2.7)";
        for (auto& pt : std::vector<Patch> {
            P ("Amapiano soft sub (round sine + octave)", K_Lead, { 3, 3, 12, 0, .22f, 0, 0, .5f, 0, 5000, 0, 0, 0, .006f, .4f, .9f, .12f, .4f, 1.f, .05f, 0, 1.1f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 1.3f),
            P ("Funk Minimoog bass (plucky)",            K_Lead, { 0, 0, 0, 0, 0, .5f, 0, .5f, 0, 380, .7f, 3.2f, .3f, .001f, .18f, .2f, .06f, .15f, 0, .02f, 0, 2.0f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 1.0f),
            P ("Deep house organ bass",                  K_Lead, { 1, 1, 12, 0, .5f, 0, 0, .5f, 0, 900, .2f, .8f, .3f, .002f, .3f, .8f, .08f, .3f, .5f, 0, 0, 1.2f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 0.9f),
            P ("Reese growl (moving)",                   K_Lead, { 4, 0, 0, 25, .8f, .4f, 0, .5f, .3f, 650, .35f, .6f, .3f, .01f, .5f, .9f, .12f, .4f, .8f, .02f, 0, 2.4f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 0.7f),
            P ("Trap 808 glide (long)",                  K_Sub,  { 3.2f, 1.2f, 2.6f, 0.f }, 0, 0, 0, 0, 1.0f),
            P ("Afro-house pluck bass",                  K_Lead, { 1, 0, 0, 7, .5f, .3f, 0, .4f, 0, 700, .5f, 2.4f, .4f, .001f, .22f, .1f, .08f, .2f, .05f, 0, 0, 1.4f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 1.1f) }) a[kInstBass].push_back (pt);
        // ---- v3.0: Amapiano log drums on the original recipe. The amapiano log drum is the "Log Drum" factory preset of Paul Kellett's
        // mda DX10 FM synth (FL Studio's Fruity DX10); its algorithm is MIT-licensed and reproduced here exactly, then extended with
        // glide, a clean sub, drop, drive and the panel controls.
        //  K_LogDX: p0..p15 = the 16 DX10 parameters (attack, decay, release, coarse, fine, mod init, mod dec, mod sus, mod rel, mod vel,
        //           vibrato, octave, fine tune, waveform, mod thru, lfo rate); p16 sub, p17 drive, p18 drop st, p19 drop time s,
        //           p21 width, p22 glide s
        grp = "Amapiano log drums (DX10 recipe)";
        //                                                 A    D     R    Crs   Fine  MInit MDec  MSus  MRel  MVel Vib  Oct  FT   Wave  Thru  LFO   sub   drv   drop dropT  -   width glide
        for (auto& pt : std::vector<Patch> {
            P ("Log drum: The Original (DX10 preset)",  K_LogDX, { 0, .30f, .5f, .32f, 0, .467f, .079f, .158f, .5f, .5f, 0, .4f, .5f, .151f, .02f, .5f,  0.f, .08f, 0, .03f, 0, .08f, .06f }, 0, 0, 0, 0, 1.f),
            P ("Log drum: Classic Amapiano (warm)",     K_LogDX, { 0, .32f, .5f, .32f, 0, .467f, .079f, .158f, .5f, .5f, 0, .4f, .5f, .151f, .02f, .5f,  .35f, .25f, 0, .03f, 0, .15f, .07f }, 0, 0, 0, 0, 1.f),
            P ("Log drum: Deep Soulful",                K_LogDX, { 0, .36f, .55f, .32f, 0, .38f, .10f, .12f, .5f, .45f, 0, .4f, .5f, .12f, .01f, .5f,  .50f, .15f, 0, .03f, 0, .12f, .09f }, 0, 0, 0, 0, 1.f),
            P ("Log drum: Punchy Club",                 K_LogDX, { 0, .26f, .45f, .32f, 0, .58f, .06f, .17f, .5f, .55f, 0, .4f, .5f, .17f, .03f, .5f,  .30f, .45f, 0, .03f, 0, .18f, .05f }, 0, 0, 0, 0, 1.f),
            P ("Log drum: Sub-Heavy",                   K_LogDX, { 0, .33f, .5f, .32f, 0, .42f, .08f, .14f, .5f, .5f, 0, .4f, .5f, .14f, .01f, .5f,  .90f, .20f, 0, .03f, 0, .10f, .07f }, 0, 0, 0, 0, 1.f),
            P ("Log drum: Percussive Knock",            K_LogDX, { 0, .20f, .4f, .32f, 0, .60f, .05f, .16f, .5f, .55f, 0, .4f, .5f, .16f, .03f, .5f,  .15f, .35f, 0, .03f, 0, .20f, .04f }, 0, 0, 0, 0, 1.f),
            P ("Log drum: Bacardi Bounce",              K_LogDX, { 0, .24f, .45f, .32f, 0, .52f, .07f, .16f, .5f, .55f, 0, .4f, .5f, .16f, .02f, .5f,  .25f, .30f, 3, .03f, 0, .15f, .05f }, 0, 0, 0, 0, 1.f),
            P ("Log drum: Sgija Deep Drop",             K_LogDX, { 0, .38f, .6f, .32f, 0, .45f, .09f, .15f, .5f, .5f, 0, .4f, .5f, .14f, .01f, .5f,  .70f, .22f, 7, .06f, 0, .10f, .12f }, 0, 0, 0, 0, .62f) }) a[kInstBass].push_back (pt);
        // ---- v3.2: more lead synths (every Enoch Solo instrument) and synth basses (Enoch Bass); appended, older sound numbers do not move
        grp = "Lead synths (v3.2)";
        //                                       A    B    Bst  Bct  Blv  sub  nz   pw   pwm  cut     res   fenv  key  A      D     S     R     fD    fS    glide vib   drv   echo eT    vow  sync fb   vRate
        const std::vector<Patch> leads3 = {
            P ("Afrobeats pluck lead (short, bright)", K_Lead, { 1, 0, 12, 4, .35f, 0, 0, .35f, 0, 3800, .25f, 2.2f, .5f, .002f, .22f, .15f, .10f, .18f, .1f, .0f, .04f, 1.1f, .22f, .25f, 0, 0, .30f, 5.4f }, 1, 0, 0, 0, 1.15f),
            P ("Amapiano soft lead (sine, airy)",      K_Lead, { 3, 2, 12, 0, .25f, 0, .03f, .5f, 0, 4500, 0, .2f, .3f, .02f, .5f, .9f, .25f, .5f, .8f, .04f, .12f, .6f, .25f, .375f, 0, 0, .35f, 5.0f }, 1, 0, 0, 0, 1.2f),
            P ("Neo-soul glide lead (round)",          K_Lead, { 2, 3, 0, 0, .5f, .2f, 0, .5f, 0, 2400, .15f, .7f, .4f, .01f, .45f, .9f, .20f, .45f, .65f, .09f, .22f, .9f, .18f, .33f, 0, 0, .30f, 5.2f }, 0, 0, 0, 0, 1.15f),
            P ("Gospel organ lead (drawbar-ish)",      K_Lead, { 3, 3, 12, 0, .7f, .5f, 0, .5f, 0, 6000, 0, 0, .2f, .004f, .2f, 1.f, .08f, .3f, 1.f, .02f, .18f, 1.1f, .12f, .30f, 0, 0, .25f, 6.4f }, 0, 0, 0, 0, 0.95f),
            P ("Chiptune square (8-bit)",              K_Lead, { 1, 1, 12, 0, .3f, 0, 0, .25f, 0, 8000, 0, 0, .2f, .001f, .2f, .95f, .05f, .2f, 1.f, 0, .10f, .8f, .10f, .25f, 0, 0, .20f, 6.0f }, 0, 0, 0, 0, 0.9f),
        };
        for (int i = 0; i < 5; ++i) a[(size_t) i].insert (a[(size_t) i].end(), leads3.begin(), leads3.end());
        grp = "Synth basses (v3.2)";
        for (auto& pt : std::vector<Patch> {
            P ("Moog ladder bass (fat, round)",        K_Lead, { 0, 1, -12, 5, .7f, .5f, 0, .5f, 0, 520, .45f, 2.0f, .3f, .002f, .30f, .6f, .08f, .25f, .25f, .03f, 0, 1.5f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 1.0f),
            P ("Acid bass (303-style squelch)",        K_Lead, { 0, 0, 0, 0, 0, 0, 0, .5f, 0, 380, 1.5f, 3.6f, .3f, .001f, .20f, .3f, .05f, .14f, .05f, .07f, 0, 1.8f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 0.85f),
            P ("Dancehall square sub",                 K_Lead, { 1, 3, -12, 0, .6f, .3f, 0, .5f, 0, 1400, .1f, .3f, .2f, .003f, .3f, .9f, .10f, .3f, .9f, .02f, 0, 1.2f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 1.0f),
            P ("Amapiano warm saw sub",                K_Lead, { 0, 3, -12, 0, .8f, .2f, 0, .5f, 0, 650, .1f, .4f, .2f, .006f, .4f, .9f, .12f, .4f, .9f, .05f, 0, 1.1f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 1.15f),
            P ("808 short (punchy)",                   K_Sub,  { 0.9f, 1.6f, 1.4f, 0.f }, 0, 0, 0, 0, 1.0f),
            P ("Kizomba smooth sub (sine glide)",      K_Lead, { 3, 3, 12, 0, .15f, 0, 0, .5f, 0, 3000, 0, 0, 0, .01f, .5f, .95f, .15f, .5f, 1.f, .08f, 0, 1.0f, 0, .3f, 0, 0, 0, 5.4f }, 0, 0, 0, 0, 1.3f) }) a[kInstBass].push_back (pt);
        return a;
    }();
    return all[(size_t) std::clamp (inst, 0, 5)];
}

//------------------------------------------------------------------------------------------------ small DSP pieces
struct Noise
{
    uint32_t s = 22222u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
    float uni() { return next() * 0.5f + 0.5f; }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set (float nb0, float nb1, float nb2, float a0, float na1, float na2) { b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0; }
    static float clampF (float f, float sr) { return std::min (std::max (f, 15.f), 0.46f * sr); }
    void lowpass (float f, float q, float sr)
    { const float w = kTwoPi * clampF (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2.f * q); set ((1 - c) * .5f, 1 - c, (1 - c) * .5f, 1 + al, -2 * c, 1 - al); }
    void highpass (float f, float q, float sr)
    { const float w = kTwoPi * clampF (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2.f * q); set ((1 + c) * .5f, -(1 + c), (1 + c) * .5f, 1 + al, -2 * c, 1 - al); }
    void bandpass (float f, float q, float sr)      // constant 0 dB peak
    { const float w = kTwoPi * clampF (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2.f * q); set (al, 0, -al, 1 + al, -2 * c, 1 - al); }
    void peak (float f, float q, float dB, float sr)
    {
        const float A = std::pow (10.f, dB / 40.f), w = kTwoPi * clampF (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2.f * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (float f, float dB, float sr)
    {
        const float A = std::pow (10.f, dB / 40.f), w = kTwoPi * clampF (f, sr) / sr, c = std::cos (w), s = std::sin (w), be = std::sqrt (A) * s;
        set (A * ((A + 1) - (A - 1) * c + be), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - be), (A + 1) + (A - 1) * c + be, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - be);
    }
    void highShelf (float f, float dB, float sr)
    {
        const float A = std::pow (10.f, dB / 40.f), w = kTwoPi * clampF (f, sr) / sr, c = std::cos (w), s = std::sin (w), be = std::sqrt (A) * s;
        set (A * ((A + 1) + (A - 1) * c + be), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - be), (A + 1) - (A - 1) * c + be, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - be);
    }
};

inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

/** 4-pole zero-delay-feedback ladder low-pass (Moog / Prophet style), with a soft saturation at its input */
struct Ladder
{
    float s[4] = { 0, 0, 0, 0 }, G = 0.1f, k = 0.f, g = 0.1f;
    void set (float fc, float res, float sr)
    {
        g = std::tan (kPi * std::min (std::max (fc, 20.f), 0.45f * sr) / sr); G = g / (1.f + g); k = res;
    }
    float process (float x)
    {
        const float G2 = G * G, G4 = G2 * G2;
        const float S = (1.f - G) * (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]);
        float u = (x - k * S) / (1.f + k * G4);
        u = std::tanh (u);
        float in = u;
        for (int i = 0; i < 4; ++i) { const float y = G * in + (1.f - G) * s[i]; s[i] = 2.f * y - s[i]; in = y; }
        return in;
    }
};

struct Chorus
{
    std::vector<float> bl, br; int w = 0, size = 4096; float ph = 0, rate = 0.45f, depth = 0.f, mix = 0.f, sr = 44100.f;
    void prepare (float s) { sr = s; size = 1 << (int) std::ceil (std::log2 (s * 0.06f)); bl.assign ((size_t) size, 0.f); br.assign ((size_t) size, 0.f); w = 0; }
    void clear() { std::fill (bl.begin(), bl.end(), 0.f); std::fill (br.begin(), br.end(), 0.f); }
    void set (int mode) { depth = mode == 0 ? 0.f : (mode == 1 ? 0.0016f : 0.0030f); mix = mode == 0 ? 0.f : (mode == 1 ? 0.35f : 0.5f); rate = mode == 2 ? 0.38f : 0.62f; }
    static float tap (const std::vector<float>& b, int w, int size, float d)
    {
        float rp = (float) w - d; while (rp < 0) rp += (float) size;
        const int i0 = (int) rp; const float fr = rp - (float) i0; const int mask = size - 1;
        return b[(size_t) (i0 & mask)] * (1.f - fr) + b[(size_t) ((i0 + 1) & mask)] * fr;
    }
    void process (float& L, float& R)
    {
        if (mix <= 0.f || bl.empty()) return;
        bl[(size_t) w] = L; br[(size_t) w] = R;
        ph += rate / sr; if (ph >= 1.f) ph -= 1.f;
        const float m1 = std::sin (kTwoPi * ph), m2 = std::sin (kTwoPi * (ph + 0.5f));
        const float d1 = (0.011f + depth * m1) * sr, d2 = (0.014f + depth * m2) * sr;
        const float cl = tap (bl, w, size, d1), cr = tap (br, w, size, d2);
        const float x = 0.5f * (L + R);
        (void) x;
        const float oL = L * (1.f - mix * 0.5f) + cl * mix, oR = R * (1.f - mix * 0.5f) + cr * mix;
        L = oL; R = oR;
        w = (w + 1) & (size - 1);
    }
};

//------------------------------------------------------------------------------------------------ the engine
class Engine
{
public:
    static constexpr int kVoices = 20, kMaxOsc = 80, kBuf = 8192, kHist = 4096;

    void prepare (double rate)
    {
        sr = (float) rate;
        if (voices.empty()) voices.resize (kVoices);
        chorus.prepare (sr);
        monoHzSet = -1.f; setLogControls (logc);
        echoL.assign ((size_t) (sr * 1.1f), 0.f); echoR.assign ((size_t) (sr * 1.1f), 0.f); echoW = 0;
        reset();
        applyEq();
    }

    void setSound (int inst, int idx, bool altoAuto)
    {
        instIdx = inst; soundIdx = idx; saxAlto = altoAuto;
        const auto& l = patchList (inst);
        cur = l[(size_t) std::clamp (idx, 0, (int) l.size() - 1)];
        // sax "Auto" follows the artist's sax
        if (inst == kInstSax && idx == 0) { cur.p[0] = altoAuto ? 1.f : 0.f; cur.p[1] = altoAuto ? 1.1f : 1.f; }
        chorus.set (cur.chorus);
        applyEq();
    }
    bool usesClassic() const { return cur.kind == K_Classic; }
    /** the log drum control panel (EnochBass). Relative controls: 1 = the selected type's own value */
    struct LogCtl
    {
        float tune = 0.f, dropMul = 1.f, dropTimeMul = 1.f, attack = 0.0005f, punchMul = 1.f, decayMul = 1.f, subDb = 0.f, bodyMul = 1.f,
              tone = 0.f, drive = 0.f, width = 0.2f, velResp = 0.6f, glide = 0.06f, human = 0.3f, outDb = 0.f, monoHz = 120.f;
        bool scSafe = true;
    };
    void setLogControls (const LogCtl& c) { logc = c; if (std::abs (c.monoHz - monoHzSet) > 0.5f) { monoHzSet = c.monoHz; monoLpL.lowpass (std::max (20.f, c.monoHz), 0.707f, sr); monoLpR = monoLpL; } }
    bool logDrumActive() const { return isLogKind (cur.kind); }
    /** v2.7 global sound controls (both plugins). 0.5 = the natural / previous amount where it says so */
    struct GlobalCtl
    {
        float realism = 0.6f;      // model detail: mechanical noises, resonances, per-note variation
        float human = 0.3f;        // how different each note is (pitch drift, level, attack)
        float dynamics = 0.5f;     // velocity range: 0 = even, 0.5 = as played, 1 = wide
        float exprResp = 0.5f;     // how much expression (CC11) moves level and tone: 0.5 = as before
        float bright = 0.f;        // -1 dark .. +1 bright (tilt)
        float mech = 0.5f;         // mechanical noise amount (0.5 = natural)
        float mic = 0.35f;         // 0 close .. 1 far (0.35 = neutral)
        float glideTime = 0.2f;    // 0 fast and subtle .. 1 slow
        int glideMode = 1;         // 0 off, 1 legato glide, 2 always glide, 3 overlap glide, 4 portamento
        bool humanPlay = true;     // phrasing engine
        uint32_t seed = 1;
    };
    void setGlobal (const GlobalCtl& g)
    {
        const bool tone = (g.bright != gc.bright || g.mic != gc.mic || ! toneInit);
        gc = g;
        if (tone) setTone();
    }
    const GlobalCtl& global() const { return gc; }
    /** v2.7 batch 3: modulation matrix (4 slots) and creative effects */
    enum ModSrc { MS_Off = 0, MS_Lfo1, MS_Lfo2, MS_Velocity, MS_ModWheel, MS_Aftertouch, MS_Expression, MS_Key, MS_Envelope, MS_Random, MS_Count };
    enum ModDst { MD_Pitch = 0, MD_Bright, MD_Level, MD_Pan, MD_Vibrato, MD_Drive, MD_Count };
    struct ModSlot { int src = 0, dst = 0; float amt = 0.f; };
    struct ModCtl { std::array<ModSlot, 4> slot {}; float lfo1Hz = 5.f, lfo2Hz = 0.3f; int fx = 0; float fxAmt = 0.5f; };
    void setMod (const ModCtl& m) { mmc = m; mmOn = false; for (auto& sl : m.slot) mmOn |= (sl.src != MS_Off && std::fabs (sl.amt) > 1e-4f); }
    const Patch& patch() const { return cur; }

    void reset()
    {
        for (auto& v : voices) v.active = false;
        chorus.clear();
        std::fill (echoL.begin(), echoL.end(), 0.f); std::fill (echoR.begin(), echoR.end(), 0.f);
        lastLeadNote.fill (-1);
        bend.fill (0.f); mod.fill (0.f); aft.fill (0.f); expr.fill (100.f / 127.f); exprT.fill (100.f / 127.f); pedal.fill (false);
        orderCounter = 0; noteCounter = 0; clock = 0.0;
        for (auto& p : phr) p = Phrase();
        lastMono.fill (-1);
        for (auto& m : mech) m.a = 0.f;
        resReset(); toneReset();
        std::fill (fxL.begin(), fxL.end(), 0.f); std::fill (fxR.begin(), fxR.end(), 0.f); for (auto& b : fxBq) b.reset(); fxSet = -1;
        eqReset();
    }
    void setBend (int ch, float semis) { bend[(size_t) clampCh (ch)] = semis; }
    void setMod (int ch, float v) { mod[(size_t) clampCh (ch)] = v; }
    void setAftertouch (int ch, float v) { aft[(size_t) clampCh (ch)] = v; }
    void setExpression (int ch, float v) { exprT[(size_t) clampCh (ch)] = v; }
    void setPedal (int ch, bool down)
    {
        ch = clampCh (ch);
        if (pedal[(size_t) ch] != down && (cur.kind == K_Piano || cur.kind == K_Rhodes || cur.kind == K_Wurli)) mechEvent (0.008f * mechMul(), 130.f, 1.f, 0.07f, 0.5f);
        pedal[(size_t) ch] = down;
        if (! down) for (auto& v : voices) if (v.active && v.ch == ch && v.heldByPedal) { v.heldByPedal = false; release (v); }
    }
    void allNotesOff()
    {
        pedal.fill (false);                                // a stuck sustain pedal was the cause of piano notes that never ended
        for (auto& v : voices) if (v.active) { v.held = false; v.heldByPedal = false; release (v); }
    }

    void noteOn (int ch, int note, int vel, int art)
    {
        ch = clampCh (ch);
        if (cur.kind == K_Classic) return;
        ++noteCounter;
        const bool lead = (cur.kind == K_Lead);
        const bool logd = isLogKind (cur.kind);
        const bool mono = lead || logd || (cur.kind == K_Sax || cur.kind == K_Brass || cur.kind == K_Bow || cur.kind == K_Sub || isPM (cur.kind) || (cur.kind == K_Poly && instIdx == kInstBass));
        const bool legatoArt = (art == A_Legato || art == A_HammerOn || art == A_PullOff || art == A_Slide || art == A_Gliss);
        const bool plucked = (cur.kind == K_Pluck);
        const bool pluckLeg = plucked && (art == A_HammerOn || art == A_PullOff || art == A_Slide);
        const bool slideArt = (art == A_Slide || art == A_Gliss);
        // phrasing engine + dynamics + humanisation shape the velocity (and leave hints for the voice in 'pend')
        vel = shapeNote (ch, note, vel, art, legatoArt);
        const int gm = gc.glideMode;
        const float k = glideScale();
        if (mono || pluckLeg)
            for (auto& v : voices)
            {
                if (! (v.active && v.ch == ch && v.kind == cur.kind)) continue;
                const bool heldNow = v.held && ! v.releasing;
                const bool ringing = v.releasing && v.env > 0.002f && (clock - phr[(size_t) ch].lastOff) < 0.25f;
                bool connect = false;
                if (logd) connect = (heldNow || (art == A_Slide && v.t < 2.f)) && logc.glide > 0.0005f;
                else
                {
                    const bool artConnect = mono ? legatoArt : pluckLeg;
                    const int maxJump = (gm == 2 || gm == 3) ? 12 : 7;
                    if (gm == 3)      connect = heldNow && (mono || pluckLeg);                     // overlap glide: any overlap connects
                    else if (gm == 2) connect = (heldNow || (ringing && mono)) && (mono || pluckLeg); // always: overlaps and notes that just ended
                    else              connect = heldNow && (lead || artConnect);                   // off / legato / portamento: the articulation decides (as before)
                    if (! lead && std::abs (v.note - note) > maxJump) connect = false;
                }
                if (! connect) continue;
                const int from = v.note;
                v.note = note; v.vel = vel;                                  // same voice, new pitch: no new attack
                if (logd || ! heldNow) { v.held = true; v.releasing = false; }
                if (! logd && v.releasing == false && ! heldNow) v.relCoef = 1.f;
                float gt = logd ? std::max (0.004f, logc.glide)
                         : lead ? std::max (0.004f, cur.p[19] * (slideArt ? 2.5f : 1.f) * k)
                                : (slideArt ? std::max (0.075f, 0.075f * k) : (plucked ? 0.006f : 0.014f) * k);
                if (gm == 0 && ! slideArt && ! logd) gt = 0.f;                // glide off: legato notes change pitch at once (slides still slide)
                v.glideCoef = gt <= 0.f ? 1.f : 1.f - std::exp (-1.f / (gt * sr));
                if (plucked && (art == A_HammerOn || art == A_PullOff)) v.pluckBoost = art == A_HammerOn ? 0.55f : 0.f;
                // mechanics of the change: sax / trumpet keys and valves, a finger sliding on a string
                if (cur.kind == K_Sax || cur.kind == K_SaxPM) mechEvent (0.0045f * mechMul(), 2300.f, 2.f, 0.007f, 0.5f);
                if (cur.kind == K_Brass || cur.kind == K_BrassPM) mechEvent (0.0025f * mechMul(), 1500.f, 2.f, 0.006f, 0.5f);
                if (plucked && slideArt && std::abs (from - note) >= 2) mechEvent (0.006f * mechMul(), 2700.f, 1.5f, 0.05f, 0.5f);
                lastMono[(size_t) ch] = note;
                return;
            }
        if (mono) for (auto& v : voices) if (v.active && v.ch == ch && (v.held || logd || (gm == 2 && v.kind == cur.kind)) && v.kind == cur.kind) { v.held = false; release (v, logd ? 0.012f : 0.03f); }
        Voice* slot = nullptr;
        for (auto& v : voices) if (! v.active) { slot = &v; break; }
        if (! slot) { long long best = -1; for (auto& v : voices) { const long long score = v.age + (v.releasing ? 1000000LL : 0); if (score > best) { best = score; slot = &v; } } }
        start (*slot, ch, note, vel, art);
        const int last = lastMono[(size_t) ch];
        if (lead)
        {
            // portamento: long-glide sounds slide in from the last note even when the notes do not overlap
            const int lastL = lastLeadNote[(size_t) ch];
            if (cur.p[19] >= 0.08f && lastL >= 0 && lastL != note && gm != 0)
            { slot->curSemis = (float) lastL; slot->glideCoef = 1.f - std::exp (-1.f / (cur.p[19] * k * sr)); }
            lastLeadNote[(size_t) ch] = note;
        }
        if (gm == 4 && mono && ! logd && last >= 0 && last != note && std::abs (last - note) <= 12)
        {   // portamento mode: every new note slides in from the previous one (with its own attack)
            slot->curSemis = (float) last;
            slot->glideCoef = 1.f - std::exp (-1.f / (std::max (0.02f, 0.03f * k) * sr));
        }
        lastMono[(size_t) ch] = note;
    }
    void noteOff (int ch, int note)
    {
        ch = clampCh (ch);
        phr[(size_t) ch].lastOff = clock;
        for (auto& v : voices)
            if (v.active && v.ch == ch && v.note == note && v.held)
            {
                v.held = false;
                // dampers / fingers leaving the string
                if (! pedal[(size_t) ch])
                {
                    if (v.kind == K_Piano) mechEvent (0.0035f * mechMul() * (0.5f + (float) v.vel / 254.f), 420.f, 1.2f, 0.045f, v.pan);
                    else if (v.kind == K_Rhodes || v.kind == K_Wurli) mechEvent (0.005f * mechMul(), 260.f, 1.4f, 0.04f, 0.5f);
                }
                if (v.kind == K_Pluck && v.t > 0.08f && v.art != A_PalmMute && v.art != A_Mute) mechEvent (0.0025f * mechMul(), 1900.f, 1.f, 0.02f, 0.5f);
                if (pedal[(size_t) ch] && (v.kind == K_Piano || v.kind == K_Rhodes || v.kind == K_Wurli || v.kind == K_FMEP || (v.kind == K_Poly && instIdx == kInstPiano))) v.heldByPedal = true;
                else release (v);
            }
    }

    /** adds into L / R (R may be null) */
    void render (float* L, float* R, int num, float gain)
    {
        if (cur.kind == K_Classic) return;
        const float exprSm = 1.f - std::exp (-1.f / (0.006f * sr));
        for (int i = 0; i < num; ++i)
        {
            for (int c = 0; c < 16; ++c) expr[(size_t) c] += (exprT[(size_t) c] - expr[(size_t) c]) * exprSm;
            float oL = 0, oR = 0;
            if (mmOn) { lfo1Ph += mmc.lfo1Hz / sr; if (lfo1Ph >= 1.f) lfo1Ph -= 1.f; lfo2Ph += mmc.lfo2Hz / sr; if (lfo2Ph >= 1.f) lfo2Ph -= 1.f; }
            for (auto& v : voices)
            {
                if (! v.active) continue;
                float sl = 0, sr_ = 0;
                if (! mmOn) { if (! tick (v, sl, sr_)) { v.active = false; continue; } oL += sl; oR += sr_; continue; }
                // modulation matrix: sources -> pitch / brightness / level / pan / vibrato / drive
                float d[MD_Count] = {};
                for (auto& m : mmc.slot) if (m.src != MS_Off && m.dst >= 0 && m.dst < MD_Count) d[m.dst] += m.amt * modSource (v, m.src);
                v.pm[15] = v.baseDetune + 2.f * d[MD_Pitch] + 0.6f * d[MD_Vibrato] * std::sin (kTwoPi * 5.5f * v.t);
                if (! tick (v, sl, sr_)) { v.active = false; continue; }
                if (d[MD_Bright] != 0.f)
                {
                    v.mmLpL += (sl - v.mmLpL) * 0.12f; v.mmLpR += (sr_ - v.mmLpR) * 0.12f;
                    const float k = 1.f + std::clamp (d[MD_Bright], -1.f, 1.5f);
                    sl = v.mmLpL + k * (sl - v.mmLpL); sr_ = v.mmLpR + k * (sr_ - v.mmLpR);
                }
                if (d[MD_Drive] > 0.f) { const float dr = 1.f + 5.f * d[MD_Drive]; const float mk = (1.f + 0.5f * d[MD_Drive]) / dr; sl = std::tanh (sl * dr) * mk; sr_ = std::tanh (sr_ * dr) * mk; }
                const float g = std::max (0.f, 1.f + d[MD_Level]);
                const float pn = std::clamp (d[MD_Pan], -1.f, 1.f);
                oL += sl * g * std::min (1.f, 1.f - pn); oR += sr_ * g * std::min (1.f, 1.f + pn);
            }
            oL *= gain * cur.level; oR *= gain * cur.level;
            chorus.process (oL, oR);
            if (cur.kind == K_Lead && cur.p[22] > 0.f) echoProcess (oL, oR);
            eqProcess (oL, oR);
            if (cur.kind == K_Piano) resProcess (oL, oR);
            mechRender (oL, oR);
            toneProcess (oL, oR);
            if (mmc.fx != 0) fxProcess (oL, oR);
            if (isLogKind (cur.kind))
            {
                if (logc.monoHz > 0.f)                                               // everything under the mono point in the middle
                {
                    const float lL = monoLpL.process (oL), lR = monoLpR.process (oR), m = 0.5f * (lL + lR);
                    oL = oL - lL + m; oR = oR - lR + m;
                }
                const float g = dbToLin (logc.outDb);
                oL *= g; oR *= g;
                if (logc.scSafe)                                                     // clean ceiling at -1 dBFS, no DC: safe to side-chain and to bus
                {
                    auto soft = [] (float x) { const float c = 0.891f; return std::abs (x) < 0.7f * c ? x : std::copysign (0.7f * c + 0.3f * c * std::tanh ((std::abs (x) - 0.7f * c) / (0.3f * c)), x); };
                    oL = soft (oL); oR = soft (oR);
                }
            }
            L[i] += oL; if (R) R[i] += oR;
        }
        clock += (double) num / sr;
        if ((blockCount++ & 15) == 0) for (auto& v : voices) if (v.active && v.nOsc > 0) compact (v);
    }

private:
    static int clampCh (int c) { return std::min (15, std::max (0, c)); }

    struct Osc { float c = 1, s = 0, cw = 1, sw = 0, a = 0, dec = 1, rel = 1, pl = .7f, pr = .7f; };

    struct Voice
    {
        bool active = false, held = false, heldByPedal = false, releasing = false;
        int kind = K_Piano, ch = 0, note = 60, vel = 90, art = 0;
        long long age = 0; float t = 0;
        float env = 0, attackInc = 0.01f, relCoef = 0.999f, gain = 1.f;
        float curSemis = 60.f, glideCoef = 1.f, lfo = 0, vibDelay = 0.15f, autoVib = 0.f, scoop = 0.f, jit1 = 0, jit2 = 0, jitT = 0, jitV = 0, jitA = 0;
        // modal oscillators (piano / rhodes / wurlitzer / organ)
        int nOsc = 0; Osc osc[kMaxOsc];
        float thump = 0, thumpDec = 0.99f, nz = 0, pan = 0.5f, drive = 1.f, bias = 0.f, tremPh = 0, tremDepth = 0;
        float x1 = 0, y1 = 0;                                        // dc blocker
        // phases
        float ph[6] = { 0, 0, 0, 0, 0, 0 }, lp[4] = { 0, 0, 0, 0 };
        float par[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        Biquad bq[6];
        Patch pt;                                                    // the sound this voice was started with
        Ladder lad;
        // poly synth envelopes
        float aEnv = 0, fEnv = 0; int aStage = 0, fStage = 0; float drift = 0, drift2 = 0;
        float sp[16] = {}; float lfo2 = 0, vowM = 0;     // lead synth: supersaw / sync / sub phases
        // plucked string
        int wp = 0, hp = 0; float D = 100.f, loopG = 0.999f, loopD = 0.4f, filt = 0, pluckBoost = 0.f, tick_ = 0, tickDec = 0.99f, hpz = 0;
        std::array<float, kBuf> buf {};
        std::array<float, kHist> hist {};
        float pm[16] = {};                                           // physical-model state (see startSaxPM / startBrassPM / startBowPM)
        float mmLpL = 0, mmLpR = 0, mmRand = 0, baseDetune = 0;    // modulation matrix state
    };

    //---------------------------------------------------------------- helpers
    void release (Voice& v, float seconds = 0.f)
    {
        v.releasing = true;
        if (v.kind == K_Piano)
        {
            // damper: every partial gets its own (fast) release
            for (int i = 0; i < v.nOsc; ++i) v.osc[i].dec = v.osc[i].rel;
            if (seconds > 0) for (int i = 0; i < v.nOsc; ++i) v.osc[i].dec = std::min (v.osc[i].dec, std::exp (-6.9f / (seconds * sr)));
            return;
        }
        float rel = seconds;
        if (rel <= 0.f)
        {
            switch (v.kind)
            {
                case K_Rhodes: rel = 0.22f; break; case K_Wurli: rel = 0.18f; break; case K_FMEP: rel = 0.25f; break;
                case K_Organ: rel = 0.03f; break; case K_Poly: rel = std::max (0.02f, v.pt.p[7]); break; case K_Lead: rel = std::max (0.02f, v.pt.p[16]); break;
                case K_Pluck: rel = (v.art == A_PalmMute || v.art == A_Mute) ? 0.04f : (instIdx == kInstBass ? 0.09f : 0.16f); break;     // finger / palm damps the string
                case K_Sax: rel = 0.07f; break; case K_Brass: rel = 0.06f; break; case K_Bow: rel = 0.12f; break;
                case K_SaxPM: rel = 0.08f; break; case K_BrassPM: rel = 0.07f; break; case K_BowPM: rel = 0.14f; break;
                case K_Sub: rel = 0.25f; break; case K_Log: rel = 0.12f; break;
                case K_LogDrum: rel = v.pt.p[10] > 0.f ? 0.12f : 2.0f; break;
                case K_LogDX: rel = 3.0f; break;                                  // the DX10 release envelope runs inside the voice     // a log drum rings out after note-off (a hybrid's sustain stops)
                default: rel = 0.1f;
            }
        }
        v.relCoef = std::exp (-6.9f / (rel * sr));
        if (v.kind == K_Poly || v.kind == K_Lead) { v.aStage = 3; v.fStage = 3; }
    }

    void start (Voice& v, int ch, int note, int vel, int art)
    {
        v = Voice();
        v.active = true; v.held = true; v.ch = ch; v.note = note; v.vel = vel; v.art = art; v.kind = cur.kind;
        v.pt = cur;
        if ((cur.kind == K_Bow || cur.kind == K_BowPM) && (art == A_Pizz || art == A_ColLegno)) { v.pt = pizzPatch (art == A_ColLegno); v.kind = K_Pluck; }
        v.curSemis = (float) note; v.glideCoef = 1.f; v.t = 0; v.env = 0;
        v.nz = 0;
        Noise nzr; nzr.s = 0x9e3779b9u * (uint32_t) (note * 131 + vel * 17 + (int) (orderCounter++ & 1023) * 7919 + 12345);
        for (int k = 0; k < 4; ++k) nzr.next();
        rng = nzr;
        const float fv = (float) vel / 127.f;
        switch (v.kind)
        {
            case K_Piano: startPiano (v, fv); break;
            case K_Rhodes: startRhodes (v, fv); break;
            case K_Wurli: startWurli (v, fv); break;
            case K_FMEP: startFM (v, fv); break;
            case K_Organ: startOrgan (v, fv); break;
            case K_Poly: startPoly (v, fv); break;
            case K_Lead: startLead (v, fv); break;
            case K_Pluck: startPluck (v, fv, art); break;
            case K_Sax: case K_Brass: case K_Bow: startWind (v, fv, art); break;
            // physical models: the classic set-up gives the formants / body, vibrato and attack; then the waveguide is initialised
            case K_SaxPM:   v.kind = K_Sax;   startWind (v, fv, art); v.kind = K_SaxPM;   startSaxPM (v, fv); break;
            case K_BrassPM: v.kind = K_Brass; startWind (v, fv, art); v.kind = K_BrassPM; startBrassPM (v, fv); break;
            case K_BowPM:   v.kind = K_Bow;   startWind (v, fv, art); if (note <= 88) { v.kind = K_BowPM; startBowPM (v, fv, art); } break;   // above E6 the classic engine plays (a very short string does not speak reliably)
            case K_Sub: startSub (v, fv); break;
            case K_Log: startLog (v, fv); break;
            case K_LogDrum: startLogDrum (v, fv); break;
            case K_LogDX: startLogDX (v); break;
            default: break;
        }
        // per-note humanisation: a few cents of intonation (more on the winds, which a player bends into tune), bow direction
        const bool windy = (v.kind == K_Sax || v.kind == K_Brass || v.kind == K_Bow || isPM (v.kind));
        if (! isLogKind (v.kind) && v.kind != K_Piano) v.pm[15] = gc.human * (windy ? 0.07f : 0.03f) * hashSigned (noteCounter * 7u + 3u);
        if ((v.kind == K_Bow || v.kind == K_BowPM) && pend.bowUp) v.attackInc *= 0.75f;      // up-bow: a slightly softer start
        v.baseDetune = v.pm[15]; v.mmRand = hashSigned (noteCounter * 13u + 5u);
    }

    static Patch pizzPatch (bool colLegno)
    {
        Patch p; p.name = "pizz"; p.kind = K_Pluck;
        const float v[16] = { colLegno ? .12f : .55f, .45f, .22f, colLegno ? .95f : .5f, 0, 0, 0, 0, .05f, 0, 1.f, colLegno ? .9f : .3f, 0, 80.f, 0, 0 };
        for (int i = 0; i < 16; ++i) p.p[i] = v[i];
        return p;
    }
    static float f0Of (float semis) { return 440.f * std::exp2 ((semis - 69.f) / 12.f); }
    float rnd() { return rng.uni(); }
    static float dbToLin (float d) { return std::pow (10.f, d / 20.f); }
    float decayCoef (float t60) const { return std::exp (-6.908f / (std::max (0.003f, t60) * sr)); }

    //---------------------------------------------------------------- piano
    void addOsc (Voice& v, float f, float amp, float t60, float relT, float pan)
    {
        if (v.nOsc >= kMaxOsc || f > 0.46f * sr || amp < 1e-6f) return;
        Osc& o = v.osc[v.nOsc++];
        const float phi = kTwoPi * rnd();
        o.c = std::cos (phi); o.s = std::sin (phi);
        o.cw = std::cos (kTwoPi * f / sr); o.sw = std::sin (kTwoPi * f / sr);
        o.a = amp; o.dec = decayCoef (t60); o.rel = decayCoef (relT);
        o.pl = std::cos (pan * kPi * 0.5f); o.pr = std::sin (pan * kPi * 0.5f);
    }
    void startPiano (Voice& v, float vel)
    {
        const Patch& p = v.pt;
        const int note = v.note;
        const float f0 = f0Of ((float) note);
        const float B = std::min (0.02f, 4e-5f * std::exp (0.065f * (float) (note - 21)) * p.p[5]);
        const int maxN = note < 40 ? 38 : (note < 60 ? 30 : (note < 80 ? 20 : 12));
        const float fmax = std::min (0.45f * sr, 17000.f);
        const float T1 = 20.f * std::exp2 (-(float) (note - 21) / 20.f) * p.p[2];
        const float fc = (850.f + 9500.f * std::pow (vel, 1.6f)) * p.p[0];
        const float slope = 0.65f + 1.6f * std::pow (1.f - vel, 1.2f);
        const float width = p.p[6];
        const float pb = std::min (0.82f, std::max (0.18f, 0.18f + (float) (note - 21) / 87.f * 0.64f));
        const float detune = p.p[3];
        float sumSq = 0.f;
        const bool twoStrings = note >= 28;
        for (int k = 1; k <= maxN; ++k)
        {
            const float fk = (float) k * f0 * std::sqrt (1.f + B * (float) (k * k));
            if (fk > fmax) break;
            float a = std::fabs (std::sin (kPi * (float) k * p.p[1])) * std::pow ((float) k, -slope) / std::sqrt (1.f + std::pow (fk / fc, 4.f));
            a = std::max (a, 0.f);
            float Tk = T1 / (1.f + 0.2f * std::pow ((float) (k - 1), 1.1f)) / (1.f + std::pow (fk / 3000.f, 2.f));
            Tk = std::max (Tk, 0.06f);
            const float relT = (0.20f + 0.55f * std::min (2.f, std::max (0.2f, std::pow (220.f / f0, 0.8f)))) / (1.f + 0.4f * (float) (k - 1));
            if (twoStrings)
            {
                const float sign = (k & 1) ? 1.f : -1.f;
                const float dA = detune * (0.5f + 0.5f * rnd()) * sign;
                const float rB = k <= 4 ? 0.75f : 0.45f;
                addOsc (v, fk * std::exp2 (dA / 2400.f), a, Tk, relT, std::max (0.02f, pb - width * 0.5f));
                addOsc (v, fk * std::exp2 (-dA / 2400.f), a * rB, Tk * 2.6f, relT, std::min (0.98f, pb + width * 0.5f));
                sumSq += a * a * (1.f + rB * rB);
            }
            else { addOsc (v, fk, a, Tk, relT, pb); sumSq += a * a; }
        }
        const float target = 0.50f * (0.10f + 0.90f * std::pow (vel, 1.45f));
        v.gain = target / std::max (1e-3f, 1.7f * std::sqrt (sumSq));
        v.thump = p.p[4] * 0.10f * std::pow (vel, 1.2f); v.thumpDec = std::exp (-1.f / (0.010f * sr));
        v.bq[0].lowpass (std::min (3500.f, 500.f + f0 * 3.f), 0.8f, sr);
        v.attackInc = 1.f / (0.0012f * sr);
        v.pan = pb;
    }

    //---------------------------------------------------------------- rhodes / wurlitzer / fm ep
    void startRhodes (Voice& v, float vel)
    {
        const Patch& p = v.pt; const float f0 = f0Of ((float) v.note);
        const float T = std::min (9.f, std::max (0.8f, 6.f * std::exp2 (-(float) (v.note - 40) / 24.f))) * p.p[2];
        const float relT = 0.22f;
        const float pan = 0.5f;
        addOsc (v, f0, 1.f, T, relT, pan);
        addOsc (v, f0 * 2.f * 1.0004f, p.p[4] + 0.12f * vel, T * 0.55f, relT, pan);
        const float bell = p.p[0] * (0.02f + 0.55f * vel * vel);
        addOsc (v, f0 * 6.267f, bell, 0.18f + 0.5f * std::exp2 (-(float) (v.note - 40) / 18.f), 0.06f, pan);
        addOsc (v, f0 * 17.55f, bell * 0.3f, 0.07f, 0.04f, pan);
        v.drive = p.p[1] * (0.45f + 0.9f * vel); v.bias = 0.12f + 0.12f * vel;
        v.gain = 0.34f * (0.12f + 0.88f * std::pow (vel, 1.3f));
        v.attackInc = 1.f / (0.002f * sr); v.tremPh = rnd(); v.tremDepth = p.p[3];
        v.thump = 0.02f * vel; v.thumpDec = std::exp (-1.f / (0.006f * sr));
        v.bq[0].lowpass (1800.f, 0.8f, sr);
    }
    void startWurli (Voice& v, float vel)
    {
        const Patch& p = v.pt; const float f0 = f0Of ((float) v.note);
        const float T = std::min (5.f, std::max (0.7f, 2.6f * std::exp2 (-(float) (v.note - 48) / 24.f))) * p.p[1];
        const float amps[7] = { 1.f, .62f, .38f, .22f, .12f, .07f, .04f };
        for (int k = 1; k <= 7; ++k) addOsc (v, f0 * (float) k * (1.f + 0.0002f * (float) k * (float) k), amps[k - 1] * std::pow (p.p[3], (float) (k - 1) * 0.35f) * (0.6f + 0.6f * vel), T / (1.f + 0.7f * (float) (k - 1)), 0.1f, 0.5f);
        v.drive = p.p[0] * (0.3f + 1.1f * vel); v.bias = 0.2f;
        v.gain = 0.30f * (0.12f + 0.88f * std::pow (vel, 1.25f));
        v.attackInc = 1.f / (0.0015f * sr); v.tremPh = rnd(); v.tremDepth = p.p[2];
        v.thump = 0.03f * vel; v.thumpDec = std::exp (-1.f / (0.008f * sr));
        v.bq[0].lowpass (4200.f, 0.7f, sr);
    }
    void startFM (Voice& v, float vel)
    {
        const Patch& p = v.pt; const float f0 = f0Of ((float) v.note);
        v.par[0] = f0 / sr; v.par[1] = std::min (0.45f, f0 * p.p[3] / sr);
        v.par[2] = (0.9f + 1.6f * vel) * p.p[0];                          // body modulation index
        v.par[3] = p.p[1] * (0.3f + 1.2f * vel);                            // tine index
        v.par[4] = std::min (7.f, std::max (0.7f, 4.f * std::exp2 (-(float) (v.note - 48) / 22.f))) * p.p[2];   // t60
        v.gain = 0.30f * (0.10f + 0.90f * std::pow (vel, 1.3f));
        v.attackInc = 1.f / (0.0015f * sr);
        v.ph[0] = v.ph[1] = v.ph[2] = v.ph[3] = 0.f;
    }

    //---------------------------------------------------------------- organ
    void startOrgan (Voice& v, float vel)
    {
        const Patch& p = v.pt; const float f0 = f0Of ((float) v.note);
        static const float ratio[9] = { .5f, 1.5f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 8.f };
        for (int i = 0; i < 9; ++i)
        {
            const float lv = p.p[i]; if (lv <= 0.f) continue;
            const float amp = std::pow (10.f, -(8.f - lv) * 3.f / 20.f) * 0.55f;
            addOsc (v, f0 * ratio[i], amp, 1000.f, 0.03f, 0.5f);
        }
        if (p.p[9] > 0.f)                                              // percussion: 3rd harmonic that dies away
            addOsc (v, f0 * 3.f, 0.5f * p.p[9], 0.35f, 0.03f, 0.5f);
        v.gain = 0.22f * (0.55f + 0.45f * vel);
        v.attackInc = 1.f / (0.004f * sr);
        v.thump = 0.12f; v.thumpDec = std::exp (-1.f / (0.004f * sr));
        v.bq[0].bandpass (2400.f, 1.2f, sr);
        v.drive = p.p[11]; v.tremPh = rnd();
    }

    //---------------------------------------------------------------- Prophet-style poly / moog bass
    void startPoly (Voice& v, float vel)
    {
        const Patch& p = v.pt; const float f0 = f0Of ((float) v.note);
        v.ph[0] = rnd(); v.ph[1] = rnd(); v.ph[2] = rnd();
        v.drift = (rnd() - 0.5f) * 2.f; v.drift2 = (rnd() - 0.5f) * 2.f;
        v.par[0] = p.p[0] * (0.7f + 0.6f * rnd());                           // detune of osc B in cents
        v.par[1] = f0;
        v.gain = 0.22f * (0.35f + 0.65f * vel);
        v.aStage = 0; v.fStage = 0; v.aEnv = 0; v.fEnv = 0;
        v.par[2] = vel;
        v.lad.set (p.p[1], p.p[3], sr);
    }
    void startLead (Voice& v, float vel)
    {
        for (int i = 0; i < 16; ++i) v.sp[i] = rnd();
        v.ph[0] = rnd(); v.ph[1] = rnd();
        v.drift = 0.f; v.lfo = 0.f; v.lfo2 = rnd();
        v.gain = 0.26f * (0.45f + 0.55f * vel);
        v.aStage = 0; v.fStage = 0; v.aEnv = 0; v.fEnv = 0;
        v.par[2] = vel;
        v.autoVib = v.pt.p[20] + (v.art == A_Vibrato ? 0.2f : 0.f);
        v.vibDelay = v.art == A_Vibrato ? 0.1f : 0.22f;
        v.attackInc = 1.f / (0.0015f * sr);
        v.lad.set (v.pt.p[9], v.pt.p[10], sr);
        v.bq[0].bandpass (700.f, 6.f, sr); v.bq[1].bandpass (1150.f, 7.f, sr);
    }
    /** one oscillator of the lead synth: 0 saw, 1 pulse, 2 triangle, 3 sine, 4 supersaw, 5 hard-sync saw */
    float leadOsc (Voice& v, int type, float& ph, float dt, float pw, float syncRatio, int spBase)
    {
        ph += dt; bool wrapped = false; if (ph >= 1.f) { ph -= 1.f; wrapped = true; }
        switch (type)
        {
            case 1: { float y = ph < pw ? 1.f : -1.f; y += polyBlep (ph, dt) - polyBlep (std::fmod (ph - pw + 1.f, 1.f), dt); return y; }
            case 2: return 4.f * std::fabs (ph - 0.5f) - 1.f;
            case 3: return std::sin (kTwoPi * ph);
            case 4:
            {
                static const float cents[7] = { 0.f, -11.f, 11.f, -23.f, 23.f, -37.f, 37.f };
                float y = 2.f * ph - 1.f - polyBlep (ph, dt);
                for (int k = 1; k < 7; ++k)
                {
                    float& q = v.sp[spBase + (k - 1) % 6];
                    const float d = dt * std::exp2 (cents[k] / 1200.f);
                    q += d; if (q >= 1.f) q -= 1.f;
                    y += 0.8f * (2.f * q - 1.f - polyBlep (q, d));
                }
                return y * 0.36f;
            }
            case 5:
            {
                float& sl = v.sp[15];
                const float ds = dt * syncRatio;
                if (wrapped) sl = ph * syncRatio; else { sl += ds; }
                sl -= std::floor (sl);
                return 2.f * sl - 1.f - (ds < 0.5f ? polyBlep (sl, ds) : 0.f);
            }
            default: return 2.f * ph - 1.f - polyBlep (ph, dt);
        }
    }
    bool tickLead (Voice& v, float& oL, float& oR, float ex)
    {
        const auto& P = v.pt.p;
        const int ch = v.ch;
        v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
        const float rate = P[27] > 0.f ? P[27] : 5.4f;
        v.lfo += rate / sr; if (v.lfo >= 1.f) v.lfo -= 1.f;
        v.lfo2 += 0.7f / sr; if (v.lfo2 >= 1.f) v.lfo2 -= 1.f;
        const float onset = std::min (1.f, std::max (0.f, (v.t - v.vibDelay) / 0.45f));
        const float modAmt = std::min (1.f, mod[(size_t) ch] + aft[(size_t) ch] * 0.8f);
        v.drift += (rng.next() * 0.5f - v.drift) * 0.0005f;
        const float semis = v.curSemis + bend[(size_t) ch] + std::sin (kTwoPi * v.lfo) * (v.autoVib * onset + modAmt * 0.5f) + v.drift * 0.05f * (0.5f + gc.realism) + v.pm[15];
        const float f = f0Of (semis);
        if (f > 0.45f * sr) return false;
        const float dt = f / sr;
        // envelopes (the filter shares the amp attack: brassy sounds open with the note)
        if (v.releasing) { v.aStage = 3; v.fStage = 3; }
        const float ae = adsrStep (v.aEnv, v.aStage, P[13], P[14], P[15], P[16], sr);
        const float fe = adsrStep (v.fEnv, v.fStage, P[13], P[17], P[18], P[16], sr);
        const float pw = std::min (0.95f, std::max (0.05f, P[7] + P[8] * 0.4f * std::sin (kTwoPi * v.lfo2)));
        const float syncR = std::exp2 ((P[25] * (0.25f + 0.75f * fe)) / 12.f);
        float x = leadOsc (v, (int) P[0], v.ph[0], dt, pw, syncR, 0);
        if (P[1] >= 0.f && P[4] > 0.f)
        {
            const float dB = dt * std::exp2 ((P[2] * 100.f + P[3]) / 1200.f);
            if (dB < 0.45f) x += P[4] * leadOsc (v, (int) P[1], v.ph[1], dB, pw, syncR, 6);
        }
        if (P[5] > 0.f) { v.sp[14] += dt * 0.5f; if (v.sp[14] >= 1.f) v.sp[14] -= 1.f; x += P[5] * (v.sp[14] < 0.5f ? 0.8f : -0.8f); }
        if (P[6] > 0.f) x += P[6] * rng.next();
        // talkbox: two vowel formants sweep from "oo" to "ah" with the filter envelope and the mod wheel
        if (P[24] > 0.f)
        {
            if ((v.age & 15) == 0)
            {
                const float m = std::min (1.f, 0.25f + 0.55f * fe + 0.6f * modAmt);
                v.vowM += (m - v.vowM) * 0.5f;
                v.bq[0].bandpass (320.f + 480.f * v.vowM, 5.f, sr); v.bq[1].bandpass (800.f + 450.f * v.vowM, 6.f, sr);
            }
            x = (1.f - P[24]) * x + P[24] * (2.4f * v.bq[0].process (x) + 1.8f * v.bq[1].process (x));
        }
        if ((v.age & 7) == 0)
        {
            const float key = std::exp2 (P[12] * (semis - 60.f) / 12.f);
            const float fc = P[9] * key * std::exp2 (P[11] * fe * (0.45f + 0.55f * v.par[2]) + 0.4f * (v.par[2] - 0.5f) + 0.8f * modAmt * (P[24] > 0.f ? 0.f : 1.f));
            v.lad.set (fc, P[10], sr);
        }
        float y = v.lad.process (x * 0.55f * std::max (0.3f, P[21]));
        y = std::tanh (y * 1.6f) * 0.9f;
        const float a = ae * v.gain * (0.35f + 0.65f * ex) * 2.f;
        oL = oR = y * a;
        if (v.aStage == 3 && v.aEnv < 0.0004f) return false;
        return true;
    }
    static float adsrStep (float& env, int& stage, float a, float d, float s, float r, float sr)
    {
        switch (stage)
        {
            case 0: env += 1.f / std::max (1.f, a * sr) * (1.f); if (env >= 1.f) { env = 1.f; stage = 1; } break;
            case 1: env += (s - env) * (1.f - std::exp (-4.6f / std::max (1.f, d * sr))); break;
            case 3: env += (0.f - env) * (1.f - std::exp (-4.6f / std::max (1.f, r * sr))); break;
            default: break;
        }
        return env;
    }

    //---------------------------------------------------------------- plucked strings (guitars, basses, clavinet)
    void startPluck (Voice& v, float vel, int art)
    {
        const Patch& p = v.pt; const float f0 = f0Of ((float) v.note);
        const bool mute = (art == A_PalmMute || art == A_Mute), ghost = (art == A_Ghost);
        v.loopD = std::min (0.93f, std::max (0.04f, p.p[1] + (mute ? 0.32f : 0.f) + (ghost ? 0.5f : 0.f)));
        float t60 = p.p[0] * std::min (3.f, std::max (0.25f, std::pow (110.f / f0, 0.35f)));
        if (mute) t60 *= 0.22f;
        if (ghost) t60 *= 0.05f;
        // fretboard (Human Play): thicker strings higher up the neck are darker and shorter, open strings ring longer
        t60 *= 1.f + (pend.t60Mul - 1.f) * gc.realism;
        v.loopD = std::min (0.95f, v.loopD + pend.dampAdd * gc.realism);
        v.loopG = std::min (0.99996f, std::pow (10.f, -3.f / (t60 * f0)));
        v.D = std::max (4.f, sr / f0 - v.loopD / (1.f - v.loopD) - 0.5f);
        const int N = (int) std::ceil (v.D) + 2;
        // excitation: noise with the pick position comb and a velocity dependent brightness
        float* ex = scratch.data();
        for (int i = 0; i < N; ++i) ex[i] = rng.next();
        const int pp = std::max (1, (int) std::lround (p.p[2] * (float) N));
        for (int i = N - 1; i >= pp; --i) ex[i] -= ex[i - pp];
        const float hard = std::min (1.f, p.p[3] * (0.55f + 0.7f * vel) + (art == A_Marcato || art == kArtSlap ? 0.15f : 0.f));
        const float a = 0.06f + 0.9f * hard * hard;
        float y = 0.f;
        for (int pass = 0; pass < 2; ++pass) for (int i = 0; i < N; ++i) { y += (ex[i] - y) * a; ex[i] = y; }
        float mean = 0; for (int i = 0; i < N; ++i) mean += ex[i]; mean /= (float) N;
        float peak = 1e-6f; for (int i = 0; i < N; ++i) { ex[i] -= mean; peak = std::max (peak, std::fabs (ex[i])); }
        const float amp = (0.10f + 0.90f * std::pow (vel, 1.35f)) * (ghost ? 0.5f : 1.f);
        for (int i = 0; i < N; ++i) v.buf[(size_t) ((v.wp + i) & (kBuf - 1))] = ex[i] / peak * amp;
        v.wp += N;
        v.filt = 0.f;
        v.tick_ = p.p[11] * vel * (ghost ? 1.5f : 1.f); v.tickDec = std::exp (-1.f / (0.0025f * sr));
        v.gain = 0.55f;
        // pickups, resonance, cab, body
        if (p.p[6] > 0.f) v.bq[0].lowpass (p.p[6], p.p[7], sr); else v.bq[0].set (1, 0, 0, 1, 0, 0);
        if (p.p[9] > 0.f) v.bq[1].lowpass (p.p[9], 0.7f, sr); else v.bq[1].set (1, 0, 0, 1, 0, 0);
        v.bq[2].highpass (std::max (20.f, p.p[13]), 0.7f, sr);
        v.bq[3].bandpass (98.f, 5.f, sr); v.bq[4].bandpass (205.f, 4.f, sr); v.bq[5].bandpass (480.f, 3.f, sr);
        v.drive = p.p[8] * (0.65f + 0.7f * vel);
        v.autoVib = p.p[14] + (art == A_Vibrato ? 0.22f : 0.f);
        if (art == A_Vibrato) v.vibDelay = 0.12f;
        v.vibDelay = 0.25f; v.attackInc = 1.f / (0.0008f * sr);
        v.par[0] = (float) std::max (2, (int) std::lround (2.f * p.p[4] * v.D));         // pickup 1 comb delay
        v.par[1] = (float) std::max (2, (int) std::lround (2.f * p.p[5] * v.D));         // pickup 2 comb delay
        v.par[2] = std::exp2 (0.f);
        if (instIdx == kInstGuitar && art == A_Strum) v.gain *= 0.9f;
    }

    //---------------------------------------------------------------- winds / bowed
    void startWind (Voice& v, float vel, int art)
    {
        const Patch& p = v.pt; const float f0 = f0Of ((float) v.note);
        v.ph[0] = rnd(); v.ph[1] = rnd();
        v.attackInc = 1.f / (std::max (0.004f, (art == A_Staccato || art == A_Marcato ? 0.010f : (art == A_Legato ? 0.045f : 0.030f)) * (v.kind == K_Sax ? p.p[5] : (v.kind == K_Brass ? p.p[3] : p.p[3]))) * sr);
        if (v.kind == K_Bow) v.attackInc = 1.f / (std::max (0.015f, 0.07f * p.p[3] * (0.6f + 0.6f * (1.f - vel))) * sr);
        v.gain = v.kind == K_Sax ? 0.75f : (v.kind == K_Brass ? 0.62f : 0.85f);
        v.autoVib = (v.kind == K_Sax || v.kind == K_Brass) ? p.p[6] : p.p[4];
        if (art == A_Vibrato) v.autoVib = std::max (v.autoVib * 2.2f, 0.16f);
        if (art == A_Staccato) v.autoVib = 0.f;
        v.vibDelay = v.kind == K_Bow ? 0.18f : 0.28f;
        v.scoop = (art == A_Scoop || v.kind == K_Brass) ? 0.35f : (v.kind == K_Sax ? 0.22f : 0.f);
        v.par[0] = f0;
        // formants (absolute frequencies)
        if (v.kind == K_Sax)
        {
            static const float F[4][3] = { { 520.f, 1250.f, 2750.f }, { 650.f, 1550.f, 3100.f }, { 900.f, 2100.f, 3900.f }, { 380.f, 950.f, 2200.f } };
            const int set = std::min (3, std::max (0, (int) std::lround (p.p[0])));
            v.bq[0].bandpass (F[set][0], 2.2f, sr); v.bq[1].bandpass (F[set][1], 2.6f, sr); v.bq[2].bandpass (F[set][2], 2.2f, sr);
            v.bq[3].bandpass (3300.f, 0.9f, sr);                                       // breath
        }
        else if (v.kind == K_Brass)
        {
            const float s = p.p[1];
            v.bq[0].bandpass (1150.f * s, 2.f, sr); v.bq[1].bandpass (2400.f * s, 2.f, sr); v.bq[2].bandpass (1100.f, 3.5f, sr);   // 2 = harmon-mute nasal peak
            v.bq[3].bandpass (3800.f, 0.9f, sr);
            v.bq[4].lowpass (std::max (1200.f, 6000.f - 4200.f * p.p[2]), 0.7f, sr);
        }
        else
        {
            const float b = p.p[0];                                                      // body modes of the instrument
            v.bq[0].bandpass (290.f * b, 4.f, sr); v.bq[1].bandpass (450.f * b, 4.f, sr); v.bq[2].bandpass (1050.f * b, 2.5f, sr);
            v.bq[3].bandpass (2600.f, 1.2f, sr); v.bq[4].bandpass (3600.f, 3.5f, sr);
            v.bq[5].bandpass (6500.f, 0.8f, sr);                                         // bow noise
        }
    }

    //---------------------------------------------------------------- bass synths
    void startSub (Voice& v, float vel)
    {
        v.gain = 0.45f * (0.3f + 0.7f * vel); v.attackInc = 1.f / (0.002f * sr); v.ph[0] = 0.f;
        v.par[0] = v.pt.p[0];
        if (v.pt.p[3] > 0.f) v.lad.set (v.pt.p[3], 0.f, sr);
    }

    //---------------------------------------------------------------- v3.0 log drum: the DX10 "Log Drum" recipe (Paul Kellett, mda DX10, MIT licence)
    // One carrier and one sine modulator. The modulator's index starts high and falls to a low sustain within a few milliseconds:
    // that is the wooden knock. The carrier is a 5th-order sine approximation whose "waveform" setting adds odd harmonics: the hollow log body.
    // Added here: glide (the slide between notes that amapiano is built on), a clean sine sub, an optional pitch drop, drive, tone, width.
    void startLogDX (Voice& v)
    {
        const auto& P = v.pt.p;
        const float Fs = sr, ifs = 1.f / Fs;
        float* q = v.pm;
        std::fill (std::begin (v.pm), std::end (v.pm), 0.f);
        const float h = logc.human;
        v.par[5] = h * 0.06f * hashSigned (noteCounter * 17u + 1u);                      // +/- 6 cents at 100 % humanize
        const float lvlVar = dbToLin (h * 1.5f * hashSigned (noteCounter * 29u + 3u));
        // DX10 update(): envelope coefficients from the preset (decay scaled by the panel)
        const float decRate = std::exp (5.f - 8.f * P[1]) / std::max (0.25f, logc.decayMul);
        v.par[0] = std::exp (-ifs * decRate);                                               // cdec
        v.par[1] = std::exp (-ifs * std::exp (5.f - 5.f * P[2]) / std::max (0.25f, logc.decayMul));   // crel
        v.par[2] = 1.f - std::exp (-ifs * std::exp (6.f - 7.f * P[6]));                     // mdec
        v.par[3] = 1.f - std::exp (-ifs * std::exp (5.f - 8.f * P[8]));                     // mrel
        v.par[4] = 1.f - std::exp (-1.f / (std::max (0.00005f, logc.attack) * Fs));         // catt (DX10 preset: about 0.3 ms)
        float rati = std::floor (40.1f * P[3] * P[3]);
        float ratf = P[4] < 0.5f ? 0.2f * P[4] * P[4] : 0.75f;
        v.par[6] = 0.5f * (rati + ratf);                                                    // modulator / carrier frequency ratio
        // DX10 noteOn(): key tracking + velocity sensitivity of the modulation index
        float l = std::exp (0.05776226505f * (float) (v.note + 12));                       // key tracking of the DX10 note that sounds at this pitch
        if (l > 50.f) l = 50.f;
        const float velsens = std::clamp (logc.velResp, 0.f, 1.f);
        l *= 64.f + velsens * ((float) v.vel - 64.f);
        const float depth = 0.0002f * P[5] * P[5], dept2 = 0.0002f * P[7] * P[7] * logc.bodyMul;
        q[2] = depth * l * logc.punchMul;                                                   // menv (falls toward mlev)
        q[3] = dept2 * l;                                                                   // mlev
        q[4] = v.par[2];                                                                    // current mod envelope rate
        const float velAmp = std::pow (((float) v.vel + 10.f) / 137.f, 0.4f + 1.2f * velsens);
        q[5] = (1.5f - P[13]) * 0.0035f * 137.f * velAmp * lvlVar;                         // env (DX10 level at full velocity)
        q[6] = 0.f;                                                                         // cenv
        q[7] = v.par[4];                                                                    // catt
        q[8] = v.par[0];                                                                    // cdec
        v.par[7] = 0.50f - 3.0f * P[13] * P[13];                                            // rich (waveform)
        v.par[8] = 0.25f * P[14] * P[14];                                                   // mod thru
        q[0] = 0.f; q[1] = 0.f;                                                             // carrier / modulator phase
        v.par[9] = std::exp (-1.f / (0.9f * Fs) / std::max (0.25f, logc.decayMul));          // sub decay (a little longer than the body)
        q[9] = 0.f; q[10] = 1.f; q[11] = 0.f;                                                            // sub phase, sub level env
        v.wp = 0; v.lp[0] = v.lp[1] = v.lp[2] = 0.f;
        v.gain = 1.f;
    }
    bool tickLogDX (Voice& v, float& oL, float& oR)
    {
        const auto& P = v.pt.p;
        float* q = v.pm;
        const int ch = v.ch;
        if (v.releasing && q[11] == 0.f)                                               // DX10 note-off: release envelopes
        {
            q[11] = 1.f; q[8] = v.par[1]; q[5] = q[6]; q[7] = 1.f; q[3] = 0.f; q[4] = v.par[3];
        }
        v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
        const float drop = P[18] * logc.dropMul + std::max (0.f, logc.dropMul - 1.f) * 3.f;
        float dropSemis = 0.f;
        if (drop > 0.001f)
        {
            const float dT = std::max (0.004f, (P[19] > 0.f ? P[19] : 0.03f) * logc.dropTimeMul), e3 = 0.0497871f;
            dropSemis = drop * std::max (0.f, (std::exp (-v.t / dT) - e3) / (1.f - e3));     // lands exactly on pitch
        }
        const float semis = v.curSemis + logc.tune + bend[(size_t) ch] + v.par[5] + dropSemis + v.pm[15] * 0.f;
        const float f = f0Of (semis);
        if (f > 0.4f * sr) return false;
        // ---- the DX10 voice (per sample, as in mdaDX10::processReplacing)
        const float e = q[5];
        if (e <= 0.0003f) return false;
        q[5] = e * q[8];                                                                     // decay & release
        q[6] += q[7] * (e - q[6]);                                                           // attack
        const float dcar = 2.f * f / sr;                                                     // carrier phase step (phase runs -1..1)
        q[1] += kTwoPi * v.par[6] * f / sr; if (q[1] > kTwoPi) q[1] -= kTwoPi;               // modulator (a free sine, so it can glide)
        const float m = -std::sin (q[1]);                                                     // the DX10 recursive sine starts as -sin
        q[2] += q[4] * (q[3] - q[2]);                                                        // modulation index envelope
        float x = q[0] + dcar + m * q[2];
        while (x > 1.f) x -= 2.f;
        while (x < -1.f) x += 2.f;
        q[0] = x;
        const float w = std::clamp (v.par[7], -1.f, 0.6f);
        float body = q[6] * (v.par[8] * m + (x + x * x * x * (w * x * x - 1.f - w)));
        // ---- additions: tone, drive, clean sub, width
        if (std::fabs (logc.tone) > 0.01f)
        {
            const float fc = 1400.f * std::exp2 (logc.tone * 2.f);
            v.lp[0] += (body - v.lp[0]) * (1.f - std::exp (-kTwoPi * fc / sr));
            body = logc.tone < 0.f ? v.lp[0] : body + (body - v.lp[0]) * logc.tone * 0.8f;
        }
        const float drv = std::clamp (P[17] + logc.drive, 0.f, 1.5f);
        if (drv > 0.001f) { const float d = 1.f + 6.f * drv; body = std::tanh (body * d * 2.2f) / (2.2f * (1.f + 0.8f * drv)); }
        q[9] += f / sr; if (q[9] >= 1.f) q[9] -= 1.f;
        q[10] *= v.par[9];
        if (v.releasing) q[10] *= v.par[1];
        const float subLvl = std::clamp (P[16] * dbToLin (logc.subDb), 0.f, 3.f);
        const float sub = subLvl * 0.35f * std::sin (kTwoPi * q[9]) * q[10] * std::min (1.f, q[6] / std::max (1e-4f, e) * 4.f) * std::min (1.f, e * 3.f + 0.2f);
        // width: the upper part of the body only, as a tiny Haas offset (the low end stays mono)
        v.lp[1] += (body - v.lp[1]) * (1.f - std::exp (-kTwoPi * 220.f / sr));
        const float hi = body - v.lp[1];
        v.buf[(size_t) (v.wp & (kBuf - 1))] = hi;
        const int dl = std::max (1, (int) ((0.0003f + 0.0009f * logc.width) * sr));
        const float hiD = v.buf[(size_t) ((v.wp - dl) & (kBuf - 1))];
        ++v.wp;
        const float wAmt = std::clamp (logc.width + P[21] * 0.5f, 0.f, 1.f);
        const float lowAll = v.lp[1] + sub;
        oL = (lowAll + hi) * 1.9f;
        oR = (lowAll + hi * (1.f - wAmt) + hiD * wAmt) * 1.9f;
        return true;
    }
    //---------------------------------------------------------------- v2.7 Amapiano log drum (tuned modal drum, separate layers)
    void startLogDrum (Voice& v, float vel)
    {
        const auto& P = v.pt.p;
        // seeded per-note humanisation: tiny pitch, level, decay and click differences (the same seed gives the same result)
        const float h = logc.human;
        v.par[5] = (rnd() - 0.5f) * 2.f * 0.06f * h;                   // semitones (up to +/- 6 cents)
        const float lvlVar = dbToLin ((rnd() - 0.5f) * 3.f * h);
        v.par[6] = 1.f + (rnd() - 0.5f) * 0.24f * h;                  // decay
        v.par[7] = 1.f + (rnd() - 0.5f) * 0.4f * h;                   // click
        const float vr = std::clamp (logc.velResp, 0.f, 1.f);
        const float vg = (1.f - vr) + vr * std::pow (std::max (0.05f, vel), 1.2f);
        // even level across the keyboard: low notes carry more energy
        const float f0 = f0Of ((float) v.note + logc.tune);
        v.gain = 0.62f * vg * lvlVar * std::pow (std::max (20.f, f0) / 82.f, 0.18f);
        v.par[0] = vel;
        v.par[1] = std::clamp ((float) (v.note - 36) / 36.f, -1.f, 1.5f);   // key position for decay scaling
        v.ph[0] = 0.f; v.ph[1] = 0.f; v.ph[2] = 0.f; v.ph[3] = 0.f;      // all modes start together at zero phase: no click, no cancellation
        v.attackInc = 1.f / (std::max (0.0002f, logc.attack) * sr);
        v.tick_ = std::clamp (P[3] * v.par[7] * (0.4f + 0.6f * vel * (0.4f + 0.6f * vr)), 0.f, 1.5f); v.tickDec = std::exp (-1.f / (0.0028f * sr));
        v.bq[0].bandpass (2400.f, 1.2f, sr);                            // strike noise
        v.bq[1].bandpass (std::clamp (f0 * 4.5f, 280.f, 1800.f), 3.f, sr); // the hollow wood knock
        v.bq[2].set (1, 0, 0, 1, 0, 0);
        v.lp[0] = v.lp[1] = 0.f;
        v.wp = 0;
    }
    bool tickLogDrum (Voice& v, float& oL, float& oR, float env)
    {
        const auto& P = v.pt.p;
        const int ch = v.ch;
        v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
        const float tAtk = v.t;                                           // time since the strike (a glide keeps it running)
        const float drop = P[1] * logc.dropMul, dropT = std::max (0.004f, P[2] * logc.dropTimeMul);
        // the drop lands exactly on pitch after 3 drop-times (an exponential that is pulled down to zero), so the note is in tune, not "nearly"
        const float e3 = 0.0497871f;
        const float dropShape = std::max (0.f, (std::exp (-tAtk / dropT) - e3) / (1.f - e3));
        const float semis = v.curSemis + logc.tune + bend[(size_t) ch] + v.par[5] + drop * dropShape;
        const float f = f0Of (semis);
        if (f > 0.45f * sr) return false;
        const float dt = f / sr;
        const float keyDecay = std::exp2 (-v.par[1] * 0.9f);
        const float tau = std::max (0.03f, P[0] * logc.decayMul * v.par[6] * keyDecay) / 6.9f;
        const float subTau = tau * std::max (0.3f, P[13]);
        float envB = std::exp (-tAtk / tau);
        if (P[10] > 0.f && v.held) envB = std::max (envB, P[10] * 0.6f);         // hybrid: a bass tone that sustains while the key is held
        float envS = std::exp (-tAtk / subTau);
        if (P[10] > 0.f && v.held) envS = std::max (envS, P[10] * 0.7f);
        const float punch = 1.f + std::clamp (P[4] * logc.punchMul, 0.f, 2.f) * 1.1f * std::exp (-tAtk / 0.012f);
        // phases: the fundamental and the harmonic wood modes share one pitch, so they stay locked
        v.ph[0] += dt; if (v.ph[0] >= 1.f) v.ph[0] -= 1.f;
        const float s1 = std::sin (kTwoPi * v.ph[0]);
        const float s2 = std::sin (kTwoPi * std::fmod (v.ph[0] * P[14], 1.f));
        const float s3 = std::sin (kTwoPi * std::fmod (v.ph[0] * P[15], 1.f));
        const float subLvl = std::clamp (P[5], 0.f, 1.5f) * dbToLin (logc.subDb);
        const float body = std::clamp (P[6] * logc.bodyMul, 0.f, 2.f);
        const float fund = s1 * (envB * punch * 0.75f + subLvl * envS * 0.6f);
        float harm = body * (0.42f * s2 * std::exp (-tAtk / (tau * 0.32f)) + 0.18f * s3 * std::exp (-tAtk / (tau * 0.16f))) * punch;
        // strike: band-limited noise + the wooden knock resonance, a few ms long
        float strike = 0.f;
        if (v.tick_ > 1e-4f)
        {
            const float n = rng.next();
            strike = v.tick_ * (0.55f * v.bq[0].process (n) + 1.4f * v.bq[1].process (n));
            v.tick_ *= v.tickDec;
        }
        // tone: a low-pass on everything above the fundamental (the fundamental itself is never filtered)
        const float tone = std::clamp (P[7] * std::exp2 (logc.tone * 1.5f), 200.f, 12000.f);
        const float k = 1.f - std::exp (-kTwoPi * tone / sr);
        v.lp[0] += ((harm + strike) - v.lp[0]) * k;
        float upper = v.lp[0];
        // drive: one smooth curve over the whole note; everything is harmonic, so it adds weight without mud
        float x = fund + upper;
        const float drv = std::clamp (P[8] + logc.drive, 0.f, 1.2f);
        if (drv > 0.01f) { const float d = 1.f + 4.f * drv; x = std::tanh (d * x) / std::tanh (d); }
        // stereo width only above the fundamental (the low end stays mono)
        const float w = std::clamp (logc.width, 0.f, 1.f);
        v.buf[(size_t) (v.wp & (kBuf - 1))] = upper;
        const int dly = std::max (1, (int) (0.0007f * sr));
        const float upD = v.buf[(size_t) ((v.wp - dly) & (kBuf - 1))];
        ++v.wp;
        const float side = w * 0.5f * (upper - upD);
        const float a = v.gain * env * (1.f + 0.f * fund);
        oL = (x + side) * a; oR = (x - side) * a;
        if (envB < 0.0004f && envS < 0.0004f && v.tick_ < 1e-4f) return false;
        return true;
    }

    void startLog (Voice& v, float vel)
    {
        v.gain = 0.5f * (0.3f + 0.7f * vel); v.attackInc = 1.f / (0.0008f * sr); v.ph[0] = v.ph[1] = 0.f;
        v.tick_ = 0.35f * vel; v.tickDec = std::exp (-1.f / (0.004f * sr));
        v.bq[0].bandpass (std::min (2400.f, f0Of ((float) v.note) * 5.f), 1.5f, sr);
    }

    //---------------------------------------------------------------- per-sample voice rendering
    bool tick (Voice& v, float& oL, float& oR)
    {
        const int ch = v.ch;
        // amplitude envelope (attack ramp, release)
        float env;
        if (v.releasing) { v.env *= v.relCoef; env = v.env; if (v.env < 0.0003f && v.kind != K_Piano) return false; }
        else { v.env = std::min (1.f, v.env + v.attackInc); env = v.env; }
        v.t += 1.f / sr; ++v.age;
        const float fv = (float) v.vel / 127.f;
        const float ex = exprExp == 1.f ? expr[(size_t) ch] : std::pow (std::max (0.f, expr[(size_t) ch]), exprExp);
        switch (v.kind)
        {
            case K_Piano:
            {
                float l = 0, r = 0, n = 0;
                for (int i = 0; i < v.nOsc; ++i)
                {
                    Osc& o = v.osc[i];
                    const float ns = o.s * o.cw + o.c * o.sw, nc = o.c * o.cw - o.s * o.sw;
                    o.s = ns; o.c = nc; o.a *= o.dec;
                    const float y = o.s * o.a; l += y * o.pl; r += y * o.pr;
                }
                if ((v.age & 2047) == 0) for (int i = 0; i < v.nOsc; ++i) { Osc& o = v.osc[i]; const float m = 1.f / std::sqrt (o.s * o.s + o.c * o.c); o.s *= m; o.c *= m; }
                if (v.thump > 1e-5f) { v.nz += (rng.next() - v.nz) * 0.2f; n = v.bq[0].process (v.nz) * v.thump; v.thump *= v.thumpDec; l += n; r += n; }
                if (v.releasing && v.nOsc > 0) { float mx = 0; for (int i = 0; i < v.nOsc; ++i) mx = std::max (mx, v.osc[i].a); if (mx < 2e-5f) return false; }
                const float a = (v.releasing ? 1.f : env) * v.gain * 1.4f;
                oL = l * a; oR = r * a; return true;
            }
            case K_Rhodes: case K_Wurli:
            {
                float x = 0;
                for (int i = 0; i < v.nOsc; ++i)
                {
                    Osc& o = v.osc[i];
                    const float ns = o.s * o.cw + o.c * o.sw, nc = o.c * o.cw - o.s * o.sw;
                    o.s = ns; o.c = nc; o.a *= v.releasing ? o.rel : o.dec; x += o.s * o.a;
                }
                if ((v.age & 2047) == 0) for (int i = 0; i < v.nOsc; ++i) { Osc& o = v.osc[i]; const float m = 1.f / std::sqrt (o.s * o.s + o.c * o.c); o.s *= m; o.c *= m; }
                float y;                                                                   // pickup / reed non-linearity: the bark of a hard hit
                const float b = v.bias, d = std::max (0.2f, v.drive);
                y = (std::tanh (d * (x + b)) - std::tanh (d * b)) / std::sqrt (d);
                if (v.thump > 1e-5f) { v.nz += (rng.next() - v.nz) * 0.25f; y += v.bq[0].process (v.nz) * v.thump; v.thump *= v.thumpDec; }
                v.tremPh += (v.kind == K_Rhodes ? 4.6f : 5.6f) / sr; if (v.tremPh >= 1.f) v.tremPh -= 1.f;
                const float tr = std::sin (kTwoPi * v.tremPh);
                const float amp = v.gain * env * (v.kind == K_Wurli ? (1.f + v.tremDepth * (0.5f * tr + 0.5f * mod[(size_t) ch])) : 1.f);
                if (v.kind == K_Rhodes) { const float pn = 0.5f + 0.5f * v.tremDepth * tr; oL = y * amp * std::cos (pn * kPi * 0.5f) * 1.4f; oR = y * amp * std::sin (pn * kPi * 0.5f) * 1.4f; }
                else { oL = oR = y * amp * 1.2f; }
                return ! (v.releasing && v.env < 0.0004f);
            }
            case K_FMEP:
            {
                const float tt = v.t;
                const float envBody = std::exp (-tt * 6.9f / v.par[4]);
                const float idxBody = v.par[2] * (0.25f + 0.75f * envBody) , idxTine = v.par[3] * std::exp (-tt / 0.07f);
                v.ph[0] += v.par[0]; v.ph[1] += v.par[1]; if (v.ph[0] >= 1.f) v.ph[0] -= 1.f; if (v.ph[1] >= 1.f) v.ph[1] -= 1.f;
                const float body = std::sin (kTwoPi * v.ph[0] + idxBody * std::sin (kTwoPi * v.ph[0]));
                const float tine = std::sin (kTwoPi * v.ph[0] + idxTine * std::sin (kTwoPi * v.ph[1]));
                const float y = (body * envBody + 0.35f * tine * std::exp (-tt / 0.25f)) * v.gain * env;
                const float pn = 0.5f + 0.1f * std::sin (kTwoPi * v.t * 0.6f);
                oL = y * std::cos (pn * kPi * 0.5f) * 1.4f; oR = y * std::sin (pn * kPi * 0.5f) * 1.4f;
                return ! (envBody < 0.0006f && ! v.held);
            }
            case K_Organ:
            {
                const bool fast = mod[(size_t) ch] > 0.5f || v.pt.p[10] > 0.5f;
                v.tremPh += (fast ? 6.6f : 0.85f) / sr; if (v.tremPh >= 1.f) v.tremPh -= 1.f;
                const float lf = std::sin (kTwoPi * v.tremPh);
                float x = 0;
                for (int i = 0; i < v.nOsc; ++i)
                {
                    Osc& o = v.osc[i];
                    const float ns = o.s * o.cw + o.c * o.sw, nc = o.c * o.cw - o.s * o.sw;
                    o.s = ns; o.c = nc; o.a *= o.dec; x += o.s * o.a;
                }
                if ((v.age & 1023) == 0) for (int i = 0; i < v.nOsc; ++i) { Osc& o = v.osc[i]; const float m = 1.f / std::sqrt (o.s * o.s + o.c * o.c); o.s *= m; o.c *= m; }
                if (v.thump > 1e-5f) { x += v.bq[0].process (rng.next()) * v.thump; v.thump *= v.thumpDec; }
                const float d = v.drive;
                float y = std::tanh (d * x) / std::max (0.5f, std::sqrt (d));
                const float am = 1.f + (fast ? 0.18f : 0.07f) * lf;
                oL = y * v.gain * env * ex * (am + 0.04f * lf) * 1.4f; oR = y * v.gain * env * ex * (am - 0.04f * lf) * 1.4f;
                return true;
            }
            case K_Poly:
            {
                v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
                const float semis = v.curSemis + bend[(size_t) ch];
                v.lfo += 5.2f / sr; if (v.lfo >= 1.f) v.lfo -= 1.f;
                const float vib = std::sin (kTwoPi * v.lfo) * mod[(size_t) ch] * 0.5f;
                v.drift += (rng.next() - v.drift) * 0.0002f;
                const float f = f0Of (semis + vib + v.drift * 0.04f);
                const float dA = f / sr, dB = f * std::exp2 (v.par[0] / 1200.f + v.drift * 0.0008f) / sr;
                v.ph[0] += dA; if (v.ph[0] >= 1.f) v.ph[0] -= 1.f;
                v.ph[1] += dB; if (v.ph[1] >= 1.f) v.ph[1] -= 1.f;
                const float pw = v.pt.p[13];
                float oa = 2.f * v.ph[0] - 1.f - polyBlep (v.ph[0], dA);
                float ob = 2.f * v.ph[1] - 1.f - polyBlep (v.ph[1], dB);
                if (pw > 0.f) { const float pa = (v.ph[1] < pw ? 1.f : -1.f) + polyBlep (v.ph[1], dB) - polyBlep (std::fmod (v.ph[1] - pw + 1.f, 1.f), dB); ob = pa; }
                float x = 0.5f * (oa + ob) + v.pt.p[14] * rng.next();
                // envelopes
                if (v.releasing) { v.aStage = 3; v.fStage = 3; }
                else if (v.aStage == 1 && v.pt.p[6] <= 0.001f && v.aEnv < 0.001f) v.aStage = 3;
                const float ae = adsrStep (v.aEnv, v.aStage, v.pt.p[4], v.pt.p[5], v.pt.p[6], v.pt.p[7], sr);
                const float fe = adsrStep (v.fEnv, v.fStage, v.pt.p[8], v.pt.p[9], v.pt.p[10], v.pt.p[11], sr);
                if ((v.age & 7) == 0)
                {
                    const float key = std::exp2 (v.pt.p[12] * (semis - 60.f) / 12.f);
                    const float vel = v.par[2];
                    const float fc = v.pt.p[1] * key * std::exp2 (v.pt.p[2] * fe * (0.4f + 0.6f * vel) + 0.5f * (vel - 0.5f) + ex * 0.f);
                    v.lad.set (fc, v.pt.p[3], sr);
                }
                const float y = v.lad.process (x * 1.3f) * 1.6f;
                const float a = ae * v.gain * (instIdx == kInstBass ? 1.4f : 1.f);
                oL = oR = y * a;
                if (v.releasing && v.aEnv < 0.0004f) return false;
                if (v.aStage == 3 && v.aEnv < 0.0004f && ! v.releasing) return false;
                return true;
            }
            case K_Pluck: return tickPluck (v, oL, oR, env, fv);
            case K_Lead: return tickLead (v, oL, oR, ex);
            case K_Sax: case K_Brass: case K_Bow: return tickWind (v, oL, oR, env, fv, ex);
            case K_SaxPM: case K_BrassPM: case K_BowPM: return tickPM (v, oL, oR, env, fv, ex);
            case K_Sub:
            {
                v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
                const float f = f0Of (v.curSemis + bend[(size_t) ch]) * (1.f + v.pt.p[1] * 0.35f * std::exp (-v.t / 0.035f));
                v.ph[0] += f / sr; if (v.ph[0] >= 1.f) v.ph[0] -= 1.f;
                float x = std::sin (kTwoPi * v.ph[0]);
                const float d = v.pt.p[2];
                x = std::tanh (d * x) / std::tanh (d);
                if (v.pt.p[3] > 0.f) x = v.lad.process (x);
                const float dec = std::exp (-v.t * 6.9f / (v.par[0] * (0.5f + 0.5f * std::exp2 (-(float) (v.note - 36) / 24.f))));
                const float y = x * v.gain * env * (v.held ? std::max (dec, 0.f) : dec);
                oL = oR = y; return dec > 0.0004f || v.held;
            }
            case K_LogDrum: return tickLogDrum (v, oL, oR, env);
            case K_LogDX: return tickLogDX (v, oL, oR);
            default:   // K_Log
            {
                const float f = f0Of ((float) v.note);
                const float drop = 1.f + v.pt.p[1] * 0.12f * std::exp (-v.t / 0.03f);
                v.ph[0] += f * drop / sr; v.ph[1] += f * v.pt.p[2] * drop / sr;
                if (v.ph[0] >= 1.f) v.ph[0] -= 1.f;
                if (v.ph[1] >= 1.f) v.ph[1] -= 1.f;
                const float dec0 = std::exp (-v.t * 6.9f / v.pt.p[0]), dec1 = std::exp (-v.t * 6.9f / (v.pt.p[0] * 0.22f));
                float x = std::sin (kTwoPi * v.ph[0]) * dec0 + 0.38f * std::sin (kTwoPi * v.ph[1]) * dec1;
                if (v.tick_ > 1e-4f) { x += v.bq[0].process (rng.next()) * v.tick_; v.tick_ *= v.tickDec; }
                x = std::tanh (1.5f * x);
                oL = oR = x * v.gain * env; return dec0 > 0.0005f;
            }
        }
    }

    bool tickPluck (Voice& v, float& oL, float& oR, float env, float /*fv*/)
    {
        const int ch = v.ch;
        v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
        v.lfo += 5.4f / sr; if (v.lfo >= 1.f) v.lfo -= 1.f;
        const float onset = std::min (1.f, std::max (0.f, (v.t - v.vibDelay) / 0.4f));
        const float modAmt = std::min (1.f, mod[(size_t) ch] + aft[(size_t) ch] * 0.8f);
        const float semis = v.curSemis + bend[(size_t) ch] + std::sin (kTwoPi * v.lfo) * (modAmt * 0.5f + v.autoVib) * onset + v.pm[15];
        const float f0 = f0Of (semis);
        if (f0 > 0.2f * sr || f0 < 15.f) return false;
        const float Dt = std::max (4.f, sr / f0 - v.loopD / (1.f - v.loopD) - 0.5f);
        v.D += (Dt - v.D) * 0.02f; if (std::fabs (Dt - v.D) < 0.01f) v.D = Dt;
        // read the delay line (cubic Hermite)
        const float rp = (float) v.wp - v.D;
        const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0; const int m = kBuf - 1;
        const float ym1 = v.buf[(size_t) ((i0 - 1) & m)], y0 = v.buf[(size_t) (i0 & m)], y1 = v.buf[(size_t) ((i0 + 1) & m)], y2 = v.buf[(size_t) ((i0 + 2) & m)];
        const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        float y = ((c3 * fr + c2) * fr + c1) * fr + y0;
        v.filt = (1.f - v.loopD) * y + v.loopD * v.filt;
        float inject = 0.f;
        if (v.pluckBoost > 0.f) { inject = v.pluckBoost * 0.02f * rng.next(); v.pluckBoost *= 0.9995f; }       // hammer-on: the finger re-excites the string a little
        v.buf[(size_t) (v.wp & m)] = v.filt * v.loopG * (v.releasing ? 0.9f : 1.f) + inject;
        ++v.wp;
        v.hist[(size_t) (v.hp & (kHist - 1))] = y; ++v.hp;
        // pickups: two positions along the string (comb), blended
        auto histAt = [&] (int d) { return v.hist[(size_t) ((v.hp - 1 - std::min (d, kHist - 2)) & (kHist - 1))]; };
        float o;
        const float q1 = v.pt.p[4], q2 = v.pt.p[5];
        if (q1 > 0.f)
        {
            const float a = y - histAt ((int) v.par[0]);
            if (q2 > 0.f) { const float b = y - histAt ((int) v.par[1]); const float bl = v.pt.p[12]; o = (a * bl + b * (1.f - bl)) * 0.7f; }
            else o = a * 0.7f;
        }
        else o = y;
        o = v.bq[0].process (o);
        if (v.tick_ > 1e-4f) { o += v.bq[2].process (rng.next()) * v.tick_ * 0.5f; v.tick_ *= v.tickDec; }
        if (v.drive > 0.01f) { const float d = v.drive; o = std::tanh (d * o) / std::tanh (std::max (0.5f, d) * 0.6f + 0.2f) * (d > 1.f ? 0.55f : 1.f); }
        if (v.pt.p[9] > 0.f) o = v.bq[1].process (o);
        // acoustic body (guitar / upright): wood resonances
        if (v.pt.p[10] > 0.f) o = o * 0.7f + v.pt.p[10] * (1.0f * v.bq[3].process (y) + 0.9f * v.bq[4].process (y) + 0.5f * v.bq[5].process (y));
        // dc / sub-sonic rumble
        const float hpo = o - v.x1 + 0.9975f * v.y1; v.x1 = o; v.y1 = hpo; o = hpo;
        const float amp = v.gain * env * (v.kind == K_Pluck && instIdx == kInstGuitar ? 1.2f : 1.f);
        oL = oR = o * amp * 1.5f;
        if (v.releasing && v.env < 0.0004f) return false;
        if (std::fabs (y0) + std::fabs (y1) < 1e-7f && v.t > 0.2f && ! v.held) return false;
        return true;
    }

    bool tickWind (Voice& v, float& oL, float& oR, float env, float fv, float ex)
    {
        const int ch = v.ch;
        v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
        v.lfo += (v.kind == K_Bow ? 5.8f : 5.3f) / sr; if (v.lfo >= 1.f) v.lfo -= 1.f;
        const float onset = std::min (1.f, std::max (0.f, (v.t - v.vibDelay) / 0.4f));
        const float modAmt = std::min (1.f, mod[(size_t) ch] + aft[(size_t) ch] * 0.8f);
        // slow random fluctuation: no two periods are quite alike
        v.jitT -= 1.f; if (v.jitT <= 0.f) { v.jitT = sr / 28.f; v.jit1 = (rng.next()) * 0.045f; v.jitV = rng.next() * 0.05f; }
        v.jit2 += (v.jit1 - v.jit2) * 0.003f; v.jitA += (v.jitV - v.jitA) * 0.004f;
        float semis = v.curSemis + bend[(size_t) ch] + std::sin (kTwoPi * v.lfo) * (modAmt * 0.6f + v.autoVib * (v.kind == K_Bow ? 1.f : 1.f)) * onset + v.jit2 + v.pm[15];
        if (v.art == A_Trill || v.art == A_Shake) { const float rate = v.art == A_Shake ? 7.f : 11.f; if (std::fmod (v.t * rate, 1.f) > 0.5f && v.t > 0.1f) semis += (v.art == A_Shake ? 2.f : 1.5f); }
        semis -= v.scoop * std::exp (-v.t / 0.03f);
        const float f0 = f0Of (semis);
        if (f0 > 0.4f * sr || f0 < 15.f) return false;
        const float dph = f0 / sr;
        const float lvl = std::min (1.f, ex * (0.45f + 0.55f * fv));
        float y;
        if (v.kind == K_Sax)
        {
            v.ph[0] += dph; if (v.ph[0] >= 1.f) v.ph[0] -= 1.f;
            const float saw = 2.f * v.ph[0] - 1.f - polyBlep (v.ph[0], dph);
            float pw = v.ph[0] < 0.38f ? 1.f : -1.f; pw += polyBlep (v.ph[0], dph) - polyBlep (std::fmod (v.ph[0] - 0.38f + 1.f, 1.f), dph);
            float src = 0.62f * saw + 0.5f * pw;
            src = std::tanh (v.pt.p[4] * (1.f + 1.2f * lvl) * src) / std::tanh (v.pt.p[4] * (1.f + 1.2f * lvl)) ;
            const float bright = lvl * (v.art == A_Subtone ? 0.45f : 1.f);
            const float fc = std::min (9000.f, std::max (350.f, f0 * (2.f + 5.5f * bright) * v.pt.p[1]));
            v.lp[0] += (src - v.lp[0]) * (1.f - std::exp (-kTwoPi * fc / sr));
            v.lp[1] += (v.lp[0] - v.lp[1]) * (1.f - std::exp (-kTwoPi * fc * 1.6f / sr));
            y = v.lp[1] * 0.5f + 1.0f * v.bq[0].process (v.lp[1]) + 0.75f * v.bq[1].process (v.lp[1]) + 0.35f * v.bq[2].process (v.lp[1]);
            const float breath = v.pt.p[2] * (v.art == A_Subtone ? 2.2f : 1.f) + 0.10f * std::exp (-v.t / 0.04f);
            y += breath * v.bq[3].process (rng.next()) * (0.4f + 0.6f * ex);
            const float grow = std::max (v.pt.p[3], v.art == A_Growl ? 0.6f : 0.f);
            if (grow > 0.f) { y *= 1.f - grow * 0.5f * (0.5f + 0.5f * std::sin (kTwoPi * 38.f * v.t)); y = std::tanh (y * (1.f + 2.f * grow)) * 0.8f; }
            if (v.art == A_Flutter) y *= (std::fmod (v.t * 24.f, 1.f) < 0.5f) ? 1.f : 0.25f;
            y = std::tanh (1.25f * y) * (1.f + v.jitA);
            y *= std::pow (ex, 1.3f) * (0.35f + 0.65f * fv);
            v.pluckBoost = 0.f;
        }
        else if (v.kind == K_Brass)
        {
            v.ph[0] += dph; if (v.ph[0] >= 1.f) v.ph[0] -= 1.f;
            float src = 2.f * v.ph[0] - 1.f - polyBlep (v.ph[0], dph);
            src = std::tanh (v.pt.p[4] * 0.9f * src) / std::tanh (v.pt.p[4] * 0.9f);
            const float fc = std::min (11000.f, std::max (400.f, f0 * (1.4f + 15.f * lvl * lvl * v.pt.p[0])));
            v.lp[0] += (src - v.lp[0]) * (1.f - std::exp (-kTwoPi * fc / sr));
            const float mute = v.pt.p[2];
            y = (mute > 0.5f ? 0.12f : 0.5f) * v.lp[0] + 0.8f * v.bq[0].process (v.lp[0]) + 0.5f * v.bq[1].process (v.lp[0]);
            if (mute > 0.5f) y += 1.4f * v.bq[2].process (v.lp[0]);
            y += (v.pt.p[5] + 0.10f * std::exp (-v.t / 0.02f)) * v.bq[3].process (rng.next());
            if (v.art == A_Growl) y *= 0.6f + 0.4f * std::sin (kTwoPi * 34.f * v.t);
            if (v.art == A_Flutter) y *= (std::fmod (v.t * 22.f, 1.f) < 0.5f) ? 1.f : 0.3f;
            y = v.bq[4].process (y);
            y = std::tanh (1.7f * y) * (1.f + v.jitA);
            y *= std::pow (ex, 1.3f) * (0.35f + 0.65f * fv) * (mute > 0.5f ? 0.8f : 1.f);
        }
        else
        {
            const float det = v.pt.p[5] * 0.0017f;
            v.ph[0] += dph; if (v.ph[0] >= 1.f) v.ph[0] -= 1.f;
            v.ph[1] += dph * (1.f + det); if (v.ph[1] >= 1.f) v.ph[1] -= 1.f;
            const float saw = 0.6f * (2.f * v.ph[0] - 1.f - polyBlep (v.ph[0], dph)) + 0.4f * (2.f * v.ph[1] - 1.f - polyBlep (v.ph[1], dph * (1.f + det)));
            const bool harm = (v.art == A_Harmonic);
            const float bright = lvl * 1.1f;
            const float fc = harm ? f0 * 1.6f : std::min (8000.f, std::max (400.f, f0 * (3.f + 7.f * bright) * v.pt.p[1]));
            v.lp[0] += (saw - v.lp[0]) * (1.f - std::exp (-kTwoPi * fc / sr));
            y = v.lp[0] * 0.5f + 0.6f * v.bq[0].process (v.lp[0]) + 0.5f * v.bq[1].process (v.lp[0]) + 0.45f * v.bq[2].process (v.lp[0]) + 0.2f * v.bq[3].process (v.lp[0]) + 0.25f * v.bq[4].process (v.lp[0]);
            y += (v.pt.p[2] * 0.7f + 0.14f * std::exp (-v.t / 0.05f)) * v.bq[5].process (rng.next()) * (0.5f + 0.5f * lvl);
            if (v.art == A_Tremolo) y *= (std::fmod (v.t * 13.f, 1.f) < 0.55f) ? 1.f : 0.2f;
            y *= (1.f + v.jitA * 0.8f) * std::pow (ex, 1.3f) * (0.35f + 0.65f * fv);
        }
        oL = oR = y * v.gain * env;
        return ! (v.releasing && v.env < 0.0004f);
    }


    //---------------------------------------------------------------- v2.7 physical models
    static bool isPM (int k) { return k == K_SaxPM || k == K_BrassPM || k == K_BowPM; }
    // fractional delay read (linear) from a circular line written at index w (the last write is at w - 1)
    template <size_t N> static float dRead (const std::array<float, N>& b, int w, float d)
    {
        const float rp = (float) w - std::max (1.f, d);
        const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0; const int m = (int) N - 1;
        return b[(size_t) (i0 & m)] * (1.f - fr) + b[(size_t) ((i0 + 1) & m)] * fr;
    }
    // pitch of a waveguide note: semitones with vibrato, jitter, bend (shared by the three models)
    float pmSemis (Voice& v, float rate)
    {
        const int ch = v.ch;
        v.curSemis += ((float) v.note - v.curSemis) * v.glideCoef;
        v.lfo += rate / sr; if (v.lfo >= 1.f) v.lfo -= 1.f;
        const float onset = std::min (1.f, std::max (0.f, (v.t - v.vibDelay) / 0.4f));
        const float modAmt = std::min (1.f, mod[(size_t) ch] + aft[(size_t) ch] * 0.8f);
        v.jitT -= 1.f; if (v.jitT <= 0.f) { v.jitT = sr / 28.f; v.jit1 = rng.next() * 0.035f * (0.4f + 1.2f * gc.human); v.jitV = rng.next() * 0.04f; }
        v.jit2 += (v.jit1 - v.jit2) * 0.003f; v.jitA += (v.jitV - v.jitA) * 0.004f;
        float semis = v.curSemis + bend[(size_t) ch] + std::sin (kTwoPi * v.lfo) * (modAmt * 0.6f + v.autoVib) * onset + v.jit2 + v.pm[15];
        if (v.art == A_Trill || v.art == A_Shake) { const float r = v.art == A_Shake ? 7.f : 11.f; if (std::fmod (v.t * r, 1.f) > 0.5f && v.t > 0.1f) semis += (v.art == A_Shake ? 2.f : 1.5f); }
        semis -= v.scoop * std::exp (-v.t / 0.03f);
        return semis;
    }

    //  reed + conical bore (after Cook's Saxofony): two delay lines split at the blow position, reed table, one-zero bore loss
    void startSaxPM (Voice& v, float vel)
    {
        std::fill (v.buf.begin(), v.buf.end(), 0.f); std::fill (v.hist.begin(), v.hist.end(), 0.f);
        v.wp = v.hp = 0;
        const Patch& p = v.pt;
        v.pm[0] = 0.f;                                                // one-zero filter memory
        v.pm[1] = 0.55f + 0.30f * vel;                                // breath pressure target
        v.pm[2] = p.p[7] > 0.f ? p.p[7] : 0.2f;                       // blow position
        v.pm[3] = std::max (0.15f, p.p[3]);                           // reed slope
        v.pm[4] = std::max (0.4f, p.p[4]);                            // reed offset (rest opening)
        v.pm[5] = 0.f;                                                // last output
        v.gain = 0.42f;
    }
    //  lip + bore (after Cook's Brass): lip resonance tuned to the note, one bore delay of two periods, dc blocker
    void startBrassPM (Voice& v, float vel)
    {
        std::fill (v.buf.begin(), v.buf.end(), 0.f);
        v.wp = 0;
        v.pm[0] = 0.75f + 0.45f * vel;                                // max blowing pressure
        v.pm[1] = v.pm[2] = 0.f;                                      // dc blocker
        v.pm[3] = v.pm[4] = v.pm[5] = v.pm[6] = 0.f;                  // lip filter state
        v.pm[7] = -1.f;
        v.gain = 0.62f;
    }
    //  bowed string (after Smith / McIntyre-Woodhouse): neck + bridge delays, friction table at the bow, lossy bridge reflection, then the body
    void startBowPM (Voice& v, float vel, int art)
    {
        std::fill (v.buf.begin(), v.buf.end(), 0.f); std::fill (v.hist.begin(), v.hist.end(), 0.f);
        v.wp = v.hp = 0;
        const Patch& p = v.pt;
        v.pm[0] = std::clamp (p.p[6] > 0.f ? p.p[6] : 0.127f, 0.05f, 0.4f);        // bow position
        v.pm[1] = 5.f - 4.f * std::clamp (p.p[5] + (art == A_Marcato ? 0.15f : 0.f), 0.f, 1.f);   // friction slope (less = more pressure)
        v.pm[2] = 0.f;                                                // bridge filter memory
        v.pm[3] = 0.03f + 0.22f * vel;                                // max bow velocity
        v.pm[4] = 2.5f + 4.f * (1.f - vel);                           // bow length in seconds before a bow change
        v.pm[5] = -10.f;                                              // time of the last bow change
        v.gain = 1.7f;
    }

    bool tickPM (Voice& v, float& oL, float& oR, float env, float fv, float ex)
    {
        const float lvl = std::min (1.f, ex * (0.45f + 0.55f * fv));
        float y = 0.f;
        if (v.kind == K_SaxPM)
        {
            const float semis = pmSemis (v, 5.3f);
            const float f0 = f0Of (semis);
            if (f0 > 0.2f * sr || f0 < 20.f) return false;
            const float total = std::max (8.f, sr / f0 * pmSaxCal (f0) - 0.6f);
            const float d0 = total * (1.f - v.pm[2]), d1 = total * v.pm[2];
            // the reed only speaks above a threshold pressure: soft playing = lower pressure inside the speaking range (+ darker tone, lower level below)
            float breath = env * (0.62f + 0.25f * fv * std::pow (std::min (1.f, 260.f / f0), 2.f)) * (0.9f + 0.1f * ex);
            breath += breath * (0.12f + v.pt.p[2] * 0.6f) * rng.next() * 0.25f;
            // embouchure: a little energy at the written pitch keeps the bore in the right register (the player 'voices' the note)
            v.ph[1] += f0 / sr; if (v.ph[1] >= 1.f) v.ph[1] -= 1.f;
            breath += 0.08f * env * std::sin (kTwoPi * v.ph[1]);
            const float o0 = dRead (v.buf, v.wp, d0), o1 = dRead (v.hist, v.hp, d1);
            const float temp = -0.95f * 0.5f * (o0 + v.pm[0]); v.pm[0] = o0;
            const float out = temp - o1;
            const float pd = breath - out;
            float reed = v.pm[4] + v.pm[3] * pd; reed = std::clamp (reed, -1.f, 1.f);
            v.hist[(size_t) (v.hp++ & (kHist - 1))] = temp;
            v.buf[(size_t) (v.wp++ & (kBuf - 1))] = breath - pd * reed - temp;
            // body: tone filter that opens with the dynamic, formants of the horn, breath noise
            const float fc = std::min (9000.f, std::max (500.f, f0 * (3.f + 6.f * lvl) * v.pt.p[1]));
            v.lp[0] += (out - v.lp[0]) * (1.f - std::exp (-kTwoPi * fc / sr));
            const float x = v.lp[0];
            y = x * 0.6f + 0.8f * v.bq[0].process (x) + 0.6f * v.bq[1].process (x) + 0.3f * v.bq[2].process (x);
            y += (v.pt.p[2] * (0.6f + mechMul()) + 0.08f * std::exp (-v.t / 0.04f)) * v.bq[3].process (rng.next()) * (0.4f + 0.6f * ex) * 0.6f;
            if (v.art == A_Growl) { y *= 1.f - 0.3f * (0.5f + 0.5f * std::sin (kTwoPi * 38.f * v.t)); y = std::tanh (y * 1.8f) * 0.8f; }
            if (v.art == A_Flutter) y *= (std::fmod (v.t * 24.f, 1.f) < 0.5f) ? 1.f : 0.25f;
            y *= (1.f + v.jitA) * std::pow (ex, 0.8f) * (0.2f + 0.8f * std::pow (fv, 1.3f)) * 1.35f;   // pp .. ff: about 10 dB between velocity 40 and 120
        }
        else if (v.kind == K_BrassPM)
        {
            // hybrid: band-limited lip pulse (narrower and brighter when played louder) -> waveguide bore -> brassy steepening -> bell + formants
            const float semis = pmSemis (v, 5.3f);
            const float f0 = f0Of (semis);
            if (f0 > 0.2f * sr || f0 < 30.f) return false;
            const float dph = f0 / sr;
            v.ph[0] += dph; if (v.ph[0] >= 1.f) v.ph[0] -= 1.f;
            const float pw = std::clamp (0.48f - 0.30f * lvl * (0.6f + 0.4f * v.pt.p[0]), 0.12f, 0.48f);
            float ph2 = v.ph[0] + pw; if (ph2 >= 1.f) ph2 -= 1.f;
            const float sawA = 2.f * v.ph[0] - 1.f - polyBlep (v.ph[0], dph), sawB = 2.f * ph2 - 1.f - polyBlep (ph2, dph);
            float src = 0.5f * (sawA - sawB);
            src += v.pt.p[5] * rng.next() * 0.15f * (0.5f + lvl);                       // air through the lips
            // bore: one-period waveguide loop with a lossy, level dependent reflection
            const float pole = 0.55f - 0.35f * lvl;
            const float P = std::max (4.f, sr / f0 - pole / (1.f - pole));
            const float back = dRead (v.buf, v.wp, P);
            v.pm[1] = (1.f - pole) * back + pole * v.pm[1];
            const float g = (0.50f + 0.30f * lvl) * env;
            const float bore = src + g * v.pm[1];
            v.buf[(size_t) (v.wp++ & (kBuf - 1))] = bore;
            float x = bore * (1.f - g * 0.6f);
            const float k = 0.9f * lvl * lvl * v.pt.p[0];                             // non-linear steepening in the bore: the "blat" of a loud note
            x = std::tanh (x + k * x * std::fabs (x));
            const float fc = std::min (12000.f, std::max (600.f, f0 * (2.5f + 12.f * lvl * lvl * v.pt.p[0])));
            v.lp[0] += (x - v.lp[0]) * (1.f - std::exp (-kTwoPi * fc / sr));
            const float bell = v.lp[0] - v.pm[2]; v.pm[2] += bell * (1.f - std::exp (-kTwoPi * 220.f / sr));   // bell: the lows do not radiate
            const float mute = v.pt.p[2];
            y = (mute > 0.5f ? 0.15f : 0.6f) * bell + 0.7f * v.bq[0].process (bell) + 0.4f * v.bq[1].process (bell);
            if (mute > 0.5f) y += 1.3f * v.bq[2].process (bell);
            y += (v.pt.p[5] * (0.5f + mechMul()) + 0.08f * std::exp (-v.t / 0.02f)) * v.bq[3].process (rng.next()) * 0.5f;
            if (v.art == A_Growl) y *= 0.6f + 0.4f * std::sin (kTwoPi * 34.f * v.t);
            if (v.art == A_Flutter) y *= (std::fmod (v.t * 22.f, 1.f) < 0.5f) ? 1.f : 0.3f;
            y = v.bq[4].process (y);
            y = std::tanh (1.4f * y) * (1.f + v.jitA);
            y *= std::pow (ex, 0.8f) * (0.2f + 0.8f * std::pow (fv, 1.3f)) * 1.25f * (mute > 0.5f ? 0.85f : 1.f) * env;
        }
        else   // K_BowPM
        {
            const float semis = pmSemis (v, 5.8f);
            const float f0 = f0Of (semis);
            if (f0 > 0.12f * sr || f0 < 30.f) return false;
            const float pole = std::clamp (0.75f - 0.2f * 22050.f / sr + 0.1f * (1.f - v.pt.p[1]), 0.3f, 0.9f);
            const float wv = kTwoPi * f0 / sr;
            const float pDelay = std::atan2 (pole * std::sin (wv), 1.f - pole * std::cos (wv)) / wv;     // the bridge filter's own delay at this pitch
            const float base = std::max (8.f, sr / f0 - pDelay + 0.25f);
            const float beta = std::min (0.45f, v.pm[0] * (1.f + 0.f * std::max (0.f, f0 - 500.f) / 1000.f));   // higher up the string the bow sits relatively further from the bridge
            const float neckOut = dRead (v.buf, v.wp, base * (1.f - beta)), bridgeOut = dRead (v.hist, v.hp, base * beta);
            // lossy bridge reflection (one-pole), rigid nut
            v.pm[2] = 0.95f * (1.f - pole) * bridgeOut + pole * v.pm[2];
            const float bridgeRefl = -v.pm[2], nutRefl = -neckOut;
            const float stringVel = bridgeRefl + nutRefl;
            // bow: velocity follows the envelope and the dynamic; a bow change dips through zero
            if (v.held && v.t - std::max (0.f, v.pm[5]) > v.pm[4] && v.t > 0.5f) { v.pm[5] = v.t; mechEvent (0.012f * mechMul(), 4200.f, 1.2f, 0.03f, 0.5f); }
            const float sinceChange = v.t - v.pm[5];
            const float dip = sinceChange < 0.08f ? 1.f - 0.85f * std::exp (-std::pow ((sinceChange - 0.03f) / 0.02f, 2.f)) : 1.f;
            const float hiAdj = std::pow (std::min (1.f, 500.f / f0), 1.5f);          // short strings need a slower bow (or they break into a raucous sub-harmonic)
            const float bowVel = v.pm[3] * hiAdj * env * (0.5f + 0.5f * lvl) * dip * (v.releasing ? env : 1.f);
            const float vd = bowVel - stringVel;
            float bt = std::pow (std::fabs (vd * v.pm[1]) + 0.75f, -4.f); bt = std::clamp (bt, 0.01f, 1.f);
            const float nv = vd * bt;
            v.buf[(size_t) (v.wp++ & (kBuf - 1))] = bridgeRefl + nv;
            v.hist[(size_t) (v.hp++ & (kHist - 1))] = nutRefl + nv;
            const float x = bridgeOut;
            v.lp[0] += (x - v.lp[0]) * (1.f - std::exp (-kTwoPi * std::min (12000.f, 2500.f + 7000.f * v.pt.p[1] * lvl) / sr));
            const float s = v.lp[0];
            y = s * 0.5f + 0.6f * v.bq[0].process (s) + 0.5f * v.bq[1].process (s) + 0.45f * v.bq[2].process (s) + 0.2f * v.bq[3].process (s) + 0.25f * v.bq[4].process (s);
            y += (v.pt.p[2] * (0.4f + mechMul()) * 0.7f) * v.bq[5].process (rng.next()) * (bowVel * 6.f);
            if (v.art == A_Tremolo) y *= (std::fmod (v.t * 13.f, 1.f) < 0.55f) ? 1.f : 0.2f;
            y *= (1.f + v.jitA * 0.6f) * (0.5f + 0.5f * std::pow (ex, 0.8f));
            if (v.releasing) y *= std::min (1.f, v.env * 3.f);
        }
        oL = oR = y * v.gain;
        return ! (v.releasing && v.env < 0.0004f);
    }
    // tuning corrections of the waveguides (measured: the loops' filters add delay that depends on the pitch)
    static float pmSaxCal (float)   { return 1.f; }
    static float pmLipCal (float)   { return 1.f; }
    static float pmBoreCal (float)  { return 1.f; }
    static float pmBowCal (float)   { return 1.f; }

    //---------------------------------------------------------------- v2.7 mechanical noises (keys, fingers, dampers, bow changes): short filtered noise bursts
    struct MechEv { float a = 0, dec = 0, pan = 0.5f; Biquad bq; };
    std::array<MechEv, 8> mech {};
    float mechMul() const { return gc.mech * 2.f * (0.4f + 0.6f * gc.realism); }      // 0.5 = the natural amount
    void mechEvent (float amp, float freq, float q, float decayS, float pan)
    {
        if (amp <= 1e-5f) return;
        MechEv* e = &mech[0];
        for (auto& m : mech) if (m.a < e->a) e = &m;
        e->a = amp; e->dec = std::exp (-1.f / (std::max (0.002f, decayS) * sr)); e->pan = pan;
        e->bq.reset(); e->bq.bandpass (std::min (0.45f * sr, freq), q, sr);
    }
    void mechRender (float& L, float& R)
    {
        for (auto& m : mech)
        {
            if (m.a < 1e-6f) continue;
            const float n = m.bq.process (rng.next()) * m.a; m.a *= m.dec;
            L += n * std::cos (m.pan * kPi * 0.5f) * 1.4f; R += n * std::sin (m.pan * kPi * 0.5f) * 1.4f;
        }
    }

    void compact (Voice& v)
    {
        if (v.kind != K_Piano && v.kind != K_Rhodes && v.kind != K_Wurli) return;
        int w = 0;
        for (int i = 0; i < v.nOsc; ++i) if (v.osc[i].a > 1.0e-5f) { if (w != i) v.osc[w] = v.osc[i]; ++w; }
        v.nOsc = w;
        if (v.nOsc == 0 && (v.releasing || v.kind != K_Piano)) v.active = false;
        if (v.nOsc == 0 && v.kind == K_Piano) v.active = false;
    }


    //---------------------------------------------------------------- v2.7 global controls, phrasing engine, tone
    GlobalCtl gc; bool toneInit = false; float exprExp = 1.f;
    struct Pend { float t60Mul = 1.f, dampAdd = 0.f; bool bowUp = false, newPhrase = false; };
    Pend pend;
    struct Phrase { bool first = true; double start = 0, lastOn = -10, lastOff = -10; float avgLen = 2.5f; int n = 0; bool bowUp = false; int pos = 2, lastString = -1; };
    std::array<Phrase, 16> phr {};
    std::array<int, 16> lastMono = [] { std::array<int, 16> a; a.fill (-1); return a; }();
    uint32_t noteCounter = 0; double clock = 0.0;
    float glideScale() const { return 0.4f * std::pow (100.f, gc.glideTime) / 1.0047f; }    // 1 at the default (0.2), 0.4 .. 40 over the range
    float hashSigned (uint32_t x) const                                                      // seeded: the same take sounds the same every time
    {
        x ^= gc.seed * 0x9e3779b9u; x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
        return (float) (x & 0xffffff) / 8388607.5f - 1.f;
    }
    int shapeNote (int ch, int note, int vel, int art, bool legatoArt)
    {
        pend = Pend();
        exprExp = std::max (0.05f, 2.f * gc.exprResp);
        auto& ph = phr[(size_t) ch];
        bool overlap = false;
        for (auto& v : voices) if (v.active && v.ch == ch && v.held && ! v.releasing) { overlap = true; break; }
        const double gap = overlap ? 0.0 : clock - ph.lastOff;
        const bool newPhrase = ph.first || (! overlap && gap > 0.30);
        if (newPhrase)
        {
            if (! ph.first) ph.avgLen = 0.7f * ph.avgLen + 0.3f * (float) std::clamp (ph.lastOff - ph.start, 0.8, 12.0);
            ph.start = clock; ph.n = 0; ph.first = false; pend.newPhrase = true;
        }
        float fv = (float) vel / 127.f;
        fv = std::clamp (0.62f + (fv - 0.62f) * (0.3f + 1.4f * gc.dynamics), 0.04f, 1.f);
        const int k = cur.kind;
        const bool wind = (k == K_Sax || k == K_Brass || k == K_SaxPM || k == K_BrassPM);
        const bool bowed = (k == K_Bow || k == K_BowPM);
        if (gc.humanPlay && ! isLogKind (k))
        {
            const float tp = (float) (clock - ph.start), L = std::max (1.f, ph.avgLen);
            const float arc = std::sin (kPi * std::min (1.f, tp / L));
            float mul = 1.f + 0.10f * (arc - 0.35f);                         // a phrase rises to its middle and settles at its end
            if (ph.n == 0) mul += 0.04f;                                     // the opening note speaks
            if (tp > 1.4f * L) mul -= std::min (0.08f, 0.02f * (tp - 1.4f * L));   // a very long phrase runs out of air / bow
            if (! newPhrase && clock - ph.lastOn < 0.11) mul -= 0.04f;       // fast runs are a little lighter
            if (bowed && art != A_Pizz && art != A_ColLegno)
            {
                if (! legatoArt || newPhrase) { ph.bowUp = newPhrase ? false : ! ph.bowUp; if (! newPhrase) mechEvent (0.005f * mechMul(), 3800.f, 1.f, 0.025f, 0.5f); }
                if (ph.bowUp) mul -= 0.03f;                                  // up-bow is lighter than down-bow
                pend.bowUp = ph.bowUp;
            }
            if (wind && newPhrase) mechEvent (0.016f * mechMul(), 1300.f, 0.7f, 0.06f, 0.5f);   // the breath that starts a phrase
            if (k == K_Pluck && (instIdx == kInstGuitar || instIdx == kInstBass)) fretboard (ph, note, art);
            fv *= mul;
        }
        fv *= 1.f + gc.human * 0.10f * hashSigned (noteCounter);
        ph.n++; ph.lastOn = clock;
        return std::clamp ((int) std::lround (fv * 127.f), 1, 127);
    }
    // string and fret choice like a player's hand: stay in a position, prefer short moves, open strings when they fit
    void fretboard (Phrase& ph, int note, int art)
    {
        static const int gtr[] = { 40, 45, 50, 55, 59, 64 }, bas[] = { 28, 33, 38, 43 };
        const int* open = instIdx == kInstBass ? bas : gtr; const int nS = instIdx == kInstBass ? 4 : 6;
        int best = -1, bestF = 0; float bestC = 1e9f;
        for (int st = 0; st < nS; ++st)
        {
            const int f = note - open[st];
            if (f < 0 || f > 22) continue;
            float c = (f < ph.pos - 1 ? (float) (ph.pos - 1 - f) : f > ph.pos + 4 ? (float) (f - ph.pos - 4) : 0.f);
            c += (f == 0 ? -0.3f : 0.f) + 0.15f * (ph.lastString < 0 ? 0.f : (float) std::abs (st - ph.lastString)) + 0.02f * (float) f;
            if (c < bestC) { bestC = c; best = st; bestF = f; }
        }
        if (best < 0) return;
        int dist = 0;
        if (bestF > 0 && (bestF < ph.pos - 1 || bestF > ph.pos + 4)) { const int np = std::clamp (bestF - 1, 0, 19); dist = std::abs (np - ph.pos); ph.pos = np; }
        ph.lastString = best;
        const float thick = (float) (nS - 1 - best) / (float) (nS - 1);
        pend.dampAdd = 0.10f * thick * std::min (1.f, (float) bestF / 12.f);
        pend.t60Mul = bestF == 0 ? 1.15f : 1.f - 0.25f * std::min (1.f, (float) bestF / 15.f) * (0.5f + 0.5f * thick);
        const bool mute = (art == A_PalmMute || art == A_Mute);
        if (dist >= 3 && clock - ph.lastOn < 0.4 && ! mute) mechEvent (0.009f * mechMul(), 2400.f + 600.f * (1.f - thick), 2.f, 0.06f, 0.5f);   // hand shift: the string squeaks
    }

    // global tone: brightness tilt + microphone distance (proximity bass close, air loss and early reflections far)
    Biquad toneLo[2], toneHi[2]; bool toneOn = false; float erMix = 0.f;
    std::vector<float> erBuf; int erW = 0;
    void setTone()
    {
        toneInit = true;
        const float close = std::max (0.f, (0.35f - gc.mic) / 0.35f), far = std::max (0.f, (gc.mic - 0.35f) / 0.65f);
        const float lo = -2.f * gc.bright + 2.5f * close - 2.f * far, hi = 6.f * gc.bright + 1.5f * close - 5.f * far;
        toneOn = std::fabs (lo) > 0.05f || std::fabs (hi) > 0.05f;
        for (int c = 0; c < 2; ++c) { toneLo[c].lowShelf (180.f, lo, sr); toneHi[c].highShelf (2600.f, hi, sr); }
        erMix = 0.3f * far;
        if (erBuf.empty()) erBuf.assign ((size_t) (sr * 0.04f) + 4, 0.f);
    }
    void toneReset() { for (int c = 0; c < 2; ++c) { toneLo[c].reset(); toneHi[c].reset(); } std::fill (erBuf.begin(), erBuf.end(), 0.f); }
    void toneProcess (float& L, float& R)
    {
        if (toneOn) { L = toneHi[0].process (toneLo[0].process (L)); R = toneHi[1].process (toneLo[1].process (R)); }
        if (erMix > 0.001f && ! erBuf.empty())
        {
            const int n = (int) erBuf.size();
            erBuf[(size_t) erW] = 0.5f * (L + R);
            auto tap = [&] (float ms) { int r = erW - (int) (ms * 0.001f * sr); while (r < 0) r += n; return erBuf[(size_t) r]; };
            const float a = tap (7.3f), b = tap (11.9f), c = tap (17.1f), d = tap (23.7f);
            L += erMix * (0.6f * a + 0.35f * c); R += erMix * (0.6f * b + 0.35f * d);
            erW = (erW + 1) % n;
        }
    }
    // piano: with the pedal down the undamped strings and the soundboard bloom (realism)
    std::array<std::vector<float>, 4> resBuf; std::array<int, 4> resW {}; std::array<float, 4> resLp {};
    void resReset() { for (auto& b : resBuf) std::fill (b.begin(), b.end(), 0.f); resLp.fill (0.f); }
    void resProcess (float& L, float& R)
    {
        bool anyPedal = false; for (bool b : pedal) anyPedal |= b;
        static const int len[4] = { 1116, 1188, 1277, 1356 };
        if (resBuf[0].empty()) for (int i = 0; i < 4; ++i) resBuf[(size_t) i].assign ((size_t) std::max (8, (int) ((float) len[i] * sr / 44100.f)), 0.f);
        const float in = 0.5f * (L + R) * (anyPedal ? 0.12f : 0.025f) * gc.realism;
        float o0 = 0, o1 = 0;
        for (int i = 0; i < 4; ++i)
        {
            auto& b = resBuf[(size_t) i]; const int n = (int) b.size(); int& w = resW[(size_t) i];
            const float y = b[(size_t) w];
            resLp[(size_t) i] = y * 0.6f + resLp[(size_t) i] * 0.4f;
            b[(size_t) w] = in + resLp[(size_t) i] * (anyPedal ? 0.82f : 0.6f);
            w = (w + 1) % n;
            (i & 1 ? o1 : o0) += y;
        }
        L += o0 * 0.5f; R += o1 * 0.5f;
    }


    //---------------------------------------------------------------- v2.7 batch 3: modulation matrix + creative effects
    ModCtl mmc; bool mmOn = false; float lfo1Ph = 0.f, lfo2Ph = 0.f;
    float modSource (const Voice& v, int src) const
    {
        switch (src)
        {
            case MS_Lfo1: return std::sin (kTwoPi * lfo1Ph);
            case MS_Lfo2: return std::sin (kTwoPi * lfo2Ph);
            case MS_Velocity: return (float) v.vel / 127.f;
            case MS_ModWheel: return mod[(size_t) v.ch];
            case MS_Aftertouch: return aft[(size_t) v.ch];
            case MS_Expression: return expr[(size_t) v.ch];
            case MS_Key: return std::clamp (((float) v.note - 60.f) / 24.f, -1.f, 1.f);
            case MS_Envelope: return std::exp (-v.t / 0.4f);
            case MS_Random: return v.mmRand;
            default: return 0.f;
        }
    }
    std::vector<float> fxL, fxR; int fxW = 0; float fxPh = 0.f, fxPh2 = 0.f, fxHoldL = 0.f, fxHoldR = 0.f, fxCnt = 0.f; Biquad fxBq[4];
    int fxSet = -1;
    void fxProcess (float& L, float& R)
    {
        const float a = std::clamp (mmc.fxAmt, 0.f, 1.f);
        if (fxL.empty()) { fxL.assign ((size_t) (sr * 0.06f) + 8, 0.f); fxR = fxL; }
        if (fxSet != mmc.fx)
        {
            fxSet = mmc.fx; for (auto& b : fxBq) b.reset();
            if (mmc.fx == 2) { fxBq[0].lowpass (5200.f, 0.7f, sr); fxBq[1].lowpass (5200.f, 0.7f, sr); }
            if (mmc.fx == 4) { fxBq[0].bandpass (1100.f, 0.9f, sr); fxBq[1].bandpass (1100.f, 0.9f, sr); fxBq[2].highpass (320.f, 0.7f, sr); fxBq[3].highpass (320.f, 0.7f, sr); }
        }
        const int n = (int) fxL.size();
        fxL[(size_t) fxW] = L; fxR[(size_t) fxW] = R;
        auto rd = [&] (const std::vector<float>& b, float dSamp) { float rp = (float) fxW - dSamp; while (rp < 0.f) rp += (float) n; const int i0 = (int) rp; const float fr = rp - (float) i0; return b[(size_t) (i0 % n)] * (1.f - fr) + b[(size_t) ((i0 + 1) % n)] * fr; };
        switch (mmc.fx)
        {
            case 1:   // tape wobble: slow wow + fast flutter on a short delay (pitch drifts like an old tape)
            {
                fxPh += 0.55f / sr; if (fxPh >= 1.f) fxPh -= 1.f; fxPh2 += 6.5f / sr; if (fxPh2 >= 1.f) fxPh2 -= 1.f;
                const float dly = sr * (0.006f + a * (0.004f * std::sin (kTwoPi * fxPh) + 0.0004f * std::sin (kTwoPi * fxPh2)));
                L = rd (fxL, dly); R = rd (fxR, dly * 1.01f);
                break;
            }
            case 2:   // lo-fi: lower sample rate and bit depth, dull top
            {
                const float hold = 1.f + a * 10.f;
                if ((fxCnt += 1.f) >= hold) { fxCnt -= hold; const float q = std::exp2 (16.f - a * 10.f); fxHoldL = std::round (L * q) / q; fxHoldR = std::round (R * q) / q; }
                L = fxBq[0].process (fxHoldL); R = fxBq[1].process (fxHoldR);
                break;
            }
            case 3:   // wide: Haas delay on one side + more side signal
            {
                const float dR = rd (fxR, sr * (0.004f + 0.014f * a));
                const float m = 0.5f * (L + dR), sd = 0.5f * (L - dR) * (1.f + a), g = 1.f / (1.f + 0.35f * a);
                L = (m + sd) * g; R = (m - sd) * g;
                break;
            }
            case 4:   // telephone / radio: band-limited and a little crunchy
            {
                const float wl = std::tanh (2.f * fxBq[2].process (fxBq[0].process (L))), wr = std::tanh (2.f * fxBq[3].process (fxBq[1].process (R)));
                L = L * (1.f - a) + wl * a * 1.0f; R = R * (1.f - a) + wr * a * 1.0f;
                break;
            }
            case 5:   // warm saturation
            {
                const float dr = 1.f + 6.f * a;
                const float mk = (1.f + 0.6f * a) / dr; L = std::tanh (L * dr) * mk; R = std::tanh (R * dr) * mk;
                break;
            }
            default: break;
        }
        fxW = (fxW + 1) % n;
    }

    //---------------------------------------------------------------- global EQ
    Biquad eq[2][3]; bool eqOn[3] = { false, false, false };
    void eqReset() { for (auto& c : eq) for (auto& b : c) b.reset(); }
    void applyEq()
    {
        for (int c = 0; c < 2; ++c)
        {
            eqOn[0] = std::fabs (cur.eqLow) > 0.01f; eqOn[1] = std::fabs (cur.eqMid) > 0.01f; eqOn[2] = std::fabs (cur.eqHigh) > 0.01f;
            if (eqOn[0]) eq[c][0].lowShelf (150.f, cur.eqLow, sr);
            if (eqOn[1]) eq[c][1].peak (2500.f, 0.8f, cur.eqMid, sr);
            if (eqOn[2]) eq[c][2].highShelf (8000.f, cur.eqHigh, sr);
        }
    }
    void eqProcess (float& L, float& R)
    {
        for (int i = 0; i < 3; ++i) if (eqOn[i]) { L = eq[0][i].process (L); R = eq[1][i].process (R); }
    }

    float sr = 44100.f;
    int instIdx = kInstPiano, soundIdx = 0; bool saxAlto = false;
    Patch cur;
    std::vector<Voice> voices;
    std::array<float, 16> bend {}, mod {}, aft {}, expr {}, exprT {};
    std::array<bool, 16> pedal {};
    Chorus chorus;
    std::array<float, kBuf> scratch {};
    LogCtl logc;
    float monoHzSet = -1.f;
    Biquad monoLpL, monoLpR;
    std::array<int, 16> lastLeadNote = [] { std::array<int, 16> a; a.fill (-1); return a; }();
    // ping-pong echo for the lead synths
    std::vector<float> echoL, echoR; int echoW = 0;
    void echoProcess (float& L, float& R)
    {
        if (echoL.empty()) return;
        const int n = (int) echoL.size();
        const int d = std::min (n - 1, std::max (1, (int) (cur.p[23] * sr)));
        int r = echoW - d; if (r < 0) r += n;
        const float fb = std::min (0.8f, cur.p[26]), mix = cur.p[22];
        const float el = echoL[(size_t) r], er = echoR[(size_t) r];
        echoL[(size_t) echoW] = 0.5f * (L + R) * 0.7f + er * fb;      // left feeds right: ping-pong
        echoR[(size_t) echoW] = el * fb;
        echoW = (echoW + 1) % n;
        L += el * mix; R += er * mix;
    }
    Noise rng;
    uint32_t orderCounter = 0, blockCount = 0;
};
}
}
