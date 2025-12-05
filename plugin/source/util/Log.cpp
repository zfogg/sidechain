#include "Log.h"
#include <iostream>
#include <mutex>

namespace Log
{

//==============================================================================
// Internal state
namespace
{
    std::mutex logMutex;
    std::unique_ptr<juce::FileOutputStream> fileStream;
    juce::File logFile;
    bool fileLoggingEnabled = true;
    bool consoleLoggingEnabled = true;
    bool initialized = false;
    bool initializationAttempted = false;

#ifdef NDEBUG
    Level minLevel = Level::Info;  // Release: Info and above
#else
    Level minLevel = Level::Debug; // Debug: All levels
#endif

    //==========================================================================
    // Get log directory based on NDEBUG macro
    // - When NDEBUG is defined (Release builds): Use platform-specific standard log directories
    // - When NDEBUG is not defined (development): Use current working directory
    juce::File getLogDirectory()
    {
#ifdef NDEBUG
        // Release builds (NDEBUG defined): Use platform-specific standard log directories
#if JUCE_MAC
        // macOS: ~/Library/Logs/Sidechain/
        auto logsDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                           .getChildFile("Library")
                           .getChildFile("Logs")
                           .getChildFile("Sidechain");
#elif JUCE_WINDOWS
        // Windows: %LOCALAPPDATA%/Sidechain/logs/
        auto logsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile("Sidechain")
                           .getChildFile("logs");
#else
        // Linux: ~/.local/share/Sidechain/logs/
        auto logsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile("Sidechain")
                           .getChildFile("logs");
#endif
        return logsDir;
#else
        // Development builds (NDEBUG not defined): Current working directory
        return juce::File::getCurrentWorkingDirectory();
#endif
    }

    //==========================================================================
    // Initialize file logging (lazy, thread-safe)
    void initializeFileLogging()
    {
        if (initializationAttempted)
            return;

        initializationAttempted = true;

        try
        {
            auto logDir = getLogDirectory();

            // Try to create directory if it doesn't exist
            if (!logDir.exists())
            {
                auto result = logDir.createDirectory();
                if (result.failed())
                {
                    // Can't create directory - disable file logging silently
                    fileLoggingEnabled = false;
                    return;
                }
            }

            // Check if directory is writable
            if (!logDir.hasWriteAccess())
            {
                fileLoggingEnabled = false;
                return;
            }

            logFile = logDir.getChildFile("plugin.log");

            // Try to open file for appending
            fileStream = std::make_unique<juce::FileOutputStream>(logFile);

            if (fileStream->failedToOpen())
            {
                fileStream.reset();
                fileLoggingEnabled = false;
                return;
            }

            initialized = true;

            // Write session header
            auto now = juce::Time::getCurrentTime();
            juce::String header = "\n";
            header += "================================================================================\n";
            header += "Sidechain Log Session Started: " + now.toString(true, true, true, true) + "\n";
            header += "================================================================================\n";
            fileStream->writeText(header, false, false, nullptr);
            fileStream->flush();
        }
        catch (...)
        {
            // Any exception - disable file logging silently
            fileStream.reset();
            fileLoggingEnabled = false;
        }
    }

    //==========================================================================
    // Format timestamp
    juce::String getTimestamp()
    {
        auto now = juce::Time::getCurrentTime();
        return now.formatted("%Y-%m-%d %H:%M:%S");
    }

    //==========================================================================
    // Format log entry
    juce::String formatLogEntry(Level level, const juce::String& message)
    {
        return "[" + getTimestamp() + "] [" + levelToString(level) + "] " + message;
    }

    //==========================================================================
    // Write to console
    void writeToConsole(Level level, const juce::String& formattedMessage)
    {
        if (!consoleLoggingEnabled)
            return;

        // Use stderr for warnings and errors, stdout for others
        if (level == Level::Warn || level == Level::Error)
        {
            std::cerr << formattedMessage.toStdString() << std::endl;
        }
        else
        {
            std::cout << formattedMessage.toStdString() << std::endl;
        }
    }

    //==========================================================================
    // Write to file
    void writeToFile(const juce::String& formattedMessage)
    {
        if (!fileLoggingEnabled)
            return;

        if (!initialized)
            initializeFileLogging();

        if (!initialized || !fileStream)
            return;

        try
        {
            fileStream->writeText(formattedMessage + "\n", false, false, nullptr);
            // Flush on each write to ensure logs are saved even on crash
            fileStream->flush();
        }
        catch (...)
        {
            // If writing fails, disable file logging
            fileStream.reset();
            fileLoggingEnabled = false;
            initialized = false;
        }
    }

}  // anonymous namespace

//==============================================================================
// Public API implementation

const char* levelToString(Level level)
{
    switch (level)
    {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
        default:           return "?????";
    }
}

void log(Level level, const juce::String& message)
{
    // Check minimum level
    if (static_cast<int>(level) < static_cast<int>(minLevel))
        return;

    std::lock_guard<std::mutex> lock(logMutex);

    auto formattedMessage = formatLogEntry(level, message);

    writeToConsole(level, formattedMessage);
    writeToFile(formattedMessage);
}

void debug(const juce::String& message)
{
    log(Level::Debug, message);
}

void info(const juce::String& message)
{
    log(Level::Info, message);
}

void warn(const juce::String& message)
{
    log(Level::Warn, message);
}

void error(const juce::String& message)
{
    log(Level::Error, message);
}

void setMinLevel(Level level)
{
    std::lock_guard<std::mutex> lock(logMutex);
    minLevel = level;
}

Level getMinLevel()
{
    std::lock_guard<std::mutex> lock(logMutex);
    return minLevel;
}

void setFileLoggingEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(logMutex);
    fileLoggingEnabled = enabled;
}

bool isFileLoggingEnabled()
{
    std::lock_guard<std::mutex> lock(logMutex);
    return fileLoggingEnabled;
}

void setConsoleLoggingEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(logMutex);
    consoleLoggingEnabled = enabled;
}

bool isConsoleLoggingEnabled()
{
    std::lock_guard<std::mutex> lock(logMutex);
    return consoleLoggingEnabled;
}

juce::String getLogFilePath()
{
    std::lock_guard<std::mutex> lock(logMutex);

    if (!initialized && !initializationAttempted)
        initializeFileLogging();

    if (initialized && logFile.exists())
        return logFile.getFullPathName();

    return {};
}

void flush()
{
    std::lock_guard<std::mutex> lock(logMutex);

    if (fileStream)
    {
        try
        {
            fileStream->flush();
        }
        catch (...)
        {
            // Ignore flush errors
        }
    }
}

}  // namespace Log
