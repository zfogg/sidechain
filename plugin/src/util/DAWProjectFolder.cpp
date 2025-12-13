#include "DAWProjectFolder.h"
#include "Log.h"

namespace DAWProjectFolder
{
    // Internal DAW detection (duplicated from NetworkClient to avoid circular dependency)
    static juce::String detectDAWNameInternal()
    {
        // Try to detect DAW from process name or environment
        // This is platform-specific and may not always work

        #if JUCE_MAC
            // On macOS, try to get the parent process name
            juce::String processName = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                .getParentDirectory().getParentDirectory().getParentDirectory().getFileName();

            // Common DAW identifiers on macOS
            if (processName.containsIgnoreCase("Ableton"))
                return "Ableton Live";
            if (processName.containsIgnoreCase("Logic"))
                return "Logic Pro";
            if (processName.containsIgnoreCase("Pro Tools"))
                return "Pro Tools";
            if (processName.containsIgnoreCase("Cubase"))
                return "Cubase";
            if (processName.containsIgnoreCase("Studio One"))
                return "Studio One";
            if (processName.containsIgnoreCase("Reaper"))
                return "REAPER";
            if (processName.containsIgnoreCase("Bitwig"))
                return "Bitwig Studio";
            if (processName.containsIgnoreCase("FL Studio"))
                return "FL Studio";
        #elif JUCE_WINDOWS
            // On Windows, detection is more limited without process enumeration
        #elif JUCE_LINUX
            // On Linux, similar limitations apply
        #endif

        // Fallback: Try to detect from JUCE plugin wrapper info
        if (auto* app = juce::JUCEApplication::getInstance())
        {
            juce::String hostName = app->getApplicationName();

            if (hostName.isNotEmpty())
            {
                if (hostName.containsIgnoreCase("Ableton"))
                    return "Ableton Live";
                if (hostName.containsIgnoreCase("Logic"))
                    return "Logic Pro";
                if (hostName.containsIgnoreCase("Pro Tools"))
                    return "Pro Tools";
                if (hostName.containsIgnoreCase("Cubase"))
                    return "Cubase";
                if (hostName.containsIgnoreCase("Studio One"))
                    return "Studio One";
                if (hostName.containsIgnoreCase("Reaper"))
                    return "REAPER";
                if (hostName.containsIgnoreCase("Bitwig"))
                    return "Bitwig Studio";
                if (hostName.containsIgnoreCase("FL Studio"))
                    return "FL Studio";
                if (hostName.containsIgnoreCase("Audacity"))
                    return "Audacity";
            }
        }

        // Default fallback
        return "Unknown";
    }

    DAWProjectInfo detectDAWProjectFolder(const juce::String& detectedDAWName)
    {
        DAWProjectInfo info;
        juce::String dawName = detectedDAWName.isEmpty() ? detectDAWNameInternal() : detectedDAWName;
        info.dawName = dawName;

        // Try to detect project folder based on DAW type
        juce::File projectFolder;
        juce::File midiFolder;

        if (dawName.containsIgnoreCase("Ableton"))
        {
            // For Ableton Live, try to detect project folder
            // Project folder structure: Project.als exists in project root
            // MIDI files typically go in: Project Folder / Samples / Imported / MIDI Files
            juce::File currentDir = juce::File::getCurrentWorkingDirectory();
            
            // Check if current directory looks like an Ableton project (has .als file)
            auto alsFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.als");
            if (alsFiles.size() > 0)
            {
                projectFolder = currentDir;
                midiFolder = projectFolder.getChildFile("Samples").getChildFile("Imported").getChildFile("MIDI Files");
            }
            else
            {
                // Try common Ableton project locations
                juce::File userProjects = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("Ableton").getChildFile("User Library");
                
                if (userProjects.exists())
                {
                    // Use a generic location within Ableton user library
                    midiFolder = userProjects.getChildFile("Samples").getChildFile("Imported");
                }
            }
        }
        else if (dawName.containsIgnoreCase("FL Studio"))
        {
            // FL Studio: Project files are .flp, typically in user's FL Studio projects folder
            juce::File currentDir = juce::File::getCurrentWorkingDirectory();
            auto flpFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.flp");
            
            if (flpFiles.size() > 0)
            {
                projectFolder = currentDir;
                midiFolder = projectFolder; // FL Studio projects can have MIDI files in project folder
            }
            else
            {
                // Try FL Studio default locations
                juce::File userData = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("FL Studio");
                
                if (userData.exists())
                {
                    midiFolder = userData.getChildFile("Projects");
                }
            }
        }
        else if (dawName.containsIgnoreCase("Logic"))
        {
            // Logic Pro: Project files are .logicx, MIDI files go in project package
            juce::File currentDir = juce::File::getCurrentWorkingDirectory();
            auto logicFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.logicx");
            
            if (logicFiles.size() > 0)
            {
                projectFolder = currentDir;
                // Logic projects are packages - MIDI files typically in Audio Files or MIDI Files
                midiFolder = projectFolder.getChildFile("MIDI Files");
                if (!midiFolder.exists())
                {
                    midiFolder = projectFolder.getChildFile("Audio Files");
                }
            }
            else
            {
                // Try Logic default locations
                juce::File userMusic = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
                midiFolder = userMusic.getChildFile("Logic");
            }
        }
        else if (dawName.containsIgnoreCase("Reaper"))
        {
            // REAPER: Project files are .rpp, MIDI files in project folder
            juce::File currentDir = juce::File::getCurrentWorkingDirectory();
            auto rppFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.rpp");
            
            if (rppFiles.size() > 0)
            {
                projectFolder = currentDir;
                midiFolder = projectFolder; // REAPER projects often have MIDI in project folder
            }
            else
            {
                // Try REAPER default location
                juce::File userData = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("REAPER Media");
                
                if (userData.exists())
                {
                    midiFolder = userData;
                }
            }
        }
        else if (dawName.containsIgnoreCase("Cubase"))
        {
            // Cubase: Project files are .cpr
            juce::File currentDir = juce::File::getCurrentWorkingDirectory();
            auto cprFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.cpr");
            
            if (cprFiles.size() > 0)
            {
                projectFolder = currentDir;
                midiFolder = projectFolder.getChildFile("Audio");
            }
        }
        else if (dawName.containsIgnoreCase("Studio One"))
        {
            // Studio One: Project files are .song
            juce::File currentDir = juce::File::getCurrentWorkingDirectory();
            auto songFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.song");
            
            if (songFiles.size() > 0)
            {
                projectFolder = currentDir;
                midiFolder = projectFolder;
            }
        }
        else if (dawName.containsIgnoreCase("Pro Tools"))
        {
            // Pro Tools: Session files are .pts
            juce::File currentDir = juce::File::getCurrentWorkingDirectory();
            auto ptsFiles = currentDir.findChildFiles(juce::File::findFiles, false, "*.pts");
            
            if (ptsFiles.size() > 0)
            {
                projectFolder = currentDir;
                midiFolder = projectFolder.getChildFile("MIDI Files");
            }
        }

        // Validate folders
        if (projectFolder.exists() && projectFolder.isDirectory())
        {
            info.projectFolder = projectFolder;
            info.isAccessible = projectFolder.hasWriteAccess();
        }

        // For MIDI folder, use project folder if available, otherwise try detected MIDI folder
        if (midiFolder.exists() && midiFolder.isDirectory() && midiFolder.hasWriteAccess())
        {
            info.midiFolder = midiFolder;
        }
        else if (info.isAccessible && projectFolder.exists())
        {
            // Try creating MIDI folder in project folder
            midiFolder = projectFolder.getChildFile("MIDI Files");
            if (midiFolder.createDirectory().wasOk())
            {
                info.midiFolder = midiFolder;
            }
            else
            {
                info.midiFolder = projectFolder; // Fallback to project folder
            }
        }

        if (!info.isAccessible)
        {
            info.errorMessage = "DAW project folder not accessible or not detected";
        }

        return info;
    }

    juce::File getMIDIFileLocation(const juce::String& detectedDAWName)
    {
        DAWProjectInfo info = detectDAWProjectFolder(detectedDAWName);

        // Try DAW-specific MIDI folder first
        if (info.midiFolder.exists() && info.midiFolder.isDirectory() && info.midiFolder.hasWriteAccess())
        {
            Log::debug("DAWProjectFolder: Using DAW MIDI folder: " + info.midiFolder.getFullPathName());
            return info.midiFolder;
        }

        // Try project folder if accessible
        if (info.isAccessible && info.projectFolder.exists())
        {
            Log::debug("DAWProjectFolder: Using DAW project folder: " + info.projectFolder.getFullPathName());
            return info.projectFolder;
        }

        // Fallback to default location
        juce::File defaultFolder = getDefaultMIDIFolder();
        Log::debug("DAWProjectFolder: Using default MIDI folder: " + defaultFolder.getFullPathName());
        return defaultFolder;
    }

    juce::File getDefaultMIDIFolder()
    {
        juce::File midiFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Sidechain")
            .getChildFile("MIDI");

        // Ensure folder exists
        if (!midiFolder.exists())
        {
            auto result = midiFolder.createDirectory();
            if (result.failed())
            {
                Log::error("DAWProjectFolder: Failed to create default MIDI folder: " + result.getErrorMessage());
            }
        }

        return midiFolder;
    }

    bool isFolderAccessible(const juce::File& folder)
    {
        return folder.exists() && folder.isDirectory() && folder.hasWriteAccess();
    }
}
