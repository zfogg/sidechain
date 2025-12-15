#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <vector>

namespace Sidechain {
namespace Util {
namespace Profiling {

/**
 * PerformanceMetrics - Statistics for a measured metric
 */
struct PerformanceMetrics {
  double minMs = 0.0;
  double maxMs = 0.0;
  double avgMs = 0.0;
  double medianMs = 0.0;
  double p95Ms = 0.0; // 95th percentile
  double p99Ms = 0.0; // 99th percentile
  int sampleCount = 0;
  int slowCount = 0; // Count of measurements exceeding slowThreshold
  double slowThresholdMs = 0.0;

  /**
   * Check if metric exceeded slow threshold
   */
  bool isSlow() const {
    return slowCount > 0;
  }

  /**
   * Get percentage of slow measurements
   */
  float getSlowPercentage() const {
    if (sampleCount == 0)
      return 0.0f;
    return 100.0f * static_cast<float>(slowCount) / static_cast<float>(sampleCount);
  }
};

/**
 * ScopedTimer - Automatically measures time in a scope
 *
 * Usage:
 *   {
 *       ScopedTimer timer("networkRequest", 1000.0);  // Warn if > 1000ms
 *       performNetworkRequest();
 *   } // Automatically recorded
 */
class ScopedTimer {
public:
  ScopedTimer(const juce::String &name, double slowThresholdMs = 0.0);
  ~ScopedTimer();

  /**
   * Explicitly stop timing (called automatically in destructor)
   */
  void stop();

  /**
   * Get elapsed time in milliseconds
   */
  double getElapsedMs() const;

  // Callback for recording measurements (initialized by PerformanceMonitor)
  // Made public so PerformanceMonitor can initialize it
  static std::function<void(const juce::String &, double, double)> recordCallback_;

private:
  juce::String name_;
  std::chrono::high_resolution_clock::time_point startTime_;
  double slowThresholdMs_;
  bool hasStopped_ = false;

  friend class PerformanceMonitor;
};

/**
 * PerformanceMonitor - Track and monitor performance metrics
 *
 * Features:
 * - Low-overhead timing with minimal allocations
 * - Per-metric statistics (min, max, avg, percentiles)
 * - Slow operation detection and warnings
 * - Thread-safe measurement recording
 * - Auto-warning on slow operations
 *
 * Usage:
 *   auto monitor = PerformanceMonitor::getInstance();
 *   {
 *       ScopedTimer timer("audioProcessing", 10.0);  // Warn if > 10ms
 *       processAudio();
 *   }
 *
 *   auto metrics = monitor->getMetrics("audioProcessing");
 *   std::cout << "Avg: " << metrics.avgMs << "ms\n";
 */
class PerformanceMonitor : public juce::DeletedAtShutdown {
public:
  JUCE_DECLARE_SINGLETON(PerformanceMonitor, false)

  ~PerformanceMonitor() {
    clearSingletonInstance();
  }

  /**
   * Record a measurement for a named metric
   *
   * @param metricName Metric name
   * @param durationMs Duration in milliseconds
   * @param slowThresholdMs Threshold for slow warning (0 = no threshold)
   */
  void record(const juce::String &metricName, double durationMs, double slowThresholdMs = 0.0);

  /**
   * Get metrics for a specific measurement
   *
   * @param metricName Metric name
   * @return PerformanceMetrics for this metric
   */
  PerformanceMetrics getMetrics(const juce::String &metricName) const;

  /**
   * Get all metric names
   */
  std::vector<juce::String> getMetricNames() const;

  /**
   * Get all metrics
   */
  std::map<juce::String, PerformanceMetrics> getAllMetrics() const;

  /**
   * Reset metrics for a name
   */
  void reset(const juce::String &metricName);

  /**
   * Reset all metrics
   */
  void resetAll();

  /**
   * Set slow operation threshold globally
   *
   * @param thresholdMs Operations exceeding this time trigger warnings
   */
  void setSlowThreshold(double thresholdMs) {
    globalSlowThreshold_ = thresholdMs;
  }

  /**
   * Set slow operation threshold for specific metric
   */
  void setSlowThreshold(const juce::String &metricName, double thresholdMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    slowThresholds_[metricName] = thresholdMs;
  }

  /**
   * Set callback for slow operations
   *
   * Called when a measurement exceeds its slow threshold
   */
  void setSlowOperationCallback(std::function<void(const juce::String &, double)> callback) {
    slowOperationCallback_ = callback;
  }

  /**
   * Get memory overhead (approximate bytes used)
   */
  size_t getMemoryOverhead() const;

  /**
   * Dump metrics to log
   */
  void dumpMetrics() const;

private:
  struct Measurement {
    std::vector<double> durations; // In milliseconds
    double slowThreshold = 0.0;
    int warningCount = 0;
  };

  mutable std::mutex mutex_;
  std::map<juce::String, Measurement> measurements_;
  std::map<juce::String, double> slowThresholds_;
  double globalSlowThreshold_ = 0.0;
  std::function<void(const juce::String &, double)> slowOperationCallback_;

  /**
   * Calculate metrics from measurements
   */
  PerformanceMetrics calculateMetrics(const std::vector<double> &durations, double slowThreshold) const;

  /**
   * Get percentile value from sorted vector
   */
  double getPercentile(const std::vector<double> &sortedValues, double percentile) const;

  friend class ScopedTimer;
};

// ========== Inline Implementations ==========

// recordCallback_ declaration moved to class public section above (inline for
// header-only, C++17+)
inline std::function<void(const juce::String &, double, double)> ScopedTimer::recordCallback_ = nullptr;

inline ScopedTimer::ScopedTimer(const juce::String &name, double slowThresholdMs)
    : name_(name), startTime_(std::chrono::high_resolution_clock::now()), slowThresholdMs_(slowThresholdMs) {}

inline ScopedTimer::~ScopedTimer() {
  if (!hasStopped_)
    stop();
}

inline void ScopedTimer::stop() {
  if (hasStopped_)
    return;

  hasStopped_ = true;
  auto elapsed = getElapsedMs();

  // Record via callback
  if (recordCallback_)
    recordCallback_(name_, elapsed, slowThresholdMs_);
}

inline double ScopedTimer::getElapsedMs() const {
  auto now = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime_);
  return static_cast<double>(duration.count()) / 1000.0; // Convert to ms
}

// ========== PerformanceMonitor Implementation ==========

inline JUCE_IMPLEMENT_SINGLETON(PerformanceMonitor)

    // Initialize ScopedTimer callback to wire measurements to
    // PerformanceMonitor
    namespace {
  class PerformanceMonitorCallbackInitializer {
  public:
    PerformanceMonitorCallbackInitializer() {
      // Wire ScopedTimer's callback to PerformanceMonitor's record method
      ScopedTimer::recordCallback_ = [](const juce::String &metricName, double ms, double threshold) {
        PerformanceMonitor::getInstance()->record(metricName, ms, threshold);
      };
    }
  };

  // Create single instance to initialize callback on first load
  static PerformanceMonitorCallbackInitializer callbackInitializer;
}

inline void PerformanceMonitor::record(const juce::String &metricName, double durationMs, double slowThresholdMs) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto &measurement = measurements_[metricName];
  measurement.durations.push_back(durationMs);

  // Update slow threshold
  if (slowThresholdMs > 0.0)
    measurement.slowThreshold = slowThresholdMs;
  else if (slowThresholds_.count(metricName))
    measurement.slowThreshold = slowThresholds_[metricName];
  else if (globalSlowThreshold_ > 0.0)
    measurement.slowThreshold = globalSlowThreshold_;

  // Check if slow
  if (measurement.slowThreshold > 0.0 && durationMs > measurement.slowThreshold) {
    measurement.warningCount++;

    if (slowOperationCallback_)
      slowOperationCallback_(metricName, durationMs);
  }

  // Keep only last 1000 measurements to limit memory
  if (measurement.durations.size() > 1000)
    measurement.durations.erase(measurement.durations.begin());
}

inline PerformanceMetrics PerformanceMonitor::getMetrics(const juce::String &metricName) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = measurements_.find(metricName);
  if (it == measurements_.end())
    return PerformanceMetrics();

  return calculateMetrics(it->second.durations, it->second.slowThreshold);
}

inline std::vector<juce::String> PerformanceMonitor::getMetricNames() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<juce::String> names;
  for (const auto &pair : measurements_)
    names.push_back(pair.first);
  return names;
}

inline std::map<juce::String, PerformanceMetrics> PerformanceMonitor::getAllMetrics() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::map<juce::String, PerformanceMetrics> result;
  for (const auto &[metricName, measurement] : measurements_)
    result[metricName] = calculateMetrics(measurement.durations, measurement.slowThreshold);
  return result;
}

inline void PerformanceMonitor::reset(const juce::String &metricName) {
  std::lock_guard<std::mutex> lock(mutex_);
  measurements_.erase(metricName);
}

inline void PerformanceMonitor::resetAll() {
  std::lock_guard<std::mutex> lock(mutex_);
  measurements_.clear();
}

inline PerformanceMetrics PerformanceMonitor::calculateMetrics(const std::vector<double> &durations,
                                                               double slowThreshold) const {
  PerformanceMetrics metrics;

  if (durations.empty())
    return metrics;

  metrics.sampleCount = static_cast<int>(durations.size());
  metrics.slowThresholdMs = slowThreshold;

  // Min and max
  metrics.minMs = *std::min_element(durations.begin(), durations.end());
  metrics.maxMs = *std::max_element(durations.begin(), durations.end());

  // Average
  metrics.avgMs = std::accumulate(durations.begin(), durations.end(), 0.0) / static_cast<double>(durations.size());

  // Sorted for percentiles
  std::vector<double> sorted = durations;
  std::sort(sorted.begin(), sorted.end());

  metrics.medianMs = getPercentile(sorted, 50.0);
  metrics.p95Ms = getPercentile(sorted, 95.0);
  metrics.p99Ms = getPercentile(sorted, 99.0);

  // Count slow
  if (slowThreshold > 0.0) {
    metrics.slowCount = static_cast<int>(
        std::count_if(durations.begin(), durations.end(), [slowThreshold](double d) { return d > slowThreshold; }));
  }

  return metrics;
}

inline double PerformanceMonitor::getPercentile(const std::vector<double> &sortedValues, double percentile) const {
  if (sortedValues.empty())
    return 0.0;

  size_t index = static_cast<size_t>((percentile / 100.0) * static_cast<double>(sortedValues.size()));
  if (index >= sortedValues.size())
    index = sortedValues.size() - 1;

  return sortedValues[index];
}

inline size_t PerformanceMonitor::getMemoryOverhead() const {
  std::lock_guard<std::mutex> lock(mutex_);

  size_t total = 0;
  for (const auto &[metricName, measurement] : measurements_) {
    total += static_cast<size_t>(metricName.length());
    total += measurement.durations.size() * sizeof(double);
  }
  return total;
}

inline void PerformanceMonitor::dumpMetrics() const {
  std::lock_guard<std::mutex> lock(mutex_);

  juce::Logger::getCurrentLogger()->writeToLog("=== Performance Metrics ===");

  for (const auto &[metricName, measurement] : measurements_) {
    auto metrics = calculateMetrics(measurement.durations, measurement.slowThreshold);

    juce::String log = metricName + ": ";
    log += "avg=" + juce::String(metrics.avgMs, 2) + "ms ";
    log += "min=" + juce::String(metrics.minMs, 2) + "ms ";
    log += "max=" + juce::String(metrics.maxMs, 2) + "ms ";
    log += "p95=" + juce::String(metrics.p95Ms, 2) + "ms ";
    log += "p99=" + juce::String(metrics.p99Ms, 2) + "ms ";
    log += "samples=" + juce::String(metrics.sampleCount);

    if (metrics.slowCount > 0)
      log += " ⚠️ SLOW: " + juce::String(metrics.slowCount) + "x";

    juce::Logger::getCurrentLogger()->writeToLog(log);
  }
}

} // namespace Profiling
} // namespace Util
} // namespace Sidechain

// Convenient macro for scoped timing
#define SCOPED_TIMER(name) Sidechain::Util::Profiling::ScopedTimer _timer_##__LINE__(name)

#define SCOPED_TIMER_THRESHOLD(name, thresholdMs)                                                                      \
  Sidechain::Util::Profiling::ScopedTimer _timer_##__LINE__(name, thresholdMs)
