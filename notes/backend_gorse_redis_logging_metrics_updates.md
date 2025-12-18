# Backend Architecture Improvements: Gorse, Redis, Logging & Metrics

**Status**: Not Started
**Priority**: P0 (Logging + Redis), P1 (Metrics + Gorse)
**Estimated Effort**: 20-25 hours total
**Target Timeline**: 3 weeks

---

## PHASE 1: STRUCTURED LOGGING (4 hours) 🔴 P0

### 1.1 Add Zap Dependency
- [ ] Run `go get -u go.uber.org/zap@latest`
- [ ] Run `go get -u gopkg.in/natefinch/lumberjack.v2@latest`
- [ ] Run `go get -u github.com/gin-contrib/zap@latest`
- [ ] Run `go mod tidy`
- [ ] Verify zap imports compile in test file

### 1.2 Create Logger Package
- [ ] Create `backend/internal/logger/logger.go`
- [ ] Implement `Init(logLevel string, logFile string)` function
  - [ ] Setup console encoder for stdout
  - [ ] Setup JSON encoder for file (production-ready)
  - [ ] Implement log rotation with lumberjack (100MB, keep 5 files, 7 day retention)
  - [ ] Handle log level parsing (debug, info, warn, error)
  - [ ] Setup TeeCore for dual output (console + file)
- [ ] Create `parseLogLevel()` helper function
- [ ] Export global `Log` variable for package access
- [ ] Add error handling for initialization failures

### 1.3 Create Logging Middleware
- [ ] Create `backend/internal/middleware/logging.go`
- [ ] Implement `LoggingMiddleware()` using gin-contrib/zap
  - [ ] Use RFC3339 timestamp format
  - [ ] Include request latency
  - [ ] Include HTTP status codes
- [ ] Create `RequestIDMiddleware()` to add X-Request-ID header
  - [ ] Generate UUID if not present
  - [ ] Add request_id to logger context
  - [ ] Pass to response headers

### 1.4 Integrate with Main Server
- [ ] Update `backend/cmd/server/main.go`
  - [ ] Remove old `log.SetOutput()` pattern (lines 38-52)
  - [ ] Call `logger.Init()` at startup with env var `LOG_LEVEL` (default: "info")
  - [ ] Add logging middleware early in Gin setup
  - [ ] Add request ID middleware after logging
- [ ] Update Gin default writers to use new logger
  - [ ] Set `gin.DefaultWriter` to logger output
  - [ ] Set `gin.DefaultErrorWriter` to logger output

### 1.5 Replace Printf Calls Throughout Codebase
- [ ] Search for all `log.Printf()` calls
- [ ] Replace with `logger.Log.Info()`, `.Warn()`, `.Error()` variants
- [ ] Add structured fields (user_id, post_id, duration, etc.)
- [ ] Prioritize critical paths:
  - [ ] `internal/handlers/*.go` (search handlers especially)
  - [ ] `internal/auth/auth.go` (login/register events)
  - [ ] `internal/audio/audio.go` (processing pipeline)
  - [ ] `internal/stream/client.go` (API interactions)
  - [ ] `cmd/server/main.go` (startup/shutdown)

### 1.6 Test Logging Output
- [ ] Start backend: `make backend-run`
- [ ] Verify console logs appear in stdout
- [ ] Verify `server.log` file is created
- [ ] Check file contains JSON with: level, timestamp, message, request_id
- [ ] Make test API call and verify request_id appears in logs
- [ ] Create large test to verify log rotation works

---

## PHASE 2: REDIS INTEGRATION (6 hours) 🔴 P0

### 2.1 Add Redis Dependency
- [ ] Run `go get github.com/redis/go-redis/v9@latest`
- [ ] Run `go mod tidy`
- [ ] Verify redis client compiles in test file

### 2.2 Create Redis Package
- [ ] Create `backend/internal/cache/redis.go`
- [ ] Implement `NewRedisClient(host, port string)` function
  - [ ] Setup connection pooling (MaxConns: 50, MaxIdleConns: 10)
  - [ ] Configure timeouts (Read/Write: 3s, Idle: 5min)
  - [ ] Test connection with Ping()
  - [ ] Return error if cannot connect
- [ ] Export `GetRedisClient()` singleton getter
- [ ] Implement common operations:
  - [ ] `Get(ctx, key)` - retrieve value
  - [ ] `Set(ctx, key, value)` - store value
  - [ ] `SetEx(ctx, key, value, ttl)` - store with expiration
  - [ ] `Del(ctx, key)` - delete key
  - [ ] `LPush(ctx, key, value)` - queue operation
  - [ ] `LRange(ctx, key, start, stop)` - get queue items
  - [ ] `Ping(ctx)` - health check
- [ ] Add error logging for connection failures

### 2.3 Implement Distributed Rate Limiting
- [ ] Create new rate limiter using Redis instead of in-memory
- [ ] Update `backend/internal/middleware/rate_limit.go`
  - [ ] Replace `RateLimiter` struct with Redis token bucket
  - [ ] Implement `RateLimitRedis()` middleware function
  - [ ] Use Lua script for atomic token bucket operations (optional but recommended)
  - [ ] Make rate limit boundaries configurable per endpoint
- [ ] Update main.go to use new middleware
  - [ ] `RateLimitRedis(100, 1*time.Minute)` for default
  - [ ] `RateLimitRedis(10, 1*time.Minute)` for auth endpoints
  - [ ] `RateLimitRedis(20, 1*time.Minute)` for uploads
  - [ ] `RateLimitRedis(50, 1*time.Minute)` for search
- [ ] Test rate limiting works across multiple instances (if available)

### 2.4 Add JWT Token Caching
- [ ] Update `backend/internal/auth/middleware.go`
- [ ] Implement token cache with 5-minute TTL
  - [ ] Generate cache key: `jwt_cache:{hash(token)}`
  - [ ] Check Redis before JWT verification
  - [ ] Store parsed claims in Redis after verification
  - [ ] Add cache hit/miss logging
- [ ] Verify performance improvement (~40% CPU reduction expected)

### 2.5 Add Feed Caching
- [ ] Update relevant handlers in `backend/internal/handlers/`
  - [ ] `GetUnifiedFeed()` - cache for 30 seconds
  - [ ] `GetTimelineFeed()` - cache for 30 seconds
  - [ ] `GetUserFeed()` - cache per user for 1 minute
  - [ ] `GetTrendingFeed()` - cache for 5 minutes
- [ ] Implement cache invalidation on:
  - [ ] User posts new audio
  - [ ] User likes/reposts
  - [ ] User follows someone
- [ ] Add metrics for cache hit rate

### 2.6 Implement Session Storage (Optional for Now)
- [ ] Update `backend/internal/auth/auth.go`
- [ ] Implement session store using Redis for browser clients
  - [ ] Generate session ID on login
  - [ ] Store session data with 24-hour TTL
  - [ ] Support session validation on subsequent requests
- [ ] Can be skipped if only using JWT tokens for VST

### 2.7 Test Redis Integration
- [ ] Verify Redis connection works: `redis-cli ping`
- [ ] Test rate limiting from multiple IPs
- [ ] Test JWT token cache hit ratio
- [ ] Test feed cache invalidation
- [ ] Monitor Redis memory usage

---

## PHASE 3: METRICS & OBSERVABILITY (3 hours) 🟡 P1

### 3.1 Add Metrics Dependencies
- [ ] Run `go get github.com/gin-contrib/prometheus@latest`
- [ ] Run `go get github.com/prometheus/client_golang@latest`
- [ ] Run `go mod tidy`

### 3.2 Implement Prometheus Metrics Middleware
- [ ] Create `backend/internal/middleware/metrics.go`
- [ ] Implement `MetricsMiddleware()` using gin-contrib/prometheus
  - [ ] Configure metric prefix: "sidechain"
  - [ ] Setup metrics endpoint (port 9090 or separate)
  - [ ] Expose default metrics (request count, duration, size)
- [ ] Update `backend/cmd/server/main.go`
  - [ ] Add metrics middleware to Gin router
  - [ ] Start metrics server on different port (9090)

### 3.3 Add Service-Level Metrics
- [ ] Add audio processing metrics (in `internal/audio/audio.go`):
  - [ ] Counter for total uploads
  - [ ] Histogram for processing duration
  - [ ] Gauge for active jobs
- [ ] Add database metrics (in `internal/database/database.go`):
  - [ ] Query duration histogram
  - [ ] Connection pool size gauge
- [ ] Add cache metrics (in `internal/cache/redis.go`):
  - [ ] Hit/miss counters
  - [ ] Operation latency
- [ ] Add external service metrics:
  - [ ] getstream.io API call duration
  - [ ] Elasticsearch query duration
  - [ ] S3 upload duration
  - [ ] Gorse request latency

### 3.4 Setup Grafana Dashboard (Optional)
- [ ] Create simple dashboard config
- [ ] Add panels for:
  - [ ] Request rate (RPS)
  - [ ] Error rate
  - [ ] P95/P99 latency
  - [ ] Cache hit ratio
  - [ ] Active jobs
  - [ ] Database connection pool
- [ ] Document how to access: `http://localhost:3000`

### 3.5 Test Metrics Collection
- [ ] Start backend and make API calls
- [ ] Visit `http://localhost:9090/metrics`
- [ ] Verify metrics are incrementing
- [ ] Check Prometheus format compliance

---

## PHASE 4: GORSE IMPROVEMENTS (5 hours) 🟡 P1

### 4.1 Improve HTTP Client Connection Pooling
- [ ] Update `backend/internal/recommendations/gorse_rest.go`
- [ ] Replace single HTTP client with pooled client
  - [ ] MaxIdleConns: 100
  - [ ] MaxIdleConnsPerHost: 10
  - [ ] MaxConnsPerHost: 50
  - [ ] IdleConnTimeout: 90s
  - [ ] DisableKeepAlives: false
- [ ] Add timeout configuration (10s)

### 4.2 Implement Retry Logic
- [ ] Create `backend/internal/recommendations/gorse_retry.go`
- [ ] Implement exponential backoff retry wrapper
  - [ ] Max 3 attempts
  - [ ] Backoff: 1s, 2s, 4s
- [ ] Apply to all Gorse API calls:
  - [ ] `BatchSyncUsers()`
  - [ ] `BatchSyncItems()`
  - [ ] `BatchSyncFeedback()`
  - [ ] `GetRecommendations()`

### 4.3 Implement Real-Time Feedback Streaming
- [ ] Create `backend/internal/recommendations/gorse_eventbus.go`
- [ ] Implement `GorseEventBus` struct with:
  - [ ] Buffered feedback channel (1000 items)
  - [ ] Background worker pool (configurable, default 2)
  - [ ] Batch accumulator (batch size: 100, timeout: 5s)
- [ ] Implement worker loop:
  - [ ] Accumulates feedback from channel
  - [ ] Sends batch when size threshold OR timeout reached
  - [ ] Retries on failure
- [ ] Add fallback to Redis queue if Gorse is down

### 4.4 Implement Gorse Health Check
- [ ] Create `backend/internal/recommendations/gorse_health.go`
- [ ] Implement `GorseHealthCheck` struct
  - [ ] Background health check goroutine
  - [ ] Configurable interval (default: 30s)
  - [ ] Ping endpoint to verify connectivity
  - [ ] Track health status atomically
- [ ] Update handlers to check health:
  - [ ] Fall back to trending posts if Gorse unhealthy
  - [ ] Log warnings when health flips
- [ ] Add health endpoint: `GET /health/gorse`

### 4.5 Add Feedback Queuing for Offline Scenarios
- [ ] Create `backend/internal/recommendations/gorse_queue.go`
- [ ] Implement queue using Redis
  - [ ] Store failed feedback in `gorse_failed_feedback` list
  - [ ] Periodic flush job (every 5 minutes)
  - [ ] Retry previously failed batches
- [ ] Implement `FlushFailedFeedback()` function
  - [ ] Triggered by scheduled job in main.go
  - [ ] Also triggered when health check shows Gorse is back online

### 4.6 Update Feedback Events
- [ ] Audit current feedback tracking:
  - [ ] Post completion
  - [ ] User likes post
  - [ ] User follows user
  - [ ] User plays audio
  - [ ] User comments
- [ ] Update these to use async event bus:
  - [ ] Instead of blocking `BatchSendFeedback()` call
  - [ ] Queue to `GorseEventBus.SendFeedbackAsync()`
- [ ] Ensure critical path (user operations) not blocked

### 4.7 Add Gorse Metrics & Monitoring
- [ ] Track Gorse-specific metrics:
  - [ ] Feedback queue size
  - [ ] Failed batch count
  - [ ] Health check status
  - [ ] Last successful sync timestamp
  - [ ] API latency (p50, p95, p99)
- [ ] Add to Grafana dashboard

### 4.8 Test Gorse Improvements
- [ ] Stop Gorse: `docker-compose stop gorse`
- [ ] Verify feedback queues in Redis
- [ ] Start Gorse: `docker-compose start gorse`
- [ ] Verify queued feedback is flushed
- [ ] Monitor metrics during flush
- [ ] Test retry logic by simulating network errors

---

## PHASE 5: INFRASTRUCTURE & TESTING (4 hours) 🟡 P1

### 5.1 Update Docker Compose
- [ ] Verify redis service is properly configured
- [ ] Add healthcheck for redis
- [ ] Add prometheus service for metrics collection
- [ ] Add log volume mounts for persistent logs
- [ ] Document ports:
  - [ ] 8787: API
  - [ ] 9090: Metrics
  - [ ] 6379: Redis
  - [ ] 3000: Grafana (optional)

### 5.2 Add Configuration Management
- [ ] Create `backend/internal/config/config.go`
- [ ] Add config struct for all new features:
  - [ ] Redis: host, port, password, pool size
  - [ ] Logging: level, file path, rotation settings
  - [ ] Metrics: port, enabled flag
  - [ ] Gorse: retry count, batch size, timeout
- [ ] Load from environment variables with defaults
- [ ] Document in README.md

### 5.3 Update Environment Files
- [ ] Update `backend/.env.example`
  - [ ] Add `REDIS_HOST`, `REDIS_PORT`, `REDIS_PASSWORD`
  - [ ] Add `LOG_LEVEL`, `LOG_FILE`
  - [ ] Add `METRICS_PORT`, `METRICS_ENABLED`
  - [ ] Add `GORSE_RETRY_MAX_ATTEMPTS`, `GORSE_BATCH_SIZE`
- [ ] Document defaults for each

### 5.4 Add Graceful Shutdown
- [ ] Update `cmd/server/main.go`
- [ ] Implement graceful shutdown handler for signals (SIGTERM, SIGINT)
  - [ ] Flush pending Gorse feedback
  - [ ] Close Redis connections
  - [ ] Wait for in-flight requests (timeout: 30s)
  - [ ] Close database connections
  - [ ] Close log file

### 5.5 Unit Tests
- [ ] Create `backend/internal/cache/redis_test.go`
  - [ ] Test connection pool
  - [ ] Test Set/Get operations
  - [ ] Test expiration
- [ ] Create `backend/internal/recommendations/gorse_eventbus_test.go`
  - [ ] Test feedback batching
  - [ ] Test retry logic
  - [ ] Test offline queuing
- [ ] Create `backend/internal/middleware/rate_limit_test.go`
  - [ ] Test Redis rate limiting
  - [ ] Test multiple requests from same IP

### 5.6 Integration Tests
- [ ] Create `backend/tests/integration/redis_test.go`
  - [ ] Test with real Redis instance
- [ ] Create `backend/tests/integration/gorse_test.go`
  - [ ] Test with real Gorse instance
  - [ ] Test offline scenario
- [ ] Document how to run: `make test-integration`

### 5.7 Documentation
- [ ] Update `backend/README.md`:
  - [ ] Add Logging section with examples
  - [ ] Add Redis caching explanation
  - [ ] Add Metrics monitoring section
  - [ ] Add Gorse improvements notes
  - [ ] Add troubleshooting section
- [ ] Add inline code comments for complex logic
- [ ] Document configuration options

### 5.8 Performance Benchmarks
- [ ] Create load test script (using hey or similar)
- [ ] Benchmark before/after for:
  - [ ] Feed endpoint latency (with cache)
  - [ ] Auth endpoint latency (with token cache)
  - [ ] Rate limit handling (distributed vs in-memory)
- [ ] Document results in notes

---

## PHASE 6: OPTIONAL ENHANCEMENTS (Future) 🔵 P2

### 6.1 Distributed Tracing (OpenTelemetry)
- [ ] Add `github.com/gin-contrib/opentelemetry@latest`
- [ ] Implement trace middleware
- [ ] Setup Jaeger backend for trace collection
- [ ] Export to DataDog or Honeycomb (if using)
- [ ] Trace requests across services (getstream, Elasticsearch, etc.)

### 6.2 Circuit Breaker Pattern
- [ ] Add `github.com/grpc-ecosystem/go-grpc-middleware/retry`
- [ ] Implement circuit breaker for external service calls
- [ ] Fail fast when service is down instead of timeout
- [ ] Implement fallback strategies per service

### 6.3 Content-Based Recommendation Hybrid
- [ ] Implement `getContentBasedRecommendations()` in recommendations package
- [ ] Analyze user preferences (favorite genres, BPM range, keys)
- [ ] Query similar posts by metadata
- [ ] Merge Gorse (60%) + Content-based (30%) + Trending (10%)
- [ ] A/B test against Gorse-only approach

### 6.4 Message Queue (Optional)
- [ ] Evaluate NATS or AWS SQS
- [ ] Use for: audio processing jobs, notifications, Gorse feedback
- [ ] Add job persistence across restarts
- [ ] Implement priority queues (urgent uploads first)

---

## TESTING CHECKLIST

- [ ] **Logging**
  - [ ] Verify JSON format in logs
  - [ ] Check log rotation happens at 100MB
  - [ ] Ensure request IDs propagate through system
  - [ ] Test all log levels (DEBUG, INFO, WARN, ERROR)

- [ ] **Redis**
  - [ ] Rate limiting blocks after threshold
  - [ ] JWT cache reduces verification time
  - [ ] Feed cache invalidates correctly
  - [ ] Redis client recovers after restart
  - [ ] No memory leaks in cache operations

- [ ] **Metrics**
  - [ ] Prometheus endpoint returns metrics
  - [ ] All HTTP endpoints increment request count
  - [ ] Latency histograms are accurate
  - [ ] Error rates tracked correctly

- [ ] **Gorse**
  - [ ] Feedback sent in real-time when online
  - [ ] Feedback queued in Redis when offline
  - [ ] Health check detects Gorse down
  - [ ] Handlers fall back to trending when Gorse unhealthy
  - [ ] Retry logic works (simulate network errors)
  - [ ] Queued feedback flushed when online

- [ ] **Load Testing**
  - [ ] System handles 100 concurrent requests
  - [ ] Rate limiting doesn't cause crashes
  - [ ] Cache hit ratio > 70% for feeds
  - [ ] No memory leaks under sustained load

---

## SUCCESS CRITERIA

✅ **Phase 1 Complete**: All logs are JSON, rotated, include request IDs
✅ **Phase 2 Complete**: Redis working, rate limiting distributed, feed caching active
✅ **Phase 3 Complete**: Prometheus metrics endpoint available at :9090
✅ **Phase 4 Complete**: Gorse feedback real-time, offline queue working, health checks active
✅ **Phase 5 Complete**: All tests pass, documentation updated, graceful shutdown works

**Performance Targets**:
- Feed endpoint latency: < 100ms (p99) with cache
- Auth endpoint: < 50ms (p99) with token cache
- Rate limiter: < 1ms overhead (distributed)
- Gorse feedback: async, never blocks user request

---

## ROLLOUT STRATEGY

1. **Local Development**: Implement all phases locally, test thoroughly
2. **Staging**: Deploy to staging environment, run load tests
3. **Production Gradual Rollout**:
   - Week 1: Logging + Redis (low risk)
   - Week 2: Metrics + Gorse improvements (monitor closely)
   - Week 3: Validate metrics, optimize cache TTLs based on real data

---

## NOTES & DEPENDENCIES

- Redis must be running in docker-compose for local development
- All changes backward compatible (Redis/metrics optional at first)
- No database schema changes required
- Can be implemented incrementally (don't need to do all at once)
- Gorse improvements don't require changes to VST plugin

---

**Last Updated**: 2025-12-18
**Owner**: Backend Team
**Status**: Ready for Implementation
