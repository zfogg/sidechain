# Testing Distributed Tracing

This guide shows how to test the OpenTelemetry distributed tracing setup and verify traces are flowing through the system.

## Prerequisites

Ensure the observability stack is running:

```bash
docker-compose -f docker-compose.dev.yml up -d tempo loki promtail grafana
```

Verify all services are healthy:

```bash
curl http://localhost:3200/ready   # Tempo
curl http://localhost:3100/ready   # Loki
curl http://localhost:3000         # Grafana (should redirect to login)
```

## 1. Generate Sample Traces

Make API requests to generate trace data. The backend automatically creates spans for all HTTP requests:

```bash
# Health check (creates 1 trace with 1 span)
curl http://localhost:8787/health

# Generate 20 traces quickly
for i in {1..20}; do curl -s http://localhost:8787/health > /dev/null; done
```

Wait a few seconds for traces to be batched and flushed to Tempo (default batch size: 1024 spans or 10-second timeout).

## 2. View Traces in Tempo

Query the Tempo API directly:

```bash
# Get all traces from sidechain-backend service
curl -s http://localhost:3200/api/search | python3 -m json.tool

# Search by trace ID
TRACE_ID="<trace-id-from-above>"
curl -s http://localhost:3200/api/traces/$TRACE_ID | python3 -m json.tool
```

Example response showing trace structure:

```json
{
  "traces": [
    {
      "traceID": "412002cd5d0071ead25af237f943c60c",
      "rootServiceName": "sidechain-backend",
      "rootTraceName": "GET /health",
      "startTimeUnixNano": "1766268480563110166"
    }
  ]
}
```

## 3. Access Traces in Grafana

Open Grafana: http://localhost:3000 (Username: `admin`, Password: `hunter2`)

### Option 1: View Distributed Tracing Dashboard

1. Go to **Dashboards** → **Distributed Tracing**
2. See the "Recent Traces" table showing latest traces
3. Click on any trace ID to view full trace details
4. View latency percentiles and request status distribution

### Option 2: Search Traces in Explore

1. Go to **Explore** tab
2. Select **Tempo** datasource
3. Search by:
   - **Service**: `sidechain-backend`
   - **Operation**: `GET /health`, `GET /api/v1/feed/timeline/enriched`, etc.
   - **Duration**: Filter by latency (`> 100ms`)
   - **Status**: `success` or `error`

### Example: Find Slow Traces

```
{ service.name = "sidechain-backend" } | select(span.duration > 100ms)
```

## 4. Examine Trace Details

Click on a trace to see the full span hierarchy:

```
HTTP Request Span (root)
├── Duration: 15.3ms
├── Status: 200 OK
├── Attributes:
│   ├── http.method: GET
│   ├── http.url: /health
│   ├── user.id: (if authenticated)
├── Events:
│   └── (any errors or notable events)
└── Child Spans:
    ├── (Database queries if applicable)
    ├── (Cache operations if applicable)
    └── (External API calls if applicable)
```

## 5. View Logs with Trace Correlation

### Option 1: Logs & Traces Correlation Dashboard

1. Go to **Dashboards** → **Logs & Traces Correlation**
2. See "Backend Logs with Trace IDs" showing structured logs
3. Look for `trace_id` field in log entries
4. Click on `trace_id` to jump to corresponding trace in Tempo

### Option 2: Search Logs in Explore

1. Go to **Explore** tab
2. Select **Loki** datasource
3. Query logs:

```
{job="sidechain-backend"} | json | level="error"
```

4. View `trace_id` field in results
5. Click `trace_id` value to open trace in Tempo

## 6. Verify End-to-End Correlation

**From Trace to Logs:**

1. Open a trace in Tempo
2. Look for "Logs for this span" button
3. View all logs associated with that span

**From Logs to Trace:**

1. View a log entry in Loki
2. Find the `trace_id` field
3. Click the value to jump to the trace

## 7. Check Trace Sampling

The default sampling rate is 100% for development (trace every request):

```bash
# Check environment variable
echo $OTEL_TRACE_SAMPLER_RATE  # Should be 1.0

# For production, use lower sampling (e.g., 0.1 = 10%)
# OTEL_TRACE_SAMPLER_RATE=0.1
```

Sampling decisions are made at the root span level. Child spans inherit the parent's decision.

## 8. Performance Impact Verification

Monitor the overhead of tracing:

```bash
# Check backend latency with tracing enabled
curl -s -w "Time: %{time_total}s\n" http://localhost:8787/health

# Typical overhead: < 5ms per request
```

View latency in Grafana metrics:

1. **Dashboards** → **API Performance**
2. **Request Latency (p95, p99)** panel
3. Should be minimal increase when tracing enabled

## 9. Debugging: No Traces Appearing

If traces don't appear in Tempo:

### Check 1: OpenTelemetry Enabled

```bash
# Verify OTEL_ENABLED is set to "true"
echo $OTEL_ENABLED

# Check backend logs
docker logs sidechain-backend-dev | grep -i "otel\|telemetry"
```

Expected log output:

```
✅ OpenTelemetry tracing enabled
✅ OpenTelemetry GORM tracing plugin registered
✅ OpenTelemetry tracing middleware registered
```

### Check 2: Tempo Health

```bash
# Verify Tempo is running and healthy
curl http://localhost:3200/ready
# Should return: ready

# Check Tempo logs
docker logs sidechain-tempo-dev | tail -20
```

### Check 3: Network Connectivity

Verify backend can reach Tempo:

```bash
# From host
curl http://tempo:4318  # From inside backend container

# Check Docker network
docker network inspect sidechain-network | grep -A 30 Containers
```

### Check 4: Batch Flushing

The OTLP batcher waits for either:
- 1024 spans (batch size)
- 10 seconds (timeout)
- Application shutdown (forced flush)

Make multiple requests to trigger batch:

```bash
# Generate 30+ spans to ensure batch flush
for i in {1..30}; do curl -s http://localhost:8787/health > /dev/null; done
sleep 3
curl -s http://localhost:3200/api/search | python3 -c "import sys, json; print(f'Traces: {len(json.load(sys.stdin)[\"traces\"])}')"
```

### Check 5: Datasource Configuration

Verify Tempo datasource in Grafana:

1. **Configuration** → **Data sources**
2. Find **Tempo** datasource
3. Click **Save & Test** (should show "Data source is working")

If not working:
- Check Tempo URL: `http://tempo:3200`
- Check HTTP method: should be `GET`
- Verify datasource can reach Tempo

## 10. Common Patterns in Traces

### Successful Request Trace

```
GET /health
├── Duration: 2.1ms
├── Status: 200 OK
└── Child Spans:
    └── (none for /health endpoint)
```

### Request with Database Query

```
GET /api/v1/users/me
├── Duration: 45.8ms
├── Status: 200 OK
└── Child Spans:
    ├── db.query: SELECT users WHERE id = ? [5.2ms]
    └── db.query: SELECT preferences WHERE user_id = ? [3.1ms]
```

### Failed Request Trace

```
GET /api/v1/invalid-endpoint
├── Duration: 1.3ms
├── Status: 404 Not Found
├── Error: route not found
└── Child Spans: (none)
```

### Slow Request Trace (Possible Issue)

```
GET /api/v1/feed/timeline
├── Duration: 850ms ⚠️
├── Status: 200 OK
└── Child Spans:
    ├── db.query: SELECT posts... [420ms]
    ├── redis.get: feed:user:123 [2ms] [MISS]
    ├── redis.set: feed:user:123 [15ms]
    └── json.encode: [300ms]
```

## 11. Monitoring & Alerting

### Key Metrics to Monitor

- **Trace Rate**: `rate(http_requests_total[5m])` - Should match request rate
- **P99 Latency**: `histogram_quantile(0.99, ...)` - Watch for increases
- **Error Rate**: Traces with status_code = ERROR
- **Storage Growth**: Tempo disk usage (`/tmp/tempo/traces`)

### Recommended Alerts

1. **High Error Rate**: > 5% of traces have errors
2. **High Latency**: p99 > 1s
3. **Tempo Health**: Health check failing
4. **Storage Full**: Tempo disk > 80% capacity

## 12. Advanced: Custom Span Attributes

Traces automatically capture:
- `service.name` = "sidechain-backend"
- `http.method`, `http.url`, `http.status`
- `user.id` (if authenticated)
- `feed.type` (if applicable)

See `internal/middleware/telemetry.go` to add more custom attributes.

## 13. Troubleshooting Tips

| Issue | Cause | Solution |
|-------|-------|----------|
| No traces in Tempo | OTEL_ENABLED not set or false | Set `OTEL_ENABLED=true` in .env |
| Traces appear slowly | Batch timeout waiting | Make more requests or reduce batch size |
| High latency overhead | Tracing enabled on audio thread | Review async context propagation |
| Storage growing fast | 100% sampling with high traffic | Reduce `OTEL_TRACE_SAMPLER_RATE` |
| Incomplete traces | Async context not propagated | Add context to background tasks |
| Logs don't have trace_id | Promtail not shipping logs | Check Promtail health and config |

## Next Steps

### Phase 2 Enhancements

- [ ] Add tracing to audio processing pipeline (FFmpeg stages)
- [ ] Trace Stream.io API calls with custom attributes
- [ ] Implement tail-based sampling (keep 100% of errors, 10% of success)
- [ ] Add alerts for slow traces and high error rates

### Documentation

- For architecture details, see [OBSERVABILITY_ARCHITECTURE.md](./OBSERVABILITY_ARCHITECTURE.md)
- For OpenTelemetry setup, see [OPENTELEMETRY_SETUP.md](./OPENTELEMETRY_SETUP.md)
