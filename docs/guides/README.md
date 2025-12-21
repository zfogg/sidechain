# Development & Operations Guides

Step-by-step guides for common tasks and operations.

## Integration Guides

### [DI Container Integration](DI_CONTAINER_INTEGRATION.md)

Step-by-step instructions for integrating the DI container into `cmd/server/main.go`:

1. Create container after services are initialized
2. Register all services using fluent API
3. Update handler constructors
4. Register cleanup hooks
5. Update graceful shutdown

**Estimated Time**: 30 minutes
**Required Changes**: main.go, handler initialization

## Operational Guides

### [Distributed Tracing Guide](DISTRIBUTED_TRACING.md)

Complete guide to using OpenTelemetry for distributed tracing:

- **Setup & Configuration**: Enable tracing with environment variables
- **Core Concepts**: Spans, context propagation, baggage
- **Instrumentation Patterns**: 5 detailed handler examples
- **Service-Specific Tracing**: Stream.io, Elasticsearch, S3, Redis, Gorse
- **Correlation IDs**: Track business transactions across requests
- **Monitoring & Querying**: Use Tempo/Grafana to search traces
- **Performance Considerations**: Sampling rates, overhead analysis
- **Troubleshooting**: Common issues and solutions

**Use When**: Debugging request flow issues, analyzing performance, tracking user transactions

### [Tracing Test Results](TRACING_TEST_RESULTS.md)

Validation report for distributed tracing implementation:

- Test methodology and results
- All compilation and syntax checks
- Issues found and resolutions
- Performance impact analysis
- Integration checklist

**Use When**: Validating tracing implementation, understanding what was tested

## Feature Guides

### Dependency Injection

**Overview**: [See Architecture](../architecture/DI_CONTAINER_OVERVIEW.md)

**Quick Patterns**:

1. **Creating a Container**
```go
container := container.New()
container.
    WithDB(db).
    WithStreamClient(stream).
    WithAuthService(auth)
```

2. **Using in Handlers**
```go
h := handlers.NewHandlers(container)
```

3. **Accessing Services**
```go
db := h.container.DB()
stream := h.container.Stream()
auth := h.container.Auth()
```

4. **Testing with Mocks**
```go
container := container.MinimalMock().
    WithMockDB(testDB).
    WithMockStreamClient(mockStream)
```

### Distributed Tracing

**Quick Setup**:
```bash
OTEL_ENABLED=true
OTEL_EXPORTER_OTLP_ENDPOINT=localhost:4318
OTEL_TRACE_SAMPLER_RATE=1.0
```

**Quick Pattern**:
```go
ctx := c.Request.Context()  // Already has trace info

// Automatic tracing for DB
db.WithContext(ctx).First(&user, id)

// Manual tracing for APIs
ctx, span := TraceStreamIOCall(ctx, "follow", map[string]interface{}{
    "user_id": userID,
})
result, err := stream.FollowUser(ctx, userID)
span.End()
```

## Common Tasks

### Adding a New Handler

1. Create handler struct that embeds container
2. Implement handler methods accessing container services
3. Register in main.go with appContainer
4. Write tests using MockContainer

See: [DI Container Overview](../architecture/DI_CONTAINER_OVERVIEW.md#testing-with-dicontainer)

### Adding Tracing to Existing Code

1. Import telemetry helpers
2. Create span for operation
3. Pass context to downstream calls
4. Set span attributes with details
5. Handle errors with RecordServiceError()

See: [Distributed Tracing Guide - Handler Examples](DISTRIBUTED_TRACING.md#handler-examples)

### Debugging Performance Issues

1. Enable distributed tracing
2. Query Tempo for slow operations
3. Look for long database queries
4. Check external API latency
5. Profile with Prometheus metrics

See: [Distributed Tracing Guide - Querying](DISTRIBUTED_TRACING.md#querying-traces-in-tempografana)

### Setting Up Development Environment

```bash
# Install dependencies
make install-deps

# Start backend with hot reload
make backend-dev

# Run tests
make test-backend

# Build for production
make backend

# Run locally with Docker Compose
docker-compose up
```

### Deploying to Production

1. Build: `make backend`
2. Configure environment variables (see `backend/.env.example`)
3. Set tracing sampling rate to 0.1 (10%)
4. Set log level to "warn"
5. Deploy with proper health checks

## Troubleshooting

### Container Issues

**Q: Missing dependency error?**
A: Call `container.Validate()` after setup - it reports missing required services

**Q: Can't access a service?**
A: Make sure it's registered with container - check that set method was called

**Q: Handler not receiving service?**
A: Handler must accept container as parameter - see DI_CONTAINER_INTEGRATION.md

### Tracing Issues

**Q: No traces appearing in Tempo?**
A: Check OTEL_ENABLED=true and OTEL_EXPORTER_OTLP_ENDPOINT is reachable

**Q: Missing database traces?**
A: Always pass `ctx` to db.WithContext(ctx) - breaks trace chain if missing

**Q: Too much data in Tempo?**
A: Lower OTEL_TRACE_SAMPLER_RATE - 0.1 (10%) or 0.01 (1%) for high traffic

### Performance Issues

**Q: Slow request handling?**
A: Use tracing to find bottleneck - could be DB query, external API, or processing

**Q: High memory usage?**
A: Check cache size, database connections, and goroutine count

**Q: CPU spiking?**
A: Profile with pprof - check CPU-intensive operations

## Testing Strategies

### Unit Testing

```go
// Minimal setup with MockContainer
container := container.MinimalMock().
    WithMockDB(testDB).
    WithMockLogger(logger.Log)

h := handlers.NewHandlers(container)
// Test individual handler...
```

### Integration Testing

```go
// Full setup with multiple services
container := container.NewMock().
    WithMockDB(testDB).
    WithMockStreamClient(mockStream).
    WithMockS3Uploader(mockS3).
    WithMockAudioProcessor(mockAudio)

// Test complete flow...
container.Clean(context.Background())
```

### Load Testing

1. Use Apache Bench or wrk
2. Monitor metrics: `curl http://localhost:8787/metrics`
3. Check Tempo traces for latency distribution
4. Identify bottlenecks

## References & Resources

- [OpenTelemetry Documentation](https://opentelemetry.io/docs/instrumentation/go/)
- [Tempo Query Language](https://grafana.com/docs/tempo/latest/querying/trace-query-language/)
- [Stream.io API Docs](https://getstream.io/docs/)
- [GORM Guide](https://gorm.io/docs/)
- [Go Best Practices](https://golang.org/doc/effective_go)

## Questions?

Refer to the appropriate guide:
- Architecture questions → [Architecture Docs](../architecture/)
- How-to questions → [This folder](./README.md)
- API questions → [API Docs](../api/)
- General questions → [Main README](../README.md)

---

**Last Updated**: 2025-12-21
**Status**: Production Ready ✅
