// Builds the starter MIDI library: one folder per genre, sparse / medium / hot takes of an intro, a verse, a solo and an outro.
// g++ -O2 -std=c++17 -I Source/Engine -I Source Tools/makelib.cpp -o makelib && ./makelib "Examples/Starter Library"
// (the same file builds the Enoch Bass library when compiled in the Enoch Bass folder)
#include "Generator.h"
#if __has_include("BassGen.h")
 #include "BassGen.h"
#endif
#include "MidiWriter.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace enochsolo;

static double tempoFor (int g)
{
    switch (g)
    {
        case G_Bebop: return 220; case G_HardBop: return 150; case G_ModalJazz: return 140; case G_CoolJazz: return 130; case G_LatinJazz: return 120;
        case G_JazzFunk: return 105; case G_SmoothJazz: return 90; case G_Kizomba: return 88; case G_Zouk: return 100;
        case G_PrivateSchool: return 112; case G_Bacardi: return 116; case G_Sgija: return 110;
        default: break;
    }
    switch (baseGenre (g))
    {
        case G_Afrobeat: return 110; case G_AfroJazz: return 120; case G_Highlife: return 122; case G_AfroFusion: return 100; case G_Swing: return 160;
        case G_Ballad: return 70; case G_Amapiano: return 112; case G_Juju: return 105; case G_Soukous: return 132; case G_EthioJazz: return 95;
        case G_Township: return 120; case G_Gospel: return 85; case G_Pop: return 100; case G_Rock: return 120; case G_Bossa: return 130;
        case G_Samba: return 100; case G_Flamenco: return 120;
        default: return 105;
    }
}
static bool majorGenre (int g)
{
    const int b = baseGenre (g);
    return b == G_Highlife || b == G_Pop || b == G_Bossa || b == G_Gospel || g == G_CoolJazz || g == G_SmoothJazz || g == G_Zouk || b == G_Township;
}
static std::string clean (std::string s)
{
    for (size_t k; (k = s.find (": ")) != std::string::npos;) s.replace (k, 2, " - ");
    for (auto& c : s) if (c == '/' || c == ':' || c == '\\' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '-';
    while (s.find ("  ") != std::string::npos) s.erase (s.find ("  "), 1);
    return s;
}

// v3.2: each take is played by the Session Player Profile that suits the genre (its "best with" list), with the genre's timing feel
static int sessionArtistFor (int inst, int g)
{
    const auto idx = stylesFor (inst);
    const std::string gname = genreInfo (g).name, bname = genreInfo (baseGenre (g)).name;
    int any = -1;
    for (size_t k = 0; k < idx.size(); ++k)
    {
        const auto& st = allStyles()[(size_t) idx[k]];
        if (st.cat != kSessionCat) continue;
        if (any < 0) any = (int) k;
        if (st.fav.find (gname) != std::string::npos) return (int) k;
    }
    for (size_t k = 0; k < idx.size(); ++k)
    {
        const auto& st = allStyles()[(size_t) idx[k]];
        if (st.cat == kSessionCat && st.fav.find (bname) != std::string::npos) return (int) k;
    }
    return any >= 0 ? any : 0;
}
static int feelFor (int g)
{
    if (g == G_Amapiano || g == G_PrivateSchool || g == G_Bacardi || g == G_Sgija) return 5;
    if (g == G_SmoothJazz) return 7;
    switch (baseGenre (g)) { case G_AfroFusion: case G_Afrobeat: case G_Highlife: case G_Juju: return 4; case G_Gospel: return 6; case G_Swing: return 8; case G_Rock: case G_Pop: return 1; default: return 0; }
}

int main (int argc, char** argv)
{
    namespace fs = std::filesystem;
    const fs::path root = argc > 1 ? argv[1] : "Starter Library";
#ifdef I_BASS_ONLY
    const std::vector<int> insts = { I_Bass };
#else
    std::vector<int> insts; for (int i = 0; i < I_Count; ++i) insts.push_back (i);
#endif
    struct Heat { const char* name; float busy, intensity, space; int subdiv; };
    const Heat heats[] = { { "Sparse", 0.22f, 0.35f, 0.75f, 0 }, { "Medium", 0.50f, 0.60f, 0.50f, 0 }, { "Hot", 0.85f, 0.90f, 0.25f, 3 } };
    struct Part { const char* name; int bars; float intensityMul, busyAdd; };
    const Part parts[] = { { "1 Intro", 4, 0.7f, -0.15f }, { "2 Verse", 8, 0.85f, -0.05f }, { "3 Solo", 16, 1.0f, 0.1f }, { "4 Outro", 4, 0.75f, -0.2f } };
    int files = 0;
    for (int g = 0; g < G_Count; ++g)
    {
        const std::string gname = clean (genreInfo (g).name);
        for (int inst : insts)
            for (auto& h : heats)
                for (auto& pt : parts)
                {
                    Settings s;
                    s.instrument = inst; s.artist = sessionArtistFor (inst, g); s.genre = g; s.feel = feelFor (g);
                    s.keyRoot = majorGenre (g) ? 0 : 9; s.scale = majorGenre (g) ? SC_Major : SC_Minor;
                    s.bars = pt.bars; s.harmony = 1;                                      // the genre's own progressions
                    s.busy = std::clamp (h.busy + pt.busyAdd, 0.05f, 1.f);
                    s.intensity = std::clamp (h.intensity * pt.intensityMul, 0.1f, 1.f);
                    s.space = h.space;
                    s.subdiv = (baseGenre (g) == G_Ballad || g == G_Kizomba) ? 0 : (std::string (pt.name) == "3 Solo" ? h.subdiv : 0);
                    s.exportMode = 1;                                                     // no key-switches: plays on any instrument
                    s.bpm = tempoFor (g);
                    s.seed = (uint64_t) (1000003u * (unsigned) (g + 1) + 7919u * (unsigned) (inst + 1) + 131u * (unsigned) (&h - heats) + (unsigned) (&pt - parts));
                    const Pattern p = Generator::generate (s);
                    fs::path dir = root / gname;
#ifndef I_BASS_ONLY
                    dir /= instName (inst);
#endif
                    fs::create_directories (dir);
                    const std::string file = gname + " - " +
#ifndef I_BASS_ONLY
                        std::string (instName (inst)) + " - " +
#endif
                        pt.name + " - " + h.name + " (" + std::to_string ((int) s.bpm) + " bpm, " + (majorGenre (g) ? "C major" : "A minor") + ").mid";
                    const auto bytes = MidiWriter::build (p);
                    std::ofstream o (dir / file, std::ios::binary);
                    o.write ((const char*) bytes.data(), (std::streamsize) bytes.size());
                    ++files;
                }
    }
    std::printf ("%d MIDI files in %d genres\n", files, (int) G_Count);
}
