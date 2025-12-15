#pragma once

#include <JuceHeader.h>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Sidechain {
namespace Util {
namespace Error {

/**
 * ErrorTracking - Error aggregation and reporting system
 *
 * Collects, deduplicates, and reports errors for:
 * - Debugging in development
 * - Analytics in production
 * - Alerting on critical errors
 *
 * Usage:
 *   auto tracker = ErrorTracker::getInstance();
 *   tracker->recordError("network", "Failed to connect",
 * ErrorSeverity::Warning, {
 *       {"endpoint", "api.sidechain.io"},
 *       {"statusCode", "500"}
 *   });
 */

/**
 * Error severity levels
 */
enum class ErrorSeverity {
  Info = 0,    // Informational message
  Warning = 1, // Warning, may need attention
  Error = 2,   // Error, operation failed
  Critical = 3 // Critical error, system may be unstable
};

/**
 * Error source categories
 */
enum class ErrorSource { Network, Audio, UI, Database, FileSystem, Authentication, Validation, Internal, Unknown };

/**
 * ErrorInfo - Detailed error information
 */
struct ErrorInfo {
  /**
   * Unique error identifier
   */
  juce::String id;

  /**
   * Error source/category
   */
  ErrorSource source = ErrorSource::Unknown;

  /**
   * Human-readable error message
   */
  juce::String message;

  /**
   * Error severity level
   */
  ErrorSeverity severity = ErrorSeverity::Error;

  /**
   * Stack trace (if available)
   */
  juce::String stackTrace;

  /**
   * Timestamp of when error occurred
   */
  std::chrono::system_clock::time_point timestamp;

  /**
   * Custom context data
   */
  std::map<juce::String, juce::String> context;

  /**
   * How many times this error has occurred
   */
  int occurrenceCount = 1;

  /**
   * Hash for deduplication
   */
  size_t hash = 0;

  /**
   * Convert severity to string
   */
  static juce::String severityToString(ErrorSeverity sev) {
    switch (sev) {
    case ErrorSeverity::Info:
      return "Info";
    case ErrorSeverity::Warning:
      return "Warning";
    case ErrorSeverity::Error:
      return "Error";
    case ErrorSeverity::Critical:
      return "Critical";
    }
    return "Unknown";
  }

  /**
   * Convert source to string
   */
  static juce::String sourceToString(ErrorSource src) {
    switch (src) {
    case ErrorSource::Network:
      return "Network";
    case ErrorSource::Audio:
      return "Audio";
    case ErrorSource::UI:
      return "UI";
    case ErrorSource::Database:
      return "Database";
    case ErrorSource::FileSystem:
      return "FileSystem";
    case ErrorSource::Authentication:
      return "Authentication";
    case ErrorSource::Validation:
      return "Validation";
    case ErrorSource::Internal:
      return "Internal";
    case ErrorSource::Unknown:
      return "Unknown";
    }
    return "Unknown";
  }
};

/**
 * ErrorTracker - Main error tracking singleton
 */
class ErrorTracker : public juce::DeletedAtShutdown {
public:
  JUCE_DECLARE_SINGLETON(ErrorTracker, false)

  /**
   * Record an error
   *
   * @param source Error source category
   * @param message Human-readable error message
   * @param severity Error severity level
   * @param context Additional context data
   */
  void recordError(ErrorSource source, const juce::String &message, ErrorSeverity severity = ErrorSeverity::Error,
                   const std::map<juce::String, juce::String> &context = {});

  /**
   * Record an error with exception information
   *
   * @param source Error source
   * @param exception Exception caught
   * @param context Additional context
   */
  void recordException(ErrorSource source, const std::exception &exception,
                       const std::map<juce::String, juce::String> &context = {});

  /**
   * Get all recorded errors
   */
  std::vector<ErrorInfo> getAllErrors() const;

  /**
   * Get errors by severity
   */
  std::vector<ErrorInfo> getErrorsBySeverity(ErrorSeverity severity) const;

  /**
   * Get errors by source
   */
  std::vector<ErrorInfo> getErrorsBySource(ErrorSource source) const;

  /**
   * Get recent errors (last N minutes)
   */
  std::vector<ErrorInfo> getRecentErrors(int minutesBack = 60) const;

  /**
   * Get error by ID
   */
  std::optional<ErrorInfo> getError(const juce::String &errorId) const;

  /**
   * Clear all errors
   */
  void clear();

  /**
   * Clear errors older than specified time
   */
  void clearOlderThan(int minutesBack);

  /**
   * Get error statistics
   */
  struct ErrorStats {
    int totalErrors = 0;
    int criticalCount = 0;
    int errorCount = 0;
    int warningCount = 0;
    int infoCount = 0;
    std::map<ErrorSource, int> bySource;
    std::map<juce::String, int> topErrors; // Top 10 most frequent
  };

  ErrorStats getStatistics() const;

  /**
   * Set a callback for critical errors
   *
   * Called immediately when a critical error is recorded
   */
  void setOnCriticalError(std::function<void(const ErrorInfo &)> callback) {
    onCriticalError_ = callback;
  }

  /**
   * Set a callback for all errors
   */
  void setOnError(std::function<void(const ErrorInfo &)> callback) {
    onError_ = callback;
  }

  /**
   * Export errors as JSON
   */
  juce::var exportAsJson() const;

  /**
   * Export errors as CSV
   */
  juce::String exportAsCsv() const;

  /**
   * Get number of recorded errors
   */
  size_t getErrorCount() const;

  /**
   * Check if there are unacknowledged critical errors
   */
  bool hasCriticalErrors() const;

  /**
   * Mark all errors as acknowledged
   */
  void acknowledgeAll();

private:
  ErrorTracker();
  ~ErrorTracker();

  mutable std::mutex mutex_;
  std::vector<ErrorInfo> errors_;
  static constexpr size_t MAX_ERRORS = 1000;

  std::function<void(const ErrorInfo &)> onCriticalError_;
  std::function<void(const ErrorInfo &)> onError_;

  size_t computeHash(const ErrorInfo &error) const;
  std::optional<size_t> findDuplicateError(const ErrorInfo &error) const;
};

/**
 * ScopedErrorContext - RAII helper for recording errors with context
 *
 * Usage:
 *   {
 *       ScopedErrorContext ctx("network", {{"endpoint", "api.example.com"}});
 *       // If exception occurs here, context is automatically added
 *       makeNetworkCall();
 *   }
 */
class ScopedErrorContext {
public:
  ScopedErrorContext(const juce::String &operationName, const std::map<juce::String, juce::String> &context = {})
      : operationName_(operationName), context_(context) {}

  ~ScopedErrorContext() {
    // Contexts are used to enrich error information
  }

  /**
   * Add context information
   */
  void addContext(const juce::String &key, const juce::String &value) {
    context_[key] = value;
  }

  /**
   * Record error within this context
   */
  void recordError(const juce::String &message, ErrorSeverity severity = ErrorSeverity::Error) {
    auto tracker = ErrorTracker::getInstance();
    tracker->recordError(ErrorSource::Internal, message, severity, context_);
  }

  /**
   * Get context map
   */
  const std::map<juce::String, juce::String> &getContext() const {
    return context_;
  }

private:
  juce::String operationName_;
  std::map<juce::String, juce::String> context_;
};

/**
 * Convenience macros for error tracking
 */

#define LOG_ERROR(source, message)                                                                                     \
  do {                                                                                                                 \
    auto tracker = Sidechain::Util::Error::ErrorTracker::getInstance();                                                \
    tracker->recordError(source, message, ErrorSeverity::Error);                                                       \
  } while (false)

#define LOG_WARNING(source, message)                                                                                   \
  do {                                                                                                                 \
    auto tracker = Sidechain::Util::Error::ErrorTracker::getInstance();                                                \
    tracker->recordError(source, message, ErrorSeverity::Warning);                                                     \
  } while (false)

#define LOG_CRITICAL(source, message)                                                                                  \
  do {                                                                                                                 \
    auto tracker = Sidechain::Util::Error::ErrorTracker::getInstance();                                                \
    tracker->recordError(source, message, ErrorSeverity::Critical);                                                    \
  } while (false)

} // namespace Error
} // namespace Util
} // namespace Sidechain
