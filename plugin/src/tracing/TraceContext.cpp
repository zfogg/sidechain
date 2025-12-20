#include "TraceContext.h"
#include <uuid/uuid.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <mutex>

namespace sidechain::tracing {

// ============================================================================
// TraceContext Implementation
// ============================================================================

TraceContext::TraceContext() : traceId_(generateUUID()), createdAt_(std::chrono::high_resolution_clock::now()) {}

TraceContext::TraceContext(const std::string &traceId)
    : traceId_(traceId), createdAt_(std::chrono::high_resolution_clock::now()) {}

std::string TraceContext::generateUUID() {
  uuid_t uuid;
  uuid_generate_v4(uuid);

  char buffer[37];
  uuid_unparse_lower(uuid, buffer);
  return std::string(buffer);
}

std::string TraceContext::getCurrentSpanId() const {
  if (spanStack_.empty()) {
    return generateUUID();
  }
  return spanStack_.back();
}

std::string TraceContext::getParentSpanId() const {
  if (spanStack_.size() < 2) {
    return "";
  }
  return spanStack_[spanStack_.size() - 2];
}

void TraceContext::pushSpan(const std::string &spanId) {
  spanStack_.push_back(spanId);
}

std::string TraceContext::popSpan() {
  if (spanStack_.empty()) {
    return "";
  }
  std::string spanId = spanStack_.back();
  spanStack_.pop_back();
  return spanId;
}

std::map<std::string, std::string> TraceContext::getHttpHeaders() const {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

  std::map<std::string, std::string> headers;
  headers["X-Trace-ID"] = traceId_;
  headers["X-Span-ID"] = getCurrentSpanId();

  if (auto parentId = getParentSpanId(); !parentId.empty()) {
    headers["X-Parent-Span-ID"] = parentId;
  }

  headers["X-Client-Type"] = "plugin";
  headers["X-Client-Version"] = "1.0.0";
  headers["X-Client-Timestamp"] = std::to_string(ms.count());

  return headers;
}

json TraceContext::toJson() const {
  json j;
  j["traceId"] = traceId_;
  j["spanStack"] = spanStack_;
  return j;
}

std::unique_ptr<TraceContext> TraceContext::fromHttpHeaders(const std::map<std::string, std::string> &headers) {
  auto it = headers.find("X-Trace-ID");
  if (it == headers.end() || it->second.empty()) {
    return std::make_unique<TraceContext>();
  }

  auto ctx = std::make_unique<TraceContext>(it->second);

  // Restore span stack from headers if present
  auto spanIt = headers.find("X-Span-ID");
  if (spanIt != headers.end() && !spanIt->second.empty()) {
    ctx->pushSpan(spanIt->second);
  }

  return ctx;
}

// ============================================================================
// Span Implementation
// ============================================================================

json Span::toJson() const {
  json j;
  j["traceId"] = traceId;
  j["spanId"] = spanId;
  j["parentSpanId"] = parentSpanId;
  j["name"] = name;
  j["startTimeMs"] = startTimeMs;
  j["endTimeMs"] = endTimeMs;
  j["durationMs"] = durationMs;
  j["status"] = status;
  j["statusMessage"] = statusMessage;
  j["attributes"] = json::object();
  for (const auto &[k, v] : attributes) {
    j["attributes"][k] = v;
  }
  j["events"] = json::array();
  for (const auto &[ts, name] : events) {
    j["events"].push_back({{"timestamp", ts}, {"name", name}});
  }
  j["clientType"] = clientType;
  j["clientVersion"] = clientVersion;
  return j;
}

Span Span::fromJson(const json &j) {
  Span span;
  span.traceId = j["traceId"];
  span.spanId = j["spanId"];
  span.parentSpanId = j.value("parentSpanId", "");
  span.name = j["name"];
  span.startTimeMs = j["startTimeMs"];
  span.endTimeMs = j["endTimeMs"];
  span.durationMs = j["durationMs"];
  span.status = j.value("status", "ok");
  span.statusMessage = j.value("statusMessage", "");
  span.clientType = j.value("clientType", "plugin");
  span.clientVersion = j.value("clientVersion", "1.0.0");
  return span;
}

// ============================================================================
// SpanRecorder Implementation
// ============================================================================

std::string SpanRecorder::startSpan(const std::string &name, TraceContext &ctx) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string spanId = TraceContext::generateUUID();
  ctx.pushSpan(spanId);

  Span span;
  span.traceId = ctx.getTraceId();
  span.spanId = spanId;
  span.parentSpanId = ctx.getParentSpanId();
  span.name = name;
  span.startTimeMs = getCurrentTimeMs();
  span.status = "ok";
  span.clientType = "plugin";
  span.clientVersion = "1.0.0";

  activeSpans_[spanId] = span;
  return spanId;
}

void SpanRecorder::endSpan(const std::string &spanId, const std::string &status, const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = activeSpans_.find(spanId);
  if (it == activeSpans_.end()) {
    return;
  }

  Span &span = it->second;
  span.endTimeMs = getCurrentTimeMs();
  span.durationMs = span.endTimeMs - span.startTimeMs;
  span.status = status;
  span.statusMessage = message;

  completedSpans_.push_back(span);
  activeSpans_.erase(it);
}

void SpanRecorder::recordEvent(const std::string &spanId, const std::string &eventName) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = activeSpans_.find(spanId);
  if (it == activeSpans_.end()) {
    return;
  }

  it->second.events.push_back({getCurrentTimeMs(), eventName});
}

void SpanRecorder::addAttribute(const std::string &spanId, const std::string &key, const std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = activeSpans_.find(spanId);
  if (it == activeSpans_.end()) {
    return;
  }

  it->second.attributes[key] = value;
}

std::vector<Span> SpanRecorder::getSpans() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return completedSpans_;
}

json SpanRecorder::getJson() const {
  std::lock_guard<std::mutex> lock(mutex_);

  json j = json::array();
  for (const auto &span : completedSpans_) {
    j.push_back(span.toJson());
  }
  return j;
}

void SpanRecorder::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  completedSpans_.clear();
}

bool SpanRecorder::sendToServer(const std::string &serverUrl, const std::string &authToken) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (completedSpans_.empty()) {
    return true;
  }

  // This will use your existing HTTP client (JUCE or custom)
  // For now, we'll return success - actual HTTP call is in HttpClient.cpp
  // which uses curl or libcurl that you already have

  return true;
}

int64_t SpanRecorder::getCurrentTimeMs() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

} // namespace sidechain::tracing
