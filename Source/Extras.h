#pragma once
// v3.8 (identical in Enoch Solo and Enoch Bass): favourite takes, ghost notes shared between Enoch Solo and Enoch Bass, humanize.
// Everything here runs on the message thread and only uses plain files: nothing static that needs cleaning up when the plugin is
// removed (so it can never hold up the DAW while it unloads the plugin).
#include <juce_core/juce_core.h>
#include <set>
#include <vector>
#include "Engine/BarLocks.h"
#ifdef _WIN32
 #include <process.h>
#else
 #include <unistd.h>
#endif

namespace enochx
{
using enochsolo::Note;

/** %APPDATA%\Enoch (created when needed) */
/** this DAW process (Enoch Solo and Enoch Bass are separate plugins, so they compare the process number) */
inline juce::int64 processNumber()
{
#ifdef _WIN32
    return (juce::int64) _getpid();
#else
    return (juce::int64) getpid();
#endif
}

inline juce::File enochDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Enoch");
    if (! d.isDirectory()) d.createDirectory();
    return d;
}

//------------------------------------------------------------------------------ notes as text (the same fields a project stores)
inline juce::String noteToText (const Note& n)
{
    juce::String s;
    s << juce::String (n.start, 5) << ',' << juce::String (n.length, 5) << ',' << n.pitch << ',' << n.vel << ',' << n.art << ','
      << n.track << ',' << n.voice << ',' << juce::String (n.weight, 3) << ',' << n.bend << ',' << (n.vib ? 1 : 0) << ',' << (n.mute ? 1 : 0)
      << ',' << enochsolo::barlock::roleOf (n) << ',' << ((n.ksManual ? 1 : 0) | (n.keyPlayed ? 2 : 0));
    return s;
}

inline bool noteFromText (const juce::String& line, Note& n)
{
    auto f = juce::StringArray::fromTokens (line, ",", "");
    if (f.size() < 11) return false;
    n = Note();
    n.start = juce::jmax (0.0, f[0].getDoubleValue()); n.length = juce::jmax (0.01, f[1].getDoubleValue());
    n.pitch = juce::jlimit (0, 127, f[2].getIntValue()); n.vel = juce::jlimit (1, 127, f[3].getIntValue());
    { const int a = f[4].getIntValue(); n.art = enochsolo::isCustomArt (a) ? a : juce::jlimit (0, (int) enochsolo::A_Count - 1, a); }
    n.track = juce::jlimit (0, 2, f[5].getIntValue()); n.voice = f[6].getIntValue(); n.weight = f[7].getDoubleValue();
    n.bend = f[8].getIntValue(); n.vib = f[9].getIntValue() != 0; n.mute = f[10].getIntValue() != 0;
    if (f.size() > 11) enochsolo::barlock::setRole (n, f[11].getIntValue());
    { const int kf = f.size() > 12 ? f[12].getIntValue() : 0; n.ksManual = (kf & 1) != 0; n.keyPlayed = (kf & 2) != 0; }   // v3.12
    return std::isfinite (n.start) && std::isfinite (n.length);
}

//------------------------------------------------------------------------------ favourite takes (kept in a file, in every project)
struct Favourite
{
    juce::String name, when;
    int bars = 0, inst = 0;
    std::vector<Note> notes;
};

inline juce::File favFile (const juce::String& plugin)
{
    return enochDir().getChildFile ("favourites-" + plugin.removeCharacters (" ") + ".txt");
}

inline std::vector<Favourite> loadFavourites (const juce::String& plugin)
{
    std::vector<Favourite> out;
    const auto f = favFile (plugin);
    if (! f.existsAsFile() || f.getSize() > 8 * 1024 * 1024) return out;
    juce::StringArray lines; f.readLines (lines);
    for (auto& raw : lines)
    {
        const auto line = raw.trim();
        if (line.startsWith ("#take|"))
        {
            auto p = juce::StringArray::fromTokens (line.fromFirstOccurrenceOf ("|", false, false), "|", "");
            Favourite fv; fv.name = p[0]; fv.when = p[1]; fv.bars = p[2].getIntValue(); fv.inst = p[3].getIntValue();
            out.push_back (fv);
        }
        else if (line.isNotEmpty() && ! out.empty() && out.size() <= 500)
        {
            Note n; if (noteFromText (line, n) && out.back().notes.size() < 20000) out.back().notes.push_back (n);
        }
    }
    return out;
}

inline bool saveFavourites (const juce::String& plugin, const std::vector<Favourite>& favs)
{
    juce::String t;
    t << "# Enoch favourite takes (" << plugin << "). One '#take' line per take, then its notes.\n";
    for (auto& fv : favs)
    {
        t << "#take|" << fv.name.removeCharacters ("|\n\r") << '|' << fv.when << '|' << fv.bars << '|' << fv.inst << '\n';
        for (auto& n : fv.notes) t << noteToText (n) << '\n';
    }
    return favFile (plugin).replaceWithText (t);
}

//------------------------------------------------------------------------------ ghost notes: each instance publishes its notes in a small file
struct GhostNote { double start = 0, length = 0; int pitch = 60, track = 0; };
struct GhostSet
{
    juce::String plugin, id;
    double barBeats = 4.0, lengthBeats = 0.0;
    std::vector<GhostNote> notes;
};

inline juce::File shareDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EnochShare");
    if (! d.isDirectory()) d.createDirectory();
    return d;
}

inline juce::File shareFile (const juce::String& plugin, const juce::String& id)
{
    return shareDir().getChildFile (plugin.removeCharacters (" ") + "_" + id + ".txt");
}

inline void publishShare (const GhostSet& g)
{
    juce::String t;
    t << "plugin=" << g.plugin << "\npid=" << processNumber() << "\nbarBeats=" << g.barBeats << "\nlength=" << g.lengthBeats << "\n";
    for (auto& n : g.notes) t << juce::String (n.start, 4) << ',' << juce::String (n.length, 4) << ',' << n.pitch << ',' << n.track << '\n';
    shareFile (g.plugin, g.id).replaceWithText (t);
}

inline void removeShare (const juce::String& plugin, const juce::String& id)
{
    const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EnochShare").getChildFile (plugin.removeCharacters (" ") + "_" + id + ".txt");
    if (f.existsAsFile()) f.deleteFile();
}

/** the newest notes published by an Enoch plugin other than `self` (Enoch Solo reads Enoch Bass and the other way round).
    Files from this DAW (same process) win over older ones; anything older than a day is ignored. */
inline bool readOtherShare (const juce::String& self, GhostSet& out, juce::Time& stamp)
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EnochShare");
    if (! dir.isDirectory()) return false;
    const auto mine = self.removeCharacters (" ");
    const auto pidNow = juce::String (processNumber());
    juce::File best; juce::Time bestT; bool bestSame = false;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*.txt"))
    {
        const auto f = e.getFile();
        if (f.getFileName().startsWith (mine + "_")) continue;
        const auto t = f.getLastModificationTime();
        if (juce::Time::getCurrentTime() - t > juce::RelativeTime::days (1)) continue;
        const bool same = f.loadFileAsString().fromFirstOccurrenceOf ("pid=", false, false).upToFirstOccurrenceOf ("\n", false, false) == pidNow;
        if (best == juce::File() || (same && ! bestSame) || (same == bestSame && t > bestT)) { best = f; bestT = t; bestSame = same; }
    }
    if (best == juce::File()) return false;
    if (best.getLastModificationTime() == stamp && ! out.notes.empty()) return true;     // unchanged
    stamp = best.getLastModificationTime();
    GhostSet g;
    juce::StringArray lines; best.readLines (lines);
    for (auto& l : lines)
    {
        if (l.startsWith ("plugin=")) g.plugin = l.fromFirstOccurrenceOf ("=", false, false);
        else if (l.startsWith ("barBeats=")) g.barBeats = juce::jmax (0.5, l.fromFirstOccurrenceOf ("=", false, false).getDoubleValue());
        else if (l.startsWith ("length=")) g.lengthBeats = l.fromFirstOccurrenceOf ("=", false, false).getDoubleValue();
        else if (l.containsChar (',') && g.notes.size() < 20000)
        {
            auto f = juce::StringArray::fromTokens (l, ",", "");
            if (f.size() < 4) continue;
            GhostNote n; n.start = f[0].getDoubleValue(); n.length = f[1].getDoubleValue(); n.pitch = juce::jlimit (0, 127, f[2].getIntValue()); n.track = f[3].getIntValue();
            if (std::isfinite (n.start) && std::isfinite (n.length)) g.notes.push_back (n);
        }
    }
    out = std::move (g);
    return true;
}

//------------------------------------------------------------------------------ humanize
/** small random changes to the timing and velocity of the chosen notes (amount 1 light, 2 medium, 3 strong).
    The same notes and amount always give the same result (seeded by the note ids), so Undo / Redo stay predictable. */
inline void humanize (std::vector<Note>& v, const std::set<int>& ids, int amount, double totalBeats, double bpm, uint64_t seed)
{
    const double ms = amount <= 1 ? 8.0 : (amount == 2 ? 16.0 : 28.0);
    const int vel = amount <= 1 ? 6 : (amount == 2 ? 12 : 20);
    const double beatsPerMs = juce::jmax (20.0, bpm) / 60000.0;
    for (auto& n : v)
    {
        if (! ids.count (n.id)) continue;
        juce::Random r ((juce::int64) (seed * 1315423911ULL + (uint64_t) (n.id + 7) * 2654435761ULL));
        const double tri = (r.nextDouble() + r.nextDouble()) - 1.0;           // -1..1, more often near 0 (how a player drifts)
        const double dt = tri * ms * beatsPerMs;
        n.start = juce::jlimit (0.0, juce::jmax (0.0, totalBeats - n.length), n.start + dt);
        n.vel = juce::jlimit (1, 127, n.vel + (int) std::lround (((r.nextDouble() + r.nextDouble()) - 1.0) * vel));
    }
}
} // namespace enochx
