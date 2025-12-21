# Sidechain Documentation

Complete documentation for the Sidechain VST plugin and Go backend.

## Architecture & Design

### Dependency Injection (DI) Container

The backend uses a centralized DI kernel for managing service dependencies.

- **Overview**: [DI Container Overview](architecture/DI_CONTAINER_OVERVIEW.md) - Quick start, patterns, testing
- **Implementation**: [DI Container Implementation](architecture/DI_CONTAINER_IMPLEMENTATION.md) - Architecture, technical details, benefits
- **Integration Guide**: [DI Container Integration](guides/DI_CONTAINER_INTEGRATION.md) - Step-by-step main.go integration

**Key Benefits:**
- Type-safe service resolution
- Simplified handler constructors (1 parameter vs 10+)
- Lifecycle management with cleanup hooks
- Easy testing with MockContainer
- Zero performance overhead

### Distributed Tracing

The backend uses OpenTelemetry with Tempo for distributed tracing across services.

- **Guide**: [Distributed Tracing Guide](guides/DISTRIBUTED_TRACING.md) - Complete tracing guide with patterns
- **Test Results**: [Tracing Test Results](guides/TRACING_TEST_RESULTS.md) - Validation and testing report

**Key Capabilities:**
- End-to-end request flow visibility
- Database query tracing (automatic via GORM)
- Cache operation tracking (Redis)
- External service call tracing (Stream.io, Elasticsearch, S3, Gorse)
- Correlation IDs for business transaction tracking
- Configurable sampling for production use

## Quick Start Guides

### Dependency Injection Container

```go
// Create and register services
container := kernel.New()
container.
    WithDB(db).
    WithLogger(logger).
    WithStreamClient(streamClient).
    WithAudioProcessor(audioProcessor)

// Validate
if err := container.Validate(); err != nil {
    log.Fatal(err)
}

// Use in handlers
h := handlers.NewHandlers(container)
```

See [DI Container Overview](architecture/DI_CONTAINER_OVERVIEW.md) for more patterns.

### Distributed Tracing

```bash
# Enable tracing
OTEL_ENABLED=true
OTEL_EXPORTER_OTLP_ENDPOINT=localhost:4318
OTEL_TRACE_SAMPLER_RATE=1.0  # 100% for development
```

Traces automatically captured for:
- HTTP requests (via otelgin middleware)
- Database queries (via GORM plugin)
- Cache operations
- External API calls

See [Distributed Tracing Guide](guides/DISTRIBUTED_TRACING.md) for patterns and querying.

## Backend Development

### Project Structure

```
backend/
├── cmd/
│   └── server/main.go          # Server entry point
├── internal/
│   ├── handlers/               # HTTP request handlers
│   ├── container/              # Dependency injection
│   ├── models/                 # Data models (GORM)
│   ├── database/               # Database configuration
│   ├── auth/                   # Authentication
│   ├── stream/                 # Stream.io client wrapper
│   ├── cache/                  # Redis cache client
│   ├── search/                 # Elasticsearch client
│   ├── storage/                # AWS S3 storage
│   ├── audio/                  # Audio processing
│   ├── telemetry/              # OpenTelemetry instrumentation
│   ├── middleware/             # HTTP middleware
│   ├── logger/                 # Structured logging (Zap)
│   ├── metrics/                # Prometheus metrics
│   └── ...                     # Other services
├── .env.example                # Environment configuration
└── Makefile                    # Development commands
```

### Services

**Core Infrastructure:**
- Database (PostgreSQL via GORM)
- Logger (Zap structured logging)
- Cache (Redis)
- Metrics (Prometheus)
- Tracing (OpenTelemetry → Tempo)

**API Clients:**
- Stream.io (social feed, notifications, chat)
- Elasticsearch (search engine)
- Gorse (recommendation engine)
- AWS S3 (audio CDN storage)
- Authentication (OAuth2, JWT, 2FA)
- WebSocket (real-time updates)

**Audio Processing:**
- FFmpeg-based audio transcoding
- Waveform generation
- Audio fingerprinting

## Common Tasks

### Running Development Server

```bash
make install-deps    # Install dependencies (JUCE, Go packages)
make backend-dev     # Start backend with hot reload
make test-backend    # Run tests
```

### Adding New Handler

1. Create handler with DI kernel dependency:
```go
type MyHandlers struct {
    container *kernel.Kernel
}

func NewMyHandlers(c *kernel.Kernel) *MyHandlers {
    return &MyHandlers{container: c}
}
```

2. Access services through container:
```go
func (h *MyHandlers) HandleRequest(c *gin.Context) {
    db := h.container.DB()
    stream := h.container.Stream()
    // Use services...
}
```

3. Register in main.go:
```go
myHandlers := NewMyHandlers(appKernel)
```

### Tracing a Handler

```go
func (h *Handlers) GetUser(c *gin.Context) {
    ctx := c.Request.Context()  // Context has trace info

    // Database queries automatically traced
    var user models.User
    h.container.DB().WithContext(ctx).First(&user, id)

    // External API calls traced
    ctx, span := TraceStreamIOCall(ctx, "get_user", map[string]interface{}{
        "user_id": userID,
    })
    result, err := h.container.Stream().GetUser(ctx, userID)
    if err != nil {
        RecordServiceError(span, "stream.io", err)
    }
    span.End()

    c.JSON(200, user)
}
```

## Configuration

### Environment Variables

See `backend/.env.example` for complete configuration.

**Key Variables:**
- `DATABASE_URL` - PostgreSQL connection string
- `REDIS_HOST` / `REDIS_PORT` - Redis cache (optional)
- `OTEL_ENABLED` - Enable distributed tracing
- `OTEL_EXPORTER_OTLP_ENDPOINT` - Tempo endpoint
- `JWT_SECRET` - JWT signing key
- `AWS_BUCKET` - S3 bucket for audio storage
- `STREAM_API_KEY` / `STREAM_API_SECRET` - Stream.io credentials
- `GORSE_URL` / `GORSE_API_KEY` - Gorse recommendation engine

### Docker Compose

```yaml
version: '3.8'
services:
  postgres:
    image: postgres:15
    environment:
      POSTGRES_PASSWORD: postgres

  redis:
    image: redis:7

  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:8.5.0

  tempo:
    image: grafana/tempo:latest
    ports:
      - "4318:4318"  # OTLP HTTP

  backend:
    build: ./backend
    environment:
      DATABASE_URL: postgres://postgres:postgres@postgres:5432/sidechain
      REDIS_HOST: redis
      ELASTICSEARCH_URL: http://elasticsearch:9200
      OTEL_EXPORTER_OTLP_ENDPOINT: http://tempo:4318
    ports:
      - "8787:8787"
```

## Testing

### Unit Tests with DI Container

```go
func TestGetUser(t *testing.T) {
    container := container.MinimalMock().
        WithMockDB(setupTestDB(t)).
        WithMockLogger(logger.Log)

    h := handlers.NewHandlers(container)
    // Test handler...
}
```

### Integration Tests

```go
func TestCreatePostFlow(t *testing.T) {
    container := container.NewMock().
        WithMockDB(testDB).
        WithMockStreamClient(mockStream).
        WithMockS3Uploader(mockS3).
        WithMockAudioProcessor(mockAudio)

    h := handlers.NewHandlers(container)
    // Test full flow...
}
```

## Monitoring & Debugging

### Distributed Tracing

1. **Start Tempo:**
```bash
docker run -p 4318:4318 grafana/tempo:latest
```

2. **View Traces:**
- Open Tempo UI at http://localhost:3200
- Search for traces by service name, duration, status, etc.

3. **Example Queries:**
```
{ service.name="sidechain-backend" }
{ duration > 1000ms }
{ status="error" }
{ user.id="user123" }
```

### Metrics

- Prometheus endpoint: `http://localhost:8787/metrics`
- HTTP request metrics (latency, count, errors)
- Database query metrics
- Cache hit/miss rates
- External API latency

### Logs

- Structured logging with Zap
- Log file: `server.log`
- JSON format for easy parsing
- Log level configurable via `LOG_LEVEL` env var

## Deployment

### Production Configuration

```bash
# Enable tracing with lower sampling
OTEL_ENABLED=true
OTEL_TRACE_SAMPLER_RATE=0.1  # Sample 10% of requests

# Performance tuning
LOG_LEVEL=warn
DATABASE_MAX_OPEN_CONNS=100

# Scale settings
REDIS_HOST=redis.prod.internal
ELASTICSEARCH_URL=https://es.prod.internal
```

### Graceful Shutdown

```go
// Cleanup is handled by container automatically
ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
defer cancel()

if err := appKernel.Cleanup(ctx); err != nil {
    logger.Error("Cleanup failed", err)
}
```

## Resources

- [OpenTelemetry Documentation](https://opentelemetry.io/docs/instrumentation/go/)
- [Tempo Query Language](https://grafana.com/docs/tempo/latest/querying/trace-query-language/)
- [Stream.io Documentation](https://getstream.io/docs/)
- [Elasticsearch Guide](https://www.elastic.co/guide/en/elasticsearch/reference/)
- [GORM Documentation](https://gorm.io/docs/)

## Contributing

When adding features or improving the backend:

1. **Follow DI Container Pattern**: Inject dependencies through container
2. **Add Tracing**: Use telemetry helpers for API calls and business logic
3. **Test with Mocks**: Use MockContainer for unit tests
4. **Document Changes**: Update relevant docs in `docs/` folder
5. **Run Tests**: `make test-backend`
6. **Format Code**: `go fmt ./...`

---

**Last Updated**: 2025-12-21
**Status**: Production Ready ✅
