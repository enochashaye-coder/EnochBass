// Enoch Bass - Standard MIDI File writer (header only, no JUCE dependency)
#pragma once
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "Generator.h"

namespace enochsolo
{

class MidiWriter
{
public:
    static constexpr int kPPQ = 960;

    /** Format-1 file: tempo/meta track + solo track (ch1) + optional accompaniment track (ch2). */
    static std::vector<uint8_t> build (const Pattern& p)
    {
        std::vector<std::vector<uint8_t>> tracks;
        tracks.push_back (metaTrack (p));
        if (p.hasSolo)
        {
            std::map<int, int> used;
            if (p.s.exportMode == 3)
                for (auto& e : p.events) if (e.type == EV_ON && e.track == 0 && e.ord == 2) ++used[e.art];
            if (used.size() > 1)
            {
                // one MIDI track per articulation: drag each onto its own patch of your instrument
                std::vector<std::pair<int, int>> order (used.begin(), used.end());
                std::stable_sort (order.begin(), order.end(), [] (const auto& a, const auto& b) { return a.second > b.second; });
                for (auto& o : order)
                    tracks.push_back (noteTrack (p, 0, 0, std::string ("Enoch Bass - ") + instName (p.s.instrument) + " - " + artName (o.first), o.first));
            }
            else tracks.push_back (noteTrack (p, 0, 0, std::string ("Enoch Bass - ") + instName (p.s.instrument) + " solo"));
        }
        if (p.hasComp) tracks.push_back (noteTrack (p, 1, p.hasSolo ? 1 : 0, std::string ("Enoch Bass - ") + instName (p.s.instrument) + " accompaniment"));
        if (p.hasChords) tracks.push_back (noteTrack (p, 2, (p.hasSolo ? 1 : 0) + (p.hasComp ? 1 : 0), "Enoch Bass - chords"));

        std::vector<uint8_t> f;
        put4 (f, "MThd"); put32 (f, 6); put16 (f, 1); put16 (f, (uint16_t) tracks.size()); put16 (f, kPPQ);
        for (auto& t : tracks) { put4 (f, "MTrk"); put32 (f, (uint32_t) t.size()); f.insert (f.end(), t.begin(), t.end()); }
        return f;
    }

private:
    static void put4 (std::vector<uint8_t>& v, const char* s) { for (int i = 0; i < 4; ++i) v.push_back ((uint8_t) s[i]); }
    static void put32 (std::vector<uint8_t>& v, uint32_t x) { v.push_back ((uint8_t) (x >> 24)); v.push_back ((uint8_t) (x >> 16)); v.push_back ((uint8_t) (x >> 8)); v.push_back ((uint8_t) x); }
    static void put16 (std::vector<uint8_t>& v, uint16_t x) { v.push_back ((uint8_t) (x >> 8)); v.push_back ((uint8_t) x); }
    static void putVar (std::vector<uint8_t>& v, uint32_t x)
    {
        uint8_t buf[5]; int n = 0; buf[n++] = (uint8_t) (x & 0x7f);
        while ((x >>= 7) > 0) buf[n++] = (uint8_t) (0x80 | (x & 0x7f));
        while (n > 0) v.push_back (buf[--n]);
    }
    static void putMeta (std::vector<uint8_t>& v, uint32_t dt, uint8_t type, const std::vector<uint8_t>& data)
    {
        putVar (v, dt); v.push_back (0xFF); v.push_back (type); putVar (v, (uint32_t) data.size());
        v.insert (v.end(), data.begin(), data.end());
    }
    static std::vector<uint8_t> text (const std::string& s) { return std::vector<uint8_t> (s.begin(), s.end()); }

    static std::vector<uint8_t> metaTrack (const Pattern& p)
    {
        std::vector<uint8_t> t;
        putMeta (t, 0, 0x03, text ("Enoch Bass"));
        const uint32_t mpq = (uint32_t) std::lround (60000000.0 / std::max (20.0, p.s.bpm));
        putMeta (t, 0, 0x51, { (uint8_t) (mpq >> 16), (uint8_t) (mpq >> 8), (uint8_t) mpq });
        int dd = 2; { int d = p.s.tsDen; dd = 0; while (d > 1) { d >>= 1; ++dd; } }
        putMeta (t, 0, 0x58, { (uint8_t) p.s.tsNum, (uint8_t) dd, 24, 8 });
        // key signature (sharps/flats for the major key a minor-ish scale relates to)
        static const int sfMajor[12] = { 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5 };
        int rootMajor = p.s.keyRoot;
        const bool minor = scaleFamily (p.s.scale) == 1;
        if (minor) rootMajor = (p.s.keyRoot + 3) % 12;
        putMeta (t, 0, 0x59, { (uint8_t) (int8_t) sfMajor[rootMajor], (uint8_t) (minor ? 1 : 0) });
        putMeta (t, 0, 0x01, text (std::string ("Key: ") + kNoteNames[p.s.keyRoot] + " " + scaleName (p.s.scale)));
        putMeta (t, 0, 0x01, text (std::string ("Style: ") + p.styleName + " | " + p.genreName + " | " + p.progName));
        if (p.s.emitKS && p.s.exportMode == 0 && p.s.lib && p.s.lib->inst == p.s.instrument && ! p.s.lib->standard)
        {
            std::string ks = "Instrument of choice: " + p.s.lib->name + (p.s.lib->noKeySwitches ? " (no key-switches, controllers only)" : " - key-switches: ");
            for (size_t i = 0; i < p.s.lib->arts.size(); ++i)
            {
                const auto& a = p.s.lib->arts[i];
                ks += std::string (artName (a.art)) + "=";
                for (size_t k = 0; k < a.press.size(); ++k) ks += noteName (a.press[k].note) + (k + 1 < a.press.size() ? "+" : "");
                ks += (i + 1 < p.s.lib->arts.size() ? ", " : "");
            }
            putMeta (t, 0, 0x01, text (ks));
        }
        else if (p.s.emitKS && p.s.exportMode == 0)
        {
            std::string ks = "Key-switches (base " + noteName (p.s.ksBase) + "): ";
            const auto& l = artList (p.s.instrument);
            for (size_t i = 0; i < l.size(); ++i) ks += noteName (p.s.ksBase + (int) i) + "=" + artName (l[i]) + (i + 1 < l.size() ? ", " : "");
            putMeta (t, 0, 0x01, text (ks));
        }
        // chord symbols as markers: most DAWs show them on the timeline
        uint32_t last = 0;
        for (auto& c : p.chords)
        {
            const uint32_t tick = (uint32_t) std::lround (c.start * kPPQ);
            putMeta (t, tick - last, 0x06, text (c.name)); last = tick;
        }
        putMeta (t, (uint32_t) std::max<long> (0, std::lround (p.lengthBeats * kPPQ) - (long) last), 0x2F, {});
        return t;
    }

    static std::vector<uint8_t> noteTrack (const Pattern& p, int track, int channel, const std::string& name, int artFilter = -1)
    {
        struct E { uint32_t tick; int ord; std::vector<uint8_t> bytes; };
        std::vector<E> ev;
        const uint8_t ch = (uint8_t) channel;
        auto tickOf = [] (double beats) { return (uint32_t) std::max (0.0, std::llround (beats * kPPQ) + 0.0); };
        for (auto& e : p.events)
        {
            if (e.track != track) continue;
            if (artFilter >= 0 && (e.type == EV_ON || e.type == EV_OFF) && e.art != artFilter) continue;
            E x; x.tick = tickOf (e.t); x.ord = e.ord;
            switch (e.type)
            {
                case EV_ON:   x.bytes = { (uint8_t) (0x90 | ch), (uint8_t) e.a, (uint8_t) e.b }; break;
                case EV_OFF:  x.bytes = { (uint8_t) (0x80 | ch), (uint8_t) e.a, 0 }; break;
                case EV_CC:   x.bytes = { (uint8_t) (0xB0 | ch), (uint8_t) e.a, (uint8_t) e.b }; break;
                case EV_AT:   x.bytes = { (uint8_t) (0xD0 | ch), (uint8_t) e.a }; break;
                case EV_BEND: x.bytes = { (uint8_t) (0xE0 | ch), (uint8_t) (e.a & 0x7f), (uint8_t) ((e.a >> 7) & 0x7f) }; break;
            }
            ev.push_back (std::move (x));
        }
        std::stable_sort (ev.begin(), ev.end(), [] (const E& a, const E& b) { return a.tick != b.tick ? a.tick < b.tick : a.ord < b.ord; });
        std::vector<uint8_t> t;
        putMeta (t, 0, 0x03, text (name));
        uint32_t last = 0;
        for (auto& e : ev) { putVar (t, e.tick - last); t.insert (t.end(), e.bytes.begin(), e.bytes.end()); last = e.tick; }
        const uint32_t endTick = std::max (last, tickOf (p.lengthBeats));
        putMeta (t, endTick - last, 0x2F, {});
        return t;
    }
};

} // namespace enochsolo
