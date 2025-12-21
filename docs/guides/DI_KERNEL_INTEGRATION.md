# DI Container Integration Guide

This guide explains how to integrate the DI kernel into the main.go server setup.

## Current Status: Handlers Updated ✅

All handlers have been refactored to use the DI kernel:
- ✅ `Handlers` - Main handlers
- ✅ `AuthHandlers` - Auth handlers
- ✅ `ErrorTrackingHandler` - Error tracking
- ✅ `SoundHandlers` - Sound management

## Integration Steps for main.go

### Step 1: Create Container After Services are Initialized

In `cmd/server/main.go`, after all services are initialized:

```go
// ... existing service initialization code ...

// Create the dependency injection kernel
appKernel := kernel.New()

// Register all services
appKernel.
    WithDB(database.DB).
    WithLogger(logger.Log).
    WithCache(redisClient).
    WithStreamClient(streamClient).
    WithAuthService(authService).
    WithS3Uploader(s3Uploader).
    WithAudioProcessor(audioProcessor).
    WithSearchClient(baseSearchClient).
    WithGorseClient(gorseClient).
    WithWebSocketHandler(wsHandler).
    WithAlertManager(alertManager).
    WithAlertEvaluator(alertEvaluator)

// Validate container
if err := appKernel.Validate(); err != nil {
    logger.FatalWithFields("Kernel validation failed", err)
}
```

### Step 2: Register Cleanup Hooks

Register cleanup functions for graceful shutdown:

```go
// Register cleanup hooks in FIFO order (they'll be called in LIFO order)
appKernel.
    OnCleanup(func(ctx context.Context) error {
        if audioProcessor != nil {
            return audioProcessor.Stop(ctx)
        }
        return nil
    }).
    OnCleanup(func(ctx context.Context) error {
        if redisClient != nil {
            return redisClient.Close(ctx)
        }
        return nil
    }).
    OnCleanup(func(ctx context.Context) error {
        return database.DB.Migrator().DropTable(&models.ErrorLog{})
    })
```

### Step 3: Update Handler Initialization

Replace the old handler initialization:

**Before:**
```go
h := handlers.NewHandlers(streamClient, audioProcessor)
h.SetWebSocketHandler(wsHandler)
h.SetGorseClient(gorseClient)
h.SetSearchClient(baseSearchClient)
h.SetWaveformTools(waveformGenerator, waveformStorage)

authHandlers := handlers.NewAuthHandlers(authService, s3Uploader, streamClient)
errorTrackingHandler := handlers.NewErrorTrackingHandler()
soundHandlers := handlers.NewSoundHandlers()

// Static setters
handlers.SetAlertManager(alertManager)
handlers.SetAlertEvaluator(alertEvaluator)
```

**After:**
```go
// Create handlers with container
h := handlers.NewHandlers(appKernel)
authHandlers := handlers.NewAuthHandlers(appKernel)
errorTrackingHandler := handlers.NewErrorTrackingHandler(appKernel)
soundHandlers := handlers.NewSoundHandlers(appKernel)
```

### Step 4: Update Graceful Shutdown

Modify the shutdown handler to use the container:

**Before:**
```go
sigChan := make(chan os.Signal, 1)
signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

go func() {
    <-sigChan
    logger.Log.Info("Shutdown signal received")

    // Manual cleanup
    if redisClient != nil {
        _ = redisClient.Close()
    }
    if audioProcessor != nil {
        audioProcessor.Stop()
    }
    os.Exit(0)
}()
```

**After:**
```go
sigChan := make(chan os.Signal, 1)
signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

go func() {
    <-sigChan
    logger.Log.Info("Shutdown signal received")

    // Use container cleanup - handles all registered services
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    if err := appKernel.Cleanup(ctx); err != nil {
        logger.Log.Error("Cleanup error", zap.Error(err))
    }

    os.Exit(0)
}()
```

### Step 5: Pass Container to Route Handlers

When registering routes, ensure the container is available:

```go
// Existing route setup with container access
api := router.Group("/api/v1")

// Auth routes
authGroup := api.Group("/auth")
{
    authGroup.POST("/register", authHandlers.RegisterNativeUser)
    authGroup.POST("/login", authHandlers.AuthenticateNativeUser)
    authGroup.POST("/refresh", authHandlers.RefreshToken)
    // ... more auth routes
}

// User routes
userGroup := api.Group("/users", middleware.AuthRequired(appKernel))
{
    userGroup.GET("/me", h.GetMe)
    userGroup.PUT("/profile", h.UpdateUserProfile)
    // ... more user routes
}

// Sound routes
soundGroup := api.Group("/sounds")
{
    soundGroup.GET("/:id", soundHandlers.GetSound)
    soundGroup.GET("", soundHandlers.ListSounds)
    // ... more sound routes
}
```

### Step 6: Update Middleware if Needed

If any middleware needs access to the container:

```go
// Example: Auth middleware that uses container
func AuthRequired(c *kernel.Kernel) gin.HandlerFunc {
    return func(ctx *gin.Context) {
        authService := c.Auth()

        token := ctx.GetHeader("Authorization")
        if token == "" {
            ctx.JSON(401, gin.H{"error": "unauthorized"})
            ctx.Abort()
            return
        }

        // Use auth service from container...
        ctx.Next()
    }
}
```

## Code Changes Summary

### Files Modified:
1. ✅ `internal/handlers/handlers.go` - Updated to use container
2. ✅ `internal/handlers/auth.go` - Updated to use container
3. ✅ `internal/handlers/error_tracking.go` - Updated to use container
4. ✅ `internal/handlers/sounds.go` - Updated to use container

### Files Created:
1. ✅ `internal/kernel/container.go` - Main container implementation
2. ✅ `internal/kernel/errors.go` - Error types
3. ✅ `internal/kernel/mock.go` - Testing utilities
4. ✅ `internal/kernel/README.md` - Container documentation
5. ✅ `internal/kernel/INTEGRATION_GUIDE.md` - This file

## Manual Integration in main.go

To complete the integration, edit `cmd/server/main.go`:

### Location: After Service Initialization (around line 380)

```go
// ============================================================
// SETUP DEPENDENCY INJECTION CONTAINER
// ============================================================
appKernel := kernel.New()

// Register all services
appKernel.
    WithDB(database.DB).
    WithLogger(logger.Log)

if redisClient != nil {
    appKernel.WithCache(redisClient)
}

appKernel.
    WithStreamClient(streamClient).
    WithAuthService(authService)

if s3Uploader != nil {
    appKernel.WithS3Uploader(s3Uploader)
}

appKernel.
    WithAudioProcessor(audioProcessor)

if baseSearchClient != nil {
    appKernel.WithSearchClient(baseSearchClient)
}

if gorseClient != nil {
    appKernel.WithGorseClient(gorseClient)
}

appKernel.
    WithWebSocketHandler(wsHandler).
    WithAlertManager(alertManager).
    WithAlertEvaluator(alertEvaluator)

// Validate container
if err := appKernel.Validate(); err != nil {
    logger.FatalWithFields("Kernel validation failed", err)
}

// Register cleanup hooks
appKernel.
    OnCleanup(func(ctx context.Context) error {
        audioProcessor.Stop()
        return nil
    }).
    OnCleanup(func(ctx context.Context) error {
        if redisClient != nil {
            return redisClient.Close()
        }
        return nil
    })
```

### Location: Replace Handler Initialization (around line 552)

**Before:**
```go
h := handlers.NewHandlers(streamClient, audioProcessor)
h.SetWebSocketHandler(wsHandler)
h.SetGorseClient(gorseClient)
h.SetSearchClient(baseSearchClient)
h.SetWaveformTools(waveformGenerator, waveformStorage)

authHandlers := handlers.NewAuthHandlers(authService, s3Uploader, streamClient)
errorTrackingHandler := handlers.NewErrorTrackingHandler()
soundHandlers := handlers.NewSoundHandlers()

handlers.SetAlertManager(alertManager)
handlers.SetAlertEvaluator(alertEvaluator)
```

**After:**
```go
h := handlers.NewHandlers(appKernel)
authHandlers := handlers.NewAuthHandlers(appKernel)
errorTrackingHandler := handlers.NewErrorTrackingHandler(appKernel)
soundHandlers := handlers.NewSoundHandlers(appKernel)
```

### Location: Update Graceful Shutdown (around line 1000)

**Before:**
```go
sigChan := make(chan os.Signal, 1)
signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

go func() {
    <-sigChan
    logger.Log.Info("Shutdown signal received")
    audioProcessor.Stop()
    if redisClient != nil {
        _ = redisClient.Close()
    }
    os.Exit(0)
}()
```

**After:**
```go
sigChan := make(chan os.Signal, 1)
signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

go func() {
    <-sigChan
    logger.Log.Info("Shutdown signal received")

    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    if err := appKernel.Cleanup(ctx); err != nil {
        logger.Log.Error("Cleanup error", zap.Error(err))
    }

    os.Exit(0)
}()
```

## Testing with DI Container

### Unit Test Example

```go
func TestGetUser(t *testing.T) {
    // Create minimal container for testing
    container := container.MinimalMock().
        WithMockDB(setupTestDB(t)).
        WithMockLogger(logger.Log)

    h := handlers.NewHandlers(container)

    // Test handler...
}
```

### Integration Test Example

```go
func TestCreatePostFlow(t *testing.T) {
    // Create full container with mocks
    container := container.NewMock().
        WithMockDB(setupTestDB(t)).
        WithMockStreamClient(mockStreamClient(t)).
        WithMockS3Uploader(mockS3(t)).
        WithMockAudioProcessor(mockAudioProcessor(t))

    h := handlers.NewHandlers(container)
    authHandlers := handlers.NewAuthHandlers(container)

    // Test full flow...

    container.Clean(context.Background())
}
```

## Benefits of This Architecture

1. **Centralized Dependency Management**
   - Single source of truth for all services
   - Easy to see what services are available

2. **Simplified Testing**
   - Mock container for unit tests
   - No need to create 10+ mock objects separately

3. **Cleaner Code**
   - Handlers don't need giant constructor parameter lists
   - Easy to add new services without updating all handlers

4. **Better Lifecycle Management**
   - Cleanup functions in reverse order (LIFO)
   - Graceful shutdown is guaranteed

5. **Type Safety**
   - All dependencies are strongly typed
   - Compiler catches missing dependencies

6. **Flexibility**
   - Easy to swap implementations for testing
   - Container can be extended for future features

## Next Steps

1. Apply the changes from this guide to `cmd/server/main.go`
2. Run `go fmt ./...` to format code
3. Run `go test ./...` to ensure all tests pass
4. Update any other files that create handlers to use the container
5. Commit the changes to the feature branch

## FAQ

### Q: Can I still use direct service assignment?
A: It's possible but not recommended. The container provides better organization and lifecycle management.

### Q: What if a handler doesn't need all services?
A: That's fine. Just access the services you need from the container. Unused services don't cause any issues.

### Q: How do I add a new service to the container?
A: Add a Set*/Get* method pair to the Container struct following the existing pattern.

### Q: Can the container be used in middleware?
A: Yes, pass the container to middleware and access services from it.

### Q: What happens if a required service is missing?
A: Call `container.Validate()` which returns an error with the list of missing dependencies.
