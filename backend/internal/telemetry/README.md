# Telemetry Package

This package provides comprehensive distributed tracing capabilities using OpenTelemetry.

## Quick Start

### 1. Enable Tracing

```bash
# In .env
OTEL_ENABLED=true
OTEL_EXPORTER_OTLP_ENDPOINT=localhost:4318
OTEL_TRACE_SAMPLER_RATE=1.0  # 100% for development
```

### 2. Trace a Handler

```go
func (h *Handlers) GetUser(c *gin.Context) {
    ctx := c.Request.Context()  // Get context with trace info
    userID := c.Param("id")

    // Database tracing happens automatically via GORM plugin
    user := models.User{}
    if err := h.DB.WithContext(ctx).First(&user, userID).Error; err != nil {
        c.JSON(404, gin.H{"error": "Not found"})
        return
    }

    c.JSON(200, user)
}
// Result: HTTP request span → automatic db.select child span
```

### 3. Trace Cache Operations

```go
ctx, span := telemetry.TraceCacheCall(ctx, "get", map[string]interface{}{
    "key": "user:123",
})
defer span.End()

cached, err := h.Cache.Get(ctx, "user:123")
if err != nil {
    span.SetAttributes(attribute.Bool("cache.hit", false))
}
```

### 4. Trace External API Calls

```go
ctx, span := telemetry.TraceStreamIOCall(ctx, "follow", map[string]interface{}{
    "user_id": currentUserID,
    "target_id": targetUserID,
})
defer span.End()

result, err := h.StreamClient.FollowUser(ctx, currentUserID, targetUserID)
if err != nil {
    telemetry.RecordServiceError(span, "stream.io", err)
} else {
    telemetry.RecordServiceSuccess(span, map[string]interface{}{})
}
```

## Files in This Package

### Core Files

| File | Purpose |
|------|---------|
| `tracer.go` | OpenTelemetry initialization and configuration |
| `database.go` | GORM plugin for automatic database query tracing |
| `business_events.go` | Helper functions for high-level domain operations |
| `http_client.go` | HTTP client instrumentation and context propagation |
| `service_calls.go` | Service-specific tracing helpers (Stream.io, Gorse, Elasticsearch, S3, Cache) |
| `handler_examples.go` | Example patterns for using tracing in handlers |

### Configuration

- OpenTelemetry is initialized in `backend/cmd/server/main.go`
- Configuration is read from environment variables (see `.env.example`)
- GORM plugin is automatically registered in `backend/internal/database/database.go`

## Usage Patterns

### Pattern 1: Automatic Tracing (Database)

No code needed - just pass `ctx` to database calls:

```go
// Database calls are automatically traced
h.DB.WithContext(ctx).First(&user, id)
h.DB.WithContext(ctx).Create(&post)
h.DB.WithContext(ctx).Find(&posts)
```

### Pattern 2: Cache with Business Event

```go
ctx, span := GetBusinessEvents().TraceGetFeed(ctx, "timeline", FeedEventAttrs{
    Limit: 20,
    Offset: 0,
})
defer span.End()

// Try cache
data, err := h.Cache.Get(ctx, "feed:user123")
if err == nil {
    // Cache hit
    return data
}

// Cache miss - fetch from DB
// This creates db.select child span automatically
h.DB.WithContext(ctx).Find(&posts)
```

### Pattern 3: External API with Error Handling

```go
ctx, span := TraceStreamIOCall(ctx, "follow", map[string]interface{}{
    "user_id": userID,
})
defer span.End()

var lastErr error
for attempt := 0; attempt < 3; attempt++ {
    result, err := h.StreamClient.FollowUser(ctx, userID, targetUserID)
    if err == nil {
        RecordServiceSuccess(span, map[string]interface{}{})
        return result
    }
    lastErr = err
    time.Sleep(exponentialBackoff(attempt))
}

RecordServiceError(span, "stream.io", lastErr)
return nil, lastErr
```

### Pattern 4: Multi-Step Operation

```go
ctx, mainSpan := GetBusinessEvents().TraceCreatePost(ctx, postID, "mp3")
defer mainSpan.End()

// Step 1: Save to DB (automatic tracing)
if err := h.DB.WithContext(ctx).Create(&post).Error; err != nil {
    mainSpan.RecordError(err)
    return err
}

// Step 2: Upload to S3
ctx, s3Span := TraceS3Call(ctx, "put_object", map[string]interface{}{
    "bucket": "audio",
    "size_bytes": fileSize,
})
if err := h.Storage.Upload(ctx, fileData); err != nil {
    RecordServiceError(s3Span, "s3", err)
    s3Span.End()
    return err
}
s3Span.End()

// Step 3: Index in Elasticsearch
ctx, esSpan := TraceElasticsearchCall(ctx, "index", map[string]interface{}{
    "index": "posts",
})
if err := h.Search.Index(ctx, post); err != nil {
    RecordServiceError(esSpan, "elasticsearch", err)
    esSpan.End()
    // Don't fail if search fails
}
esSpan.End()
```

## Key Functions

### Business Events

```go
// High-level domain operations
be := GetBusinessEvents()

be.TraceGetFeed(ctx, feedType, attrs)
be.TraceCreatePost(ctx, postID, audioFormat)
be.TracePostEnrichment(ctx, enrichmentType, count)
be.TraceFollowUser(ctx, userID, targetUserID)
be.TraceSocialInteraction(ctx, actionType, attrs)
be.TraceCreateComment(ctx, postID, commentID, hasReply)
be.TraceAudioUpload(ctx, audioPostID, attrs)
be.TraceAudioProcessing(ctx, audioPostID, status)
be.TraceSearch(ctx, attrs)
be.TraceEngagement(ctx, eventType, attrs)
be.TraceExternalAPI(ctx, service, operation)
```

### Service Calls

```go
// Service-specific tracing
TraceStreamIOCall(ctx, "follow", map[string]interface{}{...})
TraceGorseCall(ctx, "insert_user_item", map[string]interface{}{...})
TraceElasticsearchCall(ctx, "search", map[string]interface{}{...})
TraceS3Call(ctx, "put_object", map[string]interface{}{...})
TraceCacheCall(ctx, "get", map[string]interface{}{...})

// Error/success recording
RecordServiceError(span, "service-name", err)
RecordServiceSuccess(span, map[string]interface{}{...})
```

### Context & Correlation

```go
// Set user/request context
SetUserContext(span, userID, organizationID)
SetRequestContext(span, requestID, userAgent)
SetCorrelationID(span, correlationID)
```

## Best Practices

1. **Always defer span.End()**
   ```go
   ctx, span := tracer.Start(ctx, "operation")
   defer span.End()  // Always do this!
   ```

2. **Pass context through the chain**
   ```go
   h.DB.WithContext(ctx)           // ✓ Correct
   h.Cache.Get(ctx, key)            // ✓ Correct
   h.StreamClient.Follow(ctx, ...)   // ✓ Correct
   ```

3. **Use helper functions for consistency**
   ```go
   TraceStreamIOCall(ctx, "follow", attrs)  // ✓ Consistent naming
   tracer.Start(ctx, "stream_follow")       // ✗ Inconsistent
   ```

4. **Truncate long values**
   ```go
   query := bigQuery
   if len(query) > 200 {
       query = query[:200] + "..."
   }
   span.SetAttributes(attribute.String("db.statement", query))
   ```

5. **Record meaningful attributes**
   ```go
   span.SetAttributes(
       attribute.String("user.id", userID),    // ✓ Specific
       attribute.Int("post.count", 42),        // ✓ Specific
   )
   ```

## Monitoring & Querying

### In Tempo/Grafana

```
# Find traces for a user
{ user.id="user123" }

# Find slow requests
{ duration > 1000ms }

# Find failed operations
{ status="error" }

# Specific service
{ external.service="stream.io" }
```

### Local Testing

```bash
# Check if tracing is enabled
grep "OpenTelemetry" server.log

# Test connection to Tempo
curl http://localhost:4318/v1/traces

# View backend logs with trace context
docker logs backend 2>&1 | grep "trace_id"
```

## Configuration

### Development

```bash
OTEL_ENABLED=true
OTEL_TRACE_SAMPLER_RATE=1.0  # Trace everything
OTEL_EXPORTER_OTLP_ENDPOINT=localhost:4318
```

### Production

```bash
OTEL_ENABLED=true
OTEL_TRACE_SAMPLER_RATE=0.1  # Trace 10% of requests
OTEL_EXPORTER_OTLP_ENDPOINT=tempo.monitoring:4318
```

## Related Documentation

- **Full Guide**: See `backend/TRACING_GUIDE.md` for comprehensive documentation
- **OpenTelemetry**: https://opentelemetry.io/docs/instrumentation/go/
- **Tempo**: https://grafana.com/docs/tempo/latest/
- **Example Handlers**: See `handler_examples.go` in this package

## Troubleshooting

**Q: Traces not appearing in Tempo?**
A: Check OTEL_ENABLED=true and OTEL_EXPORTER_OTLP_ENDPOINT is correct

**Q: Missing database traces?**
A: Ensure you're passing `ctx` to db.WithContext(ctx)

**Q: Too much data / storage issues?**
A: Lower OTEL_TRACE_SAMPLER_RATE (e.g., 0.1 for 10% sampling)

**Q: Slow performance?**
A: Reduce sampling rate or disable OTEL_ENABLED in load testing
