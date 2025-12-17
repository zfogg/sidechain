package metrics

import (
	"sync"
)

// Manager manages all application metrics
type Manager struct {
	Search *SearchMetrics
	mu     sync.RWMutex
}

// Global metrics manager instance
var globalManager *Manager
var managerOnce sync.Once

// GetManager returns the global metrics manager (singleton)
func GetManager() *Manager {
	managerOnce.Do(func() {
		globalManager = &Manager{
			Search: NewSearchMetrics(),
		}
	})
	return globalManager
}

// ResetAll resets all metrics
func (m *Manager) ResetAll() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Search.Reset()
}

// GetAllMetrics returns all metrics as a map
func (m *Manager) GetAllMetrics() map[string]interface{} {
	m.mu.RLock()
	defer m.mu.RUnlock()

	return map[string]interface{}{
		"search": m.Search.GetStats(),
	}
}

// GetSearchStats returns only search metrics
func (m *Manager) GetSearchStats() map[string]interface{} {
	m.mu.RLock()
	defer m.mu.RUnlock()

	return m.Search.GetStats()
}
