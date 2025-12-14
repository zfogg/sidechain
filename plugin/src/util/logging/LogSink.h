#pragma once

#include <JuceHeader.h>
#include <string>
#include <memory>
#include <fstream>
#include <ctime>

namespace Sidechain {
namespace Util {

/**
 * LogLevel - Severity levels for logging
 */
enum class LogLevel
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4
};

/**
 * LogEntry - Structured log entry with all context
 */
struct LogEntry
{
    LogLevel level;
    juce::String category;      // e.g. "Network", "Audio", "UI"
    juce::String message;       // Log message
    juce::String context;       // Additional context (file:line, thread info, etc)
    juce::String timestamp;     // ISO 8601 timestamp
    juce::uint64 threadId;      // Thread ID that generated the log
};

/**
 * LogSink - Abstract base class for log output destinations
 *
 * Implementations can write to:
 * - Console (stdout/stderr)
 * - Files (with rotation)
 * - Network endpoints
 * - Remote logging services
 */
class LogSink
{
public:
    virtual ~LogSink() = default;

    /**
     * Write log entry to this sink
     * @param entry The structured log entry
     */
    virtual void write(const LogEntry& entry) = 0;

    /**
     * Flush any buffered output
     */
    virtual void flush() { }

    /**
     * Get name of this sink for identification
     */
    virtual juce::String getName() const = 0;

protected:
    /**
     * Format log level as string
     */
    static juce::String levelToString(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error:   return "ERROR";
            case LogLevel::Fatal:   return "FATAL";
            default:                return "UNKNOWN";
        }
    }

    /**
     * Get ANSI color code for log level
     */
    static const char* getColorCode(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::Debug:   return "\033[36m";  // Cyan
            case LogLevel::Info:    return "\033[32m";  // Green
            case LogLevel::Warning: return "\033[33m";  // Yellow
            case LogLevel::Error:   return "\033[31m";  // Red
            case LogLevel::Fatal:   return "\033[35m";  // Magenta
            default:                return "\033[0m";   // Reset
        }
    }

    static const char* getResetCode() { return "\033[0m"; }
};

/**
 * ConsoleSink - Output to stdout/stderr with optional colors
 */
class ConsoleSink : public LogSink
{
public:
    explicit ConsoleSink(bool useColors = true)
        : colored(useColors)
    {
    }

    void write(const LogEntry& entry) override
    {
        juce::String level = levelToString(entry.level);

        // Build formatted message
        juce::String formatted;
        if (colored)
            formatted << getColorCode(entry.level);

        formatted << "[" << entry.timestamp << "] "
                  << "[" << level << "] "
                  << "[" << entry.category << "] "
                  << entry.message;

        if (!entry.context.isEmpty())
            formatted << " (" << entry.context << ")";

        if (colored)
            formatted << getResetCode();

        // Write to appropriate stream
        auto& stream = (entry.level >= LogLevel::Error) ? std::cerr : std::cout;
        stream << formatted.toStdString() << "\n";
        stream.flush();
    }

    juce::String getName() const override { return "ConsoleSink"; }

private:
    bool colored;
};

/**
 * FileSink - Output to log file with optional rotation
 */
class FileSink : public LogSink
{
public:
    /**
     * Create file sink
     * @param path Log file path
     * @param maxSizeKB Max file size before rotation (0 = no limit)
     * @param maxBackups Number of backup files to keep
     */
    explicit FileSink(const juce::String& path, size_t maxSizeKB = 10240, int maxBackups = 5)
        : logPath(path), maxSize(maxSizeKB * 1024), maxBackupFiles(maxBackups)
    {
        openFile();
    }

    ~FileSink() override
    {
        if (logFile.is_open())
            logFile.close();
    }

    void write(const LogEntry& entry) override
    {
        if (!logFile.is_open())
            openFile();

        if (!logFile.is_open())
            return;

        juce::String level = levelToString(entry.level);

        logFile << "[" << entry.timestamp.toStdString() << "] "
                << "[" << level.toStdString() << "] "
                << "[" << entry.category.toStdString() << "] "
                << entry.message.toStdString();

        if (!entry.context.isEmpty())
            logFile << " (" << entry.context.toStdString() << ")";

        logFile << "\n";
        logFile.flush();

        // Check if we need to rotate
        checkRotate();
    }

    void flush() override
    {
        if (logFile.is_open())
            logFile.flush();
    }

    juce::String getName() const override { return "FileSink"; }

private:
    juce::String logPath;
    std::ofstream logFile;
    size_t maxSize;
    int maxBackupFiles;

    void openFile()
    {
        logFile.open(logPath.toStdString(), std::ios::app);
    }

    void checkRotate()
    {
        if (maxSize == 0)
            return;

        logFile.flush();
        if (logFile.tellp() > static_cast<std::streampos>(maxSize))
            rotateFile();
    }

    void rotateFile()
    {
        logFile.close();

        // Rotate backup files
        for (int i = maxBackupFiles - 1; i >= 1; --i)
        {
            juce::String oldName = logPath + juce::String(".") + juce::String(i);
            juce::String newName = logPath + juce::String(".") + juce::String(i + 1);
            std::rename(oldName.toStdString().c_str(), newName.toStdString().c_str());
        }

        // Move current file to .1
        juce::String backupName = logPath + ".1";
        std::rename(logPath.toStdString().c_str(), backupName.toStdString().c_str());

        // Open new log file
        openFile();
    }
};

}  // namespace Util
}  // namespace Sidechain
