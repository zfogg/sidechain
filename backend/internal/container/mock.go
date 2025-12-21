package container

import (
	"context"

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

// MockContainer is a container designed for testing.
// It allows easy overriding of dependencies with test doubles (mocks, stubs, fakes).
type MockContainer struct {
	*Container
	overrides map[string]interface{}
}

// NewMock creates a new mock container pre-populated with noop/stub implementations
func NewMock() *MockContainer {
	return &MockContainer{
		Container: New(),
		overrides: make(map[string]interface{}),
	}
}

// WithMockDB sets the database for testing
func (m *MockContainer) WithMockDB(db *gorm.DB) *MockContainer {
	m.SetDB(db)
	return m
}

// WithMockLogger sets a test logger
func (m *MockContainer) WithMockLogger(l *zap.Logger) *MockContainer {
	m.SetLogger(l)
	return m
}

// WithMockStreamClient sets a mock Stream.io client
func (m *MockContainer) WithMockStreamClient(client stream.StreamClientInterface) *MockContainer {
	m.SetStreamClient(client)
	return m
}

// WithMockSearchClient sets a mock search client
func (m *MockContainer) WithMockSearchClient(client *search.Client) *MockContainer {
	m.SetSearchClient(client)
	return m
}

// WithMockGorseClient sets a mock Gorse client
func (m *MockContainer) WithMockGorseClient(client *recommendations.GorseRESTClient) *MockContainer {
	m.SetGorseClient(client)
	return m
}

// WithMockS3Uploader sets a mock S3 uploader
func (m *MockContainer) WithMockS3Uploader(uploader *storage.S3Uploader) *MockContainer {
	m.SetS3Uploader(uploader)
	return m
}

// WithMockAuthService sets a mock auth service
func (m *MockContainer) WithMockAuthService(service *auth.Service) *MockContainer {
	m.SetAuthService(service)
	return m
}

// WithMockAudioProcessor sets a mock audio processor
func (m *MockContainer) WithMockAudioProcessor(processor *audio.Processor) *MockContainer {
	m.SetAudioProcessor(processor)
	return m
}

// WithMockWebSocketHandler sets a mock WebSocket handler
func (m *MockContainer) WithMockWebSocketHandler(handler *websocket.Handler) *MockContainer {
	m.SetWebSocketHandler(handler)
	return m
}

// WithMockCache sets a mock cache
func (m *MockContainer) WithMockCache(c *cache.RedisClient) *MockContainer {
	m.SetCache(c)
	return m
}

// WithMockAlertManager sets a mock alert manager
func (m *MockContainer) WithMockAlertManager(manager *alerts.AlertManager) *MockContainer {
	m.SetAlertManager(manager)
	return m
}

// WithMockAlertEvaluator sets a mock alert evaluator
func (m *MockContainer) WithMockAlertEvaluator(evaluator *alerts.Evaluator) *MockContainer {
	m.SetAlertEvaluator(evaluator)
	return m
}

// Override sets a custom override for a specific dependency type
func (m *MockContainer) Override(key string, value interface{}) *MockContainer {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.overrides[key] = value
	return m
}

// GetOverride retrieves an override if set
func (m *MockContainer) GetOverride(key string) (interface{}, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	val, ok := m.overrides[key]
	return val, ok
}

// MinimalMock creates a mock container with only the absolute minimum dependencies
// Useful for isolated unit tests
func MinimalMock() *MockContainer {
	mock := NewMock()
	mock.SetLogger(logger.Log)
	return mock
}

// FullMock creates a mock container with stub implementations for all dependencies
// Note: This is a placeholder - actual implementations would need test doubles
func FullMock() *MockContainer {
	mock := NewMock()
	mock.SetLogger(logger.Log)
	// Additional setup would go here
	return mock
}

// Clean cleans up test containers after tests complete
func (m *MockContainer) Clean(ctx context.Context) error {
	return m.Cleanup(ctx)
}
