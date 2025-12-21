package kernel

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

// MockKernel is a kernel designed for testing.
// It allows easy overriding of dependencies with test doubles (mocks, stubs, fakes).
type MockKernel struct {
	*Kernel
	overrides map[string]interface{}
}

// NewMock creates a new mock kernel pre-populated with noop/stub implementations
func NewMock() *MockKernel {
	return &MockKernel{
		Container: New(),
		overrides: make(map[string]interface{}),
	}
}

// WithMockDB sets the database for testing
func (m *MockKernel) WithMockDB(db *gorm.DB) *MockKernel {
	m.SetDB(db)
	return m
}

// WithMockLogger sets a test logger
func (m *MockKernel) WithMockLogger(l *zap.Logger) *MockKernel {
	m.SetLogger(l)
	return m
}

// WithMockStreamClient sets a mock Stream.io client
func (m *MockKernel) WithMockStreamClient(client stream.StreamClientInterface) *MockKernel {
	m.SetStreamClient(client)
	return m
}

// WithMockSearchClient sets a mock search client
func (m *MockKernel) WithMockSearchClient(client *search.Client) *MockKernel {
	m.SetSearchClient(client)
	return m
}

// WithMockGorseClient sets a mock Gorse client
func (m *MockKernel) WithMockGorseClient(client *recommendations.GorseRESTClient) *MockKernel {
	m.SetGorseClient(client)
	return m
}

// WithMockS3Uploader sets a mock S3 uploader
func (m *MockKernel) WithMockS3Uploader(uploader *storage.S3Uploader) *MockKernel {
	m.SetS3Uploader(uploader)
	return m
}

// WithMockAuthService sets a mock auth service
func (m *MockKernel) WithMockAuthService(service *auth.Service) *MockKernel {
	m.SetAuthService(service)
	return m
}

// WithMockAudioProcessor sets a mock audio processor
func (m *MockKernel) WithMockAudioProcessor(processor *audio.Processor) *MockKernel {
	m.SetAudioProcessor(processor)
	return m
}

// WithMockWebSocketHandler sets a mock WebSocket handler
func (m *MockKernel) WithMockWebSocketHandler(handler *websocket.Handler) *MockKernel {
	m.SetWebSocketHandler(handler)
	return m
}

// WithMockCache sets a mock cache
func (m *MockKernel) WithMockCache(c *cache.RedisClient) *MockKernel {
	m.SetCache(c)
	return m
}

// WithMockAlertManager sets a mock alert manager
func (m *MockKernel) WithMockAlertManager(manager *alerts.AlertManager) *MockKernel {
	m.SetAlertManager(manager)
	return m
}

// WithMockAlertEvaluator sets a mock alert evaluator
func (m *MockKernel) WithMockAlertEvaluator(evaluator *alerts.Evaluator) *MockKernel {
	m.SetAlertEvaluator(evaluator)
	return m
}

// Override sets a custom override for a specific dependency type
func (m *MockKernel) Override(key string, value interface{}) *MockKernel {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.overrides[key] = value
	return m
}

// GetOverride retrieves an override if set
func (m *MockKernel) GetOverride(key string) (interface{}, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	val, ok := m.overrides[key]
	return val, ok
}

// MinimalMock creates a mock kernel with only the absolute minimum dependencies
// Useful for isolated unit tests
func MinimalMock() *MockKernel {
	mock := NewMock()
	mock.SetLogger(logger.Log)
	return mock
}

// FullMock creates a mock kernel with stub implementations for all dependencies
// Note: This is a placeholder - actual implementations would need test doubles
func FullMock() *MockKernel {
	mock := NewMock()
	mock.SetLogger(logger.Log)
	// Additional setup would go here
	return mock
}

// Clean cleans up test kernels after tests complete
func (m *MockKernel) Clean(ctx context.Context) error {
	return m.Cleanup(ctx)
}
