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

enum Kind { K_Piano = 0, K_Rhodes, K_Wurli, K_FMEP, K_Organ, K_Poly, K_Pluck, K_Sax, K_Brass, K_Bow, K_Sub, K_Log, K_Lead, K_Classic };
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

        // ---------------- violin family
        grp = "Strings";
        a[kInstViolin] = withLeads ({
            P ("Violin", K_Bow, { 1.f, 1.f, .05f, 1.f, .07f, 1.f }),
            P ("Viola", K_Bow, { .82f, .85f, .06f, 1.1f, .07f, 1.f }),
            P ("Fiddle (bright, edgy)", K_Bow, { 1.f, 1.35f, .09f, .7f, .05f, 1.3f }),
            P ("Cello", K_Bow, { .55f, .8f, .07f, 1.4f, .08f, 1.f }),
        });

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
            P ("808 long (slides)", K_Sub, { 2.8f, 1.0f, 2.2f, 0.f }, 0, 0, 0, 0, 1.0f),
            P ("Log drum (amapiano)", K_Log, { .55f, .9f, 1.58f }, 0, 0, 0, 0, 1.1f),
            P ("Log drum deep (long)", K_Log, { .9f, .6f, 1.5f }, 0, 0, 0, 0, 1.1f),
            P ("Log drum short (knock)", K_Log, { .3f, 1.3f, 1.62f }, 0, 0, 0, 0, 1.2f) }) bs.push_back (pt);
        bs.push_back (classic());
        a[kInstBass] = bs;
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
    const Patch& patch() const { return cur; }

    void reset()
    {
        for (auto& v : voices) v.active = false;
        std::fill (echoL.begin(), echoL.end(), 0.f); std::fill (echoR.begin(), echoR.end(), 0.f);
        lastLeadNote.fill (-1);
        bend.fill (0.f); mod.fill (0.f); aft.fill (0.f); expr.fill (100.f / 127.f); exprT.fill (100.f / 127.f); pedal.fill (false);
        orderCounter = 0;
        eqReset();
    }
    void setBend (int ch, float semis) { bend[(size_t) clampCh (ch)] = semis; }
    void setMod (int ch, float v) { mod[(size_t) clampCh (ch)] = v; }
    void setAftertouch (int ch, float v) { aft[(size_t) clampCh (ch)] = v; }
    void setExpression (int ch, float v) { exprT[(size_t) clampCh (ch)] = v; }
    void setPedal (int ch, bool down)
    {
        ch = clampCh (ch);
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
        const bool lead = (cur.kind == K_Lead);
        const bool mono = lead || (cur.kind == K_Sax || cur.kind == K_Brass || cur.kind == K_Bow || cur.kind == K_Sub || (cur.kind == K_Poly && instIdx == kInstBass));
        const bool legatoArt = (art == A_Legato || art == A_HammerOn || art == A_PullOff || art == A_Slide || art == A_Gliss);
        const bool plucked = (cur.kind == K_Pluck);
        const bool pluckLeg = plucked && (art == A_HammerOn || art == A_PullOff || art == A_Slide);
        if (mono || pluckLeg)
            for (auto& v : voices)
                if (v.active && v.ch == ch && v.held && ! v.releasing && v.kind == cur.kind && (lead || (mono ? legatoArt : true)) && (lead || std::abs (v.note - note) <= 7))
                {
                    v.note = note; v.vel = vel;                       // same voice, new pitch: no new attack
                    const float gt = lead ? std::max (0.004f, cur.p[19] * (art == A_Slide || art == A_Gliss ? 2.5f : 1.f))
                                          : (art == A_Slide || art == A_Gliss ? 0.075f : (plucked ? 0.006f : 0.014f));
                    v.glideCoef = 1.f - std::exp (-1.f / (gt * sr));
                    if (plucked && (art == A_HammerOn || art == A_PullOff)) v.pluckBoost = art == A_HammerOn ? 0.55f : 0.f;
                    return;
                }
        if (mono) for (auto& v : voices) if (v.active && v.ch == ch && v.held && v.kind == cur.kind) { v.held = false; release (v, 0.03f); }
        Voice* slot = nullptr;
        for (auto& v : voices) if (! v.active) { slot = &v; break; }
        if (! slot) { long long best = -1; for (auto& v : voices) { const long long score = v.age + (v.releasing ? 1000000LL : 0); if (score > best) { best = score; slot = &v; } } }
        start (*slot, ch, note, vel, art);
        if (lead)
        {
            // portamento: long-glide sounds slide in from the last note even when the notes do not overlap
            const int last = lastLeadNote[(size_t) ch];
            if (cur.p[19] >= 0.08f && last >= 0 && last != note)
            { slot->curSemis = (float) last; slot->glideCoef = 1.f - std::exp (-1.f / (cur.p[19] * sr)); }
            lastLeadNote[(size_t) ch] = note;
        }
    }
    void noteOff (int ch, int note)
    {
        ch = clampCh (ch);
        for (auto& v : voices)
            if (v.active && v.ch == ch && v.note == note && v.held)
            {
                v.held = false;
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
            for (auto& v : voices)
            {
                if (! v.active) continue;
                float sl = 0, sr_ = 0;
                if (! tick (v, sl, sr_)) { v.active = false; continue; }
                oL += sl; oR += sr_;
            }
            oL *= gain * cur.level; oR *= gain * cur.level;
            chorus.process (oL, oR);
            if (cur.kind == K_Lead && cur.p[22] > 0.f) echoProcess (oL, oR);
            eqProcess (oL, oR);
            L[i] += oL; if (R) R[i] += oR;
        }
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
                case K_Sub: rel = 0.25f; break; case K_Log: rel = 0.12f; break; default: rel = 0.1f;
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
        if (cur.kind == K_Bow && (art == A_Pizz || art == A_ColLegno)) { v.pt = pizzPatch (art == A_ColLegno); v.kind = K_Pluck; }
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
            case K_Sub: startSub (v, fv); break;
            case K_Log: startLog (v, fv); break;
            default: break;
        }
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
        const float semis = v.curSemis + bend[(size_t) ch] + std::sin (kTwoPi * v.lfo) * (v.autoVib * onset + modAmt * 0.5f) + v.drift * 0.05f;
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
        const float ex = expr[(size_t) ch];
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
        const float semis = v.curSemis + bend[(size_t) ch] + std::sin (kTwoPi * v.lfo) * (modAmt * 0.5f + v.autoVib) * onset;
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
        float semis = v.curSemis + bend[(size_t) ch] + std::sin (kTwoPi * v.lfo) * (modAmt * 0.6f + v.autoVib * (v.kind == K_Bow ? 1.f : 1.f)) * onset + v.jit2;
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

    void compact (Voice& v)
    {
        if (v.kind != K_Piano && v.kind != K_Rhodes && v.kind != K_Wurli) return;
        int w = 0;
        for (int i = 0; i < v.nOsc; ++i) if (v.osc[i].a > 1.0e-5f) { if (w != i) v.osc[w] = v.osc[i]; ++w; }
        v.nOsc = w;
        if (v.nOsc == 0 && (v.releasing || v.kind != K_Piano)) v.active = false;
        if (v.nOsc == 0 && v.kind == K_Piano) v.active = false;
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
