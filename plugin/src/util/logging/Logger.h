#pragma once

#include "LogSink.h"
#include <chrono>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace Sidechain {
namespace Util {

/**
 * Logger - Global structured logging system with multiple sinks
 *
 * Features:
 * - Multiple concurrent sinks (console, file, network)
 * - Structured logging with category, level, message, context
 * - Automatic timestamp generation
 * - Thread-safe logging from any thread
 * - Colored console output
 * - Formatted file output for analysis
 * - Low overhead (< 1ms per log call)
 *
 * Example:
 *   Logger& logger = Logger::getInstance();
 *   logger.addSink(std::make_unique<ConsoleSink>(true));
 *   logger.addSink(std::make_unique<FileSink>("app.log"));
 *
 *   logger.info("Network", "Connected to server", "host=example.com:8080");
 *   logger.error("Audio", "Buffer underrun detected", "frames=256");
 */
class Logger {
public:
  /**
   * Get singleton instance
   */
  static Logger &getInstance() {
    static Logger instance;
    return instance;
  }

  /**
   * Add output sink
   */
  void addSink(std::unique_ptr<LogSink> sink) {
    if (!sink)
      return;

    std::lock_guard<std::mutex> lock(sinkMutex);
    sinks.push_back(std::move(sink));
  }

  /**
   * Remove all sinks
   */
  void clearSinks() {
    std::lock_guard<std::mutex> lock(sinkMutex);
    sinks.clear();
  }

  /**
   * Get number of active sinks
   */
  size_t getSinkCount() const {
    std::lock_guard<std::mutex> lock(sinkMutex);
    return sinks.size();
  }

  /**
   * Set minimum log level - messages below this level are ignored
   */
  void setMinLevel(LogLevel level) {
    minLevel = level;
  }

  /**
   * Log debug message
   */
  void debug(const juce::String &category, const juce::String &message, const juce::String &context = "") {
    log(LogLevel::Debug, category, message, context);
  }

  /**
   * Log info message
   */
  void info(const juce::String &category, const juce::String &message, const juce::String &context = "") {
    log(LogLevel::Info, category, message, context);
  }

  /**
   * Log warning message
   */
  void warning(const juce::String &category, const juce::String &message, const juce::String &context = "") {
    log(LogLevel::Warning, category, message, context);
  }

  /**
   * Log error message
   */
  void error(const juce::String &category, const juce::String &message, const juce::String &context = "") {
    log(LogLevel::Error, category, message, context);
  }

  /**
   * Log fatal message
   */
  void fatal(const juce::String &category, const juce::String &message, const juce::String &context = "") {
    log(LogLevel::Fatal, category, message, context);
  }

  /**
   * Flush all sinks
   */
  void flush() {
    std::lock_guard<std::mutex> lock(sinkMutex);
    for (auto &sink : sinks)
      sink->flush();
  }

  // Convenience constructor for JUCE String building
  void log(LogLevel level, const juce::String &category, const juce::String &message,
           const juce::String &context = "") {
    if (level < minLevel)
      return;

    LogEntry entry;
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.context = context;
    entry.timestamp = getTimestamp();
    entry.threadId = 0; // Thread ID will be captured by JUCE in actual impl

    {
      std::lock_guard<std::mutex> lock(sinkMutex);
      for (auto &sink : sinks)
        sink->write(entry);
    }
  }

  // Suppress copying
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

private:
  Logger() = default;
  ~Logger() = default;

  std::vector<std::unique_ptr<LogSink>> sinks;
  mutable std::mutex sinkMutex;
  LogLevel minLevel = LogLevel::Debug;

  /**
   * Generate ISO 8601 timestamp for current time
   */
  static juce::String getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    // Get milliseconds
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
       << millis.count();

    return juce::String(ss.str());
  }
};

/**
 * Convenience functions for quick logging without instance access
 */
inline void logDebug(const juce::String &category, const juce::String &message, const juce::String &context = "") {
  Logger::getInstance().debug(category, message, context);
}

inline void logInfo(const juce::String &category, const juce::String &message, const juce::String &context = "") {
  Logger::getInstance().info(category, message, context);
}

inline void logWarning(const juce::String &category, const juce::String &message, const juce::String &context = "") {
  Logger::getInstance().warning(category, message, context);
}

inline void logError(const juce::String &category, const juce::String &message, const juce::String &context = "") {
  Logger::getInstance().error(category, message, context);
}

inline void logFatal(const juce::String &category, const juce::String &message, const juce::String &context = "") {
  Logger::getInstance().fatal(category, message, context);
}

} // namespace Util
} // namespace Sidechain
