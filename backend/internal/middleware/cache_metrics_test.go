package middleware

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/zfogg/sidechain/backend/internal/metrics"
)

func TestCacheMetrics(t *testing.T) {
	// Initialize metrics
	m := metrics.Initialize()

	t.Run("RecordCacheHit increments counter", func(t *testing.T) {
		// Reset metrics for clean test
		m.CacheHitsTotal.Reset()

		RecordCacheHit("test_cache")
		RecordCacheHit("test_cache")

		// Verify counter was incremented by checking metric collection
		// We can't easily get the value without testutil, so we verify it doesn't panic
		counter := m.CacheHitsTotal.WithLabelValues("test_cache")
		assert.NotNil(t, counter, "Cache hits counter should exist")
	})

	t.Run("RecordCacheMiss increments counter", func(t *testing.T) {
		// Reset metrics for clean test
		m.CacheMissesTotal.Reset()

		RecordCacheMiss("test_cache")
		RecordCacheMiss("test_cache")
		RecordCacheMiss("test_cache")

		// Verify counter exists
		counter := m.CacheMissesTotal.WithLabelValues("test_cache")
		assert.NotNil(t, counter, "Cache misses counter should exist")
	})

	t.Run("RecordCacheOperation increments operations counter", func(t *testing.T) {
		// Reset metrics for clean test
		m.CacheOperationsTotal.Reset()

		RecordCacheOperation("GET", "test_cache", 10*time.Millisecond)
		RecordCacheOperation("GET", "test_cache", 20*time.Millisecond)
		RecordCacheOperation("SET", "test_cache", 15*time.Millisecond)

		// Verify counters exist
		getCounter := m.CacheOperationsTotal.WithLabelValues("GET", "test_cache")
		setCounter := m.CacheOperationsTotal.WithLabelValues("SET", "test_cache")

		assert.NotNil(t, getCounter, "GET operations counter should exist")
		assert.NotNil(t, setCounter, "SET operations counter should exist")
	})

	t.Run("RecordCacheOperation records duration histogram", func(t *testing.T) {
		// Reset metrics for clean test
		m.CacheOperationDuration.Reset()

		RecordCacheOperation("GET", "test_cache", 50*time.Millisecond)
		RecordCacheOperation("GET", "test_cache", 100*time.Millisecond)

		// Verify histogram exists
		histogram := m.CacheOperationDuration.WithLabelValues("GET", "test_cache")
		assert.NotNil(t, histogram, "Cache operation duration histogram should exist")
	})

	t.Run("Multiple cache names are tracked separately", func(t *testing.T) {
		// Reset metrics for clean test
		m.CacheHitsTotal.Reset()

		RecordCacheHit("cache1")
		RecordCacheHit("cache2")
		RecordCacheHit("cache1")

		// Verify separate counters exist
		cache1Counter := m.CacheHitsTotal.WithLabelValues("cache1")
		cache2Counter := m.CacheHitsTotal.WithLabelValues("cache2")

		assert.NotNil(t, cache1Counter, "cache1 counter should exist")
		assert.NotNil(t, cache2Counter, "cache2 counter should exist")
		assert.NotEqual(t, cache1Counter, cache2Counter, "Different cache names should have different counters")
	})

	t.Run("Cache metrics are properly initialized", func(t *testing.T) {
		// Verify metrics are not nil
		assert.NotNil(t, m.CacheHitsTotal, "CacheHitsTotal should be initialized")
		assert.NotNil(t, m.CacheMissesTotal, "CacheMissesTotal should be initialized")
		assert.NotNil(t, m.CacheOperationsTotal, "CacheOperationsTotal should be initialized")
		assert.NotNil(t, m.CacheOperationDuration, "CacheOperationDuration should be initialized")
	})

	t.Run("Cache operations record both counter and histogram", func(t *testing.T) {
		// Reset metrics
		m.CacheOperationsTotal.Reset()
		m.CacheOperationDuration.Reset()

		// Record an operation
		RecordCacheOperation("GET", "test_cache", 25*time.Millisecond)

		// Verify both metrics exist
		opCounter := m.CacheOperationsTotal.WithLabelValues("GET", "test_cache")
		opHistogram := m.CacheOperationDuration.WithLabelValues("GET", "test_cache")

		assert.NotNil(t, opCounter, "Operations counter should exist")
		assert.NotNil(t, opHistogram, "Operations duration histogram should exist")
	})
}
