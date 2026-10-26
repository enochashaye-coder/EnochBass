#include <cstdio>
#include "../Source/Engine/MidiWriter.h"
using namespace enochsolo;
int main (int argc, char** argv)
{
    int inst = argc > 1 ? atoi (argv[1]) : I_Sax, key = argc > 2 ? atoi (argv[2]) : 9, genre = argc > 3 ? atoi (argv[3]) : 0, seed = argc > 4 ? atoi (argv[4]) : 3, artist = argc > 5 ? atoi (argv[5]) : 0;
    Settings s; s.instrument = inst; s.artist = artist; s.genre = genre; s.keyRoot = key; s.scale = SC_Minor; s.bars = 8; s.seed = (uint64_t) seed; s.humanize = false;
    Pattern p = Generator::generate (s);
    std::printf ("%s | %s | %s | key %s | prog: %s\n", instName (inst), p.styleName.c_str(), p.genreName.c_str(), kNoteNames[key], p.progName.c_str());
    for (auto& c : p.chords) std::printf ("[%s %.0f-%.0f] ", c.name.c_str(), c.start, c.end);
    std::printf ("\n");
    int bar = -1;
    for (auto& n : p.notes)
    {
        if (n.track || n.voice) continue;
        const int b = (int) (n.start / p.barBeats);
        if (b != bar) { bar = b; std::printf ("\n bar %d: ", b + 1); }
        std::printf ("%s(%.2f,%.2f,%s) ", noteName (n.pitch).c_str(), n.start - b * p.barBeats, n.length, artName (n.art));
    }
    std::printf ("\n");
}
