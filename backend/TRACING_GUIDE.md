# Distributed Tracing Guide - Sidechain Backend

This guide explains how to use distributed tracing in the Sidechain backend to understand system behavior, debug issues, and monitor performance across services.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Setup & Configuration](#setup--configuration)
4. [Core Concepts](#core-concepts)
5. [Instrumentation Patterns](#instrumentation-patterns)
6. [Service-Specific Tracing](#service-specific-tracing)
7. [Correlation & Baggage](#correlation--baggage)
8. [Best Practices](#best-practices)
9. [Troubleshooting](#troubleshooting)
10. [Performance Considerations](#performance-considerations)

---

## Overview

Distributed tracing provides end-to-end visibility into requests as they flow through multiple services. In Sidechain, we use **OpenTelemetry** with **Tempo** as the backend to collect, store, and query traces.

### What You Get

- **Complete Request Flow**: See how a request travels through HTTP handlers, database queries, cache operations, and external service calls
- **Performance Metrics**: Latency, duration, and bottleneck identification
- **Error Tracking**: Understand where failures occur with full context
- **Service Dependencies**: Visualize how services interact
- **Resource Usage**: Track database queries, cache hits/misses, S3 operations, external API calls

### Key Traces Captured

| Component | What's Traced | Span Names |
|-----------|---------------|-----------|
| **HTTP Requests** | All incoming requests | `GET /api/v1/feed`, `POST /api/v1/posts` |
| **Database** | Query, Create, Update, Delete operations | `db.select`, `db.insert`, `db.update`, `db.delete` |
| **Redis Cache** | Get, Set, SetEx, Del operations | `redis.get`, `redis.set`, `redis.setex`, `redis.del` |
| **Stream.io** | Follow, Feed, Activity, Notification calls | `stream.follow`, `stream.create_activity`, etc. |
| **Elasticsearch** | Index, Search, Delete operations | `es.search`, `es.index`, `es.bulk` |
| **S3 Storage** | Upload, Download, Delete operations | `s3.put_object`, `s3.get_object`, `s3.delete_object` |
| **Gorse** | Recommendation engine calls | `gorse.insert_user`, `gorse.get_recommend` |
| **Business Logic** | High-level domain operations | `feed.get`, `feed.create_post`, `social.follow_user` |

---

## Architecture

### Stack

```
┌─────────────────────────────────────────────────────────────┐
│ VST Plugin / Client Applications                            │
│ (Makes HTTP requests, includes traceparent header)          │
└──────────────┬──────────────────────────────────────────────┘
               │
┌──────────────▼──────────────────────────────────────────────┐
│ HTTP Request → Gin Router                                   │
│  ├─ otelgin middleware creates root span                    │
│  ├─ correlation middleware adds correlation ID              │
│  └─ Requests flow to handlers                               │
└──────────────┬──────────────────────────────────────────────┘
               │
┌──────────────▼──────────────────────────────────────────────┐
│ Handler Layer (internal/handlers/)                          │
│  ├─ Creates business-level spans                            │
│  ├─ Propagates context to service calls                     │
│  └─ Records attributes and errors                           │
└──────────────┬──────────────────────────────────────────────┘
               │
        ┌──────┴──────┬───────────┬──────────┬──────────┐
        │             │           │          │          │
    ┌───▼──┐    ┌─────▼──┐  ┌────▼──┐  ┌──▼───┐  ┌────▼──┐
    │ GORM │    │ Redis  │  │Stream │  │ S3   │  │  ES   │
    │ (DB) │    │(Cache) │  │ .io   │  │(CDN) │  │(Search)
    │      │    │        │  │       │  │      │  │       │
    └──┬───┘    └────┬───┘  └───┬───┘  └──┬───┘  └───┬───┘
       │             │          │         │          │
       │    ┌────────▼──────────▼─────────▼──────────▼─────┐
       │    │ Context Propagation & Trace Injection       │
       │    │ (W3C Trace Context + Baggage headers)       │
       │    └────────┬──────────────────────────────────────┘
       │             │
       │    ┌────────▼──────────────────────────────┐
       │    │ OpenTelemetry SDK                     │
       │    │ ├─ Span collection                    │
       │    │ ├─ Sampling (configurable %)          │
       │    │ └─ OTLP HTTP exporter                 │
       │    └────────┬──────────────────────────────┘
       │             │
       └─────────────▼──────────────────────────────┐
                Tempo (OTLP HTTP Receiver)          │
                ├─ Span aggregation                 │
                ├─ Trace storage                    │
                └─ Query interface                  │
```

### Span Hierarchy Example

```
Root Span: POST /api/v1/posts [HTTP Request]
├─ Span: feed.create_post [Business Logic]
│  ├─ Span: db.insert [GORM - Save post]
│  ├─ Span: s3.put_object [S3 - Upload audio]
│  │  └─ Span: http.send [HTTP Client]
│  ├─ Span: es.index [Elasticsearch - Index post]
│  │  └─ Span: http.send [HTTP Client]
│  └─ Span: gorse.insert_user_item [Gorse - Update recommendations]
│     └─ Span: http.send [HTTP Client]
└─ Span: middleware.response_cache [Response caching]
```

---

## Setup & Configuration

### Enable Tracing

```bash
# In .env file
OTEL_ENABLED=true
OTEL_SERVICE_NAME=sidechain-backend
OTEL_ENVIRONMENT=development
OTEL_EXPORTER_OTLP_ENDPOINT=localhost:4318
OTEL_TRACE_SAMPLER_RATE=1.0  # 100% sampling (1.0 = 100%, 0.1 = 10%)
```

### Production Configuration

```bash
# For production, use lower sampling rate to reduce load
OTEL_ENABLED=true
OTEL_SERVICE_NAME=sidechain-backend
OTEL_ENVIRONMENT=production
OTEL_EXPORTER_OTLP_ENDPOINT=tempo.monitoring.svc.cluster.local:4318
OTEL_TRACE_SAMPLER_RATE=0.1  # 10% sampling
LOG_LEVEL=warn
```

### Docker Compose with Tempo

```yaml
version: '3.8'
services:
  backend:
    environment:
      OTEL_ENABLED: "true"
      OTEL_EXPORTER_OTLP_ENDPOINT: "tempo:4318"
    depends_on:
      - tempo

  tempo:
    image: grafana/tempo:latest
    ports:
      - "4317:4317"   # OTLP gRPC receiver
      - "4318:4318"   # OTLP HTTP receiver
    volumes:
      - tempo_storage:/var/tempo

volumes:
  tempo_storage:
```

---

## Core Concepts

### Spans

A **span** represents a single operation in a distributed system.

```go
ctx, span := tracer.Start(ctx, "operation_name")
defer span.End()

// Set attributes
span.SetAttributes(
    attribute.String("user.id", userID),
    attribute.Int("count", 42),
)

// Record errors
if err != nil {
    span.RecordError(err, trace.WithStackTrace(true))
    span.SetStatus(codes.Error, err.Error())
}

// Set successful status
span.SetStatus(codes.Ok, "")
```

### Context Propagation

Context must be passed through the entire call chain for traces to be connected:

```go
// ✓ CORRECT: Pass context to db.WithContext(ctx)
user := models.User{}
db.WithContext(ctx).First(&user, id)

// ✗ WRONG: Using context.Background() breaks trace chain
user := models.User{}
db.WithContext(context.Background()).First(&user, id)
```

### Attributes

Attributes provide structured metadata for spans:

```go
// Standard semantic conventions
span.SetAttributes(
    attribute.String("http.method", "POST"),
    attribute.String("http.url", "/api/v1/posts"),
    attribute.Int("http.status_code", 201),
    attribute.String("user.id", userID),
)

// Custom attributes
span.SetAttributes(
    attribute.String("cache.key", "feed:user123"),
    attribute.Bool("cache.hit", true),
    attribute.Int("feed.item_count", 20),
)
```

### Correlation IDs

Correlation IDs track related operations across multiple requests:

```go
// In middleware, correlation ID is set automatically
// In background tasks, use correlation from context:
correlationID := middleware.GetCorrelationIDFromContext(ctx)
span.SetAttributes(attribute.String("trace.correlation_id", correlationID))
```

---

## Instrumentation Patterns

### Pattern 1: Simple Handler

Trace a handler that queries the database:

```go
func (h *Handlers) GetUser(c *gin.Context) {
    userID := c.Param("id")

    // Get context from Gin (already has trace info)
    ctx := c.Request.Context()

    // Fetch user (GORM plugin will create db.select span automatically)
    user := models.User{}
    if err := h.DB.WithContext(ctx).First(&user, userID).Error; err != nil {
        c.JSON(404, gin.H{"error": "User not found"})
        return
    }

    c.JSON(200, user)
}
// Result: Root HTTP span → db.select child span
```

### Pattern 2: Cache-Aside Pattern

Trace cache operations with database fallback:

```go
func (h *Handlers) GetUser(c *gin.Context) {
    ctx := c.Request.Context()
    userID := c.Param("id")
    cacheKey := "user:" + userID

    // Try cache first
    ctx, cacheSpan := TraceCacheCall(ctx, "get", map[string]interface{}{
        "key": cacheKey,
    })

    cached, err := h.Cache.Get(ctx, cacheKey)
    hit := err == nil
    cacheSpan.SetAttributes(attribute.Bool("cache.hit", hit))
    cacheSpan.End()

    var user models.User
    if hit {
        json.Unmarshal([]byte(cached), &user)
    } else {
        // Cache miss → fetch from DB
        if err := h.DB.WithContext(ctx).First(&user, userID).Error; err != nil {
            c.JSON(404, gin.H{"error": "Not found"})
            return
        }

        // Cache for next time
        ctx, setCacheSpan := TraceCacheCall(ctx, "set", map[string]interface{}{
            "key": cacheKey,
            "ttl_seconds": 300,
        })
        if data, _ := json.Marshal(user); data != nil {
            h.Cache.SetEx(ctx, cacheKey, string(data), 5*time.Minute)
        }
        setCacheSpan.End()
    }

    c.JSON(200, user)
}
// Result: HTTP span → cache.get span → db.select span (on miss) → cache.set span
```

### Pattern 3: External API Calls with Retries

Trace API calls with retry logic:

```go
func (h *Handlers) FollowUser(c *gin.Context) {
    ctx := c.Request.Context()
    targetUserID := c.Param("id")
    currentUserID := c.Get("user_id").(string)

    ctx, span := TraceStreamIOCall(ctx, "follow", map[string]interface{}{
        "user_id": currentUserID,
        "target_id": targetUserID,
    })
    defer span.End()

    var lastErr error
    for attempt := 0; attempt < 3; attempt++ {
        result, err := h.StreamClient.FollowUser(ctx, currentUserID, targetUserID)
        if err == nil {
            RecordServiceSuccess(span, map[string]interface{}{
                "duration_ms": time.Since(start).Milliseconds(),
            })
            c.JSON(200, result)
            return
        }

        lastErr = err
        span.SetAttributes(attribute.Int("external.retry_count", attempt+1))
        time.Sleep(time.Duration(math.Pow(2, float64(attempt))) * time.Second)
    }

    RecordServiceError(span, "stream.io", lastErr)
    c.JSON(500, gin.H{"error": "Failed to follow user"})
}
// Result: HTTP span → stream.follow span with retry attributes
```

### Pattern 4: Multi-Step Operation

Trace a complex operation with multiple child spans:

```go
func (h *Handlers) CreatePost(c *gin.Context) {
    ctx := c.Request.Context()
    postID := uuid.New().String()

    ctx, rootSpan := GetBusinessEvents().TraceCreatePost(ctx, postID, "mp3")
    defer rootSpan.End()

    post := models.AudioPost{
        ID:        postID,
        Title:     c.PostForm("title"),
        UserID:    c.Get("user_id").(string),
    }

    // Step 1: Save to database
    if err := h.DB.WithContext(ctx).Create(&post).Error; err != nil {
        rootSpan.RecordError(err)
        c.JSON(500, gin.H{"error": "Failed to create post"})
        return
    }

    // Step 2: Upload to S3
    file, _ := c.FormFile("audio")
    ctx, s3Span := TraceS3Call(ctx, "put_object", map[string]interface{}{
        "bucket": h.S3Bucket,
        "key": postID,
        "size_bytes": file.Size,
    })
    if err := h.Storage.Upload(ctx, postID, file); err != nil {
        RecordServiceError(s3Span, "s3", err)
        s3Span.End()
        c.JSON(500, gin.H{"error": "Upload failed"})
        return
    }
    s3Span.End()

    // Step 3: Index in Elasticsearch
    ctx, esSpan := TraceElasticsearchCall(ctx, "index", map[string]interface{}{
        "index": "posts",
        "doc_id": postID,
    })
    if err := h.Search.Index(ctx, postID, post); err != nil {
        RecordServiceError(esSpan, "elasticsearch", err)
        esSpan.End()
        // Don't fail if search indexing fails
    } else {
        esSpan.End()
    }

    // Step 4: Update recommendations
    ctx, gorseSpan := TraceGorseCall(ctx, "insert_user_item", map[string]interface{}{
        "user_id": post.UserID,
        "item_id": postID,
    })
    if err := h.Recommendations.IndexPost(ctx, post.UserID, postID); err != nil {
        RecordServiceError(gorseSpan, "gorse", err)
    }
    gorseSpan.End()

    c.JSON(201, post)
}
// Result: HTTP span → feed.create_post span → db.insert, s3.put_object, es.index, gorse.insert_user_item child spans
```

---

## Service-Specific Tracing

### Database (GORM)

The GORM tracing plugin is automatically registered. Traces are created for all queries:

```go
// Automatically traced - creates db.select, db.insert, db.update, db.delete spans
h.DB.WithContext(ctx).First(&user, id)
h.DB.WithContext(ctx).Create(&post)
h.DB.WithContext(ctx).Save(&user)
h.DB.WithContext(ctx).Delete(&post)

// Always pass context!
```

### Redis Cache

Cache operations are traced with hit/miss information:

```go
// Automatically traced
value, err := h.Cache.Get(ctx, "key")
h.Cache.SetEx(ctx, "key", value, 5*time.Minute)
h.Cache.Del(ctx, "key")

// Shows in traces with cache.hit, cache.key, cache.operation attributes
```

### Stream.io

Trace Stream.io integration:

```go
ctx, span := TraceStreamIOCall(ctx, "follow", map[string]interface{}{
    "user_id": currentUserID,
    "target_id": targetUserID,
})
defer span.End()

result, err := h.StreamClient.FollowUser(ctx, currentUserID, targetUserID)
if err != nil {
    RecordServiceError(span, "stream.io", err)
}
```

### Elasticsearch

Trace search operations:

```go
ctx, span := TraceElasticsearchCall(ctx, "search", map[string]interface{}{
    "index": "posts",
    "query": "electronic drums",
})
defer span.End()

results, err := h.Search.Search(ctx, "electronic drums")
if err != nil {
    RecordServiceError(span, "elasticsearch", err)
}
```

### S3 Storage

Trace file uploads/downloads:

```go
ctx, span := TraceS3Call(ctx, "put_object", map[string]interface{}{
    "bucket": "sidechain-audio",
    "key": postID,
    "size_bytes": fileSize,
})
defer span.End()

err := h.Storage.Upload(ctx, postID, fileData)
if err != nil {
    RecordServiceError(span, "s3", err)
}
```

### Gorse Recommendations

Trace recommendation engine:

```go
ctx, span := TraceGorseCall(ctx, "get_recommend", map[string]interface{}{
    "user_id": userID,
    "count": 10,
})
defer span.End()

recommendations, err := h.Recommendations.GetRecommendations(ctx, userID, 10)
if err != nil {
    RecordServiceError(span, "gorse", err)
}
```

---

## Correlation & Baggage

### Automatic Correlation ID

Correlation IDs are automatically set by middleware:

```go
// In HTTP request
GET /api/v1/posts HTTP/1.1
X-Correlation-ID: user-123-action-456

// Middleware automatically:
// 1. Extracts X-Correlation-ID from request header
// 2. Adds to response header X-Correlation-ID
// 3. Stores in context baggage
// 4. Propagates in all downstream calls
```

### Manual Correlation ID in Background Tasks

For background jobs that don't have HTTP context:

```go
func (h *Handlers) ProcessAudioInBackground(ctx context.Context, postID string) {
    // Get correlation ID from context
    correlationID := middleware.GetCorrelationIDFromContext(ctx)

    ctx, span := GetBusinessEvents().TraceAudioProcessing(ctx, postID, "processing")
    defer span.End()

    if correlationID != "" {
        span.SetAttributes(attribute.String("trace.correlation_id", correlationID))
    }

    // Do work...
}
```

### Baggage for Metadata

Baggage allows non-sensitive metadata to flow with traces:

```go
// In middleware, baggage is set automatically
// In your code, retrieve it:
ctx := c.Request.Context()
samplingHint := middleware.GetSamplingHintFromContext(ctx)

if samplingHint == "high-priority" {
    // Sample this request at higher rate
}
```

---

## Best Practices

### 1. **Always Defer Span.End()**

```go
// ✓ CORRECT
ctx, span := tracer.Start(ctx, "operation")
defer span.End()

// ✗ WRONG - Span never ends, resources leak
ctx, span := tracer.Start(ctx, "operation")
// Missing: defer span.End()
```

### 2. **Pass Context Through Entire Call Chain**

```go
// ✓ CORRECT
user, err := h.GetUser(ctx, userID)  // Pass ctx
h.DB.WithContext(ctx).First(&user)    // Pass ctx to DB

// ✗ WRONG - Context lost, trace chain broken
user, err := h.GetUser(context.Background(), userID)
h.DB.First(&user)  // No context
```

### 3. **Use Helper Functions for Consistency**

```go
// ✓ CORRECT - Consistent attribute names
ctx, span := TraceStreamIOCall(ctx, "follow", map[string]interface{}{
    "user_id": userID,
})

// ✗ WRONG - Inconsistent naming makes querying hard
ctx, span := tracer.Start(ctx, "stream_follow")
span.SetAttributes(attribute.String("uid", userID))
```

### 4. **Truncate Long Values**

```go
// ✓ CORRECT - Avoid cardinality explosion
query := "SELECT * FROM posts WHERE title LIKE ?"
if len(query) > 200 {
    query = query[:200] + "..."
}
span.SetAttributes(attribute.String("db.statement", query))

// ✗ WRONG - Every unique query creates new span series
span.SetAttributes(attribute.String("db.statement", entireQueryString))
```

### 5. **Record Error Context**

```go
// ✓ CORRECT
if err != nil {
    span.RecordError(err, trace.WithStackTrace(true))
    span.SetStatus(codes.Error, err.Error())
}

// ✗ WRONG - Error not recorded
if err != nil {
    return err
}
```

### 6. **Use Sampling Wisely**

```bash
# Development: 100% sampling
OTEL_TRACE_SAMPLER_RATE=1.0

# Production: Lower sampling to reduce overhead
OTEL_TRACE_SAMPLER_RATE=0.1  # 10%

# High-traffic production: Even lower
OTEL_TRACE_SAMPLER_RATE=0.01  # 1%
```

### 7. **Don't Trace Sensitive Data**

```go
// ✓ CORRECT - Hash or mask sensitive data
span.SetAttributes(
    attribute.String("user.email", hashEmail(email)),
)

// ✗ WRONG - Exposing sensitive data
span.SetAttributes(
    attribute.String("user.password", password),
    attribute.String("credit_card", cardNumber),
)
```

### 8. **Set Meaningful Span Names**

```go
// ✓ CORRECT - Specific operation
_, span := tracer.Start(ctx, "user.follow")
_, span := tracer.Start(ctx, "audio.upload")
_, span := tracer.Start(ctx, "feed.enrichment")

// ✗ WRONG - Vague names
_, span := tracer.Start(ctx, "do_work")
_, span := tracer.Start(ctx, "handle_request")
```

---

## Troubleshooting

### Problem: No traces appearing in Tempo

**Check**:
1. Is OTEL_ENABLED=true?
2. Is OTEL_EXPORTER_OTLP_ENDPOINT correct?
3. Is Tempo running and accessible?
4. Check logs: `docker logs backend 2>&1 | grep -i otel`

```bash
# Test connectivity to Tempo
curl http://localhost:4318/v1/traces

# Check if tracing is enabled
grep "OpenTelemetry" server.log
```

### Problem: Traces seem incomplete (missing database queries)

**Check**:
1. Are you passing `ctx` to `db.WithContext(ctx)`?
2. Is the GORM tracing plugin registered?

```go
// Verify plugin is registered in database.go
db.Use(telemetry.GORMTracingPlugin())
```

### Problem: Too many traces creating storage issues

**Solution**: Reduce sampling rate

```bash
# In production
OTEL_TRACE_SAMPLER_RATE=0.1  # Trace only 10% of requests
```

### Problem: Trace context not propagating to background jobs

**Solution**: Extract correlation ID and pass it explicitly

```go
correlationID := middleware.GetCorrelationIDFromContext(ctx)
// Pass correlationID to background task
// Recreate context with correlation ID in background task
```

---

## Performance Considerations

### Overhead

- **Database tracing**: ~1-2% overhead per query
- **Cache tracing**: <1% overhead
- **External API tracing**: ~5-10% (mostly HTTP client instrumentation)
- **Span collection**: <1% when sampling at 10%

### Optimization Tips

1. **Use appropriate sampling rate**:
   - Development/staging: 100% (1.0)
   - Production: 10% (0.1) or lower
   - High-traffic production: 1% (0.01)

2. **Avoid high-cardinality attributes**:
   ```go
   // ✓ GOOD - Low cardinality
   span.SetAttributes(attribute.String("user.id", userID))
   span.SetAttributes(attribute.String("post.id", postID))

   // ✗ BAD - High cardinality (each unique value = new span series)
   span.SetAttributes(attribute.String("user.email", userEmail))
   span.SetAttributes(attribute.String("full.sql.query", sqlQuery))
   ```

3. **Batch expensive operations**:
   ```go
   // Instead of tracing each individual operation
   for i := 0; i < 1000; i++ {
       ctx, span := tracer.Start(ctx, "operation")
       // ...
       span.End()
   }

   // Trace the batch instead
   ctx, span := tracer.Start(ctx, "batch_operation")
   span.SetAttributes(attribute.Int("batch.size", 1000))
   for i := 0; i < 1000; i++ {
       // Do work
   }
   span.End()
   ```

4. **Use span links for causality without hierarchy**:
   ```go
   // When you want to relate spans that aren't parent-child
   oldCtx := extractFromQueue()
   ctx, span := tracer.Start(ctx, "process_async",
       trace.WithLinks(trace.LinkFromContext(oldCtx)),
   )
   ```

---

## Querying Traces in Tempo/Grafana

### Example Queries

**Find all traces for a user**:
```
{ user.id="user123" }
```

**Find slow requests**:
```
{ duration > 1000ms }
```

**Find failed operations**:
```
{ status="error" }
```

**Find traces with specific service**:
```
{ span.service="stream.io" }
```

**Chain operations**:
```
root_name =~ "POST.*" && resource.service.name="sidechain-backend"
```

---

## Related Documentation

- [OpenTelemetry Go Documentation](https://opentelemetry.io/docs/instrumentation/go/)
- [Tempo Query Language](https://grafana.com/docs/tempo/latest/querying/trace-query-language/)
- [Semantic Conventions](https://opentelemetry.io/docs/specs/semconv/)
