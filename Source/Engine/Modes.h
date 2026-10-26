#pragma once
// Scales / modes the editor can show and apply per bar (JUCE-free).
#include "Theory.h"
#include <vector>

namespace enochsolo
{
enum Mode { M_Ionian = 0, M_Dorian, M_Phrygian, M_Lydian, M_Mixolydian, M_Aeolian, M_Locrian, M_MajPent, M_MinPent, M_Blues,
            M_HarmMinor, M_MelMinor, M_PhrygDom, M_LydianDom, M_Altered, M_WholeTone, M_DimHW, M_Count };

inline const char* modeName (int m)
{
    static const char* n[] = { "Ionian (major)", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian (natural minor)", "Locrian",
                               "Major pentatonic", "Minor pentatonic", "Blues", "Harmonic minor", "Melodic minor", "Phrygian dominant",
                               "Lydian dominant", "Altered", "Whole tone", "Diminished (half-whole)" };
    return n[std::clamp (m, 0, (int) M_Count - 1)];
}

inline std::vector<int> modeIntervals (int m)
{
    switch (m)
    {
        case M_Ionian:     return { 0, 2, 4, 5, 7, 9, 11 };
        case M_Dorian:     return { 0, 2, 3, 5, 7, 9, 10 };
        case M_Phrygian:   return { 0, 1, 3, 5, 7, 8, 10 };
        case M_Lydian:     return { 0, 2, 4, 6, 7, 9, 11 };
        case M_Mixolydian: return { 0, 2, 4, 5, 7, 9, 10 };
        case M_Aeolian:    return { 0, 2, 3, 5, 7, 8, 10 };
        case M_Locrian:    return { 0, 1, 3, 5, 6, 8, 10 };
        case M_MajPent:    return { 0, 2, 4, 7, 9 };
        case M_MinPent:    return { 0, 3, 5, 7, 10 };
        case M_Blues:      return { 0, 3, 5, 6, 7, 10 };
        case M_HarmMinor:  return { 0, 2, 3, 5, 7, 8, 11 };
        case M_MelMinor:   return { 0, 2, 3, 5, 7, 9, 11 };
        case M_PhrygDom:   return { 0, 1, 4, 5, 7, 8, 10 };
        case M_LydianDom:  return { 0, 2, 4, 6, 7, 9, 10 };
        case M_Altered:    return { 0, 1, 3, 4, 6, 8, 10 };
        case M_WholeTone:  return { 0, 2, 4, 6, 8, 10 };
        case M_DimHW:      return { 0, 1, 3, 4, 6, 7, 9, 10 };
        default: break;
    }
    return { 0, 2, 4, 5, 7, 9, 11 };
}

inline uint16_t modeMask (int rootPc, int mode)
{
    std::vector<int> pcs; for (int x : modeIntervals (mode)) pcs.push_back (rootPc + x);
    return maskOf (pcs);
}

/** modes that suit a chord, best first (the chord scale the generator uses by default is usually the first) */
inline std::vector<int> suggestModes (Quality q)
{
    switch (q)
    {
        case Quality::Maj7: case Quality::Maj9: case Quality::Maj: case Quality::Maj6: return { M_Ionian, M_Lydian, M_MajPent };
        case Quality::Dom7: case Quality::Dom9: case Quality::Dom13:                    return { M_Mixolydian, M_Blues, M_LydianDom, M_MajPent };
        case Quality::Min7: case Quality::Min9: case Quality::Min11: case Quality::Min: return { M_Dorian, M_Aeolian, M_MinPent, M_Blues };
        case Quality::Min7b5:                                                           return { M_Locrian, M_MinPent };
        case Quality::Dim7:                                                             return { M_DimHW };
        case Quality::Sus4: case Quality::Sus2:                                         return { M_Mixolydian, M_MajPent };
        case Quality::MinMaj7: case Quality::Min6:                                      return { M_MelMinor, M_Dorian, M_HarmMinor };
        case Quality::Aug7:                                                             return { M_WholeTone, M_LydianDom };
        case Quality::Dom7alt:                                                          return { M_Altered, M_WholeTone };
        default: break;
    }
    return { M_Mixolydian };
}
} // namespace enochsolo
