#pragma once

#include <JuceHeader.h>

// ==============================================================================
/**
 * Log - Simple logging utility for the Sidechain plugin
 *
 * Output behavior:
 * - debug/info: stdout
 * - warn/error: stderr
 * - All levels: written to log file
 *
 * Log file location (determined by NDEBUG macro):
 * - Development builds (NDEBUG not defined): ./plugin.log (current working
 * directory)
 * - Release builds (NDEBUG defined): Platform-specific standard log directory
 *   - macOS: ~/Library/Logs/Sidechain/plugin.log
 *   - Linux: ~/.local/share/Sidechain/logs/plugin.log
 *   - Windows: %LOCALAPPDATA%/Sidechain/logs/plugin.log
 *
 * Thread-safe and gracefully handles missing directories or inaccessible files.
 */
namespace Log {
// ==========================================================================
// Log levels
enum class Level { Debug, Info, Warn, Error };

// ==========================================================================
// Core logging functions
void debug(const juce::String &message);
void info(const juce::String &message);
void warn(const juce::String &message);
void error(const juce::String &message);

// Log with explicit level
void log(Level level, const juce::String &message);

// ==========================================================================
// Formatted logging (printf-style via juce::String::formatted)
template <typename... Args> void debugf(const char *format, Args... args) {
  debug(juce::String::formatted(format, args...));
}

template <typename... Args> void infof(const char *format, Args... args) {
  info(juce::String::formatted(format, args...));
}

template <typename... Args> void warnf(const char *format, Args... args) {
  warn(juce::String::formatted(format, args...));
}

template <typename... Args> void errorf(const char *format, Args... args) {
  error(juce::String::formatted(format, args...));
}

// ==========================================================================
// Configuration

// Set minimum log level (messages below this level are ignored)
void setMinLevel(Level level);
Level getMinLevel();

// Enable/disable file logging
void setFileLoggingEnabled(bool enabled);
bool isFileLoggingEnabled();

// Enable/disable console logging
void setConsoleLoggingEnabled(bool enabled);
bool isConsoleLoggingEnabled();

// Get the current log file path (empty if file logging failed)
juce::String getLogFilePath();

// Flush any buffered log entries to file
void flush();

// Shutdown logging - call before application exit to prevent leak warnings
void shutdown();

// ==========================================================================
// Utility

// Get string representation of log level
const char *levelToString(Level level);

// ==========================================================================
// Exception handling

/**
 * Log an exception with context
 *
 * Usage:
 *   Log::logException(error, "PostCard::toggleLike");
 *   // outputs: "PostCard::toggleLike - error message"
 *
 *   Log::logException(error, "PostCard", "Failed to toggle like");
 *   // outputs: "PostCard: Failed to toggle like - error message"
 */
void logException(std::exception_ptr error, const juce::String &context);
void logException(std::exception_ptr error, const juce::String &context, const juce::String &action);

} // namespace Log
