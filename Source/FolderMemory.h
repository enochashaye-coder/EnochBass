#pragma once
// Remembers the last folder used by each file picker (one-shots, SoundFonts, recordings, MIDI export), across sessions and projects,
// so nobody has to dig through file paths every time. Stored in %APPDATA%\<product>\settings.xml.
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace enochsolo
{
inline juce::PropertiesFile& folderSettings()
{
    static std::unique_ptr<juce::PropertiesFile> pf = []
    {
        juce::PropertiesFile::Options o;
        o.applicationName = JucePlugin_Name; o.filenameSuffix = ".settings"; o.osxLibrarySubFolder = "Application Support";
        o.folderName = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (JucePlugin_Name).getFullPathName();
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return std::make_unique<juce::PropertiesFile> (o);
    }();
    return *pf;
}

/** the folder this picker was last used in, or the fallback the first time */
inline juce::File lastFolder (const char* key, const juce::File& fallback)
{
    const juce::String p = folderSettings().getValue (juce::String ("dir_") + key);
    const juce::File f (p);
    return (p.isNotEmpty() && f.isDirectory()) ? f : fallback;
}

/** call with the file (or folder) the person picked */
inline void rememberFolder (const char* key, const juce::File& picked)
{
    const juce::File dir = picked.isDirectory() ? picked : picked.getParentDirectory();
    if (! dir.isDirectory()) return;
    folderSettings().setValue (juce::String ("dir_") + key, dir.getFullPathName());
    folderSettings().saveIfNeeded();
}
}
