#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <atomic>

/**
    Per-user UI preferences shared by every AtmosRiser instance (currently just the theme).
    Deliberately kept out of the processor: not a parameter, not in presets, undo or the DAW
    project, so changing the look can never touch a session.
    Stored in %APPDATA%\AtmosRiser\ui-settings.xml. Message thread only.
*/
namespace UiSettings
{
    inline juce::PropertiesFile::Options fileOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "ui-settings";
        o.folderName = "AtmosRiser";
        o.filenameSuffix = ".xml";
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return o;
    }

    /** Bumped whenever the theme changes, so open editors can follow. */
    inline std::atomic<int> themeVersion { 0 };

    inline juce::String getTheme()
    {
        juce::PropertiesFile file (fileOptions());
        return file.getValue ("theme") == "dark" ? "dark" : "light";
    }

    inline void setTheme (const juce::String& theme)
    {
        const auto value = theme == "dark" ? juce::String ("dark") : juce::String ("light");
        if (value == getTheme())
            return;

        juce::PropertiesFile file (fileOptions());
        file.setValue ("theme", value);
        file.saveIfNeeded();
        ++themeVersion;
    }
}
