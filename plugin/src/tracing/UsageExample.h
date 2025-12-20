#pragma once

/**
 * USAGE EXAMPLES: How to integrate distributed tracing into plugin code
 *
 * This file shows practical patterns for adding tracing to:
 * 1. Audio file upload operations
 * 2. Feed loading operations
 * 3. Nested operations with parent-child spans
 * 4. Error handling and span status
 *
 * NOTE: Spans are sent to the server via NetworkClient::sendTelemetrySpans()
 *       which uses the existing HTTP client infrastructure with retry logic,
 *       authentication, and async callback support.
 */

#include "TraceContext.h"
#include <memory>
#include <string>

namespace sidechain::tracing {

// ============================================================================
// Example 1: Tracing Audio Upload (Simple Case)
// ============================================================================

/**
 * Pseudo-code showing how to instrument AudioCapture::uploadAudio()
 *
 * BEFORE (without tracing):
 * @code
 * void AudioCapture::uploadAudio(const File& audioFile) {
 *     auto data = readAudioFile(audioFile);
 *     auto encoded = encodeAudio(data);
 *     auto response = httpClient->post("/api/v1/audio/upload", encoded);
 *     displayResult(response);
 * }
 * @endcode
 *
 * AFTER (with tracing):
 * @code
 * void AudioCapture::uploadAudio(const File& audioFile) {
 *     // Create new trace context for this operation
 *     auto traceCtx = std::make_unique<TraceContext>();
 *     auto spanRecorder = std::make_unique<SpanRecorder>();
 *
 *     // Start root span for entire upload operation
 *     auto rootSpanId = spanRecorder->startSpan("audio.upload", *traceCtx);
 *     spanRecorder->addAttribute(rootSpanId, "file.name", audioFile.getFileName().toStdString());
 *     spanRecorder->addAttribute(rootSpanId, "file.size_bytes", std::to_string(audioFile.getSize()));
 *
 *     try {
 *         // File reading (child span)
 *         {
 *             auto readSpanId = spanRecorder->startSpan("audio.read", *traceCtx);
 *             auto data = readAudioFile(audioFile);
 *             spanRecorder->endSpan(readSpanId, "ok");
 *         }
 *
 *         // Audio encoding (child span)
 *         {
 *             auto encodeSpanId = spanRecorder->startSpan("audio.encode", *traceCtx);
 *             auto encoded = encodeAudio(data);
 *             spanRecorder->endSpan(encodeSpanId, "ok");
 *
 *             spanRecorder->addAttribute(rootSpanId, "encoded.size_bytes",
 *                                        std::to_string(encoded.size()));
 *             spanRecorder->addAttribute(rootSpanId, "compression_ratio",
 *                                        std::to_string(double(data.size()) / encoded.size()));
 *         }
 *
 *         // HTTP upload (child span with trace headers)
 *         {
 *             auto uploadSpanId = spanRecorder->startSpan("network.post", *traceCtx);
 *
 *             // Get trace headers and include in HTTP request
 *             auto traceHeaders = traceCtx->getHttpHeaders();
 *             auto response = httpClient->post("/api/v1/audio/upload", encoded, traceHeaders);
 *
 *             spanRecorder->addAttribute(uploadSpanId, "http.status",
 *                                        std::to_string(response.statusCode));
 *             spanRecorder->endSpan(uploadSpanId, "ok");
 *         }
 *
 *         // Mark root span as successful
 *         spanRecorder->endSpan(rootSpanId, "ok");
 *         displayResult("Upload successful");
 *
 *     } catch (const std::exception& e) {
 *         // Mark spans with error status
 *         spanRecorder->endSpan(rootSpanId, "error", e.what());
 *         displayError(e.what());
 *     }
 *
 *     // Send recorded spans to server asynchronously
 *     auto client = std::make_unique<HttpClient>();
 *     auto spans = spanRecorder->getSpans();
 *     client->sendSpans(
 *         "https://api.sidechain.live/api/v1/telemetry/spans",
 *         spans,
 *         authToken_,
 *         [this](const auto& response) {
 *             logger_->info("Telemetry spans received by server");
 *         },
 *         [this](const auto& error) {
 *             logger_->warning("Failed to send telemetry spans: " + error);
 *             // Note: This doesn't affect user experience, just logging
 *         }
 *     );
 * }
 * @endcode
 */

// ============================================================================
// Example 2: Tracing Feed Load (With Multiple Spans)
// ============================================================================

/**
 * @code
 * void FeedPanel::loadFeed(const String& feedType) {
 *     auto traceCtx = std::make_unique<TraceContext>();
 *     auto spanRecorder = std::make_unique<SpanRecorder>();
 *
 *     auto rootSpanId = spanRecorder->startSpan("feed.load", *traceCtx);
 *     spanRecorder->addAttribute(rootSpanId, "feed.type", feedType.toStdString());
 *
 *     // Fetch feed from server
 *     {
 *         auto fetchSpanId = spanRecorder->startSpan("feed.fetch", *traceCtx);
 *         auto traceHeaders = traceCtx->getHttpHeaders();
 *
 *         auto [success, posts] = networkClient_->getFeed(feedType, traceHeaders);
 *
 *         if (!success) {
 *             spanRecorder->endSpan(fetchSpanId, "error", "Network request failed");
 *             spanRecorder->endSpan(rootSpanId, "error", "Could not fetch feed");
 *             return;
 *         }
 *
 *         spanRecorder->addAttribute(fetchSpanId, "posts.count", std::to_string(posts.size()));
 *         spanRecorder->endSpan(fetchSpanId, "ok");
 *     }
 *
 *     // Parse and prepare for display
 *     {
 *         auto parseSpanId = spanRecorder->startSpan("feed.parse", *traceCtx);
 *         auto feedItems = parseFeedPosts(posts);
 *         spanRecorder->addAttribute(parseSpanId, "items.parsed", std::to_string(feedItems.size()));
 *         spanRecorder->endSpan(parseSpanId, "ok");
 *     }
 *
 *     // UI update
 *     {
 *         auto uiSpanId = spanRecorder->startSpan("feed.ui_update", *traceCtx);
 *         updateFeedDisplay(feedItems);
 *         spanRecorder->endSpan(uiSpanId, "ok");
 *     }
 *
 *     spanRecorder->endSpan(rootSpanId, "ok");
 *
 *     // Send telemetry
 *     sendTelemetry(spanRecorder->getSpans());
 * }
 * @endcode
 */

// ============================================================================
// Example 3: Managing Global Trace Context
// ============================================================================

/**
 * Best practice: Keep trace context and span recorder as class members
 * so they're available throughout the operation's lifetime.
 *
 * @code
 * class AudioUploadOperation {
 * private:
 *     std::unique_ptr<TraceContext> traceCtx_;
 *     std::unique_ptr<SpanRecorder> spanRecorder_;
 *     std::shared_ptr<NetworkClient> networkClient_;  // Injected from plugin
 *
 * public:
 *     AudioUploadOperation(std::shared_ptr<NetworkClient> networkClient, const String& authToken)
 *         : traceCtx_(std::make_unique<TraceContext>()),
 *           spanRecorder_(std::make_unique<SpanRecorder>()),
 *           networkClient_(networkClient),
 *           authToken_(authToken) {
 *         // No additional configuration needed -
 *         // NetworkClient already has retry logic, auth, and async handling
 *     }
 *
 *     void execute(const File& audioFile) {
 *         auto rootSpanId = spanRecorder_->startSpan("upload", *traceCtx_);
 *
 *         try {
 *             // ... operation steps ...
 *             spanRecorder_->endSpan(rootSpanId, "ok");
 *         } catch (const std::exception& e) {
 *             spanRecorder_->endSpan(rootSpanId, "error", e.what());
 *         }
 *
 *         // Send telemetry via NetworkClient
 *         sendTelemetry();
 *     }
 *
 *     String getTraceId() const {
 *         return String(traceCtx_->getTraceId());
 *     }
 *
 * private:
 *     void sendTelemetry() {
 *         // Uses NetworkClient's sendTelemetrySpans with automatic retry, auth, async
 *         networkClient_->sendTelemetrySpans(
 *             spanRecorder_->getJson(),
 *             [this](const auto& outcome) {
 *                 if (outcome.isSuccess()) {
 *                     Log::info("Telemetry sent");
 *                 } else {
 *                     Log::warning("Failed to send telemetry: " + outcome.error());
 *                 }
 *             }
 *         );
 *     }
 *
 *     String authToken_;
 * };
 * @endcode
 */

// ============================================================================
// Example 4: Recording Events and Metadata
// ============================================================================

/**
 * Use recordEvent() for discrete events, addAttribute() for metadata
 *
 * @code
 * auto spanId = spanRecorder->startSpan("audio.encode", *traceCtx);
 *
 * // Record discrete events
 * spanRecorder->recordEvent(spanId, "encoding_started");
 * encodeAudio();
 * spanRecorder->recordEvent(spanId, "encoding_complete");
 *
 * // Add metadata attributes
 * spanRecorder->addAttribute(spanId, "codec", "mp3");
 * spanRecorder->addAttribute(spanId, "bitrate", "128k");
 * spanRecorder->addAttribute(spanId, "duration_seconds", "45.3");
 * spanRecorder->addAttribute(spanId, "input_format", "wav");
 * spanRecorder->addAttribute(spanId, "output_size_bytes", "720000");
 *
 * spanRecorder->endSpan(spanId, "ok");
 *
 * // Result in Grafana trace view:
 * // Span: audio.encode (15.3 seconds)
 * // ├─ Events:
 * // │  ├─ encoding_started (0ms)
 * // │  └─ encoding_complete (15.3s)
 * // └─ Attributes:
 * //    ├─ codec: mp3
 * //    ├─ bitrate: 128k
 * //    └─ ... etc
 * @endcode
 */

// ============================================================================
// Example 5: Trace Context Propagation (Server Response)
// ============================================================================

/**
 * When server returns response with trace headers, continue the same trace:
 *
 * @code
 * void FeedPanel::onServerResponse(const HTTPResponse& response) {
 *     // Server echoes back X-Trace-ID header
 *     auto traceIdHeader = response.getHeader("X-Trace-ID");
 *
 *     if (!traceIdHeader.empty()) {
 *         // Continue existing trace on server (same trace ID)
 *         // Server has already recorded spans under this trace ID
 *         logger_->info("Server processed under trace: " + traceIdHeader);
 *     }
 *
 *     // Later: query server for complete trace
 *     // GET /api/v1/traces/{traceId}  (admin endpoint)
 * }
 * @endcode
 */

// ============================================================================
// Example 6: Pattern - Automatic Span Cleanup
// ============================================================================

/**
 * Use RAII pattern for automatic span ending:
 *
 * @code
 * class ScopedSpan {
 * public:
 *     ScopedSpan(SpanRecorder& recorder, TraceContext& ctx, const String& name)
 *         : recorder_(recorder), spanId_(recorder.startSpan(name.toStdString(), ctx)) {}
 *
 *     ~ScopedSpan() {
 *         recorder_.endSpan(spanId_, "ok");
 *     }
 *
 *     void setError(const String& message) {
 *         hasError_ = true;
 *         errorMessage_ = message.toStdString();
 *     }
 *
 *     void addAttribute(const String& key, const String& value) {
 *         recorder_.addAttribute(spanId_, key.toStdString(), value.toStdString());
 *     }
 *
 * private:
 *     SpanRecorder& recorder_;
 *     std::string spanId_;
 *     bool hasError_ = false;
 *     std::string errorMessage_;
 * };
 *
 * // Usage:
 * {
 *     ScopedSpan span(spanRecorder, traceCtx, "audio.upload");
 *     span.addAttribute("file_name", audioFile.getFileName());
 *
 *     if (!uploadAudio()) {
 *         span.setError("Upload failed");
 *     }
 *     // Span automatically ends here
 * }
 * @endcode
 */

// ============================================================================
// Key Patterns Summary
// ============================================================================

/**
 * ✓ DO:
 * - Create TraceContext at operation start
 * - Create child spans for sub-operations
 * - Add trace headers to HTTP requests
 * - Record errors with span status
 * - Send spans asynchronously after operation completes
 * - Use addAttribute() for metadata
 * - Use recordEvent() for discrete events
 *
 * ✗ DON'T:
 * - Block on sending spans (use async callbacks)
 * - Create new TraceContext for child operations (reuse parent's)
 * - Include sensitive data in attributes (passwords, tokens)
 * - Forget to end spans (use RAII or try-catch)
 * - Send spans before operation completes
 * - Create too many spans (aim for 5-20 spans per operation)
 */

} // namespace sidechain::tracing
