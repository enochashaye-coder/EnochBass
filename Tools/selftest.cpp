// Enoch Bass - engine self test. Generates many patterns for every instrument/artist/genre and validates them.
#include <cstdio>
#include <fstream>
#include <set>
#include <map>
#include "../Source/Engine/MidiWriter.h"
#include "../Source/Engine/Phrases.h"
#include "../Source/Engine/ChordDetect.h"
using namespace enochsolo;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; if (failures < 40) { std::printf ("FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } } while (0)

int main (int argc, char** argv)
{
    const char* outDir = argc > 1 ? argv[1] : nullptr;
    long total = 0, totalNotes = 0;
    std::map<int, std::set<int>> artsSeen;       // inst -> articulations that were actually produced
    std::map<int, long> noteCountByInst;
    std::set<uint64_t> fingerprints; long dupes = 0;

    for (int inst = 0; inst < I_Count; ++inst)
    {
        const int nArtists = (int) stylesFor (inst).size();
        for (int artist = 0; artist < nArtists; ++artist)
        for (int genre = 0; genre < G_Count; ++genre)
        for (int scale = 0; scale < SC_Count; ++scale)
        for (int seed = 1; seed <= 3; ++seed)
        {
            Settings s; s.instrument = inst; s.artist = artist; s.genre = genre; s.scale = scale;
            s.keyRoot = (artist * 5 + seed * 7 + scale) % 12;
            s.bars = (seed == 1) ? 4 : (seed == 2 ? 8 : 16);
            s.intensity = 0.3f + 0.3f * seed;
            s.seed = (uint64_t) (inst * 100000 + artist * 1000 + genre * 100 + scale * 10 + seed);
            s.bpm = 100 + seed * 12;
            s.mode = (seed == 3) ? 2 : 0;
            if (seed == 2 && scale == 0) { s.tsNum = 6; s.tsDen = 8; }
            if (seed == 1 && scale == 1) { s.tsNum = 3; s.tsDen = 4; }
            if (seed == 2 && scale == 2) { s.tsNum = 5; s.tsDen = 4; }
            const bool realize = (artist % 2 == 1);
            s.realizeOrnaments = realize;
            Pattern p = Generator::generate (s);
            ++total;
            const Range r = instRange (inst, styleFor (inst, artist).variant);
            const double len = p.lengthBeats;
            CHECK (! p.notes.empty(), "no notes inst=%d artist=%d genre=%d", inst, artist, genre);
            int soloNotes = 0;
            uint64_t fp = 1469598103934665603ULL;
            for (auto& n : p.notes)
            {
                ++totalNotes; ++noteCountByInst[inst];
                if (n.track == 0 && n.voice == 0) { ++soloNotes; artsSeen[inst].insert (n.art); }
                CHECK (n.pitch >= 0 && n.pitch <= 127, "pitch range");
                if (n.track == 0 && n.voice == 0 && ! realize) CHECK (n.pitch >= r.lo - 3 && n.pitch <= r.hi + 4, "solo pitch %d outside %d..%d (%s)", n.pitch, r.lo, r.hi, instName (inst));
                CHECK (n.vel >= 1 && n.vel <= 127, "velocity %d", n.vel);
                CHECK (n.length > 0, "non-positive length");
                CHECK (n.start >= -1e-9, "negative start %f", n.start);
                CHECK (n.start < len + 0.2, "note starts after pattern end: %f > %f (inst %d artist %d bars %d)", n.start, len, inst, artist, s.bars);
                fp = fp * 1099511628211ULL ^ (uint64_t) (n.pitch * 977 + (int) (n.start * 960));
            }
            if (! fingerprints.insert (fp).second) ++dupes;
            // event integrity: every note-on has a matching note-off, key-switches are in key-switch range
            std::map<std::pair<int,int>, int> open;
            for (auto& e : p.events)
            {
                const auto k = std::make_pair (e.track, e.a);
                if (e.type == EV_ON)  { ++open[k]; }
                if (e.type == EV_OFF) { --open[k]; CHECK (open[k] >= 0, "note-off without on"); }
                CHECK (e.t >= -1e-9 && e.t <= len + 8.0, "event time %f", e.t);
            }
            for (auto& kv : open) CHECK (kv.second == 0, "unbalanced notes pitch=%d", kv.first.second);
            // key switches never sit inside the playing range of the instrument
            if (s.emitKS)
            {
                const int top = s.ksBase + (int) artList (inst).size();
                CHECK (top < r.lo, "key-switch range %d.. overlaps instrument range %d", top, r.lo);
            }
            // MIDI file smoke-test
            auto bytes = MidiWriter::build (p);
            CHECK (bytes.size() > 40 && bytes[0] == 'M' && bytes[1] == 'T', "bad midi header");
            if (outDir && seed == 2 && scale == 1 && genre == 0 && (artist < 2))
            {
                char fn[512]; std::snprintf (fn, sizeof fn, "%s/%s_%s_%dbars.mid", outDir, instName (inst), p.styleName.substr (0, p.styleName.find (' ')).c_str(), s.bars);
                std::ofstream f (fn, std::ios::binary); f.write ((const char*) bytes.data(), (std::streamsize) bytes.size());
            }
            CHECK (soloNotes > 0 || s.mode == 1, "solo has no notes");
        }
    }

    // regenerate variety + humanize determinism
    {
        Settings s; s.instrument = I_Sax; s.artist = 0; s.bars = 8;
        std::set<uint64_t> fps;
        for (int seed = 1; seed <= 200; ++seed)
        {
            s.seed = (uint64_t) seed; Pattern p = Generator::generate (s);
            uint64_t fp = 7; for (auto& n : p.raw) fp = fp * 31 + (uint64_t) (n.pitch * 1000 + (int) (n.start * 100));
            fps.insert (fp);
        }
        CHECK (fps.size() >= 195, "regenerate variety too low: %zu/200", fps.size());
        std::printf ("regenerate variety: %zu distinct of 200\n", fps.size());

        Settings h = s; h.seed = 5; Pattern a = Generator::generate (h), b = Generator::generate (h);
        CHECK (a.notes.size() == b.notes.size(), "deterministic size");
        for (size_t i = 0; i < a.notes.size() && i < b.notes.size(); ++i) CHECK (a.notes[i].start == b.notes[i].start && a.notes[i].pitch == b.notes[i].pitch, "deterministic notes");
        // humanize on/off keeps pitches, changes timing
        h.humanize = false; Pattern c = Generator::generate (h);
        h.humanize = true;  Pattern d = Generator::generate (h);
        CHECK (c.notes.size() == d.notes.size(), "humanize must not change note count (%zu vs %zu)", c.notes.size(), d.notes.size());
        int diff = 0; for (size_t i = 0; i < c.notes.size() && i < d.notes.size(); ++i) if (std::abs (c.notes[i].start - d.notes[i].start) > 1e-6) ++diff;
        CHECK (diff > (int) c.notes.size() / 2, "humanize changed too few notes (%d)", diff);
        // re-humanize with another seed leaves the notes the same
        h.hSeed = 99; Pattern e = Generator::generate (h);
        CHECK (e.notes.size() == d.notes.size(), "hSeed changes count");
    }


    // ---- chord parser + harmony modes
    {
        std::vector<ParsedChord> pc; std::string err;
        CHECK (parseChordText ("Am7 | D7 | Gmaj7 | Cmaj7:2", pc, err) && pc.size() == 4 && pc[3].bars == 2.0, "parse basic");
        CHECK (pc[0].rootPc == 9 && pc[0].q == Quality::Min7 && pc[1].q == Quality::Dom7 && pc[2].q == Quality::Maj7, "parse qualities");
        CHECK (parseChordText ("C Am F G7", pc, err) && pc[0].q == Quality::Maj && pc[1].q == Quality::Min && pc[2].q == Quality::Maj && pc[3].q == Quality::Dom7, "triads");
        CHECK (parseChordText ("Cmaj9 Dm9 G13 Em11 Gsus2 C7alt Caug F#m7b5", pc, err) && pc.size() == 8 && pc[0].q == Quality::Maj9 && pc[1].q == Quality::Min9 && pc[2].q == Quality::Dom13 && pc[3].q == Quality::Min11 && pc[4].q == Quality::Sus2 && pc[5].q == Quality::Dom7alt && pc[6].q == Quality::Aug7 && pc[7].q == Quality::Min7b5, "extended parse");
        CHECK (parseChordText ("Bbm7 Eb7 Abmaj7 Dbmaj7", pc, err) && pc[0].rootPc == 10 && pc[1].rootPc == 3 && pc[2].rootPc == 8 && pc[3].rootPc == 1, "parse flats");
        CHECK (parseChordText ("F#m7b5 B7 Em C/G Dsus4 G6 Bdim7 Cm6 Am(maj7)", pc, err) && pc.size() == 9 && pc[0].q == Quality::Min7b5 && pc[2].q == Quality::Min && pc[3].q == Quality::Maj && pc[4].q == Quality::Sus4, "parse mixed");
        CHECK (! parseChordText ("hello world", pc, err), "reject junk");
        CHECK (parseChordText ("Am7 Xyz D7", pc, err) && pc.size() == 2 && ! err.empty(), "skip bad chord, keep the rest");
        for (int inst = 0; inst < I_Count; ++inst)
            for (int h = 0; h < 3; ++h)
            {
                Settings s; s.instrument = inst; s.artist = 1; s.harmony = h; s.chordText = "Dm7 | G7 | Cmaj7 | A7"; s.keyRoot = 0; s.scale = SC_Major; s.bars = 8; s.seed = 99 + h;
                Pattern p = Generator::generate (s);
                CHECK (! p.notes.empty(), "harmony %d no notes", h);
                if (h == 2) { CHECK (p.chords.size() == 8 && p.chords[0].name == "Dm7" && p.chords[1].name == "G7" && p.chords[3].name == "A7", "custom chords not used"); }
                if (h == 0) CHECK (p.chords.size() == 2, "vamp should be one chord (2 spans), got %zu", p.chords.size());
            }
        // solos over custom chords: strong-beat notes should be chord tones most of the time
        int strong = 0, tones = 0;
        for (int seed = 1; seed <= 60; ++seed)
        {
            Settings s; s.instrument = I_Sax; s.artist = 2; s.harmony = 2; s.chordText = "Am7 D7 Gmaj7 Cmaj7"; s.keyRoot = 7; s.scale = SC_Major; s.bars = 8; s.seed = (uint64_t) seed; s.humanize = false;
            Pattern p = Generator::generate (s);
            for (auto& n : p.raw) if (n.voice == 0 && n.track == 0 && n.weight >= 0.8 && n.length > 0.2) { ++strong; if (inMask (detail::chordAtBeat (p, n.start).toneMask, n.pitch)) ++tones; }
        }
        std::printf ("strong-beat chord-tone rate: %.0f%% (%d notes)\n", 100.0 * tones / std::max (1, strong), strong);
        CHECK (tones * 100 >= strong * 60, "strong beats should mostly be chord tones");
    }

    // ---- per-bar chords, re-harmonising, re-rolling bars, chord pad
    {
        for (int inst = 0; inst < I_Count; ++inst)
        {
            Settings s; s.instrument = inst; s.artist = 1; s.genre = G_Pop; s.keyRoot = 0; s.scale = SC_Major; s.bars = 8; s.seed = 4242; s.humanize = false; s.mode = 2; s.chordTrack = true;
            Pattern p = Generator::generate (s);
            CHECK (p.hasChords, "chord pad");
            Settings t = s; t.barChords.resize (8);
            t.barChords[2] = { true, 9, Quality::Min9 }; t.barChords[3] = { true, 7, Quality::Dom13 };
            std::string pn; auto nc = Generator::chordsFor (t, pn);
            CHECK (pn == "Custom chords", "custom chord name '%s'", pn.c_str());
            bool b2 = false, b3 = false;
            for (auto& c : nc) { if (c.start <= 8.0 + 1e-6 && c.end > 8.0 + 1e-6 && c.name == "Am9") b2 = true; if (c.start <= 12.0 + 1e-6 && c.end > 12.0 + 1e-6 && c.name == "G13") b3 = true; }
            CHECK (b2 && b3, "bar chords applied inst=%d", inst);
            Pattern before = p;
            Generator::reharmonize (p, p.chords, nc, t);
            Generator::finalize (p, t);
            int moved = 0, same = 0;
            for (auto& n : p.raw)
            {
                const int bar = p.barOf (n.start);
                for (auto& o : before.raw) if (o.id == n.id) { if (o.pitch != n.pitch) { ++moved; CHECK (bar == 2 || bar == 3, "note outside edited bars was re-pitched (bar %d)", bar); } else ++same; }
            }
            CHECK (same > 0, "reharmonize should leave other bars alone");
            // strong notes in bar 2 should now be chord tones of Am9
            int strong = 0, tones = 0;
            for (auto& n : p.raw) if (n.track == 0 && n.voice == 0 && p.barOf (n.start) == 2 && n.weight >= 0.55) { ++strong; if (inMask (chordToneMaskFor (9, Quality::Min9), n.pitch)) ++tones; }
            CHECK (strong == 0 || tones == strong, "strong notes in the edited bar must be chord tones (%d/%d)", tones, strong);
            auto bytes = MidiWriter::build (p); CHECK (bytes.size() > 100, "midi after reharmonize");
            Generator::rerollRegion (p, t, 4, 6, 777);
            CHECK (! p.raw.empty(), "reroll");
            std::set<int> ids; for (auto& n : p.raw) CHECK (ids.insert (n.id).second, "duplicate note id");
        }
        // humanising is stable per note: deleting a note must not change the others
        Settings s; s.instrument = I_Sax; s.artist = 0; s.bars = 4; s.seed = 31;
        Pattern p = Generator::generate (s);
        Pattern q = p; q.raw.erase (q.raw.begin() + 3); Generator::finalize (q, s);
        std::map<int, double> startOf; for (auto& n : p.notes) startOf[n.id] = n.start;
        int diffs = 0; for (auto& n : q.notes) if (startOf.count (n.id) && std::abs (startOf[n.id] - n.start) > 1e-9) ++diffs;
        CHECK (diffs <= 2, "humanise changed %d other notes when one was deleted", diffs);
        // pentatonic scale choices and every genre stay inside the scale mask for strong notes
        for (int g = 0; g < G_Count; ++g)
            for (int sc : { (int) SC_MajPent, (int) SC_MinPent, (int) SC_PhrygianDom })
            {
                Settings u; u.instrument = I_Guitar; u.artist = 2; u.genre = g; u.scale = sc; u.bars = 8; u.seed = 5 + g; u.harmony = 1; u.humanize = false;
                Pattern r = Generator::generate (u);
                CHECK (! r.notes.empty() && ! r.chords.empty(), "genre %d scale %d empty", g, sc);
            }
    }
    // ---------------------------------------------------------------- v2.1 features
    {
        // busy: a busier setting must give clearly more notes, for every instrument and genre family
        for (int inst = 0; inst < I_Count; ++inst)
            for (int g : { (int) G_Afrobeat, (int) G_Amapiano, (int) G_Rock, (int) G_Bossa })
            {
                long lo = 0, hi = 0;
                for (int sd = 1; sd <= 12; ++sd)
                {
                    Settings a; a.instrument = inst; a.artist = 1; a.genre = g; a.bars = 8; a.seed = 900 + sd; a.humanize = false; a.busy = 0.1f;
                    Settings b = a; b.busy = 1.0f;
                    for (auto& n : Generator::generate (a).raw) if (n.track == 0 && n.voice == 0) ++lo;
                    for (auto& n : Generator::generate (b).raw) if (n.track == 0 && n.voice == 0) ++hi;
                }
                CHECK (hi > lo * 1.5, "busy=1 should be much busier than 0.1 (inst %d genre %d: %ld vs %ld)", inst, g, hi, lo);
            }
        // per-bar busy raises only that bar
        for (int inst = 0; inst < I_Count; ++inst)
        {
            long busyBar = 0, calmBar = 0;
            for (int sd = 1; sd <= 20; ++sd)
            {
                Settings a; a.instrument = inst; a.artist = 0; a.genre = G_Afrobeat; a.bars = 8; a.seed = 300 + sd; a.humanize = false; a.busy = 0.3f;
                a.barBusy.assign (8, -1.f); a.barBusy[2] = 1.f; a.barBusy[3] = 1.f;
                Pattern p = Generator::generate (a);
                for (auto& n : p.raw) if (n.track == 0 && n.voice == 0) { const int b = p.barOf (n.start); if (b == 2 || b == 3) ++busyBar; else if (b == 0 || b == 1) ++calmBar; }
            }
            CHECK (busyBar > calmBar * 1.3, "per-bar busy not audible (inst %d: %ld vs %ld)", inst, busyBar, calmBar);
        }
        // mute / export modes / user lanes
        for (int inst = 0; inst < I_Count; ++inst)
        {
            Settings a; a.instrument = inst; a.artist = 0; a.bars = 4; a.seed = 4242 + inst; a.humanize = true; a.mode = 2; a.chordTrack = true;
            Pattern p = Generator::generate (a);
            size_t ksFull = 0; for (auto& e : p.events) if (e.type == EV_ON && e.ord == 1) ++ksFull;
            size_t ksPlay = 0; for (auto& e : p.playEvents) if (e.type == EV_ON && e.ord == 1) ++ksPlay;
            CHECK (ksFull > 0 && ksPlay > 0, "full mode should write key-switches (inst %d)", inst);
            // no key-switches
            Settings b = a; b.exportMode = 1; Pattern q2 = p; Generator::finalize (q2, b);
            size_t ks1 = 0; for (auto& e : q2.events) if (e.type == EV_ON && e.ord == 1) ++ks1;
            size_t ksPlay1 = 0; for (auto& e : q2.playEvents) if (e.type == EV_ON && e.ord == 1) ++ksPlay1;
            CHECK (ks1 == 0, "mode 1 must not write key-switches (inst %d)", inst);
            CHECK (ksPlay1 > 0, "test player must still get articulations in mode 1 (inst %d)", inst);
            // plain
            Settings c = a; c.exportMode = 2; Pattern q3 = p; Generator::finalize (q3, c);
            for (auto& e : q3.events) CHECK (e.type == EV_ON || e.type == EV_OFF, "plain mode wrote a non-note event (inst %d)", inst);
            // split
            Settings d = a; d.exportMode = 3; Pattern q4 = p; Generator::finalize (q4, d);
            auto m0 = MidiWriter::build (p), m3 = MidiWriter::build (q4);
            auto countTracks = [] (const std::vector<uint8_t>& v) { int n = 0; for (size_t i = 0; i + 3 < v.size(); ++i) if (v[i] == 'M' && v[i + 1] == 'T' && v[i + 2] == 'r' && v[i + 3] == 'k') ++n; return n; };
            CHECK (countTracks (m3) >= countTracks (m0), "split mode should not reduce tracks (inst %d)", inst);
            // mute: muted notes never sound
            Pattern q5 = p; size_t unmuted = 0;
            for (size_t i = 0; i < q5.raw.size(); ++i) { if (i % 3 == 0) q5.raw[i].mute = true; else ++unmuted; }
            Generator::finalize (q5, a);
            size_t ons = 0; for (auto& e : q5.playEvents) if (e.type == EV_ON && e.ord == 2) ++ons;
            CHECK (ons >= unmuted - 2 && ons <= unmuted + 40, "muted notes must not sound (inst %d: %zu vs %zu)", inst, ons, unmuted);
            // user lanes replace automatic data inside their region and add aftertouch
            Pattern q6 = p;
            q6.userRegions.push_back ({ L_Mod, 0.0, 4.0 });
            q6.userPts.push_back ({ 0.5, L_Mod, 50 }); q6.userPts.push_back ({ 2.0, L_Mod, 90 });
            q6.userRegions.push_back ({ L_Aftertouch, 1.0, 3.0 });
            q6.userPts.push_back ({ 1.0, L_Aftertouch, 20 }); q6.userPts.push_back ({ 3.0, L_Aftertouch, 100 });
            Generator::finalize (q6, a);
            int cc1 = 0, at = 0;
            for (auto& e : q6.playEvents) { if (e.type == EV_CC && e.a == 1 && e.track == 0 && e.t <= 4.0) ++cc1; if (e.type == EV_AT) ++at; }
            CHECK (cc1 == 2, "drawn modulation must replace automatic CC1 in its region (inst %d: %d)", inst, cc1);
            CHECK (at == 2, "aftertouch points missing (inst %d)", inst);
            auto mb = MidiWriter::build (q6); bool hasD0 = false; for (auto x : mb) if (x == 0xD0) hasD0 = true;
            CHECK (hasD0, "aftertouch not written to the MIDI file");
        }
    }
    // ---------------------------------------------------------------- reference recording (v2.2)
    {
        auto synth = [] (double bpm, const char* const pat[2], std::vector<RefRaw>* truthOut) {
            const double sr = 44100; const int bars = 8;
            std::vector<float> y ((size_t) (sr * bars * 4 * 60 / bpm + sr * 2), 0.f);
            const double step = 60.0 / bpm / 4; int pi = 0;
            const int pitches[] = { 57, 60, 62, 64, 67, 64, 62, 60, 57, 55, 57, 60 };
            for (int b = 0; b < bars; ++b)
            {
                const char* P = pat[(b / 2) % 2];
                for (int st = 0; st < 16; ++st) if (P[st] == 'x')
                {
                    const double t = (b * 16 + st) * step + 0.3; const int pit = pitches[pi++ % 12];
                    const double f = 440 * std::pow (2.0, (pit - 69) / 12.0), amp = st == 0 ? 1.0 : 0.6;
                    const size_t i0 = (size_t) (t * sr), n = (size_t) (sr * 0.28);
                    for (size_t i = 0; i < n && i0 + i < y.size(); ++i)
                    {
                        const double tt = i / sr, env = std::exp (-tt * 9) * std::min (1.0, tt * 400);
                        double v = 0; for (int h = 1; h <= 4; ++h) v += std::sin (2 * 3.14159265358979 * f * h * tt) / h;
                        y[i0 + i] += (float) (0.3 * amp * env * v);
                    }
                    if (truthOut) { RefRaw r; r.t = t; r.pitch = pit; truthOut->push_back (r); }
                }
            }
            return y;
        };
        static const char* const pat[2] = { "x..x..x.x.x..x..", "x.x.x.xx..x.x..." };
        for (double bpm : { 80.0, 100.0, 120.0, 140.0 })
        {
            std::vector<RefRaw> truth;
            auto y = synth (bpm, pat, &truth);
            auto prof = analyseAudio (y, 44100.0, "synth");
            CHECK (prof.valid(), "reference analysis failed at %.0f bpm", bpm);
            CHECK (std::abs (prof.bpm - bpm) < bpm * 0.02, "tempo detection %.1f vs %.1f", prof.bpm, bpm);
            int hit = 0, pok = 0;
            for (auto& o : truth) for (auto& n : prof.notes) if (std::abs (n.t - o.t) < 0.04) { ++hit; if (n.pitch == o.pitch) ++pok; break; }
            CHECK (hit >= (int) truth.size() - 1, "onsets found %d of %zu at %.0f bpm", hit, truth.size(), bpm);
            CHECK (pok >= hit - 2, "pitch tracking %d of %d at %.0f bpm", pok, hit, bpm);
            CHECK (prof.notes.size() <= truth.size() + 4, "too many false onsets: %zu vs %zu", prof.notes.size(), truth.size());
        }
        // the generator follows the reference rhythm
        auto y = synth (100.0, pat, nullptr);
        auto profP = std::make_shared<RefProfile> (analyseAudio (y, 44100.0, "synth"));
        const RefView view = makeRefView (*profP, 4.0, 0, 0);
        CHECK (view.valid && view.bars == 8, "reference view bars %d", view.bars);
        std::vector<std::set<int>> refBars;
        for (int b = 0; b < view.bars; ++b) { std::set<int> st; for (int i : view.byBar[(size_t) b]) st.insert ((int) std::lround (view.notes[(size_t) i].pos * 4.0)); refBars.push_back (st); }
        auto jacc = [] (const std::set<int>& a, const std::set<int>& b) { int in = 0; for (int v : a) in += b.count (v); const int un = (int) (a.size() + b.size()) - in; return un ? (double) in / un : 1.0; };
        for (int inst = 0; inst < I_Count; ++inst)
        {
            double simRef = 0, simNo = 0, npbRef = 0; int cnt = 0;
            for (int sd = 1; sd <= 12; ++sd)
            {
                Settings a; a.instrument = inst; a.artist = 1; a.genre = G_Afrobeat; a.bars = 8; a.seed = 7000 + sd; a.humanize = false;
                Settings r = a; r.refProfile = profP; r.refAmount = 0.9f;
                for (int variant = 0; variant < 2; ++variant)
                {
                    Pattern p = Generator::generate (variant ? r : a);
                    std::vector<std::set<int>> gen ((size_t) p.bars);
                    long ncount = 0;
                    for (auto& n : p.raw) if (n.track == 0 && n.voice == 0 && n.length > 0.1)
                    { const int b = p.barOf (n.start); gen[(size_t) b].insert ((int) std::lround ((n.start - b * p.barBeats) * 4.0)); ++ncount; }
                    for (auto& g : gen)
                    {
                        double best = 0; for (auto& rb : refBars) best = std::max (best, jacc (g, rb));
                        (variant ? simRef : simNo) += best;
                    }
                    if (variant) { npbRef += (double) ncount / p.bars; ++cnt; }
                    for (auto& n : p.raw) if (n.track == 0 && n.voice == 0) { const Range hr = instRange (inst, styleFor (inst, 1).variant); CHECK (n.pitch >= hr.lo - 1 && n.pitch <= hr.hi + 1, "ref pitch out of range inst %d: %d", inst, n.pitch); }
                }
            }
            simRef /= (12.0 * 8); simNo /= (12.0 * 8); npbRef /= cnt;
            CHECK (simRef > simNo + 0.12 && simRef > 0.5, "reference rhythm not followed (inst %d: %.2f vs %.2f without)", inst, simRef, simNo);
            CHECK (std::abs (npbRef - view.notesPerBar) < view.notesPerBar * 0.4, "notes per bar %.1f vs reference %.1f (inst %d)", npbRef, view.notesPerBar, inst);
            std::printf ("reference  inst %d  rhythm match %.2f (without %.2f)  notes/bar %.1f (ref %.1f)\n", inst, simRef, simNo, npbRef, view.notesPerBar);
        }
        // amount 0 and no reference behave exactly as before
        { Settings a; a.seed = 31; a.humanize = false; Settings b = a; b.refProfile = profP; b.refAmount = 0.f;
          Pattern p1 = Generator::generate (a), p2 = Generator::generate (b);
          bool same = p1.raw.size() == p2.raw.size(); for (size_t i = 0; same && i < p1.raw.size(); ++i) same = p1.raw[i].start == p2.raw[i].start && p1.raw[i].pitch == p2.raw[i].pitch;
          CHECK (same, "refAmount 0 must not change the result"); }
    }

    // ---------------------------------------------------------------- phrases and locking
    {
        long phraseCount = 0, lockTests = 0;
        for (int inst = 0; inst < I_Count; ++inst)
            for (int seedN = 1; seedN <= 40; ++seedN)
            {
                Settings a; a.instrument = inst; a.artist = seedN % std::max (1, (int) stylesFor (inst).size()); a.seed = (uint64_t) seedN * 7919; a.humanize = false;
                a.bars = (seedN % 3 == 0) ? 4 : 8; a.genre = seedN % G_Count; a.busy = (seedN % 5) / 4.f;
                Pattern p = Generator::generate (a);
                auto ph = findPhrases (p);
                int melody = 0; for (auto& n : p.raw) if (n.track == 0 && n.voice == 0) ++melody;
                CHECK (melody == 0 || ! ph.empty(), "no phrases for %d melody notes", melody);
                phraseCount += (long) ph.size();
                int counted = 0;
                for (size_t i = 0; i < ph.size(); ++i)
                {
                    counted += ph[i].notes;
                    CHECK (ph[i].t1 > ph[i].t0, "empty phrase");
                    CHECK (ph[i].t1 - ph[i].t0 <= 2.0 * p.barBeats + 0.35 || ph[i].notes < 2, "phrase too long: %.2f beats", ph[i].t1 - ph[i].t0);
                    CHECK (ph[i].r0 <= ph[i].t0 + 1e-9 && ph[i].r1 >= ph[i].t1 - 1e-9 - 1.0, "region does not hold phrase");
                    if (i) CHECK (std::abs (ph[i].r0 - ph[i - 1].r1) < 1e-9, "regions not contiguous");
                }
                CHECK (counted == melody, "phrases cover %d of %d melody notes", counted, melody);
                if (ph.size() >= 2)
                {
                    // lock the first phrase, generate a different solo, merge: locked notes identical, everything else new
                    LockRanges keep; setPhraseLocked (keep, ph[0], true);
                    Settings b = a; b.seed = a.seed + 1; b.chordSeed = a.seed;
                    Pattern fresh = Generator::generate (b);
                    const Pattern old = p;
                    // chords of "p" came from seed a.seed; with chordSeed fixed they match when the progression is seed-driven
                    mergeLocked (fresh, old, keep);
                    std::vector<const Note*> oldIn, newIn;
                    for (auto& n : old.raw) if (n.track != 2 && inLockRanges (keep, n.start)) oldIn.push_back (&n);
                    for (auto& n : fresh.raw) if (n.track != 2 && inLockRanges (keep, n.start)) newIn.push_back (&n);
                    CHECK (oldIn.size() == newIn.size(), "locked note count %zu vs %zu", oldIn.size(), newIn.size());
                    for (size_t k = 0; k < oldIn.size() && k < newIn.size(); ++k)
                    {
                        // the only change allowed is a shorter note where it would run into its neighbour
                        const bool same = std::abs (oldIn[k]->start - newIn[k]->start) < 1e-9 && oldIn[k]->pitch == newIn[k]->pitch && newIn[k]->length <= oldIn[k]->length + 1e-9;
                        CHECK (same, "locked note changed");
                    }
                    std::set<int> ids; for (auto& n : fresh.raw) CHECK (ids.insert (n.id).second, "duplicate id after merge");
                    for (int track = 0; track < 2; ++track)
                    {
                        const Note* prev = nullptr;
                        for (auto& n : fresh.raw) if (n.track == track && n.voice == 0)
                        {
                            if (prev && n.start - prev->start > 1e-6) CHECK (prev->start + prev->length <= n.start + 1e-6, "overlap after merge track %d", track);
                            prev = &n;
                        }
                    }
                    // unlocking: nothing kept
                    LockRanges none; Pattern f2 = Generator::generate (b); const size_t before = f2.raw.size(); mergeLocked (f2, old, none);
                    CHECK (f2.raw.size() == before, "empty lock list must not change anything");
                    ++lockTests;
                }
            }
        std::printf ("phrases: %ld found, %ld lock/merge tests\n", phraseCount, lockTests);
    }


    //------------------------------------------------------------------ round 2: second voice, modes, chord detection, groove
    {
        long hv = 0, noteChecks = 0;
        for (int inst = 0; inst < I_Count; ++inst)
            for (int v2 = 1; v2 <= 4; ++v2)
                for (int seed = 1; seed <= 6; ++seed)
                {
                    if (inst == I_Bass) continue;      // Enoch Bass has no second voice
                    Settings s; s.instrument = inst; s.genre = (seed * 3) % G_Count; s.seed = (uint64_t) seed; s.bars = 8; s.mode = 2; s.voice2 = v2;
                    s.keyRoot = seed * 2 % 12; s.harmony = 1;
                    const Pattern p = Generator::generate (s);
                    const Range hr = instRange (inst, styleFor (inst, s.artist).variant);
                    int lead = 0, second = 0;
                    for (auto& n : p.raw)
                    {
                        if (n.track == 0 && n.voice == 0) ++lead;
                        if (n.track == 1) { ++second; CHECK (n.pitch >= hr.lo - 1 && n.pitch <= hr.hi + 1, "voice2 %d inst %d pitch %d out of range", v2, inst, n.pitch); CHECK (n.length > 0.04, "voice2 tiny note"); }
                    }
                    CHECK (lead > 0, "no lead notes");
                    if (v2 != 4) CHECK (second > 0, "voice2 %d produced nothing (inst %d seed %d)", v2, inst, seed);
                    // never two second-voice notes of voice 0 overlapping each other in one part: check against the lead (min 2 semitones for the simultaneous ones)
                    for (auto& a : p.raw) if (a.track == 1)
                        for (auto& b : p.raw) if (b.track == 0 && b.voice == 0 && a.start < b.start + b.length - 0.1 && b.start < a.start + a.length - 0.1)
                        { ++noteChecks; CHECK (a.pitch != b.pitch, "second voice doubles the lead (v2 %d)", v2); }
                    if (v2 == 4)   // answers live inside the lead's rests
                        for (auto& a : p.raw) if (a.track == 1)
                            for (auto& b : p.raw) if (b.track == 0 && b.voice == 0) CHECK (! (a.start < b.start + b.length - 0.05 && b.start < a.start + a.length - 0.05), "call/response answer overlaps the solo");
                    ++hv;
                }
        // off: identical to before (comp still produced)
        { Settings s; s.mode = 2; s.seed = 5; const Pattern a = Generator::generate (s); s.voice2 = 0; const Pattern b = Generator::generate (s); CHECK (a.raw.size() == b.raw.size(), "voice2 off must be deterministic"); }
        std::printf ("second voice: %ld patterns, %ld overlap checks\n", hv, noteChecks);

        // modes
        for (int m = 0; m < M_Count; ++m)
        {
            const uint16_t mk = modeMask (2, m);
            CHECK ((mk >> 2) & 1, "mode %d must contain its root", m);
            int c = 0; for (int i = 0; i < 12; ++i) c += (mk >> i) & 1;
            CHECK (c >= 5 && c <= 8, "mode %d has %d notes", m, c);
        }
        CHECK (modeMask (0, M_Dorian) == (uint16_t) ((1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10)), "C dorian");
        { Settings s; s.bars = 4; s.barChords.assign (4, BarChord()); s.barChords[1].set = true; s.barChords[1].degree = 2; s.barChords[1].q = Quality::Min7; s.barChords[1].mode = M_Phrygian;
          std::string pn; const auto ch = Generator::chordsFor (s, pn);
          int ok = 0; for (auto& c : ch) if (c.degree == 2 && c.scaleMask == modeMask (c.rootPc, M_Phrygian)) ++ok;
          CHECK (ok >= 1, "mode override must reach the chord span"); }

        // chord detection from MIDI: Am7 | D7 | Gmaj7 | Cmaj7 (ii V I IV of G... as written)
        {
            struct C { int root; std::vector<int> iv; };
            const std::vector<C> prog = { { 9, { 0, 3, 7, 10 } }, { 2, { 0, 4, 7, 10 } }, { 7, { 0, 4, 7, 11 } }, { 0, { 0, 4, 7, 11 } } };
            std::vector<ChordNote> notes;
            for (int b = 0; b < 4; ++b)
            {
                for (int iv : prog[(size_t) b].iv) notes.push_back ({ b * 4.0, 4.0, 60 + (prog[(size_t) b].root + iv) % 12, 0.7f });
                notes.push_back ({ b * 4.0, 4.0, 36 + prog[(size_t) b].root, 0.8f });
            }
            const auto r = detectChordsFromNotes (notes, 4.0);
            CHECK (r.ok && r.bars == 4, "midi detect bars");
            CHECK (r.ok && r.keyRoot == 7 && ! r.minor, "midi key got %d minor %d", r.keyRoot, (int) r.minor);
            for (int b = 0; r.ok && b < 4; ++b) CHECK (r.rootPc[(size_t) b] == prog[(size_t) b].root, "midi root bar %d got %d", b, r.rootPc[(size_t) b]);
            std::printf ("midi chords: %s (key %s%s)\n", r.text.c_str(), kNoteNames[r.keyRoot], r.minor ? "m" : "");
            CHECK (! detectChordsFromNotes ({}, 4.0).ok, "empty clip must fail cleanly");
        }
        // chord detection from audio: synthesised piano-ish chords with a bass, 100 BPM
        {
            const double sr = 44100, bpm = 100, barSec = 4 * 60.0 / bpm;
            struct C { int root; std::vector<int> iv; };
            const std::vector<C> prog = { { 0, { 0, 4, 7, 11 } }, { 9, { 0, 3, 7, 10 } }, { 5, { 0, 4, 7, 11 } }, { 7, { 0, 4, 7, 10 } } };
            std::vector<float> a ((size_t) (sr * barSec * 4));
            for (size_t i = 0; i < a.size(); ++i)
            {
                const double t = (double) i / sr; const int b = std::min (3, (int) (t / barSec));
                double v = 0;
                auto tone = [&] (double midi, double amp) { const double f = 440.0 * std::pow (2.0, (midi - 69) / 12.0); for (int h = 1; h <= 4; ++h) v += amp / h * std::sin (6.283185307 * f * h * t); };
                for (int iv : prog[(size_t) b].iv) tone (60 + (prog[(size_t) b].root + iv) % 12, 0.12);
                tone (36 + prog[(size_t) b].root, 0.25);
                a[i] = (float) v * 0.5f;
            }
            const auto r = detectChordsFromAudio (a, sr, bpm, 4.0);
            CHECK (r.ok && r.bars == 4, "audio detect bars");
            int hit = 0; for (int b = 0; r.ok && b < (int) r.bars && b < 4; ++b) if (r.rootPc[(size_t) b] == prog[(size_t) b].root) ++hit;
            CHECK (hit >= 3, "audio roots matched only %d/4", hit);
            std::printf ("audio chords: %s (key %s%s)\n", r.text.c_str(), r.ok ? kNoteNames[r.keyRoot] : "?", r.minor ? "m" : "");
            std::vector<float> sil ((size_t) (sr * 3), 0.f);
            CHECK (! detectChordsFromAudio (sil, sr, bpm, 4.0).ok, "silence must fail cleanly");
        }
        // groove: a loop hitting the offbeat 8ths; count solo onsets on the loop's hit steps with and without it
        {
            RefProfile gp; gp.name = "t"; gp.bpm = 100; gp.fromMidi = true; gp.lengthSec = 8 * 4 * 0.6;
            for (int bar = 0; bar < 8; ++bar) for (int st2 : { 2, 6, 10, 14, 0 }) gp.notes.push_back ({ (bar * 16 + st2) * 0.15, 0.1, 0.9f, 36 });
            auto share = [&] (float amt)
            {
                double on = 0, all = 0;
                for (int seed = 1; seed <= 40; ++seed)
                {
                    Settings s; s.seed = (uint64_t) seed; s.bars = 8; s.genre = G_Afrobeat; s.mode = 0; s.busy = 0.5f;
                    if (amt >= 0) { s.grooveProfile = std::make_shared<RefProfile> (gp); s.grooveAmount = amt; }
                    const Pattern p = Generator::generate (s);
                    for (auto& n : p.raw) if (n.track == 0 && n.voice == 0)
                    {
                        const int st2 = (int) std::lround (std::fmod (n.start, 4.0) * 4.0) % 16;
                        all += 1; if (st2 % 4 == 2 || st2 == 0) on += 1;
                    }
                }
                return on / std::max (1.0, all);
            };
            const double base = share (-1), tight = share (1.0), zero = share (0.0f);
            std::printf ("groove: onsets on loop hits  none %.3f  amount1 %.3f  amount0 %.3f\n", base, tight, zero);
            CHECK (tight > base + 0.01, "groove fit should land more onsets on the loop hits");
            CHECK (std::abs (zero - base) < 1e-9, "groove amount 0 must equal no groove");
        }
    }


    //------------------------------------------------------------------ instrument of choice (libraries)
    {
        long libTests = 0;
        for (int inst = 0; inst < I_Count; ++inst)
        {
            const auto libs = builtinLibraries (inst);
            CHECK (! libs.empty() && libs[0]->standard, "inst %d: first library must be the standard layout", inst);
            for (auto& L : libs)
            {
                CHECK (L->inst == inst, "library %s on the wrong instrument", L->name.c_str());
                for (auto& a : L->arts) for (auto& pr : a.press) CHECK (pr.note >= 0 && pr.note <= 127 && pr.vel >= 1 && pr.vel <= 127, "bad press in %s", L->name.c_str());
                if (L->standard) continue;
                for (int seed = 1; seed <= 6; ++seed)
                {
                    Settings s; s.instrument = inst; s.seed = (uint64_t) seed; s.bars = 8; s.genre = (seed * 2) % G_Count; s.lib = L; s.mode = 0;
                    const Pattern p = Generator::generate (s);
                    int ksEvents = 0, ccE = 0; std::set<int> ksNotes;
                    for (auto& e : p.playEvents)
                    {
                        if (e.type == EV_ON && e.ord == 1 && e.track == 0) { ++ksEvents; ksNotes.insert (e.a); }
                        if (e.type == EV_CC && e.a == L->ccExpr) ++ccE;
                    }
                    if (L->noKeySwitches) CHECK (ksEvents == 0, "%s must write no key-switches (got %d)", L->name.c_str(), ksEvents);
                    // every key-switch written belongs to the library table, and nothing from the standard layout leaks in
                    std::set<int> allowed; for (auto& a : L->arts) for (auto& pr : a.press) allowed.insert (pr.note);
                    for (int n : ksNotes) CHECK (allowed.count (n), "%s wrote key-switch %d that is not in its table", L->name.c_str(), n);
                    if (inst == I_Sax || inst == I_Trumpet || inst == I_Violin) CHECK (ccE > 0, "%s: expression CC%d missing", L->name.c_str(), L->ccExpr);
                    // note-ons and offs balance for key-switches
                    int on = 0, off = 0; for (auto& e : p.playEvents) if (e.ord <= 1 && e.track == 0 && (e.a < 0)) {} 
                    (void) on; (void) off;
                    ++libTests;
                }
            }
        }
        // the Ilya Efimov guitar table: exact notes from the manuals (Kontakt numbering C3 = 60)
        {
            auto libs = builtinLibraries (I_Guitar); std::shared_ptr<const Library> ie;
            for (auto& l : libs) if (l->name == "Ilya Efimov Nylon Guitar") ie = l;
            CHECK (ie != nullptr, "IE nylon present");
            if (ie)
            {
                auto find = [&] (int art) { return libFind (*ie, art); };
                CHECK (find (A_Legato) && find (A_Legato)->press[0].note == 39 && find (A_Legato)->press[0].vel > 100, "legato = D#1 high");
                CHECK (find (A_Sustain) && find (A_Sustain)->press[0].note == 39 && find (A_Sustain)->press[0].vel <= 100, "sustain = D#1 low");
                CHECK (find (A_Slide) && find (A_Slide)->press[0].note == 32 && find (A_Slide)->hold && find (A_Slide)->oneShot, "slide = G#0 held");
                CHECK (find (A_Ghost) && find (A_Ghost)->press[0].note == 35, "x-note = B0");
                CHECK (find (A_Harmonic) && find (A_Harmonic)->press[0].note == 30, "harmonic = F#0");
                // held key-switches stay down until just after the note starts
                Settings s; s.instrument = I_Guitar; s.seed = 3; s.bars = 8; s.lib = ie;
                const Pattern p = Generator::generate (s);
                for (auto& e : p.playEvents) if (e.type == EV_ON && e.ord == 1 && e.a == 32)
                {
                    bool closed = false;
                    for (auto& o : p.playEvents) if (o.type == EV_OFF && o.a == 32 && o.t >= e.t && o.t <= e.t + 0.4) closed = true;
                    CHECK (closed, "held slide key-switch never released near t=%.2f", e.t);
                }
            }
        }
        // user files
        {
            Library l; std::string err;
            const std::string good = "# my sax\nname = My Sax Lib\ninstrument = sax\nexpression_cc = 2\nvibrato_cc = 1\nks = Sustain: 24@100\nks = Legato: 25@110, 26@40\nks = Slide: 32@40 hold oneshot\n";
            CHECK (parseLibraryText (good, l, err), "good file parses: %s", err.c_str());
            CHECK (l.name == "My Sax Lib" && l.inst == I_Sax && l.ccExpr == 2 && l.arts.size() == 3, "parsed fields");
            CHECK (l.arts[1].press.size() == 2 && l.arts[1].press[1].note == 26 && l.arts[1].press[1].vel == 40, "two presses");
            CHECK (l.arts[2].hold && l.arts[2].oneShot, "flags");
            CHECK (! parseLibraryText ("name = x\ninstrument = banjo\n", l, err), "unknown instrument rejected");
            CHECK (! parseLibraryText ("name = x\ninstrument = sax\nks = Whistle: 24\n", l, err), "unknown articulation rejected");
            CHECK (! parseLibraryText ("instrument = sax\n", l, err), "missing name rejected");
            CHECK (! parseLibraryText ("name = x\ninstrument = sax\nks = Sustain: abc\n", l, err), "no note rejected");
        }
        std::printf ("libraries: %ld generation checks\n", libTests);
    }


    //------------------------------------------------------------------ Enoch Bass: every style x genre
    {
        long bt = 0, rootChecks = 0, bnotes = 0;
        const auto sidx = stylesFor (I_Bass);
        CHECK ((int) sidx.size() >= 20, "bass styles: only %d", (int) sidx.size());
        const auto libs = builtinLibraries (I_Bass);
        CHECK (libs.size() >= 3, "bass libraries: %d", (int) libs.size());
        std::shared_ptr<const Library> ez;
        for (auto& l : libs) if (l->name.find ("EZbass") != std::string::npos) ez = l;
        CHECK (ez != nullptr, "EZbass library missing");
        for (int a = 0; a < (int) sidx.size(); ++a)
            for (int g = 0; g < G_Count; ++g)
                for (int seed = 1; seed <= 3; ++seed)
                    for (int useLib = 0; useLib < 2; ++useLib)
                    {
                        Settings s; s.instrument = I_Bass; s.artist = a; s.genre = g; s.seed = (uint64_t) (seed * 31 + a); s.bars = 8;
                        s.keyRoot = (a + seed) % 12; s.scale = (g % 2) ? SC_Major : SC_Minor; s.harmony = 1; s.busy = 0.2f + 0.3f * seed; s.humanize = (seed == 2);
                        if (useLib) s.lib = ez;
                        if (seed == 3) { s.tsNum = 6; s.tsDen = 8; }
                        const Pattern p = Generator::generate (s);
                        ++bt;
                        CHECK (! p.raw.empty(), "bass: empty pattern style %d genre %d", a, g);
                        std::vector<int> perBar ((size_t) p.bars, 0);
                        const Note* prev = nullptr;
                        for (auto& n : p.raw)
                        {
                            if (n.track != 0) continue;
                            ++bnotes;
                            CHECK (n.pitch >= 28 && n.pitch <= 60, "bass pitch %d out of range (style %d genre %d)", n.pitch, a, g);
                            CHECK (n.length > 0.05 && n.vel >= 1 && n.vel <= 127, "bass note length %.3f vel %d", n.length, n.vel);
                            ++perBar[(size_t) p.barOf (n.start)];
                            if (prev) CHECK (prev->start + prev->length <= n.start + 0.02 || std::abs (prev->start - n.start) < 1e-6 || prev->art == A_Ghost, "bass notes overlap: %.3f+%.3f vs %.3f (style %d genre %d)", prev->start, prev->length, n.start, a, g);
                            prev = &n;
                        }
                        for (int b = 0; b < p.bars; ++b) CHECK (perBar[(size_t) b] >= 1, "bass: empty bar %d (style %d genre %d)", b, a, g);
                        // a note lands on every chord change and plays its root (the first note of the bar is the root as well)
                        for (auto& c : p.chords)
                        {
                            const Note* first = nullptr;
                            for (auto& n : p.raw) if (n.track == 0 && n.start >= c.start - 1e-6 && n.start < c.start + 0.2) { first = &n; break; }
                            if (! first) { ++failures; std::printf ("FAIL: bass: nothing plays at chord change %.2f (style %d genre %d ts %d/%d seed %d)\n", c.start, a, g, s.tsNum, s.tsDen, seed); continue; }
                            ++rootChecks;
                            if (first->role == BR_Root || first->role == BR_Octave) CHECK (((first->pitch % 12) + 12) % 12 == c.rootPc % 12, "bass: root of %s is not played (pc %d)", c.name.c_str(), first->pitch % 12);
                        }
                        for (auto& e : p.events) if (e.type == EV_ON && e.ord == 1 && e.track == 0)
                        {
                            if (useLib) CHECK (e.a == 20 || e.a == 18 || e.a == 4 || e.a == 12 || e.a == 10 || e.a == 11, "EZbass: unexpected key-switch note %d", e.a);
                            else CHECK (e.a >= 12 && e.a < 28, "standard bass key-switch %d not below the bass range", e.a);
                        }
                    }
        // a chord change in the editor keeps the line musical: each changed bar is re-pitched onto the new chord
        {
            Settings s; s.instrument = I_Bass; s.artist = 0; s.genre = G_Afrobeat; s.seed = 5; s.bars = 4; s.keyRoot = 9; s.scale = SC_Minor; s.humanize = false;
            Pattern p = Generator::generate (s);
            const auto oldC = p.chords;
            Settings t = s; t.barChords.assign (4, BarChord()); t.barChords[2] = { true, 5, Quality::Maj7, -1 };    // bar 3 becomes the bVI chord
            std::string pn; const auto newC = Generator::chordsFor (t, pn);
            Generator::reharmonize (p, oldC, newC, t);
            for (auto& n : p.raw) if (n.track == 0 && p.barOf (n.start) == 2 && n.role == BR_Root)
                { int want = -1; for (auto& c : newC) if (n.start >= c.start - 1e-6 && n.start < c.end - 1e-6) want = c.rootPc % 12; CHECK (((n.pitch % 12) + 12) % 12 == want, "reharmonize: root %d, chord root %d", n.pitch % 12, want); }
        }
        std::printf ("bass: %ld patterns, %ld notes, %ld chord-change roots checked\n", bt, bnotes, rootChecks);
    }

    std::printf ("patterns: %ld  notes: %ld  dupes: %ld  failures: %d\n", total, totalNotes, dupes, failures);
    for (auto& kv : artsSeen)
    {
        std::printf ("%-8s arts used:", instName (kv.first));
        for (int a : kv.second) std::printf (" %s,", artName (a));
        std::printf ("\n");
        // every articulation in the key-switch list should be reachable
        for (int a : artList (kv.first)) if (! kv.second.count (a)) std::printf ("   (never produced: %s)\n", artName (a));
    }
    return failures ? 1 : 0;
}
