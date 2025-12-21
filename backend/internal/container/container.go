// Package container provides dependency injection management for the Sidechain backend.
// It consolidates all services and provides type-safe access to dependencies.
package container

import (
	"context"
	"sync"

	"github.com/zfogg/sidechain/backend/internal/alerts"
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/auth"
	"github.com/zfogg/sidechain/backend/internal/cache"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/search"
	"github.com/zfogg/sidechain/backend/internal/storage"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/waveform"
	"github.com/zfogg/sidechain/backend/internal/websocket"
	"go.uber.org/zap"
	"gorm.io/gorm"
)

// Container holds all application dependencies and provides type-safe access.
// It implements the Service Locator pattern with additional lifecycle management.
type Container struct {
	// Core infrastructure
	db     *gorm.DB
	logger *zap.Logger
	cache  *cache.RedisClient

	// API clients
	stream    stream.StreamClientInterface
	search    *search.Client
	gorse     *recommendations.GorseRESTClient
	s3        *storage.S3Uploader
	auth      *auth.Service
	wsHandler *websocket.Handler

	// Audio processing
	audioProcessor *audio.Processor

	// Utilities
	waveformGenerator *waveform.Generator
	waveformStorage   *waveform.Storage

	// Features
	alertManager   *alerts.AlertManager
	alertEvaluator *alerts.Evaluator

	// Lifecycle hooks
	cleanupFuncs []func(context.Context) error
	mu           sync.RWMutex
}

// New creates a new empty container.
// Services should be registered using Set* methods.
func New() *Container {
	return &Container{
		cleanupFuncs: make([]func(context.Context) error, 0),
	}
}

// ============================================================================
// CORE INFRASTRUCTURE SETTERS/GETTERS
// ============================================================================

// SetDB registers the database connection
func (c *Container) SetDB(db *gorm.DB) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.db = db
	return c
}

// DB returns the database connection
func (c *Container) DB() *gorm.DB {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.db
}

// SetLogger registers the logger
func (c *Container) SetLogger(l *zap.Logger) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.logger = l
	return c
}

// Logger returns the logger instance
func (c *Container) Logger() *zap.Logger {
	c.mu.RLock()
	defer c.mu.RUnlock()
	if c.logger == nil {
		return logger.Log
	}
	return c.logger
}

// SetCache registers the Redis cache client
func (c *Container) SetCache(client *cache.RedisClient) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.cache = client
	return c
}

// Cache returns the Redis cache client
func (c *Container) Cache() *cache.RedisClient {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.cache
}

// ============================================================================
// API CLIENT SETTERS/GETTERS
// ============================================================================

// SetStreamClient registers the Stream.io client
func (c *Container) SetStreamClient(client stream.StreamClientInterface) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.stream = client
	return c
}

// Stream returns the Stream.io client
func (c *Container) Stream() stream.StreamClientInterface {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.stream
}

// SetSearchClient registers the Elasticsearch client
func (c *Container) SetSearchClient(client *search.Client) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.search = client
	return c
}

// Search returns the Elasticsearch client
func (c *Container) Search() *search.Client {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.search
}

// SetGorseClient registers the Gorse recommendation client
func (c *Container) SetGorseClient(client *recommendations.GorseRESTClient) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.gorse = client
	return c
}

// Gorse returns the Gorse recommendation client
func (c *Container) Gorse() *recommendations.GorseRESTClient {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.gorse
}

// SetS3Uploader registers the S3 storage uploader
func (c *Container) SetS3Uploader(uploader *storage.S3Uploader) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.s3 = uploader
	return c
}

// S3() returns the S3 storage uploader
func (c *Container) S3() *storage.S3Uploader {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.s3
}

// SetAuthService registers the authentication service
func (c *Container) SetAuthService(service *auth.Service) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.auth = service
	return c
}

// Auth returns the authentication service
func (c *Container) Auth() *auth.Service {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.auth
}

// SetWebSocketHandler registers the WebSocket handler
func (c *Container) SetWebSocketHandler(handler *websocket.Handler) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.wsHandler = handler
	return c
}

// WebSocket returns the WebSocket handler
func (c *Container) WebSocket() *websocket.Handler {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.wsHandler
}

// ============================================================================
// AUDIO PROCESSING SETTERS/GETTERS
// ============================================================================

// SetAudioProcessor registers the audio processor
func (c *Container) SetAudioProcessor(processor *audio.Processor) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.audioProcessor = processor
	return c
}

// AudioProcessor returns the audio processor
func (c *Container) AudioProcessor() *audio.Processor {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.audioProcessor
}

// ============================================================================
// UTILITY SETTERS/GETTERS
// ============================================================================

// SetWaveformGenerator registers the waveform generator
func (c *Container) SetWaveformGenerator(gen *waveform.Generator) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.waveformGenerator = gen
	return c
}

// WaveformGenerator returns the waveform generator
func (c *Container) WaveformGenerator() *waveform.Generator {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.waveformGenerator
}

// SetWaveformStorage registers the waveform storage
func (c *Container) SetWaveformStorage(storage *waveform.Storage) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.waveformStorage = storage
	return c
}

// WaveformStorage returns the waveform storage
func (c *Container) WaveformStorage() *waveform.Storage {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.waveformStorage
}

// ============================================================================
// FEATURE SETTERS/GETTERS
// ============================================================================

// SetAlertManager registers the alert manager
func (c *Container) SetAlertManager(manager *alerts.AlertManager) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.alertManager = manager
	return c
}

// AlertManager returns the alert manager
func (c *Container) AlertManager() *alerts.AlertManager {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.alertManager
}

// SetAlertEvaluator registers the alert evaluator
func (c *Container) SetAlertEvaluator(evaluator *alerts.Evaluator) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.alertEvaluator = evaluator
	return c
}

// AlertEvaluator returns the alert evaluator
func (c *Container) AlertEvaluator() *alerts.Evaluator {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.alertEvaluator
}

// ============================================================================
// LIFECYCLE MANAGEMENT
// ============================================================================

// OnCleanup registers a cleanup function to be called during shutdown.
// Cleanup functions are called in LIFO order (last registered, first cleaned up).
// This ensures proper dependency ordering during shutdown.
func (c *Container) OnCleanup(fn func(context.Context) error) *Container {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.cleanupFuncs = append(c.cleanupFuncs, fn)
	return c
}

// Cleanup performs graceful shutdown of all registered services.
// It calls cleanup functions in reverse order of registration.
func (c *Container) Cleanup(ctx context.Context) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	// Call cleanup functions in reverse order (LIFO)
	for i := len(c.cleanupFuncs) - 1; i >= 0; i-- {
		if err := c.cleanupFuncs[i](ctx); err != nil {
			// Log error but continue cleanup
			c.Logger().Error("Cleanup function failed", map[string]interface{}{
				"index": i,
				"error": err.Error(),
			})
		}
	}

	return nil
}

// ============================================================================
// VALIDATION
// ============================================================================

// Validate checks that all required dependencies are registered.
// This should be called after initialization and before starting the server.
func (c *Container) Validate() error {
	c.mu.RLock()
	defer c.mu.RUnlock()

	missingDeps := []string{}

	if c.db == nil {
		missingDeps = append(missingDeps, "database (DB)")
	}
	if c.stream == nil {
		missingDeps = append(missingDeps, "Stream.io client")
	}
	if c.auth == nil {
		missingDeps = append(missingDeps, "auth service")
	}
	if c.audioProcessor == nil {
		missingDeps = append(missingDeps, "audio processor")
	}

	// Optional but warn if missing
	optionalDeps := []struct {
		name  string
		value interface{}
	}{
		{"Redis cache", c.cache},
		{"Elasticsearch search", c.search},
		{"Gorse recommendations", c.gorse},
		{"S3 uploader", c.s3},
	}

	if len(missingDeps) > 0 {
		return NewInitializationError("Missing required dependencies", missingDeps)
	}

	return nil
}

// ============================================================================
// FLUENT API SUPPORT
// ============================================================================

// WithDB is a fluent setter for database
func (c *Container) WithDB(db *gorm.DB) *Container {
	return c.SetDB(db)
}

// WithLogger is a fluent setter for logger
func (c *Container) WithLogger(l *zap.Logger) *Container {
	return c.SetLogger(l)
}

// WithCache is a fluent setter for cache
func (c *Container) WithCache(client *cache.RedisClient) *Container {
	return c.SetCache(client)
}

// WithStreamClient is a fluent setter for Stream.io
func (c *Container) WithStreamClient(client stream.StreamClientInterface) *Container {
	return c.SetStreamClient(client)
}

// WithSearchClient is a fluent setter for Elasticsearch
func (c *Container) WithSearchClient(client *search.Client) *Container {
	return c.SetSearchClient(client)
}

// WithGorseClient is a fluent setter for Gorse
func (c *Container) WithGorseClient(client *recommendations.GorseRESTClient) *Container {
	return c.SetGorseClient(client)
}

// WithS3Uploader is a fluent setter for S3
func (c *Container) WithS3Uploader(uploader *storage.S3Uploader) *Container {
	return c.SetS3Uploader(uploader)
}

// WithAuthService is a fluent setter for auth
func (c *Container) WithAuthService(service *auth.Service) *Container {
	return c.SetAuthService(service)
}

// WithAudioProcessor is a fluent setter for audio processor
func (c *Container) WithAudioProcessor(processor *audio.Processor) *Container {
	return c.SetAudioProcessor(processor)
}

// WithWebSocketHandler is a fluent setter for WebSocket
func (c *Container) WithWebSocketHandler(handler *websocket.Handler) *Container {
	return c.SetWebSocketHandler(handler)
}

// WithAlertManager is a fluent setter for alert manager
func (c *Container) WithAlertManager(manager *alerts.AlertManager) *Container {
	return c.SetAlertManager(manager)
}

// WithAlertEvaluator is a fluent setter for alert evaluator
func (c *Container) WithAlertEvaluator(evaluator *alerts.Evaluator) *Container {
	return c.SetAlertEvaluator(evaluator)
}
