// Package kernel provides dependency injection management for the Sidechain backend.
// It consolidates all services and provides type-safe access to dependencies.
package kernel

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

// Kernel holds all application dependencies and provides type-safe access.
// It implements the Service Locator pattern with additional lifecycle management.
type Kernel struct {
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

// New creates a new empty kernel.
// Services should be registered using Set* methods.
func New() *Kernel {
	return &Kernel{
		cleanupFuncs: make([]func(context.Context) error, 0),
	}
}

// ============================================================================
// CORE INFRASTRUCTURE SETTERS/GETTERS
// ============================================================================

// SetDB registers the database connection
func (c *Kernel) SetDB(db *gorm.DB) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.db = db
	return c
}

// DB returns the database connection
func (c *Kernel) DB() *gorm.DB {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.db
}

// SetLogger registers the logger
func (c *Kernel) SetLogger(l *zap.Logger) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.logger = l
	return c
}

// Logger returns the logger instance
func (c *Kernel) Logger() *zap.Logger {
	c.mu.RLock()
	defer c.mu.RUnlock()
	if c.logger == nil {
		return logger.Log
	}
	return c.logger
}

// SetCache registers the Redis cache client
func (c *Kernel) SetCache(client *cache.RedisClient) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.cache = client
	return c
}

// Cache returns the Redis cache client
func (c *Kernel) Cache() *cache.RedisClient {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.cache
}

// ============================================================================
// API CLIENT SETTERS/GETTERS
// ============================================================================

// SetStreamClient registers the Stream.io client
func (c *Kernel) SetStreamClient(client stream.StreamClientInterface) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.stream = client
	return c
}

// Stream returns the Stream.io client
func (c *Kernel) Stream() stream.StreamClientInterface {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.stream
}

// SetSearchClient registers the Elasticsearch client
func (c *Kernel) SetSearchClient(client *search.Client) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.search = client
	return c
}

// Search returns the Elasticsearch client
func (c *Kernel) Search() *search.Client {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.search
}

// SetGorseClient registers the Gorse recommendation client
func (c *Kernel) SetGorseClient(client *recommendations.GorseRESTClient) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.gorse = client
	return c
}

// Gorse returns the Gorse recommendation client
func (c *Kernel) Gorse() *recommendations.GorseRESTClient {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.gorse
}

// SetS3Uploader registers the S3 storage uploader
func (c *Kernel) SetS3Uploader(uploader *storage.S3Uploader) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.s3 = uploader
	return c
}

// S3() returns the S3 storage uploader
func (c *Kernel) S3() *storage.S3Uploader {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.s3
}

// SetAuthService registers the authentication service
func (c *Kernel) SetAuthService(service *auth.Service) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.auth = service
	return c
}

// Auth returns the authentication service
func (c *Kernel) Auth() *auth.Service {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.auth
}

// SetWebSocketHandler registers the WebSocket handler
func (c *Kernel) SetWebSocketHandler(handler *websocket.Handler) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.wsHandler = handler
	return c
}

// WebSocket returns the WebSocket handler
func (c *Kernel) WebSocket() *websocket.Handler {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.wsHandler
}

// ============================================================================
// AUDIO PROCESSING SETTERS/GETTERS
// ============================================================================

// SetAudioProcessor registers the audio processor
func (c *Kernel) SetAudioProcessor(processor *audio.Processor) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.audioProcessor = processor
	return c
}

// AudioProcessor returns the audio processor
func (c *Kernel) AudioProcessor() *audio.Processor {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.audioProcessor
}

// ============================================================================
// UTILITY SETTERS/GETTERS
// ============================================================================

// SetWaveformGenerator registers the waveform generator
func (c *Kernel) SetWaveformGenerator(gen *waveform.Generator) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.waveformGenerator = gen
	return c
}

// WaveformGenerator returns the waveform generator
func (c *Kernel) WaveformGenerator() *waveform.Generator {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.waveformGenerator
}

// SetWaveformStorage registers the waveform storage
func (c *Kernel) SetWaveformStorage(storage *waveform.Storage) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.waveformStorage = storage
	return c
}

// WaveformStorage returns the waveform storage
func (c *Kernel) WaveformStorage() *waveform.Storage {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.waveformStorage
}

// ============================================================================
// FEATURE SETTERS/GETTERS
// ============================================================================

// SetAlertManager registers the alert manager
func (c *Kernel) SetAlertManager(manager *alerts.AlertManager) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.alertManager = manager
	return c
}

// AlertManager returns the alert manager
func (c *Kernel) AlertManager() *alerts.AlertManager {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.alertManager
}

// SetAlertEvaluator registers the alert evaluator
func (c *Kernel) SetAlertEvaluator(evaluator *alerts.Evaluator) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.alertEvaluator = evaluator
	return c
}

// AlertEvaluator returns the alert evaluator
func (c *Kernel) AlertEvaluator() *alerts.Evaluator {
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
func (c *Kernel) OnCleanup(fn func(context.Context) error) *Kernel {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.cleanupFuncs = append(c.cleanupFuncs, fn)
	return c
}

// Cleanup performs graceful shutdown of all registered services.
// It calls cleanup functions in reverse order of registration.
func (c *Kernel) Cleanup(ctx context.Context) error {
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
func (c *Kernel) Validate() error {
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
func (c *Kernel) WithDB(db *gorm.DB) *Kernel {
	return c.SetDB(db)
}

// WithLogger is a fluent setter for logger
func (c *Kernel) WithLogger(l *zap.Logger) *Kernel {
	return c.SetLogger(l)
}

// WithCache is a fluent setter for cache
func (c *Kernel) WithCache(client *cache.RedisClient) *Kernel {
	return c.SetCache(client)
}

// WithStreamClient is a fluent setter for Stream.io
func (c *Kernel) WithStreamClient(client stream.StreamClientInterface) *Kernel {
	return c.SetStreamClient(client)
}

// WithSearchClient is a fluent setter for Elasticsearch
func (c *Kernel) WithSearchClient(client *search.Client) *Kernel {
	return c.SetSearchClient(client)
}

// WithGorseClient is a fluent setter for Gorse
func (c *Kernel) WithGorseClient(client *recommendations.GorseRESTClient) *Kernel {
	return c.SetGorseClient(client)
}

// WithS3Uploader is a fluent setter for S3
func (c *Kernel) WithS3Uploader(uploader *storage.S3Uploader) *Kernel {
	return c.SetS3Uploader(uploader)
}

// WithAuthService is a fluent setter for auth
func (c *Kernel) WithAuthService(service *auth.Service) *Kernel {
	return c.SetAuthService(service)
}

// WithAudioProcessor is a fluent setter for audio processor
func (c *Kernel) WithAudioProcessor(processor *audio.Processor) *Kernel {
	return c.SetAudioProcessor(processor)
}

// WithWebSocketHandler is a fluent setter for WebSocket
func (c *Kernel) WithWebSocketHandler(handler *websocket.Handler) *Kernel {
	return c.SetWebSocketHandler(handler)
}

// WithAlertManager is a fluent setter for alert manager
func (c *Kernel) WithAlertManager(manager *alerts.AlertManager) *Kernel {
	return c.SetAlertManager(manager)
}

// WithAlertEvaluator is a fluent setter for alert evaluator
func (c *Kernel) WithAlertEvaluator(evaluator *alerts.Evaluator) *Kernel {
	return c.SetAlertEvaluator(evaluator)
}
