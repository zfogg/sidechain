# Container Package - Dependency Injection

The `container` package provides centralized dependency injection management for the Sidechain backend.

## Overview

Instead of passing 10+ individual service parameters to each handler, the `Container` consolidates all dependencies in a single, type-safe struct.

### Benefits

- **Type-safe**: All dependencies are strongly typed
- **Centralized**: Single source of truth for all services
- **Testable**: Easy to swap implementations with mocks
- **Lifecycle Management**: Proper initialization and cleanup
- **Circular Dependency Detection**: Validate that all required dependencies are registered
- **Fluent API**: Chain setter methods for readable initialization

## Quick Start

### Creating a Container

```go
container := kernel.New()

// Register services using fluent API
container.
    WithDB(db).
    WithLogger(logger).
    WithStreamClient(streamClient).
    WithAuthService(authService).
    WithAudioProcessor(audioProcessor)

// Validate that all required dependencies are registered
if err := container.Validate(); err != nil {
    log.Fatal(err)
}
```

### Using Container in Handlers

**Before** (Old pattern with many parameters):
```go
func NewHandlers(
    db *gorm.DB,
    cache *redis.Client,
    stream stream.StreamClientInterface,
    auth *auth.Service,
    // ... 10 more parameters
) *Handlers {
    return &Handlers{
        db:     db,
        cache:  cache,
        stream: stream,
        auth:   auth,
        // ...
    }
}
```

**After** (New pattern with container):
```go
func NewHandlers(container *kernel.Kernel) *Handlers {
    return &Handlers{
        container: container,
    }
}
```

Handler struct:
```go
type Handlers struct {
    container *kernel.Kernel
}

func (h *Handlers) GetUser(c *gin.Context) {
    db := h.container.DB()
    auth := h.container.Auth()
    stream := h.container.Stream()
    // Use services...
}
```

## Service Categories

### Core Infrastructure
- `DB()` - GORM database connection
- `Logger()` - Zap logger instance
- `Cache()` - Redis cache client

### API Clients
- `Stream()` - Stream.io client
- `Search()` - Elasticsearch client
- `Gorse()` - Gorse recommendation engine
- `S3()` - AWS S3 uploader
- `Auth()` - Authentication service
- `WebSocket()` - WebSocket handler

### Audio Processing
- `AudioProcessor()` - Audio processor service

### Utilities
- `WaveformGenerator()` - Waveform generation utility
- `WaveformStorage()` - Waveform storage utility

### Features
- `AlertManager()` - Alert management service
- `AlertEvaluator()` - Alert evaluation service

## Lifecycle Management

### Cleanup Hooks

Register cleanup functions to be called during graceful shutdown:

```go
container.OnCleanup(func(ctx context.Context) error {
    // Clean up resources
    return audioProcessor.Stop(ctx)
})

container.OnCleanup(func(ctx context.Context) error {
    // Clean up cache connection
    return cache.Close()
})

// Later, during shutdown:
ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
defer cancel()
container.Cleanup(ctx)  // Calls cleanup functions in reverse order (LIFO)
```

## Testing with MockContainer

### Minimal Testing

For isolated unit tests with minimal dependencies:

```go
func TestGetUser(t *testing.T) {
    // Create minimal container with only what you need
    container := container.MinimalMock().
        WithMockDB(setupTestDB(t)).
        WithMockLogger(testLogger)

    handlers := NewHandlers(container)
    // Test handler...
}
```

### Full Testing

For integration tests with all services:

```go
func TestCreatePost(t *testing.T) {
    container := container.NewMock().
        WithMockDB(setupTestDB(t)).
        WithMockStreamClient(mockStreamClient(t)).
        WithMockS3Uploader(mockS3(t)).
        WithMockAuthService(mockAuth(t)).
        WithMockAudioProcessor(mockAudioProcessor(t))

    // Test with full container...
}
```

### Custom Overrides

Override specific dependencies after initialization:

```go
container := container.NewMock()
container.Override("custom_key", customValue)
val, ok := container.GetOverride("custom_key")
```

## Validation

Validate that all required dependencies are registered:

```go
if err := container.Validate(); err != nil {
    // Returns InitializationError with list of missing dependencies
    log.Fatal("Kernel validation failed:", err)
}
```

Required dependencies:
- Database (DB)
- Stream.io client
- Auth service
- Audio processor

Optional but recommended:
- Redis cache
- Elasticsearch search
- Gorse recommendations
- S3 uploader

## Initialization Example

```go
// In main.go
func setupContainer(
    db *gorm.DB,
    redisClient *cache.RedisClient,
    searchClient *search.Client,
    streamClient stream.StreamClientInterface,
    authService *auth.Service,
    s3Uploader *storage.S3Uploader,
    audioProcessor *audio.Processor,
) *kernel.Kernel {
    c := kernel.New()

    c.
        WithDB(db).
        WithCache(redisClient).
        WithSearchClient(searchClient).
        WithStreamClient(streamClient).
        WithAuthService(authService).
        WithS3Uploader(s3Uploader).
        WithAudioProcessor(audioProcessor)

    // Register cleanup hooks
    c.OnCleanup(func(ctx context.Context) error {
        return audioProcessor.Stop(ctx)
    })

    if err := c.Validate(); err != nil {
        log.Fatal("Kernel validation failed:", err)
    }

    return c
}
```

## Thread Safety

The container is thread-safe:
- All getters/setters use `sync.RWMutex`
- Multiple goroutines can safely read from the container
- Configuration is typically done once during initialization

## Error Handling

### InitializationError

Returned when required dependencies are missing:

```go
if err := container.Validate(); err != nil {
    if initErr, ok := err.(*InitializationError); ok {
        fmt.Printf("Missing dependencies: %v\n", initErr.MissingDeps)
    }
}
```

### DependencyNotFoundError

Returned when accessing a dependency that wasn't registered:

```go
db := container.DB()
if db == nil {
    return NewDependencyNotFoundError("database")
}
```

## Best Practices

1. **Single Instance**: Create one container per application instance
2. **Early Validation**: Call `Validate()` immediately after setup
3. **Fluent API**: Use method chaining for readable initialization
4. **Cleanup Registration**: Register cleanup hooks as services are added
5. **Testing**: Use `NewMock()` or `MinimalMock()` for tests
6. **Access Pattern**: Get dependencies through getter methods, not direct field access

## Migration Guide

### Step 1: Create Container
```go
appKernel := kernel.New()
```

### Step 2: Register Services
```go
appKernel.
    WithDB(database.DB).
    WithStreamClient(streamClient).
    // ... register all services
```

### Step 3: Update Handler Constructors
```go
// Old:
func NewHandlers(db *gorm.DB, stream stream.StreamClientInterface, ...) *Handlers

// New:
func NewHandlers(c *kernel.Kernel) *Handlers {
    return &Handlers{container: c}
}
```

### Step 4: Update Handler Methods
```go
// Old:
func (h *Handlers) GetUser(c *gin.Context) {
    db := h.db
    auth := h.auth

// New:
func (h *Handlers) GetUser(c *gin.Context) {
    db := h.container.DB()
    auth := h.container.Auth()
```

## Related Documentation

- See `cmd/server/main.go` for complete container setup example
- See `internal/handlers/` for handler refactoring examples
- See tests in `internal/handlers/*_test.go` for testing patterns
