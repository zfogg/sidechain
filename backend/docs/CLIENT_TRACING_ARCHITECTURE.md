# Complete End-to-End Client Tracing Architecture

## Overview

This document defines a unified distributed tracing system that spans all three Sidechain clients (VST Plugin, Web App, CLI) and the backend server. This enables complete visibility into any user action from initiation through completion.

---

## 🎯 The Vision

When a user reports "something is slow or broken", you can ask for their **trace ID** and see the complete picture:

```
User Action: "Upload a loop and share it"

Trace ID: abc123def456...

Timeline:
14:23:45.000 [PLUGIN] User selects audio file
            ↓
14:23:45.100 [PLUGIN] Audio encoding starts (3000ms)
            ↓
14:23:48.100 [PLUGIN] Sends POST /api/v1/posts with trace_id=abc123...
            ↓
14:23:48.120 [NETWORK] Request in flight (120ms)
            ↓
14:23:48.240 [SERVER] POST /api/v1/posts received
            ├─ [SPAN] feed.create_post
            ├─ [SPAN] db.query: INSERT audio_posts (5ms)
            ├─ [SPAN] stream.io.create_activity (50ms)
            └─ [SPAN] response sent (75ms total)
            ↓
14:23:48.315 [PLUGIN] Response received from server
            ↓
14:23:48.400 [PLUGIN] UI updated
            ↓
14:23:48.450 [PLUGIN] User sees "Post created!"

Total: 1450ms (1000ms plugin + 120ms network + 75ms server)
```

---

## 🏗️ Architecture Overview

### Three-Tier Tracing System

```
┌─────────────────────────────────────────────────────────────┐
│                    Trace Context                             │
│              (Propagated Through All Systems)                │
│  trace_id: "abc123def456"                                   │
│  span_id: "xyz789" (changes per operation)                  │
│  parent_id: "parent123" (links parent-child)                │
└─────────────────────────────────────────────────────────────┘
         │
         ├─ VST Plugin (C++/JUCE)
         │  └─ Creates spans, records plugin operations
         │  └─ Sends trace ID to server with every request
         │
         ├─ Web App (TypeScript/React)
         │  └─ Creates spans, records user interactions
         │  └─ Includes trace ID in HTTP headers
         │
         ├─ CLI (Go)
         │  └─ Creates spans, records command execution
         │  └─ Includes trace ID in requests
         │
         └─ Backend (Go)
            └─ Receives trace ID, creates child spans
            └─ Links to database, cache, external APIs
```

### Trace Flow Diagram

```
User Action (Plugin)
    ↓
[plugin.start_action] span created
    │
    ├─ plugin.upload_audio (child span)
    │  ├─ file.read: 100ms
    │  ├─ audio.encode: 3000ms
    │  └─ network.request: 120ms
    │
    └─ HTTP Request with:
       - X-Trace-ID: abc123...
       - X-Span-ID: xyz789...
       - X-Client: plugin/1.0.1
         ↓
    [Server receives request]
         │
    ├─ [http.post.create_post] (root span on server)
    │  │
    │  ├─ [feed.create_post] (business event)
    │  ├─ [db.query] (GORM plugin)
    │  ├─ [redis.get] (cache)
    │  └─ [stream.io.api] (external)
    │
    └─ Response with:
       - X-Server-Time: 75ms
       - X-Trace-ID: abc123... (echoed back)
         ↓
    [Plugin receives response]
         │
    └─ [plugin.update_ui]
       └─ response.time: 50ms
```

---

## 📋 Implementation Details

### 1. Core Trace Context Object

All clients use the same trace context structure:

```typescript
// Shared across all clients
interface TraceContext {
  // Required
  traceId: string;        // Unique ID for entire user action (generated once)
  spanId: string;         // Current operation ID (changes per span)

  // Optional but recommended
  parentSpanId?: string;  // Parent span (for parent-child relationships)
  baggage?: Map<string, string>;  // Custom metadata passed through system

  // Client info
  clientType: 'plugin' | 'web' | 'cli';  // Which client sent this
  clientVersion: string;  // Client version (1.0.1)

  // Timing
  startTime: number;      // When trace was created (epoch ms)
  currentTime: number;    // Current time (for latency calculation)
}

// Example:
{
  "traceId": "abc123def456789",
  "spanId": "xyz789",
  "parentSpanId": "parent123",
  "clientType": "plugin",
  "clientVersion": "1.2.0",
  "startTime": 1702834225000,
  "currentTime": 1702834225500
}
```

### 2. Span Object

Every operation (on client or server) creates a span:

```typescript
interface Span {
  // Required
  traceId: string;        // Must match parent trace
  spanId: string;         // Unique within trace
  name: string;           // Operation name (e.g., "plugin.upload_audio")

  // Timing (all in milliseconds)
  startTime: number;      // When operation started
  endTime: number;        // When operation finished
  duration: number;       // endTime - startTime

  // Status
  status: 'ok' | 'error' | 'cancelled';
  statusMessage?: string; // Error details if status == 'error'

  // Relationships
  parentSpanId?: string;  // Parent span ID (for nesting)
  childSpanIds: string[]; // Child span IDs (operations we called)

  // Context
  attributes: Map<string, any>;  // Custom attributes
  events: SpanEvent[];    // Discrete events during span

  // Client info
  clientType?: 'plugin' | 'web' | 'cli';
  clientVersion?: string;
}

// Example span from plugin:
{
  "traceId": "abc123def456789",
  "spanId": "xyz789",
  "name": "plugin.upload_audio",
  "startTime": 1702834225100,
  "endTime": 1702834228100,
  "duration": 3000,
  "status": "ok",
  "parentSpanId": "parent123",
  "attributes": {
    "file.size": 10485760,
    "audio.format": "mp3",
    "audio.duration_seconds": 4.5,
    "file.path": "/Users/zack/Music/loop.wav"
  },
  "events": [
    {
      "name": "encoding_started",
      "timestamp": 1702834225200
    },
    {
      "name": "encoding_complete",
      "timestamp": 1702834228050
    }
  ]
}
```

### 3. HTTP Header Propagation

Every request between client and server includes these headers:

```
X-Trace-ID: abc123def456789       # Trace ID (required)
X-Span-ID: xyz789                 # Current span ID (required)
X-Parent-Span-ID: parent123       # Parent span for nesting (optional)
X-Client-Type: plugin             # Which client (required)
X-Client-Version: 1.2.0           # Client version (required)
X-Client-Timestamp: 1702834225000 # When client made request (for latency)
X-User-ID: user-abc123            # User ID if authenticated (optional)
X-Request-ID: req-xyz789          # Separate request ID if needed (optional)
```

Example HTTP request from plugin:
```
POST /api/v1/posts HTTP/1.1
Host: api.sidechain.live
X-Trace-ID: abc123def456789
X-Span-ID: http-request-001
X-Parent-Span-ID: xyz789
X-Client-Type: plugin
X-Client-Version: 1.2.0
X-Client-Timestamp: 1702834225050
Content-Type: application/json

{
  "audio_url": "https://cdn.example.com/audio.mp3",
  "bpm": 120,
  "key": "C"
}
```

### 4. Server Response Headers

Server echoes back trace information and adds timing:

```
X-Trace-ID: abc123def456789       # Echo back (client validates)
X-Span-ID: http-post-create-post  # Server's span ID for this request
X-Server-Time: 75                 # Time server spent (ms)
X-Server-Timestamp: 1702834225120 # When server received request
X-Request-Duration: 1450          # Total from client perspective
```

---

## 🔧 Client Implementation Details

### A. VST Plugin (C++)

#### Header: `src/tracing/TraceContext.h`

```cpp
#pragma once

#include <string>
#include <map>
#include <chrono>
#include <uuid/uuid.h>

namespace sidechain::tracing {

class TraceContext {
public:
    // Constructors
    TraceContext();  // Generate new trace ID
    explicit TraceContext(const std::string& traceId);  // Continue existing trace

    // Getters
    std::string getTraceId() const { return traceId_; }
    std::string getSpanId() const { return spanId_; }
    std::string getParentSpanId() const { return parentSpanId_; }
    std::string getClientVersion() const;
    std::chrono::milliseconds getElapsedTime() const;

    // Setters for nesting
    void setParentSpanId(const std::string& spanId) { parentSpanId_ = spanId; }
    void addAttribute(const std::string& key, const std::string& value) {
        attributes_[key] = value;
    }

    // For HTTP headers
    std::map<std::string, std::string> getHttpHeaders() const;
    static TraceContext fromHttpHeaders(const std::map<std::string, std::string>& headers);

    // Start new span (creates child span ID)
    std::string createChildSpanId();

private:
    std::string traceId_;
    std::string spanId_;
    std::string parentSpanId_;
    std::map<std::string, std::string> attributes_;
    std::chrono::high_resolution_clock::time_point startTime_;

    static std::string generateUUID();
};

}  // namespace sidechain::tracing
```

#### Implementation: `src/tracing/TraceContext.cpp`

```cpp
#include "TraceContext.h"
#include <uuid/uuid.h>
#include <sstream>
#include <iomanip>

namespace sidechain::tracing {

TraceContext::TraceContext()
    : traceId_(generateUUID()),
      spanId_(generateUUID()),
      startTime_(std::chrono::high_resolution_clock::now()) {
}

TraceContext::TraceContext(const std::string& traceId)
    : traceId_(traceId),
      spanId_(generateUUID()),
      startTime_(std::chrono::high_resolution_clock::now()) {
}

std::string TraceContext::generateUUID() {
    uuid_t uuid;
    uuid_generate_v4(uuid);

    char buffer[37];
    uuid_unparse_lower(uuid, buffer);
    return std::string(buffer);
}

std::chrono::milliseconds TraceContext::getElapsedTime() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_);
}

std::map<std::string, std::string> TraceContext::getHttpHeaders() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    );

    std::map<std::string, std::string> headers;
    headers["X-Trace-ID"] = traceId_;
    headers["X-Span-ID"] = spanId_;
    if (!parentSpanId_.empty()) {
        headers["X-Parent-Span-ID"] = parentSpanId_;
    }
    headers["X-Client-Type"] = "plugin";
    headers["X-Client-Version"] = getClientVersion();
    headers["X-Client-Timestamp"] = std::to_string(ms.count());
    return headers;
}

std::string TraceContext::createChildSpanId() {
    return generateUUID();
}

std::string TraceContext::getClientVersion() const {
    return "1.0.0";  // Read from version header file in real implementation
}

}  // namespace sidechain::tracing
```

#### Span Recorder: `src/tracing/SpanRecorder.h`

```cpp
#pragma once

#include "TraceContext.h"
#include <vector>
#include <memory>

namespace sidechain::tracing {

struct Span {
    std::string traceId;
    std::string spanId;
    std::string parentSpanId;
    std::string name;

    int64_t startTimeMs;
    int64_t endTimeMs;
    std::string status;  // "ok", "error", "cancelled"

    std::map<std::string, std::string> attributes;
    std::vector<std::pair<int64_t, std::string>> events;  // (timestamp, event_name)

    // Calculated
    int64_t getDurationMs() const { return endTimeMs - startTimeMs; }
};

class SpanRecorder {
public:
    SpanRecorder() = default;

    // Start recording a span
    std::string startSpan(const std::string& name, const TraceContext& ctx);

    // End recording a span
    void endSpan(const std::string& spanId, const std::string& status = "ok");

    // Record an event within a span
    void recordEvent(const std::string& spanId, const std::string& eventName);

    // Add attributes to a span
    void addAttribute(const std::string& spanId, const std::string& key, const std::string& value);

    // Get all recorded spans
    std::vector<Span> getSpans() const { return spans_; }

    // Send spans to server (async)
    void sendToServer(const std::string& serverUrl, const std::string& userToken);

private:
    std::vector<Span> spans_;
    std::map<std::string, size_t> spanIndex_;  // spanId -> index in spans_

    int64_t getCurrentTimeMs() const;
};

}  // namespace sidechain::tracing
```

#### Usage in Plugin Code

```cpp
// In AudioCapture::uploadAudio()
#include "tracing/SpanRecorder.h"
#include "tracing/TraceContext.h"

void AudioCapture::uploadAudio(const juce::File& audioFile) {
    // Create trace context (once per user action, reused for all requests)
    static auto traceCtx = std::make_unique<sidechain::tracing::TraceContext>();
    static auto recorder = std::make_unique<sidechain::tracing::SpanRecorder>();

    // Start span for entire upload process
    auto uploadSpanId = recorder->startSpan("plugin.upload_audio", *traceCtx);

    try {
        // Record events
        recorder->recordEvent(uploadSpanId, "file_selected");

        // Add attributes
        recorder->addAttribute(uploadSpanId, "file.path", audioFile.getFullPathName().toStdString());
        recorder->addAttribute(uploadSpanId, "file.size", std::to_string(audioFile.getSize()));

        // Start encoding span (child of upload)
        auto encodeSpanId = recorder->startSpan("plugin.encode_audio", *traceCtx);
        encodeSpanId.setParentSpanId(uploadSpanId);

        // Encode audio
        juce::MemoryOutputStream encodedData;
        encodeAudio(audioFile, encodedData);
        recorder->recordEvent(encodeSpanId, "encoding_complete");
        recorder->endSpan(encodeSpanId, "ok");

        // Start network request span (child of upload)
        auto networkSpanId = recorder->startSpan("plugin.http_request", *traceCtx);

        // Make HTTP request with trace headers
        auto headers = traceCtx->getHttpHeaders();
        auto response = httpClient.post("/api/v1/posts",
                                       encodedData,
                                       headers);

        recorder->recordEvent(networkSpanId, "response_received");
        recorder->addAttribute(networkSpanId, "http.status_code", std::to_string(response.statusCode));
        recorder->addAttribute(networkSpanId, "http.server_time_ms",
                              response.getHeader("X-Server-Time"));
        recorder->endSpan(networkSpanId, "ok");

        recorder->endSpan(uploadSpanId, "ok");

        // Send all spans to server
        recorder->sendToServer("https://api.sidechain.live", userToken);

    } catch (const std::exception& e) {
        recorder->endSpan(uploadSpanId, "error");
        recorder->addAttribute(uploadSpanId, "error.message", e.what());
        throw;
    }
}
```

---

### B. Web App (TypeScript)

#### `src/lib/tracing/TraceContext.ts`

```typescript
import { v4 as uuidv4 } from 'uuid';

export interface SpanEvent {
  timestamp: number;
  name: string;
  attributes?: Record<string, any>;
}

export interface SpanAttribute {
  [key: string]: string | number | boolean | null;
}

export class TraceContext {
  readonly traceId: string;
  private spanStack: string[] = [];  // Stack of span IDs for nesting
  readonly startTime: number;

  constructor(traceId?: string) {
    this.traceId = traceId || uuidv4();
    this.startTime = Date.now();
  }

  getCurrentSpanId(): string {
    return this.spanStack[this.spanStack.length - 1] || uuidv4();
  }

  pushSpan(spanId: string): void {
    this.spanStack.push(spanId);
  }

  popSpan(): string | undefined {
    return this.spanStack.pop();
  }

  getHttpHeaders(): Record<string, string> {
    return {
      'X-Trace-ID': this.traceId,
      'X-Span-ID': this.getCurrentSpanId(),
      'X-Parent-Span-ID': this.spanStack.length > 1 ? this.spanStack[this.spanStack.length - 2] : '',
      'X-Client-Type': 'web',
      'X-Client-Version': process.env.REACT_APP_VERSION || '1.0.0',
      'X-Client-Timestamp': Date.now().toString(),
    };
  }

  static fromHttpHeaders(headers: Record<string, string>): TraceContext {
    const ctx = new TraceContext(headers['X-Trace-ID']);
    if (headers['X-Span-ID']) {
      ctx.pushSpan(headers['X-Span-ID']);
    }
    return ctx;
  }
}

export interface Span {
  traceId: string;
  spanId: string;
  parentSpanId?: string;
  name: string;
  startTime: number;
  endTime?: number;
  duration?: number;
  status: 'ok' | 'error' | 'cancelled';
  statusMessage?: string;
  attributes: SpanAttribute;
  events: SpanEvent[];
  clientType: 'web' | 'plugin' | 'cli';
  clientVersion: string;
}

export class SpanRecorder {
  private spans: Span[] = [];
  private activeSpans: Map<string, Span> = new Map();
  private serverUrl: string;

  constructor(serverUrl: string = process.env.REACT_APP_API_URL || 'http://localhost:8787') {
    this.serverUrl = serverUrl;
  }

  startSpan(name: string, ctx: TraceContext, parentSpanId?: string): string {
    const spanId = uuidv4();
    ctx.pushSpan(spanId);

    const span: Span = {
      traceId: ctx.traceId,
      spanId,
      parentSpanId: parentSpanId || ctx.spanStack[ctx.spanStack.length - 2],
      name,
      startTime: Date.now(),
      status: 'ok',
      attributes: {},
      events: [],
      clientType: 'web',
      clientVersion: process.env.REACT_APP_VERSION || '1.0.0',
    };

    this.activeSpans.set(spanId, span);
    return spanId;
  }

  endSpan(spanId: string, status: 'ok' | 'error' = 'ok', statusMessage?: string): void {
    const span = this.activeSpans.get(spanId);
    if (!span) return;

    span.endTime = Date.now();
    span.duration = span.endTime - span.startTime;
    span.status = status;
    if (statusMessage) span.statusMessage = statusMessage;

    this.spans.push(span);
    this.activeSpans.delete(spanId);
  }

  recordEvent(spanId: string, eventName: string, attributes?: Record<string, any>): void {
    const span = this.activeSpans.get(spanId);
    if (!span) return;

    span.events.push({
      timestamp: Date.now(),
      name: eventName,
      attributes,
    });
  }

  addAttribute(spanId: string, key: string, value: any): void {
    const span = this.activeSpans.get(spanId);
    if (span) {
      span.attributes[key] = value;
    }
  }

  getSpans(): Span[] {
    return [...this.spans, ...Array.from(this.activeSpans.values())];
  }

  async sendToServer(): Promise<void> {
    const spans = this.getSpans();
    if (spans.length === 0) return;

    try {
      await fetch(`${this.serverUrl}/api/v1/telemetry/spans`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ spans }),
      });
    } catch (error) {
      console.error('Failed to send spans:', error);
    }
  }
}
```

#### `src/hooks/useTracing.ts`

```typescript
import { useContext, createContext, useEffect } from 'react';
import { TraceContext, SpanRecorder } from '../lib/tracing/TraceContext';

const TraceContext_React = createContext<{
  traceContext: TraceContext;
  recorder: SpanRecorder;
} | null>(null);

export function useTracing() {
  const context = useContext(TraceContext_React);
  if (!context) {
    throw new Error('useTracing must be used within TraceProvider');
  }
  return context;
}

export function useSpan(spanName: string) {
  const { traceContext, recorder } = useTracing();

  useEffect(() => {
    const spanId = recorder.startSpan(spanName, traceContext);

    return () => {
      recorder.endSpan(spanId);
    };
  }, [spanName, traceContext, recorder]);

  return (attribute: string, value: any) => {
    recorder.addAttribute(recorder.getSpans()[recorder.getSpans().length - 1]?.spanId || '',
                         attribute, value);
  };
}
```

#### Usage in React Components

```typescript
// src/components/FeedPage.tsx
import { useTracing, useSpan } from '../hooks/useTracing';

export function FeedPage() {
  const { traceContext, recorder } = useTracing();
  const addAttribute = useSpan('web.feed_page_load');

  useEffect(() => {
    const spanId = recorder.startSpan('web.fetch_feed', traceContext);

    fetch('/api/v1/feed/timeline', {
      headers: traceContext.getHttpHeaders(),
    })
      .then(async (res) => {
        addAttribute('http.status_code', res.status);
        addAttribute('http.server_time_ms', res.headers.get('X-Server-Time') || 'unknown');

        const duration = Date.now() - parseInt(res.headers.get('X-Client-Timestamp') || '0');
        addAttribute('total_latency_ms', duration);

        return res.json();
      })
      .then((data) => {
        recorder.recordEvent(spanId, 'feed_loaded', { count: data.length });
        recorder.endSpan(spanId, 'ok');
      })
      .catch((error) => {
        recorder.endSpan(spanId, 'error', error.message);
      });
  }, []);

  return <div>Feed Content</div>;
}
```

---

### C. CLI (Go)

#### `internal/telemetry/client_trace.go`

```go
package telemetry

import (
	"context"
	"time"

	"github.com/google/uuid"
	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"
)

// ClientTraceContext represents trace context on the client side
type ClientTraceContext struct {
	TraceID       string
	SpanStack     []string  // Stack of span IDs for nesting
	StartTime     time.Time
	ClientVersion string
}

// NewClientTraceContext creates a new trace context
func NewClientTraceContext(clientVersion string) *ClientTraceContext {
	return &ClientTraceContext{
		TraceID:       uuid.New().String(),
		SpanStack:     []string{},
		StartTime:     time.Now(),
		ClientVersion: clientVersion,
	}
}

// FromTraceID continues an existing trace
func FromTraceID(traceID string, clientVersion string) *ClientTraceContext {
	return &ClientTraceContext{
		TraceID:       traceID,
		SpanStack:     []string{},
		StartTime:     time.Now(),
		ClientVersion: clientVersion,
	}
}

// GetCurrentSpanID returns the current (top) span ID
func (ctx *ClientTraceContext) GetCurrentSpanID() string {
	if len(ctx.SpanStack) == 0 {
		return uuid.New().String()
	}
	return ctx.SpanStack[len(ctx.SpanStack)-1]
}

// GetParentSpanID returns the parent span ID if nested
func (ctx *ClientTraceContext) GetParentSpanID() string {
	if len(ctx.SpanStack) <= 1 {
		return ""
	}
	return ctx.SpanStack[len(ctx.SpanStack)-2]
}

// PushSpan adds a new span to the stack
func (ctx *ClientTraceContext) PushSpan(spanID string) {
	ctx.SpanStack = append(ctx.SpanStack, spanID)
}

// PopSpan removes the current span from the stack
func (ctx *ClientTraceContext) PopSpan() string {
	if len(ctx.SpanStack) == 0 {
		return ""
	}
	spanID := ctx.SpanStack[len(ctx.SpanStack)-1]
	ctx.SpanStack = ctx.SpanStack[:len(ctx.SpanStack)-1]
	return spanID
}

// HTTPHeaders returns headers for HTTP requests
func (ctx *ClientTraceContext) HTTPHeaders() map[string]string {
	headers := map[string]string{
		"X-Trace-ID":          ctx.TraceID,
		"X-Span-ID":           ctx.GetCurrentSpanID(),
		"X-Client-Type":       "cli",
		"X-Client-Version":    ctx.ClientVersion,
		"X-Client-Timestamp":  string(rune(time.Now().UnixMilli())),
	}
	if parentID := ctx.GetParentSpanID(); parentID != "" {
		headers["X-Parent-Span-ID"] = parentID
	}
	return headers
}

// ClientSpan represents a span created on the client
type ClientSpan struct {
	TraceID      string            `json:"trace_id"`
	SpanID       string            `json:"span_id"`
	ParentSpanID string            `json:"parent_span_id,omitempty"`
	Name         string            `json:"name"`
	StartTimeMs  int64             `json:"start_time_ms"`
	EndTimeMs    int64             `json:"end_time_ms"`
	Duration     int64             `json:"duration_ms"`
	Status       string            `json:"status"`
	Message      string            `json:"status_message,omitempty"`
	Attributes   map[string]string `json:"attributes"`
	Events       []SpanEvent       `json:"events"`
	ClientType   string            `json:"client_type"`
	ClientVer    string            `json:"client_version"`
}

type SpanEvent struct {
	TimestampMs int64             `json:"timestamp_ms"`
	Name        string            `json:"name"`
	Attributes  map[string]string `json:"attributes,omitempty"`
}

// ClientSpanRecorder records spans on the client
type ClientSpanRecorder struct {
	spans   []*ClientSpan
	active  map[string]*ClientSpan
	traceCtx *ClientTraceContext
}

// NewClientSpanRecorder creates a new recorder
func NewClientSpanRecorder(ctx *ClientTraceContext) *ClientSpanRecorder {
	return &ClientSpanRecorder{
		spans:    []*ClientSpan{},
		active:   make(map[string]*ClientSpan),
		traceCtx: ctx,
	}
}

// StartSpan starts recording a span
func (r *ClientSpanRecorder) StartSpan(name string) string {
	spanID := uuid.New().String()
	r.traceCtx.PushSpan(spanID)

	span := &ClientSpan{
		TraceID:      r.traceCtx.TraceID,
		SpanID:       spanID,
		ParentSpanID: r.traceCtx.GetParentSpanID(),
		Name:         name,
		StartTimeMs:  time.Now().UnixMilli(),
		Status:       "ok",
		Attributes:   make(map[string]string),
		Events:       []SpanEvent{},
		ClientType:   "cli",
		ClientVer:    r.traceCtx.ClientVersion,
	}

	r.active[spanID] = span
	return spanID
}

// EndSpan finishes recording a span
func (r *ClientSpanRecorder) EndSpan(spanID string, status string, message string) {
	span, ok := r.active[spanID]
	if !ok {
		return
	}

	span.EndTimeMs = time.Now().UnixMilli()
	span.Duration = span.EndTimeMs - span.StartTimeMs
	span.Status = status
	span.Message = message

	r.spans = append(r.spans, span)
	delete(r.active, spanID)
	r.traceCtx.PopSpan()
}

// AddAttribute adds an attribute to the current span
func (r *ClientSpanRecorder) AddAttribute(spanID string, key string, value string) {
	if span, ok := r.active[spanID]; ok {
		span.Attributes[key] = value
	}
}

// RecordEvent records an event in a span
func (r *ClientSpanRecorder) RecordEvent(spanID string, eventName string) {
	if span, ok := r.active[spanID]; ok {
		span.Events = append(span.Events, SpanEvent{
			TimestampMs: time.Now().UnixMilli(),
			Name:        eventName,
		})
	}
}

// GetSpans returns all recorded spans
func (r *ClientSpanRecorder) GetSpans() []*ClientSpan {
	return r.spans
}

// SendToServer sends spans to the backend
func (r *ClientSpanRecorder) SendToServer(ctx context.Context, client *http.Client, serverURL string) error {
	if len(r.spans) == 0 {
		return nil
	}

	payload := map[string]interface{}{
		"spans": r.spans,
	}

	// Marshal and send (use your HTTP client)
	// This is pseudocode - implement with actual http.Client
	return nil
}
```

#### Usage in CLI Command

```go
// cmd/cli/commands/upload.go
package commands

import (
	"github.com/zfogg/sidechain/backend/internal/telemetry"
)

func UploadCommand(audioPath string, serverURL string) error {
	// Create trace context (once per user action)
	traceCtx := telemetry.NewClientTraceContext("sidechain-cli/1.0.0")
	recorder := telemetry.NewClientSpanRecorder(traceCtx)

	// Start main span
	uploadSpanID := recorder.StartSpan("cli.upload_audio")
	defer func() {
		if r := recover(); r != nil {
			recorder.EndSpan(uploadSpanID, "error", "panic")
			panic(r)
		}
		recorder.EndSpan(uploadSpanID, "ok", "")

		// Send to server
		recorder.SendToServer(context.Background(), httpClient, serverURL)
	}()

	// Record file info
	recorder.AddAttribute(uploadSpanID, "file.path", audioPath)
	recorder.RecordEvent(uploadSpanID, "file_selected")

	// Start encoding span
	encodeSpanID := recorder.StartSpan("cli.encode_audio")

	if err := encodeAudio(audioPath); err != nil {
		recorder.EndSpan(encodeSpanID, "error", err.Error())
		return err
	}
	recorder.EndSpan(encodeSpanID, "ok", "")

	// Start network span
	networkSpanID := recorder.StartSpan("cli.http_request")

	// Make request with trace headers
	headers := traceCtx.HTTPHeaders()
	response, err := httpClient.Post(
		serverURL+"/api/v1/posts",
		"application/json",
		body,
		headers,
	)

	if err != nil {
		recorder.EndSpan(networkSpanID, "error", err.Error())
		return err
	}

	recorder.AddAttribute(networkSpanID, "http.status_code", strconv.Itoa(response.StatusCode))
	recorder.EndSpan(networkSpanID, "ok", "")

	return nil
}
```

---

## 📡 Server Endpoint for Client Spans

### Backend Handler

**`internal/handlers/telemetry.go`**

```go
package handlers

import (
	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/telemetry"
)

// ClientSpan represents a span from a client
type ClientSpan struct {
	TraceID      string            `json:"trace_id"`
	SpanID       string            `json:"span_id"`
	ParentSpanID string            `json:"parent_span_id,omitempty"`
	Name         string            `json:"name"`
	StartTimeMs  int64             `json:"start_time_ms"`
	EndTimeMs    int64             `json:"end_time_ms"`
	Status       string            `json:"status"`
	Message      string            `json:"status_message,omitempty"`
	Attributes   map[string]string `json:"attributes"`
	ClientType   string            `json:"client_type"`
	ClientVer    string            `json:"client_version"`
}

// ReceiveClientSpans receives spans from clients (plugin, web, CLI)
func (h *Handlers) ReceiveClientSpans(c *gin.Context) {
	var req struct {
		Spans []ClientSpan `json:"spans"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(400, gin.H{"error": err.Error()})
		return
	}

	userID := c.GetString("user_id")

	for _, clientSpan := range req.Spans {
		// Create a synthetic server span that links to client span
		_, span := telemetry.GetBusinessEvents().TraceClientActivity(c.Request.Context(),
			telemetry.ClientActivityAttrs{
				TraceID:    clientSpan.TraceID,
				SpanID:     clientSpan.SpanID,
				ClientType: clientSpan.ClientType,
				Operation:  clientSpan.Name,
				Duration:   clientSpan.EndTimeMs - clientSpan.StartTimeMs,
				UserID:     userID,
			},
		)
		defer span.End()

		// Record client span attributes
		for k, v := range clientSpan.Attributes {
			span.SetAttributes(attribute.String("client."+k, v))
		}

		// Log for storage/analysis
		logger.InfoWithFields("Client span received",
			zap.String("trace_id", clientSpan.TraceID),
			zap.String("client_type", clientSpan.ClientType),
			zap.String("operation", clientSpan.Name),
			zap.Int64("duration_ms", clientSpan.EndTimeMs-clientSpan.StartTimeMs),
		)
	}

	c.JSON(200, gin.H{"status": "received"})
}
```

### Register Route

```go
// In cmd/server/main.go routes setup
r.POST("/api/v1/telemetry/spans", h.ReceiveClientSpans)
```

---

## 🔄 Complete Flow Example

Let's trace a real user action end-to-end:

### Scenario: User uploads loop from VST plugin

```
TIMELINE:

14:23:45.000 [PLUGIN] User clicks "Share" button
             ↓
             New TraceContext created
             traceId = "abc123def456"

14:23:45.100 [PLUGIN] Start span: plugin.upload_audio
             spanId = "span001"

14:23:45.150 [PLUGIN] Start span: plugin.encode_audio (child)
             spanId = "span002"
             parentSpanId = "span001"

14:23:48.150 [PLUGIN] End span: plugin.encode_audio (3000ms)

14:23:48.200 [PLUGIN] Start span: plugin.http_request (child)
             spanId = "span003"

14:23:48.220 [PLUGIN] HTTP POST /api/v1/posts
             Headers:
             - X-Trace-ID: abc123def456
             - X-Span-ID: span003
             - X-Parent-Span-ID: span001
             - X-Client-Type: plugin
             - X-Client-Timestamp: 1702834228220

14:23:48.340 [NETWORK] Request in flight (120ms)

14:23:48.340 [SERVER] POST /api/v1/posts received
             Extract trace ID from headers
             Start server root span: http.post.create_post
             spanId = "server_span001"
             parentSpanId = "span003" (from client)

14:23:48.341 [SERVER] db.query: INSERT audio_posts (5ms)

14:23:48.346 [SERVER] stream.io.create_activity (50ms)

14:23:48.396 [SERVER] End span: http.post.create_post
             Total: 56ms

14:23:48.397 [SERVER] Response headers:
             - X-Trace-ID: abc123def456 (echo back)
             - X-Span-ID: server_span001
             - X-Server-Time: 56

14:23:48.517 [PLUGIN] Received response (120ms network latency)

14:23:48.518 [PLUGIN] End span: plugin.http_request (318ms)

14:23:48.520 [PLUGIN] End span: plugin.upload_audio (3520ms total)

14:23:48.550 [PLUGIN] Send client spans to server
             POST /api/v1/telemetry/spans
             Sends: span001 (3520ms), span002 (3000ms), span003 (318ms)

14:23:48.650 [SERVER] Received client spans
             Create synthetic spans that link to network request

14:23:48.700 [USER] Sees "Post created!" message

COMPLETE TRACE (14:23:45 - 14:23:48.700):

abc123def456 (3700ms end-to-end)
├─ plugin.upload_audio (3520ms) [span001]
│  ├─ plugin.encode_audio (3000ms) [span002]
│  └─ plugin.http_request (318ms) [span003]
│     ├─ network_latency (120ms)
│     └─ server.http_post_create_post (56ms) [server_span001]
│        ├─ db.query: INSERT (5ms)
│        └─ stream.io.create_activity (50ms)
└─ plugin.ui_update (50ms)

BREAKDOWN:
- Plugin encoding: 3000ms (81%)
- Network: 120ms (3%)
- Server processing: 56ms (2%)
- Plugin UI update: 50ms (1%)
- Miscellaneous: 474ms (13%)

User would see in trace visualizer:
 [Plugin: 3000ms encoding] → [Network: 120ms] → [Server: 56ms]
 Total: 3176ms (plus UI processing)
```

---

## 🏪 Storage & Retention

### Store Client Spans in Database

Add to `models/client_telemetry.go`:

```go
type ClientTelemetrySpan struct {
	ID           string
	TraceID      string    // Foreign key to correlate with server traces
	SpanID       string
	ParentSpanID string
	ClientType   string    // "plugin" | "web" | "cli"
	ClientVer    string
	Operation    string    // "plugin.upload_audio" etc
	StartTimeMs  int64
	EndTimeMs    int64
	DurationMs   int64
	Status       string
	Message      string
	Attributes   datatypes.JSONMap

	// Correlation
	UserID       string
	CreatedAt    time.Time

	// Link to server request if applicable
	ServerTraceID string  // If this client call triggered a server request
}

// Migration
func (ClientTelemetrySpan) TableName() string {
	return "client_telemetry_spans"
}
```

### Retention Policy

```go
// In database/client_telemetry.go
func PruneOldClientSpans(db *gorm.DB, retentionDays int) error {
	cutoffTime := time.Now().AddDate(0, 0, -retentionDays)
	return db.Where("created_at < ?", cutoffTime).Delete(&models.ClientTelemetrySpan{}).Error
}

// Call daily: PruneOldClientSpans(db, 7)  // Keep 7 days
```

---

## 📊 Querying End-to-End Traces

### Dashboard Query: "Show me all spans for trace ID abc123"

```sql
-- SQL to find complete trace
SELECT * FROM (
  -- Server spans
  SELECT
    'server' as source,
    trace_id,
    span_id,
    name,
    duration_ms,
    EXTRACT(EPOCH FROM start_time)*1000 as start_time_ms,
    status
  FROM otel_spans
  WHERE trace_id = 'abc123def456'

  UNION ALL

  -- Client spans (linked to same trace)
  SELECT
    client_type as source,
    trace_id,
    span_id,
    operation as name,
    duration_ms,
    start_time_ms,
    status
  FROM client_telemetry_spans
  WHERE trace_id = 'abc123def456'
)
ORDER BY start_time_ms;

-- Results show complete picture:
-- plugin      | abc123 | span001 | upload_audio  | 3520  | 1702834225100
-- plugin      | abc123 | span002 | encode_audio  | 3000  | 1702834225150
-- plugin      | abc123 | span003 | http_request  | 318   | 1702834228200
-- server      | abc123 | srv001  | post_create   | 56    | 1702834228340
-- server      | abc123 | db001   | db.query      | 5     | 1702834228341
```

### Grafana Dashboard Query

```
# Find all traces that took longer than 2 seconds
SELECT trace_id, MAX(duration_ms) as max_duration
FROM (
  SELECT trace_id, duration_ms FROM otel_spans
  UNION ALL
  SELECT trace_id, duration_ms FROM client_telemetry_spans
)
GROUP BY trace_id
HAVING MAX(duration_ms) > 2000
ORDER BY max_duration DESC
LIMIT 10;
```

---

## 🔐 Security Considerations

### 1. **Mask Sensitive Data**

```go
// In client span handler
func maskAttributes(attrs map[string]string) map[string]string {
	// Don't send passwords, tokens, or file contents
	sensitiveKeys := []string{"password", "token", "api_key", "secret", "auth"}

	for _, key := range sensitiveKeys {
        if strings.Contains(strings.ToLower(key), key) {
            delete(attrs, key)
        }
    }
    return attrs
}
```

### 2. **Validate Trace IDs**

```go
// Ensure client can only send spans for their own traces
func (h *Handlers) ReceiveClientSpans(c *gin.Context) {
    userID := c.GetString("user_id")

    for _, span := range req.Spans {
        // Verify user created this trace
        if !isUserAuthorizedForTrace(userID, span.TraceID) {
            c.JSON(403, gin.H{"error": "unauthorized"})
            return
        }
    }
}
```

### 3. **Rate Limiting**

```go
// Prevent span spam
r.POST("/api/v1/telemetry/spans",
    middleware.RateLimit("100/minute"),
    h.ReceiveClientSpans,
)
```

---

## 📈 Implementation Roadmap

### Phase 1: Core Infrastructure (Week 1)
- [ ] Create TraceContext classes for all 3 clients
- [ ] Implement SpanRecorder for all 3 clients
- [ ] Add HTTP header propagation
- [ ] Create server endpoint to receive spans

### Phase 2: Plugin Integration (Week 2)
- [ ] Add tracing to VST plugin audio capture
- [ ] Track encoding, network, UI operations
- [ ] Send spans to server after each operation
- [ ] Test with real DAW

### Phase 3: Web App Integration (Week 3)
- [ ] Add tracing to React components
- [ ] Track page loads, API calls, interactions
- [ ] Implement TraceProvider context
- [ ] Test with real user workflows

### Phase 4: CLI Integration (Week 4)
- [ ] Add tracing to Go CLI commands
- [ ] Track upload, download, search operations
- [ ] Send spans after completion
- [ ] Test with shell scripts

### Phase 5: Dashboards & Analysis (Week 5)
- [ ] Create "End-to-End Traces" dashboard
- [ ] Implement trace timeline visualization
- [ ] Build latency breakdown charts
- [ ] Create alerts for slow operations

---

## 🎯 Benefits After Implementation

### For Debugging
```
User: "Uploading loops is taking 30 seconds"
You: "Share your trace ID"
User: "abc123def456"

You view trace:
- Plugin encoding: 25 seconds ← optimization opportunity
- Network: 2 seconds
- Server: 0.5 seconds

Action: Profile and optimize plugin's audio encoder
```

### For Performance Monitoring
```
Grafana Dashboard shows:
- Average plugin encoding: 3.2s
- Average network latency: 0.8s
- Average server time: 0.4s

Alert triggers if plugin encoding > 5s (something is slow on client machines)
```

### For Feature Development
```
You add a new feature that processes audio server-side.
With tracing, you can see:
- How much faster is server-side processing?
- Does it reduce overall time despite network overhead?
- Are there new bottlenecks?
```

---

## 📚 Documentation References

Related to this architecture:
- [Backend Tracing Setup](./OPENTELEMETRY_SETUP.md)
- [Backend Testing](./TESTING_ENHANCEMENTS.md)
- [Business Events](./ENHANCEMENTS_SUMMARY.md#enhancement-3-custom-business-event-tracing)

Next to create:
- Plugin Tracing Implementation Guide
- Web App Tracing Implementation Guide
- CLI Tracing Implementation Guide
- End-to-End Testing Guide
