// Enoch v3.11 / v2.11 - MIDI validation and repair (identical in Enoch Solo and Enoch Bass).
// Every MIDI file that leaves Enoch (drag, Save .mid) is checked here first, and anything wrong is repaired before it is written:
//   - every key-switch is one the chosen instrument profile really has (an unknown key is removed, never sent)
//   - every key-switch is down before the note it is for (moved earlier when it is not)
//   - two different held key-switches never overlap (the earlier one is let go first)
//   - no played note sits on a key-switch key or outside the instrument's playable range (moved by octaves)
//   - every note-on has its note-off: no stuck notes, no orphan note-offs, a re-struck note is let go first
//   - nothing plays after the end of the clip; the sustain pedal and pitch bend are back at rest at the end
//   - controller data is thinned where it is denser than an instrument can use (repeats of the same value, < 5 ms apart)
// The report says what was repaired (shown under DRAG MIDI); "OK" when there was nothing to do.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace enochsolo
{
namespace midicheck
{
struct Report
{
    int ksUnknown = 0, ksLate = 0, ksClash = 0, onKey = 0, outOfRange = 0, stuck = 0, orphanOff = 0, retrigger = 0, afterEnd = 0, ccThinned = 0, resets = 0;
    int legatoOverlap = 0, accidentalLegato = 0, doubled = 0;      // v3.12: the techniques' own rules
    int ksVel = 0, ksMissing = 0;                                     // v3.14: key velocity as the profile says, held key under every joined note
    int wideJoin = 0;                                                 // v3.14: notice only: a legato wider than an octave (kept as you wrote it)
    int repaired() const { return ksUnknown + ksLate + ksClash + onKey + outOfRange + stuck + orphanOff + retrigger + afterEnd + legatoOverlap + accidentalLegato + doubled + ksVel + ksMissing; }
    std::string text() const
    {
        const std::string note = wideJoin > 0 ? "  Note: " + std::to_string (wideJoin) + " legato jump(s) wider than an octave; most instruments cannot slur that far." : std::string();
        if (repaired() == 0) return "MIDI check: OK" + note;
        std::string t = "MIDI check: repaired " + std::to_string (repaired()) + " (";
        bool first = true;
        auto add = [&] (int n, const char* what) { if (n <= 0) return; t += (first ? "" : ", ") + std::to_string (n) + " " + what; first = false; };
        add (ksUnknown, "unknown key-switch removed");
        add (ksLate, "key-switch moved before its note");
        add (ksClash, "overlapping key-switch released");
        add (onKey, "note moved off a key-switch key");
        add (outOfRange, "note folded into range");
        add (stuck, "missing note-off added");
        add (orphanOff, "stray note-off removed");
        add (retrigger, "re-struck note released first");
        add (afterEnd, "event after the clip end");
        add (legatoOverlap, "legato overlap lengthened so the instrument joins the notes");
        add (accidentalLegato, "plain note shortened so it is not played legato");
        add (doubled, "pitched note removed where the key itself is the sound");
        add (ksVel, "key-switch velocity set to the profile's");
        add (ksMissing, "missing legato key added under a joined note");
        return t + ")" + note;
    }
};

inline Report repairOnce (Pattern& p)
{
    Report rep;
    auto& ev = p.events;
    if (ev.empty()) return rep;
    const double E = std::max (0.25, p.lengthBeats);
    const double beatsPerMs = std::max (20.0, p.s.bpm) / 60000.0;
    std::vector<char> drop (ev.size(), 0);
    // work on the file's own grid (960 ticks a beat): two events in one tick are ordered exactly as the file will order them
    for (auto& e : ev) e.t = std::round (e.t * 960.0) / 960.0;

    for (int track = 0; track < 3; ++track)
    {
        // the profile this part plays (the same choice the driver made)
        const Library* L = nullptr;
        if (p.s.routed) { const auto& rc = p.s.routes[track]; L = rc.lib ? rc.lib.get() : (track == 0 ? p.s.lib.get() : nullptr); }
        else if (track == 0) L = p.s.lib.get();
        if (L && L->inst != p.s.instrument && track != 2) L = nullptr;
        bool anyKs = false;
        for (auto& e : ev) if (e.track == track && e.ks) { anyKs = true; break; }
        std::array<bool, 128> mask {}; mask.fill (false);
        std::array<bool, 128> holdKey {}; holdKey.fill (false);
        const bool checkKeys = L && ! L->standard && ! L->noKeySwitches;
        if (L && ! L->standard)
        {
            mask = libKeyMask (*L);
            for (auto& a : L->arts) if (a.hold) for (auto& pr : a.press) if (pr.note >= 0 && pr.note < 128) holdKey[(size_t) pr.note] = true;
            for (int k : L->stringKeys) if (k >= 0 && k < 128) holdKey[(size_t) k] = false;
            if (L->repKey >= 0 && L->repKey < 128) holdKey[(size_t) L->repKey] = false;
        }
        std::vector<int> idx;
        for (int i = 0; i < (int) ev.size(); ++i) if (ev[(size_t) i].track == track) idx.push_back (i);
        auto byTime = [&] { std::stable_sort (idx.begin(), idx.end(), [&] (int a, int b) { const auto& x = ev[(size_t) a]; const auto& y = ev[(size_t) b]; return x.t != y.t ? x.t < y.t : x.ord < y.ord; }); };
        byTime();

        // 1) key-switches the profile does not have: removed (on and off)
        if (anyKs && checkKeys)
            for (int i : idx) { auto& e = ev[(size_t) i]; if (e.ks && (e.type == EV_ON || e.type == EV_OFF) && ! mask[(size_t) std::clamp (e.a, 0, 127)]) { drop[(size_t) i] = 1; if (e.type == EV_ON) ++rep.ksUnknown; } }

        // 2) played notes: never on a key-switch key, never outside the playable range (moved by octaves, on and off together)
        if (L && ! L->standard)
        {
            int lo = L->lo, hi = L->hi;
            std::map<int, int> moveOfId;     // srcId -> new pitch
            for (int i : idx)
            {
                auto& e = ev[(size_t) i];
                if (drop[(size_t) i] || e.ks || e.type != EV_ON || e.srcId < 0) continue;
                int q = e.a; bool range = false, key = false;
                while (q < lo && q + 12 <= 127) { q += 12; range = true; }
                while (q > hi && q - 12 >= 0) { q -= 12; range = true; }
                if (anyKs && mask[(size_t) q])
                {
                    key = true;
                    int best = -1;
                    for (int d : { 12, -12, 24, -24 }) { const int c = q + d; if (c >= 0 && c <= 127 && ! mask[(size_t) c]) { best = c; if (c >= lo && c <= hi) break; } }
                    if (best >= 0) q = best;
                }
                if (q != e.a) { if (range) ++rep.outOfRange; if (key) ++rep.onKey; moveOfId[e.srcId] = q; e.a = q; }
            }
            if (! moveOfId.empty())
                for (int i : idx) { auto& e = ev[(size_t) i]; if (! e.ks && e.type == EV_OFF && moveOfId.count (e.srcId)) e.a = moveOfId[e.srcId]; }
        }

        // 2b) v3.12 techniques: a note whose key IS the sound (EZbass E0 ghost) is never also played as a pitched note; a joined note
        //     (legato, hammer-on, pull-off, slide) is overlapped by the note before by the profile's amount; on an instrument that plays
        //     any overlap as legato, a plain note never overlaps the next one
        if (L && ! L->standard)
        {
            std::set<int> replaced;
            for (int i : idx) { const auto& e = ev[(size_t) i]; if (! drop[(size_t) i] && e.ks && e.type == EV_ON && e.noteId >= 0) { const LibArt* la = libFind (*L, libResolve (*L, e.art)); if (la && la->when == KW_Replace) replaced.insert (e.noteId); } }
            for (int i : idx) { const auto& e = ev[(size_t) i]; if (! drop[(size_t) i] && ! e.ks && e.srcId >= 0 && replaced.count (e.srcId) && (e.type == EV_ON || e.type == EV_OFF)) { drop[(size_t) i] = 1; if (e.type == EV_ON) ++rep.doubled; } }
            // the played line in time order, with each note's off
            std::map<int, int> offOfId;
            for (int i : idx) { const auto& e = ev[(size_t) i]; if (! drop[(size_t) i] && ! e.ks && e.type == EV_OFF && e.srcId >= 0 && ! offOfId.count (e.srcId)) offOfId[e.srcId] = i; }
            std::vector<int> lineOn;
            for (int i : idx) { const auto& e = ev[(size_t) i]; if (! drop[(size_t) i] && ! e.ks && e.type == EV_ON && e.srcId >= 0 && offOfId.count (e.srcId)) lineOn.push_back (i); }
            const double ov = L->overlapMs * beatsPerMs, gap = 8.0 * beatsPerMs;
            for (size_t k = 1; k < lineOn.size(); ++k)
            {
                const auto& cur = ev[(size_t) lineOn[k]]; const auto& prv = ev[(size_t) lineOn[k - 1]];
                auto& prvOff = ev[(size_t) offOfId[prv.srcId]];
                if (cur.t - prv.t < 0.01 || prv.a == cur.a) continue;                  // played together / same key: other rules
                if (driver::isLegatoArt (cur.art) && L->overlapMs > 0 && prvOff.t >= cur.t - 0.25)
                { if (prvOff.t < cur.t + 0.8 * ov) { prvOff.t = cur.t + ov; ++rep.legatoOverlap; } }
                else if (L->overlapLegato && ! driver::isLegatoArt (cur.art) && prvOff.t > cur.t - 1e-9)
                { prvOff.t = std::max (prv.t + 0.01, cur.t - gap); ++rep.accidentalLegato; }
            }
        }

        // 2c) v3.14 deeper checks on the profile's own keys
        //     - a key-switch goes down with the velocity the profile gives it (some libraries pick the sound by velocity);
        //       keys whose velocity carries the note's dynamics (keys that ARE the sound), drawn keys and hand-edited notes are left alone
        //     - a joined note on a profile with a held legato key has that key down when it starts (added if it is missing)
        //     - a legato jump wider than an octave is reported (kept: you may have written it on purpose)
        if (L && ! L->standard && ! L->noKeySwitches)
        {
            std::set<int> manualNote;
            for (int i : idx) { const auto& e = ev[(size_t) i]; if (! drop[(size_t) i] && ! e.ks && e.type == EV_ON && e.manualKs && e.srcId >= 0) manualNote.insert (e.srcId); }
            for (int i : idx)
            {
                auto& e = ev[(size_t) i];
                if (drop[(size_t) i] || ! e.ks || e.type != EV_ON || e.drawn || e.noteId < 0 || manualNote.count (e.noteId)) continue;
                if (e.a == L->repKey || std::find (L->stringKeys.begin(), L->stringKeys.end(), e.a) != L->stringKeys.end()) continue;   // string / repeat keys: their own velocities
                bool initKey = false; for (auto& k : L->init) if (k.note == e.a) initKey = true;
                if (initKey) continue;
                // the profile's velocity for this key: only when every articulation on this key asks for the same one
                // (a key event does not always carry its articulation: a held key serves a run of notes)
                int want = -1; bool agree = true, replaceKey = false;
                for (auto& la : L->arts)
                    for (auto& pr : la.press)
                        if (pr.note == e.a)
                        {
                            if (la.when == KW_Replace) replaceKey = true;
                            if (want < 0) want = pr.vel; else if (want != pr.vel) agree = false;
                        }
                if (replaceKey || ! agree || want < 1 || want > 127) continue;
                if (e.b != want) { e.b = want; ++rep.ksVel; }
            }
            // presses per key: [on, off] in time
            std::map<int, std::vector<std::pair<double, double>>> downs;
            {
                std::map<int, double> open;
                for (int i : idx)
                {
                    const auto& e = ev[(size_t) i];
                    if (drop[(size_t) i] || ! e.ks) continue;
                    if (e.type == EV_ON) open[e.a] = e.t;
                    else if (e.type == EV_OFF && open.count (e.a)) { downs[e.a].push_back ({ open[e.a], e.t }); open.erase (e.a); }
                }
                for (auto& [k, t0] : open) downs[k].push_back ({ t0, E });
            }
            std::map<int, double> offT;
            for (int i : idx) { const auto& e = ev[(size_t) i]; if (! drop[(size_t) i] && ! e.ks && e.type == EV_OFF && e.srcId >= 0 && ! offT.count (e.srcId)) offT[e.srcId] = e.t; }
            const double lead = std::clamp (L->preRollMs * beatsPerMs, 0.002, 0.25);
            int prevPitch = -1; double prevOn = -1.0; int prevId = -1;
            std::vector<MidiEv> add;
            for (int i : idx)
            {
                const auto& e = ev[(size_t) i];
                if (drop[(size_t) i] || e.ks || e.type != EV_ON || e.srcId < 0) continue;
                if (e.t - prevOn < 0.01 && prevId >= 0) continue;                       // chord tone: the line is the first note
                if (driver::isLegatoArt (e.art) && prevPitch >= 0 && std::abs (e.a - prevPitch) > 12) ++rep.wideJoin;
                // joined as the writer joins: a different pitch, the note before still sounding within a quarter beat of this one
                const bool joined = driver::isLegatoArt (e.art) && prevId >= 0 && prevPitch != e.a && offT.count (prevId) && offT[prevId] >= e.t - 0.25;
                const LibArt* la = joined ? libFind (*L, libResolve (*L, e.art)) : nullptr;
                if (la && la->hold && la->when == KW_Before && ! la->press.empty() && ! manualNote.count (e.srcId) && offT.count (e.srcId))
                {
                    const int key = la->press[0].note;
                    bool down = false;
                    if (downs.count (key)) for (auto& d : downs[key]) if (d.first <= e.t + 1e-6 && d.second >= e.t - 1e-6) { down = true; break; }
                    // written for this note but late: step 3 moves it in front, nothing to add
                    for (int j : idx) { const auto& o = ev[(size_t) j]; if (! drop[(size_t) j] && o.ks && o.type == EV_ON && o.a == key && o.noteId == e.srcId) { down = true; break; } }
                    if (! down && key >= 0 && key < 128)
                    {
                        MidiEv on = e; on.ks = true; on.a = key; on.b = la->press[0].vel > 0 ? la->press[0].vel : 100; on.t = std::max (0.0, e.t - lead); on.ord = 1;
                        on.noteId = e.srcId; on.srcId = -1; on.drawn = false; on.manualKs = false;
                        MidiEv off = on; off.type = EV_OFF; off.b = 0; off.ord = 0; off.t = std::max (on.t + 0.005, offT[e.srcId]);
                        add.push_back (on); add.push_back (off);
                        downs[key].push_back ({ on.t, off.t });
                        ++rep.ksMissing;
                    }
                }
                prevPitch = e.a; prevOn = e.t; prevId = e.srcId;
            }
            for (auto& a : add) { idx.push_back ((int) ev.size()); ev.push_back (a); drop.push_back (0); }
            if (! add.empty()) byTime();
        }

        // 3) every key-switch is down before its note (the note it was written for, by id)
        if (anyKs)
        {
            std::map<int, double> noteOn;
            std::map<int, int> noteArt;
            for (int i : idx) { const auto& e = ev[(size_t) i]; if (! drop[(size_t) i] && ! e.ks && e.type == EV_ON && e.srcId >= 0 && ! noteOn.count (e.srcId)) { noteOn[e.srcId] = e.t; noteArt[e.srcId] = e.art; } }
            const double lead = std::clamp ((L ? L->preRollMs : 20.0) * beatsPerMs, 0.002, 0.25);
            for (int i : idx)
            {
                auto& e = ev[(size_t) i];
                if (drop[(size_t) i] || ! e.ks || e.type != EV_ON || e.noteId < 0) continue;
                auto it = noteOn.find (e.noteId);
                if (it == noteOn.end() || e.t <= it->second + 1e-9) continue;
                // v3.12: a key pressed WHILE its note sounds (EZbass F0 / G0 slide-outs) belongs after the note's start; it only has to be inside it
                if (L && noteArt.count (e.noteId)) { const LibArt* la = libFind (*L, libResolve (*L, noteArt[e.noteId])); if (la && la->when == KW_During) continue; }
                const double was = e.t;
                e.t = std::max (0.0, it->second - lead); ++rep.ksLate;
                // its note-off moves with it when it would now come before the press
                for (int j : idx) { auto& o = ev[(size_t) j]; if (o.ks && o.type == EV_OFF && o.a == e.a && o.noteId == e.noteId && o.t >= was - 1e-9) { if (o.t < e.t + 0.005) o.t = e.t + 0.005; break; } }
            }
            byTime();
        }

        // 4) after the clip end: notes that would start there are dropped, notes still sounding end there, controllers stop there
        for (int i : idx)
        {
            auto& e = ev[(size_t) i];
            if (drop[(size_t) i]) continue;
            if (e.type == EV_ON && e.t >= E - 1e-9) { drop[(size_t) i] = 1; ++rep.afterEnd; continue; }
            if (e.type == EV_OFF && e.t > E + 1e-9) e.t = E;
            if ((e.type == EV_CC || e.type == EV_BEND || e.type == EV_AT) && e.t > E + 1e-9) { drop[(size_t) i] = 1; ++rep.afterEnd; }
        }
        byTime();

        // 5) two different held key-switches never overlap: the earlier one is let go just before the next one goes down
        if (anyKs && L && ! L->standard)
        {
            int downKey = -1, downOff = -1; double downAt = 0.0;
            for (size_t n = 0; n < idx.size(); ++n)
            {
                const int i = idx[n];
                auto& e = ev[(size_t) i];
                if (drop[(size_t) i] || ! e.ks || ! holdKey[(size_t) std::clamp (e.a, 0, 127)]) continue;
                if (e.type == EV_ON)
                {
                    if (downKey >= 0 && downKey != e.a && downOff >= 0 && ev[(size_t) downOff].t > e.t + 1e-9)
                    { ev[(size_t) downOff].t = std::max (downAt + 0.003, e.t - 0.002); ++rep.ksClash; }
                    downKey = e.a; downOff = -1; downAt = e.t;
                    for (size_t m = n + 1; m < idx.size(); ++m) { const auto& o = ev[(size_t) idx[m]]; if (! drop[(size_t) idx[m]] && o.ks && o.type == EV_OFF && o.a == e.a) { downOff = idx[m]; break; } }
                }
                else if (e.type == EV_OFF && i == downOff) { downKey = -1; downOff = -1; }
            }
            byTime();
        }

        // 6) on / off pairs per channel and key: a re-struck key is let go first, stray offs go, missing offs are added at the end
        {
            auto chKey = [&] (const MidiEv& e) { return (e.ch >= 0 ? e.ch : track) * 128 + std::clamp (e.a, 0, 127); };
            auto ownerOf = [] (const MidiEv& e) { return e.ks ? e.noteId : e.srcId; };
            std::map<int, int> down;        // channel * 128 + key -> the index of the note-on that holds it
            std::set<int> handled;          // note-offs already moved to end a re-struck note
            for (size_t n = 0; n < idx.size(); ++n)
            {
                const int i = idx[n];
                auto& e = ev[(size_t) i];
                if (drop[(size_t) i] || (e.type != EV_ON && e.type != EV_OFF) || handled.count (i)) continue;
                const int key = chKey (e);
                auto it = down.find (key);
                if (e.type == EV_ON)
                {
                    if (it != down.end())
                    {
                        // the earlier note's own note-off (by its id, else the next one on this key) moves to here
                        const auto& prevOn = ev[(size_t) it->second];
                        int pick = -1;
                        for (size_t m = n + 1; m < idx.size(); ++m)
                        {
                            const auto& o = ev[(size_t) idx[m]];
                            if (drop[(size_t) idx[m]] || o.type != EV_OFF || chKey (o) != key || handled.count (idx[m])) continue;
                            // only the earlier note's own note-off: one that belongs to another note is never taken from it
                            if (ownerOf (prevOn) >= 0) { if (ownerOf (o) == ownerOf (prevOn)) { pick = idx[m]; break; } continue; }
                            if (ownerOf (o) < 0) { pick = idx[m]; break; }
                        }
                        if (pick >= 0) { ev[(size_t) pick].t = e.t; ev[(size_t) pick].ord = std::min (ev[(size_t) pick].ord, e.ord); handled.insert (pick); }
                        else { MidiEv off = prevOn; off.type = EV_OFF; off.b = 0; off.ord = 0; off.t = e.t; ev.push_back (off); drop.push_back (0); }
                        ++rep.retrigger;
                    }
                    down[key] = i;
                }
                else
                {
                    if (it == down.end()) { drop[(size_t) i] = 1; ++rep.orphanOff; }
                    else down.erase (it);
                }
            }
            for (auto& kv : down) { MidiEv off = ev[(size_t) kv.second]; off.type = EV_OFF; off.b = 0; off.ord = 0; off.t = E; ev.push_back (off); drop.push_back (0); ++rep.stuck; }
        }
    }

    // 7) controllers: same value twice in a row, or closer than 5 ms (bend: 3 ms), is thinned; pedal and bend at rest at the end
    {
        std::map<long, std::pair<double, int>> last;     // (track, channel, type, cc) -> time, value
        std::vector<int> order;
        for (int i = 0; i < (int) ev.size(); ++i) if (! drop[(size_t) i] && (ev[(size_t) i].type == EV_CC || ev[(size_t) i].type == EV_BEND)) order.push_back (i);
        std::stable_sort (order.begin(), order.end(), [&] (int a, int b) { return ev[(size_t) a].t < ev[(size_t) b].t; });
        const double gapCc = 5.0 * beatsPerMs, gapBend = 3.0 * beatsPerMs;
        for (size_t n = 0; n < order.size(); ++n)
        {
            const auto& e = ev[(size_t) order[n]];
            const bool bend = e.type == EV_BEND;
            const long k = ((long) e.track * 16 + (e.ch >= 0 ? e.ch : e.track)) * 1000 + (bend ? 999 : e.a);
            const int val = bend ? e.a : e.b;
            auto it = last.find (k);
            // never thin the last value of a run (where the controller comes to rest)
            const bool lastOfRun = n + 1 >= order.size() || [&] { for (size_t m = n + 1; m < order.size(); ++m) { const auto& o = ev[(size_t) order[m]]; const long ko = ((long) o.track * 16 + (o.ch >= 0 ? o.ch : o.track)) * 1000 + (o.type == EV_BEND ? 999 : o.a); if (ko == k) return o.t - e.t > 0.25; } return true; }();
            if (it != last.end() && ! lastOfRun && (it->second.second == val || e.t - it->second.first < (bend ? gapBend : gapCc)))
            { drop[(size_t) order[n]] = 1; ++rep.ccThinned; continue; }
            last[k] = { e.t, val };
        }
        for (auto& kv : last)
        {
            const long k = kv.first; const int val = kv.second.second;
            const bool bend = (k % 1000) == 999, pedal = (k % 1000) == 64;
            if ((bend && val != 8192) || (pedal && val >= 64))
            {
                MidiEv r; r.t = E; r.type = bend ? EV_BEND : EV_CC; r.a = bend ? 8192 : 64; r.b = 0; r.track = (int) (k / 1000 / 16); r.ord = 0;
                const int chv = (int) ((k / 1000) % 16); r.ch = chv == r.track ? -1 : chv;
                ev.push_back (r); drop.push_back (0); ++rep.resets;
            }
        }
    }

    std::vector<MidiEv> out; out.reserve (ev.size());
    for (size_t i = 0; i < ev.size(); ++i) if (! drop[i]) { auto e = ev[i]; e.t = std::round (e.t * 960.0) / 960.0; out.push_back (e); }
    // a note-off never lands in its own note-on's tick (it would be written first): at least one tick after it
    {
        std::map<long long, double> onAt;
        auto ownerKey = [] (const MidiEv& e) { const int o = e.ks ? e.noteId : e.srcId; return o < 0 ? -1LL : ((long long) o * 4 + e.track) * 128 + std::clamp (e.a, 0, 127); };
        for (auto& e : out) if (e.type == EV_ON) { const auto k = ownerKey (e); if (k >= 0) onAt[k] = e.t; }
        for (auto& e : out) if (e.type == EV_OFF) { const auto k = ownerKey (e); auto it = onAt.find (k); if (k >= 0 && it != onAt.end() && e.t <= it->second) e.t = it->second + 1.0 / 960.0; }
    }
    std::stable_sort (out.begin(), out.end(), [] (const MidiEv& a, const MidiEv& b) { return a.t != b.t ? a.t < b.t : a.ord < b.ord; });
    // last sweep, in exactly the order the file is written: every off ends its own note, nothing is struck twice, nothing is left down
    {
        auto keyOf = [] (const MidiEv& e) { return (e.track * 16 + (e.ch >= 0 ? e.ch : e.track)) * 128 + std::clamp (e.a, 0, 127); };
        auto owner = [] (const MidiEv& e) { return e.ks ? e.noteId : e.srcId; };
        std::map<int, std::vector<int>> down;     // key -> owners of the notes down on it (oldest first)
        std::vector<char> used (out.size(), 0);
        std::vector<MidiEv> fin; fin.reserve (out.size() + 16);
        for (size_t i = 0; i < out.size(); ++i)
        {
            if (used[i]) continue;
            const auto& e = out[i];
            if (e.type == EV_ON)
            {
                auto& d = down[keyOf (e)];
                while (! d.empty())
                {
                    // the note still down is let go here: its own off (moved here), or a new one
                    const int o = d.front(); d.erase (d.begin());
                    int pick = -1;
                    for (size_t m = i + 1; m < out.size(); ++m)
                        if (! used[m] && out[m].type == EV_OFF && keyOf (out[m]) == keyOf (e) && (o < 0 ? owner (out[m]) < 0 : owner (out[m]) == o)) { pick = (int) m; break; }
                    MidiEv off = pick >= 0 ? out[(size_t) pick] : e;
                    if (pick >= 0) used[(size_t) pick] = 1; else { off.type = EV_OFF; off.b = 0; if (e.ks) off.noteId = o; else off.srcId = o; }
                    off.t = e.t; off.ord = e.ord; fin.push_back (off); ++rep.retrigger;     // same order slot as the new note: written just before it
                }
                d.push_back (owner (e)); fin.push_back (e);
            }
            else if (e.type == EV_OFF)
            {
                auto& d = down[keyOf (e)];
                auto it = std::find (d.begin(), d.end(), owner (e));
                if (it == d.end() && owner (e) < 0 && ! d.empty()) it = d.begin();
                if (it == d.end()) { ++rep.orphanOff; continue; }
                d.erase (it); fin.push_back (e);
            }
            else fin.push_back (e);
        }
        for (auto& kv : down)
            for (int o : kv.second)
            {
                MidiEv off; off.t = std::round (E * 960.0) / 960.0; off.type = EV_OFF; off.b = 0; off.ord = 0; off.srcId = o;
                off.a = kv.first % 128; const int tc = kv.first / 128; off.track = tc / 16; off.ch = tc % 16 == off.track ? -1 : tc % 16;
                fin.push_back (off); ++rep.stuck;
            }
        out.swap (fin);
    }
    ev = std::move (out);
    return rep;
}

/** check and repair a pattern's events in place (call on a copy of the pattern that is about to be written).
    v3.14: one repair can make room for another (a stuck note given its off can now overlap the next note), so the check runs
    again until a pass finds nothing (at most three passes); the report adds up what every pass fixed */
inline Report repair (Pattern& p)
{
    Report rep = repairOnce (p);
    for (int pass = 0; pass < 2 && rep.repaired() > 0; ++pass)
    {
        const Report r = repairOnce (p);
        if (r.repaired() == 0) break;
        rep.ksUnknown += r.ksUnknown; rep.ksLate += r.ksLate; rep.ksClash += r.ksClash; rep.onKey += r.onKey; rep.outOfRange += r.outOfRange;
        rep.stuck += r.stuck; rep.orphanOff += r.orphanOff; rep.retrigger += r.retrigger; rep.afterEnd += r.afterEnd; rep.ccThinned += r.ccThinned;
        rep.legatoOverlap += r.legatoOverlap; rep.accidentalLegato += r.accidentalLegato; rep.doubled += r.doubled; rep.ksVel += r.ksVel; rep.ksMissing += r.ksMissing;
    }
    { Pattern q = p; rep.wideJoin = repairOnce (q).wideJoin; }      // notices describe the final file
    return rep;
}
} // namespace midicheck
} // namespace enochsolo
