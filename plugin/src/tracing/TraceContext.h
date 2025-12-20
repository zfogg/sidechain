#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace sidechain::tracing {

using json = nlohmann::json;

/**
 * TraceContext manages trace ID and span hierarchy for distributed tracing.
 * No external dependencies - uses standard C++ library.
 */
class TraceContext {
public:
  // Constructor: Create new trace
  TraceContext();

  // Constructor: Continue existing trace
  explicit TraceContext(const std::string &traceId);

  // Getters
  std::string getTraceId() const {
    return traceId_;
  }
  std::string getCurrentSpanId() const;
  std::string getParentSpanId() const;

  // Span nesting (for hierarchical operations)
  void pushSpan(const std::string &spanId);
  std::string popSpan();

  // For HTTP headers
  std::map<std::string, std::string> getHttpHeaders() const;
  json toJson() const;

  // Static factory from HTTP headers
  static std::unique_ptr<TraceContext> fromHttpHeaders(const std::map<std::string, std::string> &headers);

private:
  std::string traceId_;
  std::vector<std::string> spanStack_; // Stack of span IDs for nesting
  std::chrono::high_resolution_clock::time_point createdAt_;

  static std::string generateUUID();
};

/**
 * Represents a single operation (span).
 * Serializable to JSON for sending to server.
 */
struct Span {
  std::string traceId;
  std::string spanId;
  std::string parentSpanId;
  std::string name;

  // Timing (milliseconds)
  int64_t startTimeMs = 0;
  int64_t endTimeMs = 0;
  int64_t durationMs = 0;

  // Status
  std::string status = "ok"; // "ok", "error", "cancelled"
  std::string statusMessage;

  // Data
  std::map<std::string, std::string> attributes;
  std::vector<std::pair<int64_t, std::string>> events; // (timestamp_ms, event_name)

  // Client info
  std::string clientType = "plugin";
  std::string clientVersion = "1.0.0";

  json toJson() const;
  static Span fromJson(const json &j);
};

/**
 * Records spans locally before sending to server.
 * Thread-safe for plugin's multi-threaded environment.
 */
class SpanRecorder {
public:
  SpanRecorder() = default;

  // Start recording a span (returns span ID)
  std::string startSpan(const std::string &name, TraceContext &ctx);

  // End recording a span
  void endSpan(const std::string &spanId, const std::string &status = "ok", const std::string &message = "");

  // Record an event within a span
  void recordEvent(const std::string &spanId, const std::string &eventName);

  // Add attribute to a span
  void addAttribute(const std::string &spanId, const std::string &key, const std::string &value);

  // Get all recorded spans
  std::vector<Span> getSpans() const;

  // Get JSON representation for sending to server
  json getJson() const;

  // Clear spans after sending
  void clear();

  // Send spans to server (async)
  bool sendToServer(const std::string &serverUrl, const std::string &authToken);

private:
  std::vector<Span> completedSpans_;
  std::map<std::string, Span> activeSpans_;
  mutable std::mutex mutex_;

  static int64_t getCurrentTimeMs();
};

} // namespace sidechain::tracing
