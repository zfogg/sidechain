# Architecture Documentation

Comprehensive documentation of Sidechain backend architecture and design patterns.

## Table of Contents

### Dependency Injection

- **[DI Container Overview](DI_CONTAINER_OVERVIEW.md)** - Quick start, usage patterns, testing utilities
  - Service categories (14+ services)
  - Fluent API for readable initialization
  - MockContainer for testing
  - Best practices and error handling

- **[DI Container Implementation](DI_CONTAINER_IMPLEMENTATION.md)** - Deep dive into architecture
  - Technical design details
  - Thread safety with sync.RWMutex
  - Lifecycle management (LIFO cleanup)
  - Performance impact analysis
  - Future enhancement possibilities

### System Components

Each major service is documented with:
- Purpose and responsibilities
- Integration points
- Configuration options
- Common operations
- Error handling patterns

**Core Infrastructure:**
- Database (PostgreSQL/GORM)
- Logger (Zap)
- Cache (Redis)
- Tracing (OpenTelemetry)

**API Clients:**
- Stream.io
- Elasticsearch
- Gorse
- AWS S3
- Authentication
- WebSocket

## Design Principles

### 1. Dependency Injection

All services are managed through a central container:
- Single source of truth
- Type-safe access
- Easy testing with mocks
- Centralized lifecycle management

### 2. Distributed Tracing

Complete visibility into request flows:
- Automatic HTTP request tracing
- Database query instrumentation
- External API call tracing
- Correlation IDs for business transactions

### 3. Layered Architecture

```
┌─────────────────────────────────────┐
│     HTTP Handlers (gin)             │
├─────────────────────────────────────┤
│     Service Layer (business logic)   │
├─────────────────────────────────────┤
│     Repository Layer (data access)   │
├─────────────────────────────────────┤
│     Data Models (GORM)              │
├─────────────────────────────────────┤
│     External Services               │
│  (Stream.io, S3, Elasticsearch)     │
└─────────────────────────────────────┘
```

### 4. Concurrency Safety

- Thread-safe container operations
- Lock-free cache operations where possible
- Proper mutex usage for shared state
- Context-based cancellation

## Getting Started

### For New Developers

1. Read [DI Container Overview](DI_CONTAINER_OVERVIEW.md) - understand how services are accessed
2. Check the main handler in `internal/handlers/handlers.go` - see how services are used
3. Look at tests in `internal/handlers/*_test.go` - understand testing patterns
4. Review specific service you'll be working with

### For Adding Features

1. Plan which services you'll need from the container
2. Add methods to appropriate service layer
3. Wire up in handler using container
4. Add traces for important operations
5. Write tests using MockContainer

### For Performance Analysis

1. Check telemetry documentation for tracing
2. Use Prometheus metrics endpoint
3. Profile database query execution
4. Monitor cache hit rates

## Deployment Considerations

### Scaling

- Container is thread-safe for concurrent access
- Services like Redis support horizontal scaling
- Elasticsearch clusters for distributed search
- Multiple backend instances behind load balancer

### Monitoring

- Prometheus metrics on `:8787/metrics`
- OpenTelemetry traces to Tempo
- Structured logs in JSON format
- Alert rules for key metrics

### Failover

- Optional Redis cache (graceful degradation)
- Optional Elasticsearch (falls back to PostgreSQL search)
- Circuit breaker patterns for external APIs
- Retry logic with exponential backoff

## Related Documentation

- See [guides/](../guides/) for integration and operational guides
- See [api/](../api/) for API documentation
- See main [README.md](../README.md) for quick start

---

**Status**: Production Ready ✅
**Last Updated**: 2025-12-21
