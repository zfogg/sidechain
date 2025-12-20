# Observability Architecture: How Refactoring Feeds Prometheus & Grafana

## The Big Picture: Data Flow

```
Your Code (Handlers, Queue, Services)
    ↓
[Metrics Collection Points]  ← Where refactoring matters
    ↓
Prometheus
    ↓
Grafana Dashboards
    ↓
Alerts + Insights
```

The refactoring enables **better metric collection** because you'll have:
1. **Centralized error handling** → know exactly which error types occur where
2. **Standardized validation** → track validation failures by type
3. **Observable queue** → see processing bottlenecks
4. **Structured logging** → correlate with metrics

---

## How Each Refactoring Feeds into Observability

### 1. Error Package Refactoring (Section 4.0) → Error Metrics

**Current State (Bad):**
```go
// Handlers return errors inconsistently
c.JSON(http.StatusNotFound, gin.H{"error": "user not found"})
c.JSON(http.StatusNotFound, gin.H{"message": "no user found"})
c.JSON(http.StatusNotFound, gin.H{"msg": "user doesn't exist"})
// Prometheus sees 3 different error types in logs, can't aggregate
```

**After Refactoring (Good):**
```go
// All handlers return errors consistently
c.JSON(http.StatusNotFound, util.ErrorResponse(errors.NotFound("user")))
c.JSON(http.StatusNotFound, util.ErrorResponse(errors.NotFound("user")))
c.JSON(http.StatusNotFound, util.ErrorResponse(errors.NotFound("user")))

// Prometheus middleware can now do:
// - Track errors by error code (ErrNotFound, ErrConflict, etc.)
// - Track errors by handler (which endpoints fail most?)
// - Track errors by resource type (user, post, comment)
```

**New Metrics You Get:**

```prometheus
# Error rate by endpoint
rate(http_errors_total{endpoint="/api/v1/users/:id",error_code="NOT_FOUND"}[5m])

# Conflict errors (409) by type
http_errors_total{status="409",error_code="CONFLICT"}

# Validation errors by field
http_errors_total{status="422",error_code="VALIDATION",field="email"}

# Unauthorized attempts (security metric)
rate(http_errors_total{status="401"}[5m])
```

**Grafana Dashboards You Can Build:**

```
"Error Analysis" Dashboard:
┌─────────────────────────────────────┐
│ Top 10 Most Common Errors           │
│ (Error code, frequency, endpoints)  │
├─────────────────────────────────────┤
│ Error Rate by Endpoint              │
│ (Which endpoints fail most?)         │
├─────────────────────────────────────┤
│ Validation Errors by Field          │
│ (Bad email? Bad password? etc.)      │
├─────────────────────────────────────┤
│ Security Events (401/403)           │
│ (Auth failures, suspicious activity)│
└─────────────────────────────────────┘
```

---

### 2. Validation Refactoring (Section 4.2) → Validation Metrics

**Current State (Bad):**
```go
// Validation scattered everywhere, hard to track
if len(email) == 0 || !strings.Contains(email, "@") {
    return fmt.Errorf("invalid email")
}
if len(password) < 8 {
    return fmt.Errorf("password too short")
}
if bpm < 40 || bpm > 300 {
    return fmt.Errorf("invalid bpm")
}
// You can't tell: how many validation failures per type? which fields fail most?
```

**After Refactoring (Good):**
```go
// All validation goes through util package
if err := util.ValidateEmail(email); err != nil {
    metrics.ValidationFailures("email", "invalid_format", err)
    return errors.ValidationError("email", err.Error())
}
if err := util.ValidatePassword(password); err != nil {
    metrics.ValidationFailures("password", "too_short", err)
    return errors.ValidationError("password", err.Error())
}
if err := util.ValidateBPM(bpm); err != nil {
    metrics.ValidationFailures("bpm", "out_of_range", err)
    return errors.ValidationError("bpm", err.Error())
}
```

**New Metrics You Get:**

```prometheus
# Validation failures by field
validation_failures_total{field="email",reason="invalid_format"}
validation_failures_total{field="password",reason="too_short"}
validation_failures_total{field="bpm",reason="out_of_range"}

# Most problematic validation
topk(10, validation_failures_total)

# Validation error rate
rate(validation_failures_total[5m])
```

**Grafana Dashboards You Can Build:**

```
"Data Quality" Dashboard:
┌──────────────────────────────────────┐
│ Top Validation Failures              │
│ (field, reason, count)               │
├──────────────────────────────────────┤
│ Validation Failure Rate Over Time    │
│ (Spike = bad data from frontend)     │
├──────────────────────────────────────┤
│ Most Problematic Fields              │
│ (What are users entering wrong?)     │
├──────────────────────────────────────┤
│ Password Strength Requirements Met % │
│ (How many meet minimum requirements?)│
└──────────────────────────────────────┘
```

---

### 3. Redis Streams Queue (Phase 3) → Processing Pipeline Metrics

**Current State (Bad):**
```go
// Jobs in memory, no visibility
queue.SubmitJob(userID, postID, filePath, filename, metadata)
// Where's the job now? Did it fail? How long was it queued?
```

**After Refactoring (Good):**
```go
// Jobs in Redis Streams with full observability
job, err := redisQueue.SubmitJob(ctx, userID, postID, filePath, metadata)
// Metrics automatically recorded:
// - Job submitted
// - Time spent in pending state
// - Processing time
// - Success/failure
// - Retries
```

**New Metrics You Get:**

```prometheus
# Queue depth (how many jobs waiting)
audio_queue_pending_jobs
audio_queue_processing_jobs
audio_queue_dlq_jobs

# Processing time breakdown
audio_processing_duration_seconds{stage="ffmpeg"}
audio_processing_duration_seconds{stage="waveform"}
audio_processing_duration_seconds{stage="s3_upload"}

# Failure tracking
audio_processing_failures_total{reason="ffmpeg_timeout"}
audio_processing_failures_total{reason="s3_error"}
audio_processing_failures_total{reason="waveform_generation_failed"}

# Retry tracking
audio_processing_retries_total{attempt="1"}
audio_processing_retries_total{attempt="2"}
audio_processing_retries_total{attempt="3"}

# DLQ tracking
audio_queue_dlq_jobs{reason="max_retries_exceeded"}
```

**Grafana Dashboards You Can Build:**

```
"Audio Processing Pipeline" Dashboard:
┌────────────────────────────────────────┐
│ Queue Health                           │
│ Pending: 42 | Processing: 8 | DLQ: 3  │
├────────────────────────────────────────┤
│ Processing Time (p50, p95, p99)        │
│ FFmpeg: 5.2s, 12.1s, 18.3s             │
│ Waveform: 1.1s, 2.3s, 3.5s             │
│ S3: 2.4s, 4.1s, 6.2s                   │
├────────────────────────────────────────┤
│ Failure Rate by Stage                  │
│ FFmpeg: 2.1% | Waveform: 0.3% | S3: 1.2%│
├────────────────────────────────────────┤
│ Top Failure Reasons                    │
│ (timeout, network error, etc.)         │
├────────────────────────────────────────┤
│ Retry Success Rate                     │
│ Attempt 1: 92% | Attempt 2: 85% | Attempt 3: 60%│
└────────────────────────────────────────┘
```

**Alerts You Can Configure:**

```yaml
- alert: AudioQueueBacklogHigh
  expr: audio_queue_pending_jobs > 100
  for: 5m

- alert: AudioProcessingSlowDown
  expr: histogram_quantile(0.95, audio_processing_duration_seconds) > 60
  for: 10m

- alert: AudioFailureRateSpiking
  expr: rate(audio_processing_failures_total[5m]) > 0.1
  for: 5m

- alert: DLQAccumulating
  expr: audio_queue_dlq_jobs > 10
  for: 15m
```

---

### 4. Structured Logging (Phase 8) → Correlated Observability

**Current State (Bad):**
```go
logger.Log.Error("failed to process audio")
// Which audio? Which user? How does this connect to metrics?
```

**After Refactoring (Good):**
```go
logger.Log.Error("failed to process audio",
    logger.UserID(userID),
    logger.PostID(postID),
    logger.ProcessingTime(duration),
    logger.AudioStatus(status),
    zap.Error(err),
)
// In Grafana: click on metric spike → see logs from exact same time period
// Logs include structured fields that match metric labels
```

**Connection to Metrics:**

```
Prometheus metric shows: audio_processing_failures_total spike at 14:23

Click on Prometheus alert:
↓
Loki (logs) shows all ERROR logs from 14:23
↓
Logs have fields: user_id, post_id, processing_time, error_reason
↓
Filter logs by error_reason = "ffmpeg_timeout"
↓
See all users affected, patterns emerge
↓
Discover: users with large files are timing out
↓
Action: increase FFmpeg timeout for large files
```

---

## Metrics Collection Points (After Refactoring)

### Point 1: Handler Entry/Exit
```go
// middleware/telemetry.go
Middleware wraps every handler with:
- Request start time
- Handler name (derived from route)
- User ID (from context)
- Request details (method, path, body size)

Middleware exits with:
- Response status code
- Response time
- Error (if any)
- Error code (ErrNotFound, ErrConflict, etc.) ← NEW from error refactoring
```

**Metrics Recorded:**
```
http_requests_total{method,path,status,error_code}
http_request_duration_seconds{method,path,status}
http_request_size_bytes{method,path}
http_response_size_bytes{method,path,status}
handler_errors_total{handler,error_code}
```

### Point 2: Validation Layer
```go
// util/validation.go
Each validation function records:
- Which field was validated
- What validation rule failed
- Error reason

All channeled through:
metrics.RecordValidationFailure(field, reason, err)
```

**Metrics Recorded:**
```
validation_failures_total{field,reason}
validation_error_rate{field}
```

### Point 3: Cache Layer
```go
// middleware/redis_cache.go
On cache hit:
metrics.CacheHits(cacheName)

On cache miss:
metrics.CacheMisses(cacheName)

On cache operation:
metrics.CacheDuration(cacheName, duration)

On invalidation:
metrics.CacheInvalidations(reason, resource_type)
```

**Metrics Recorded:**
```
cache_hits_total{cache_name}
cache_misses_total{cache_name}
cache_hit_rate{cache_name}
cache_operation_duration_seconds{cache_name}
cache_invalidations_total{reason}
```

### Point 4: Database Layer
```go
// database/hooks.go (GORM)
On each query:
- Query duration
- Query type (select, insert, update, delete)
- Table name
- Slow query detection

metrics.RecordQuery(queryType, table, duration)
```

**Metrics Recorded:**
```
db_query_duration_seconds{type,table}
db_queries_total{type,table}
db_slow_queries_total{type,table}
db_connection_pool_usage
db_connection_wait_time_seconds
```

### Point 5: External Service Calls
```go
// stream/client.go, recommendations/gorse.go, search/client.go
Before call:
- Service name
- Operation type
- Parameters

After call:
- Duration
- Success/failure
- Error details

metrics.RecordExternalCall(service, operation, duration, err)
```

**Metrics Recorded:**
```
external_calls_total{service,operation,status}
external_call_duration_seconds{service,operation}
external_call_errors_total{service,operation,error_type}
```

### Point 6: Audio Processing Queue
```go
// queue/redis_streams.go
Job submission:
metrics.QueueJobSubmitted(userID, postID)

Job picked up:
metrics.QueueJobClaimed(jobID, workerID)

Job processing:
metrics.QueueJobStarted(jobID)
metrics.QueueStageDuration(jobID, stage, duration) // ffmpeg, waveform, s3

Job completion:
metrics.QueueJobCompleted(jobID, duration)

Job failure:
metrics.QueueJobFailed(jobID, reason, retries)
```

**Metrics Recorded:**
```
queue_jobs_submitted_total
queue_jobs_pending{status}
queue_job_duration_seconds{stage}
queue_job_failures_total{reason}
queue_job_retries_total{attempt}
queue_dlq_jobs{reason}
```

---

## Example: Tracing a Problem Through Metrics & Logs

### Scenario: Audio uploads suddenly failing at 3 PM

**Step 1: Prometheus Alert Fires**
```
⚠️ AudioProcessingFailureRateSpiking
  Rate of failures > 10% for 5 minutes
  Time: 3:00 PM
```

**Step 2: Check Prometheus Dashboard**
```
audio_processing_failures_total shows spike in "ffmpeg_timeout"
↓
Most failures at stage="ffmpeg"
↓
Failure reason breakdown:
  - ffmpeg_timeout: 87%
  - s3_error: 10%
  - network_timeout: 3%
```

**Step 3: Click to Loki (Logs)**
```
Filter by:
- timestamp: 2:55 PM - 3:05 PM
- level: ERROR
- error_code: ffmpeg_timeout
```

**Step 4: Analyze Logs**
```
ERROR | post_id=12345 | audio_file_size=512MB | ffmpeg_duration=72s | timeout=60s
ERROR | post_id=12346 | audio_file_size=480MB | ffmpeg_duration=68s | timeout=60s
ERROR | post_id=12347 | audio_file_size=520MB | ffmpeg_duration=75s | timeout=60s
...
Pattern: Large files > 450MB taking > 60s to process
```

**Step 5: Root Cause & Action**
```
Cause: New iOS version uploads higher-quality audio
       Files are 2x larger than before
       Processing takes longer

Action: Increase FFmpeg timeout from 60s to 120s
        OR: Add file size detection, adjust timeout based on size
        OR: Parallelize FFmpeg encoding
```

**Step 6: Monitor Fix**
```
After deploying fix, watch:
- audio_processing_failures_total (should drop)
- audio_processing_duration_seconds{stage="ffmpeg"} (should still be high but complete)
- handler_errors_total{error_code="INTERNAL_ERROR"} (should drop)
- user_audio_upload_success_rate (should return to 99.5%)
```

---

## Dashboard Matrix: What Metrics → Which Dashboard

| Problem | Metrics to Check | Prometheus Query | Grafana Dashboard |
|---------|------------------|------------------|------------------|
| "Users getting errors" | `http_errors_total` | `rate(http_errors_total[5m])` | "Error Analysis" |
| "API latency high" | `http_request_duration_seconds` | `histogram_quantile(0.95, ...)` | "API Performance" |
| "Audio uploads failing" | `audio_processing_failures_total` | `rate(...[5m])` | "Audio Pipeline" |
| "Database slow" | `db_query_duration_seconds` | `histogram_quantile(0.99, ...)` | "Database" |
| "Cache not working" | `cache_hit_rate` | `cache_hits_total / cache_requests_total` | "Cache Performance" |
| "Queue backlog" | `queue_jobs_pending` | Direct gauge | "Audio Pipeline" |
| "Search failing" | `external_call_errors_total{service="elasticsearch"}` | Query filter | "Search Performance" |
| "Memory high" | `process_resident_memory_bytes` | Direct gauge | "System Health" |
| "Bad data from users" | `validation_failures_total` | `topk(10, validation_failures_total)` | "Data Quality" |

---

## The Refactoring → Observability Chain

```
BEFORE (No Refactoring):
┌─ Random error messages in handlers
├─ Can't aggregate errors by type
├─ Don't know which endpoints fail most
├─ Queue jobs disappear into void
├─ No validation metrics
└─ Result: Can't debug production issues

AFTER (Full Refactoring):
┌─ Standardized error package (Phase 4.0)
│  → error_code metric label
│  → handler_errors_total{error_code}
│
├─ Validation utils (Phase 4.2)
│  → validation_failures_total{field,reason}
│  → can identify bad user input patterns
│
├─ Redis Streams queue (Phase 3)
│  → queue_job_duration_seconds{stage}
│  → audio_processing_failures_total{reason}
│  → queue_dlq_jobs{reason}
│
├─ Structured logging (Phase 8)
│  → logs have same labels as metrics
│  → can click from metric spike to exact logs
│
├─ Telemetry middleware (Phase 2.1)
│  → every request traced end-to-end
│  → can see where time is spent
│
└─ Result: Can identify, alert, and debug any issue
```

---

## Concrete Grafana Dashboards to Build

Once you've done the refactoring, you can build these dashboards:

### 1. "API Health" Dashboard
```
- Request rate (req/sec)
- Error rate (errors/sec)
- P50/P95/P99 latency
- Top slow endpoints
- Top error endpoints
- Top error codes
- Status code distribution
```
**Metrics needed:**
- http_requests_total (has: method, path, status, error_code)
- http_request_duration_seconds (has: method, path, status)

### 2. "Audio Pipeline" Dashboard
```
- Queue depth (pending jobs)
- Processing time (p50, p95, p99) per stage
- Failure rate (overall & by stage)
- Retry rate
- DLQ size
- Uploads per minute (success/failed)
- Top failure reasons
```
**Metrics needed:**
- queue_jobs_pending
- audio_processing_duration_seconds{stage}
- audio_processing_failures_total{reason}
- queue_job_retries_total
- queue_dlq_jobs

### 3. "Error Analysis" Dashboard
```
- Error rate by error code
- Error rate by endpoint
- Error rate over time
- Top error codes (last hour)
- Error distribution (pie chart)
- Errors by validation field (if validation errors)
- 401/403 security events
```
**Metrics needed:**
- handler_errors_total{error_code}
- validation_failures_total{field}

### 4. "Cache Performance" Dashboard
```
- Hit rate % (overall & per cache)
- Hits vs misses
- Evictions per minute
- Cache operation duration (p50, p95, p99)
- Cache invalidations (by reason)
```
**Metrics needed:**
- cache_hits_total
- cache_misses_total
- cache_operation_duration_seconds

### 5. "Database Health" Dashboard
```
- Query duration (p50, p95, p99) by table
- Slow queries per minute
- Connection pool usage
- Active connections
- Query rate (queries/sec) by type
- Queries per table
```
**Metrics needed:**
- db_query_duration_seconds{type,table}
- db_connection_pool_usage
- db_queries_total{type,table}

### 6. "System Health" Dashboard
```
- Goroutine count
- Memory usage (bytes & %)
- GC pause time
- Uptime
- CPU usage (if available)
- Open file descriptors
```
**Metrics needed:**
- process_resident_memory_bytes
- process_goroutines
- go_gc_duration_seconds

---

## Key Insight: Metrics are Only Good if You Can Interpret Them

**Without refactoring:**
```
You see a spike in http_requests_total
But can't tell:
- Which endpoints?
- What error codes?
- Normal spike or problem?
- Same spike across all status codes or just errors?
```

**With refactoring:**
```
You see a spike in http_requests_total{status="409",error_code="CONFLICT"}
You immediately know:
- Conflict errors spiking (409 status)
- Likely "already following" or "already liked" errors
- Can filter to specific endpoint to pinpoint
- Can correlate with time of outage
- Can find root cause in logs (has error_code label)
```

**The refactoring makes metrics actionable.**

