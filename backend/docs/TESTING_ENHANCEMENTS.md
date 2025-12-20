# Testing Distributed Tracing Enhancements

This guide walks through testing all OpenTelemetry tracing enhancements implemented in Phase 1.

## ✅ Quick Start

### 1. Start Services
```bash
# Start backend, Tempo, Loki, Promtail, Grafana
docker-compose -f docker-compose.dev.yml up -d

# Verify services are healthy
sleep 10
curl http://localhost:3200/ready       # Tempo
curl http://localhost:3100/ready       # Loki
curl http://localhost:9090/graph       # Prometheus
curl http://localhost:3000             # Grafana
```

### 2. Verify Backend with Tracing Enabled
```bash
cd backend
OTEL_ENABLED=true go run cmd/server/main.go
```

Look for log output: `✅ OpenTelemetry tracing enabled`

### 3. Generate Traces
```bash
# Health checks (simple HTTP requests)
for i in {1..5}; do curl http://localhost:8787/health; done

# Post creation (comprehensive tracing: HTTP, DB, business event, Stream.io)
TOKEN="<your-token>"
curl -X POST http://localhost:8787/api/v1/posts \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "audio_url": "https://example.com/audio.mp3",
    "bpm": 120,
    "key": "C",
    "daw": "ableton"
  }'

# Search query (business event, Elasticsearch)
curl "http://localhost:8787/api/v1/search/posts?q=electronic&genre=electronic&bpm_min=110&bpm_max=130"

# Follow user (business event, Stream.io)
curl -X POST http://localhost:8787/api/v1/users/follow \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"target_user_id": "<user-id>"}'
```

### 4. Wait for Batch Flush
Traces are batched by default:
- Batch size: 1024 spans
- Timeout: 10 seconds

```bash
sleep 3  # Wait for export
```

---

## 🔍 Testing by Component

### A. HTTP Request Tracing + Trace ID in Logs

**Files Modified:**
- `internal/middleware/telemetry.go` - HTTP span creation
- `internal/middleware/gin_logger.go` - Extract trace_id from context

**Test Steps:**

1. **Make HTTP Request:**
   ```bash
   curl http://localhost:8787/health
   ```

2. **Verify in Backend Logs:**
   ```
   ✓ Log line includes: trace_id, span_id, method, path, status, latency
   Example: {"level":"info","msg":"HTTP request","method":"GET","path":"/health","trace_id":"3fa9d5...","span_id":"8e47c...","status":200}
   ```

3. **Verify in Grafana:**
   - Go to: http://localhost:3000/d/distributed-tracing
   - Look for request metrics (already working from Prometheus)

**Expected Behavior:**
- ✅ Every HTTP request creates a root span
- ✅ Every log includes trace_id and span_id
- ✅ Trace IDs are consistent across logs from same request

---

### B. Database Query Tracing

**File Modified:**
- `internal/telemetry/database.go` - GORM plugin for tracing

**Test Steps:**

1. **Make Request That Queries Database:**
   ```bash
   TOKEN="<your-token>"
   curl -H "Authorization: Bearer $TOKEN" \
     http://localhost:8787/api/v1/users/me
   ```
   (This queries user from database)

2. **View Trace in Grafana/Tempo:**
   - Go to: http://localhost:3000/explore
   - Select "Tempo" datasource
   - Find the trace for `/api/v1/users/me`
   - Expand to see child spans

3. **Expected Trace Structure:**
   ```
   GET /api/v1/users/me (root span)
   ├── db.query (child span)
   │   ├── operation: SELECT
   │   ├── table: users
   │   └── duration: ~5ms
   └── response sent (status: 200)
   ```

**Verify in Logs:**
```bash
# Check logs for db query spans
docker logs sidechain-backend 2>&1 | grep -E "trace_id|database|query"
```

**Expected Behavior:**
- ✅ Every database query creates a child span
- ✅ Span includes table name, operation (SELECT/INSERT/UPDATE/DELETE)
- ✅ Query duration measured automatically
- ✅ Errors recorded with stack traces

---

### C. Redis Cache Tracing

**File Modified:**
- `internal/cache/redis.go` - Tracing wrappers on Get/Set

**Test Steps:**

1. **Make Request That Uses Cache:**
   ```bash
   TOKEN="<your-token>"
   # This should use Redis cache if feed is cached
   curl -H "Authorization: Bearer $TOKEN" \
     http://localhost:8787/api/v1/feed/global?limit=5
   ```

2. **View Cache Spans in Tempo:**
   - Go to Explore → Tempo
   - Search for `feed.get_global` or similar
   - Look for `redis.get` child spans

3. **Expected Span Details:**
   ```
   redis.get (span)
   ├── cache.key: "feed:*" (masked)
   ├── cache.operation: "get"
   ├── cache.hit: true/false
   └── cache.status: "success"/"miss"/"error"
   ```

4. **Verify Cache Hit vs Miss:**
   ```bash
   # First request (cache miss)
   curl http://localhost:8787/api/v1/feed/global?limit=5 -H "Authorization: Bearer $TOKEN"

   # Check Tempo for: cache.hit: false, cache.status: "miss"

   # Second request (cache hit)
   curl http://localhost:8787/api/v1/feed/global?limit=5 -H "Authorization: Bearer $TOKEN"

   # Check Tempo for: cache.hit: true, cache.status: "success"
   ```

**Expected Behavior:**
- ✅ Cache operations create spans with `redis.get`/`redis.set`
- ✅ Cache hit/miss status recorded
- ✅ Sensitive keys masked in logs
- ✅ Errors (if any) recorded in span

---

### D. Custom Business Event Tracing

**Files Modified:**
- `internal/telemetry/business_events.go` - New business event helpers
- `internal/handlers/feed.go` - TraceCreatePost
- `internal/handlers/user.go` - TraceFollowUser
- `internal/handlers/discovery.go` - TraceSearch (SearchUsers, SearchPosts)

#### D1. Feed Creation Trace

**Test:**
```bash
TOKEN="<your-token>"
curl -X POST http://localhost:8787/api/v1/posts \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "audio_url": "https://example.com/loop.mp3",
    "bpm": 120,
    "key": "C",
    "daw": "ableton",
    "duration_bars": 4
  }'
```

**Verify in Tempo:**
- Look for span: `feed.create_post`
- Check attributes:
  - `post.id`: UUID of created post
  - `audio.format`: extracted from URL

**Expected Trace Structure:**
```
POST /api/v1/posts (root HTTP span)
├── feed.create_post (business event span)
│   ├── post.id: "abc123..."
│   └── audio.format: "mp3"
├── db.query: Create AudioPost
├── db.query: Create MidiPattern
├── stream.io.create_activity (if enabled)
└── response sent (status: 201)
```

#### D2. Follow User Trace

**Test:**
```bash
TOKEN="<your-token>"
curl -X POST http://localhost:8787/api/v1/users/follow \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"target_user_id": "<another-user-id>"}'
```

**Verify in Tempo:**
- Look for span: `social.follow_user`
- Check attributes:
  - `user.id`: follower ID
  - `target_user.id`: user being followed

**Expected Structure:**
```
POST /api/v1/users/follow (root)
├── social.follow_user (business event)
│   ├── user.id: "user1..."
│   └── target_user.id: "user2..."
├── stream.io.follow (if enabled)
├── db.query: Insert Follow relationship
└── response sent (status: 200)
```

#### D3. Search Trace

**Test:**
```bash
# Search users
curl "http://localhost:8787/api/v1/search/users?q=producer"

# Search posts with filters
curl "http://localhost:8787/api/v1/search/posts?q=electronic&genre=electronic&bpm_min=110&bpm_max=130&key=A"
```

**Verify in Tempo:**
- Look for span: `search.query`
- Check attributes:
  - `search.query`: search term
  - `search.index`: "users" or "posts"
  - `search.filters`: ["genre:electronic", "bpm:110-130", "key:A"]
  - `search.result_count`: number of results

**Expected Structure:**
```
GET /api/v1/search/posts (root)
├── search.query (business event)
│   ├── search.query: "electronic"
│   ├── search.index: "posts"
│   ├── search.filters: ["genre:electronic", "bpm:110-130"]
│   └── search.result_count: 15
├── elasticsearch.search
└── response sent (status: 200)
```

**Expected Behavior for All Business Events:**
- ✅ Business event spans created with meaningful names
- ✅ Relevant attributes captured
- ✅ Spans linked to parent HTTP request
- ✅ Automatic timing measurement
- ✅ Errors recorded with details

---

## 🎯 Complete Trace Verification Checklist

### Automatic Instrumentation Layers (All Combined):

```
GET /api/v1/feed/enriched?limit=10 (user token + filters)
│
├─ HTTP Middleware Span (telemetry.go)
│  ├─ method: GET
│  ├─ path: /api/v1/feed/enriched
│  ├─ status: 200
│  ├─ user_id: extracted from token
│  └─ duration: ~150ms
│
├─ Business Event Span (feed.get - from business_events.go)
│  ├─ feed.type: "enriched"
│  ├─ feed.limit: 10
│  ├─ feed.offset: 0
│  ├─ enrichment.type: "users"
│  └─ feed.item_count: 8
│
├─ Database Query Spans (GORM plugin)
│  ├─ db.query (SELECT activities...)
│  │  ├─ table: activities
│  │  ├─ operation: SELECT
│  │  └─ duration: ~20ms
│  ├─ db.query (SELECT users...) - batch load
│  │  ├─ table: users
│  │  ├─ operation: SELECT
│  │  └─ duration: ~15ms
│  └─ db.query (SELECT follows...)
│     ├─ table: follows
│     ├─ operation: SELECT
│     └─ duration: ~10ms
│
├─ Cache Spans (redis.go)
│  ├─ redis.get ("feed:user:...")
│  │  ├─ cache.key: "feed:*" (masked)
│  │  ├─ cache.hit: false
│  │  └─ cache.status: "miss"
│  └─ redis.set ("feed:user:...")
│     ├─ cache.key: "feed:*" (masked)
│     └─ cache.status: "success"
│
└─ External API (if enabled)
   └─ stream.io.get_timeline
      ├─ duration: ~50ms
      └─ status: success
```

### Logs with Trace Context:

```json
{
  "level": "info",
  "msg": "HTTP request",
  "method": "GET",
  "path": "/api/v1/feed/enriched",
  "status": 200,
  "latency": "150ms",
  "trace_id": "3fa9d52e4c84d8f6...",
  "span_id": "8e47c3a2b1f9d6e4",
  "request_id": "req-123456"
}
```

---

## 🚀 Performance Verification

### Measure Tracing Overhead

**Test without tracing:**
```bash
OTEL_ENABLED=false go run cmd/server/main.go &
# Run requests and measure response time
time curl http://localhost:8787/api/v1/feed/global?limit=10
```

**Test with tracing:**
```bash
OTEL_ENABLED=true go run cmd/server/main.go &
# Run same requests
time curl http://localhost:8787/api/v1/feed/global?limit=10
```

**Expected:**
- ✅ Latency overhead: < 5ms per request
- ✅ Memory increase: < 50MB
- ✅ No dropped traces or errors

---

## 📊 Grafana Dashboard Verification

### 1. View in Distributed Tracing Dashboard

URL: http://localhost:3000/d/distributed-tracing

**Panels to Check:**
- ✅ "Request Rate by Endpoint" - shows requests/sec
- ✅ "Request Latency Percentiles" - p95/p99 latency
- ✅ "Recent Traces" - if available

### 2. Explore Tempo Directly

URL: http://localhost:3000/explore?datasource=Tempo

**Search Query Examples:**
```
# Find all traces from sidechain-backend
{ service.name = "sidechain-backend" }

# Find traces for specific operation
{ service.name = "sidechain-backend" && name = "GET /api/v1/feed/enriched" }

# Find slow requests
{ service.name = "sidechain-backend" && duration > 500ms }

# Find errors
{ service.name = "sidechain-backend" && status = error }

# Find business events
{ service.name = "sidechain-backend" && name = "feed.create_post" }
```

### 3. View Logs with Trace Correlation

URL: http://localhost:3000/d/logs-traces (or Explore → Loki)

**Actions:**
1. Find log entry with `trace_id`
2. Click on trace_id value
3. Should jump to Tempo and show full trace context

---

## 🐛 Troubleshooting

### No Traces Appearing

**Checklist:**
1. ✅ OTEL_ENABLED=true in backend env
2. ✅ Tempo is running: `curl http://localhost:3200/ready`
3. ✅ OTEL_EXPORTER_OTLP_ENDPOINT=tempo:4318 (or correct address)
4. ✅ Backend logs show: "OpenTelemetry tracing enabled"
5. ✅ Made requests to generate traces (at least 5-10)
6. ✅ Waited for batch flush (3-10 seconds)

**Fix:**
```bash
# Check Tempo is receiving data
curl http://localhost:3200/api/search -G -d 'q=GET'

# Check backend logs for trace export errors
docker logs sidechain-backend 2>&1 | grep -i "otel\|trace\|export"

# Restart Tempo if needed
docker-compose restart tempo
```

### Trace ID Not in Logs

**Issue:** HTTP logs don't include trace_id

**Fix:**
1. Verify `gin_logger.go` has OpenTelemetry import
2. Verify span context extraction: `trace.SpanFromContext(c.Request.Context())`
3. Check log level isn't filtering info logs
4. Restart backend: `OTEL_ENABLED=true go run cmd/server/main.go`

### Cache Spans Missing

**Issue:** No `redis.get`/`redis.set` spans appear

**Fix:**
1. Verify cache is being used (make requests that should hit cache)
2. Verify Redis is running: `redis-cli ping`
3. Check if Redis tracing code is actually being called (add log statement)
4. Verify context is being passed through: `c.Request.Context()`

### Business Event Spans Not Appearing

**Issue:** No `feed.create_post`, `social.follow_user`, `search.query` spans

**Fix:**
1. Verify operations are actually being called (check response code)
2. Verify telemetry.GetBusinessEvents() is initialized
3. Check if error occurs before span creation (check logs)
4. Verify span.End() is called (use defer)

---

## 📝 Summary

### Tests Passing Criteria

All enhancements working correctly when:

- ✅ **HTTP Tracing**: Every request has trace_id, HTTP span with method/path/status/latency
- ✅ **Gin Logger**: All logs include trace_id and span_id
- ✅ **Database Tracing**: DB queries appear as child spans with table/operation/duration
- ✅ **Redis Tracing**: Cache operations create spans with hit/miss status
- ✅ **Feed Creation**: `feed.create_post` span with post_id and audio_format
- ✅ **Follow User**: `social.follow_user` span with user IDs
- ✅ **Search**: `search.query` span with query, index, filters, result_count
- ✅ **Performance**: < 5ms latency overhead, < 50MB memory
- ✅ **Grafana**: Traces visible in Tempo, logs in Loki, correlation working

### Next Steps

Once all tests pass, you have a complete observability stack:

1. **Metrics** (Prometheus): Request rate, latency, cache hits, errors
2. **Traces** (Tempo): Complete request flow with all operations
3. **Logs** (Loki): Structured logs correlated with traces via trace_id
4. **Dashboards** (Grafana): Unified view of all observability signals

This enables:
- Fast debugging of issues (drill down from metrics → traces → logs)
- Performance optimization (identify bottlenecks)
- Business analytics (track user actions via custom events)
- Reliability monitoring (error rates, SLOs)
