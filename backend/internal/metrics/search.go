package metrics

import (
	"sync"
	"sync/atomic"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
)

// Search metrics exported to Prometheus
var (
	SearchQueriesTotal = promauto.NewCounterVec(
		prometheus.CounterOpts{
			Name: "search_queries_total",
			Help: "Total number of search queries",
		},
		[]string{"type"},
	)

	SearchQueryDuration = promauto.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "search_query_duration_seconds",
			Help:    "Search query duration in seconds",
			Buckets: []float64{0.01, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5},
		},
		[]string{"type"},
	)

	SearchResultsTotal = promauto.NewCounterVec(
		prometheus.CounterOpts{
			Name: "search_results_total",
			Help: "Total number of search results returned",
		},
		[]string{"type"},
	)

	SearchCacheHitsTotal = promauto.NewCounter(
		prometheus.CounterOpts{
			Name: "search_cache_hits_total",
			Help: "Total number of search cache hits",
		},
	)

	SearchCacheMissesTotal = promauto.NewCounter(
		prometheus.CounterOpts{
			Name: "search_cache_misses_total",
			Help: "Total number of search cache misses",
		},
	)

	SearchErrorsTotal = promauto.NewCounterVec(
		prometheus.CounterOpts{
			Name: "search_errors_total",
			Help: "Total number of search errors",
		},
		[]string{"type", "error_type"},
	)
)

// SearchMetrics tracks performance and usage metrics for search operations
type SearchMetrics struct {
	// Query counters
	QueryCount        int64
	PostsSearched     int64
	UsersSearched     int64
	AdvancedSearched  int64

	// Cache metrics
	CacheHits         int64
	CacheMisses       int64

	// Performance metrics (in milliseconds)
	TotalQueryTime    int64 // sum of all query times
	MaxQueryTime      int64 // slowest query
	MinQueryTime      int64 // fastest query

	// Error tracking
	ErrorCount        int64
	TimeoutCount      int64

	// Result metrics
	TotalResults      int64 // cumulative result count

	// Per-operation timing
	mu                sync.RWMutex
	queryTimings      []int64 // recent query timings for percentile calculation
	maxTimingsSize    int
}

// QueryMetric represents a single search query's metrics
type QueryMetric struct {
	Type          string    // "posts", "users", "advanced"
	Query         string
	ResultCount   int
	Duration      time.Duration
	CacheHit      bool
	Error         bool
	Timestamp     time.Time
}

// NewSearchMetrics creates a new search metrics tracker
func NewSearchMetrics() *SearchMetrics {
	return &SearchMetrics{
		queryTimings: make([]int64, 0, 10000),
		maxTimingsSize: 10000,
	}
}

// RecordQuery records a search query metric
func (sm *SearchMetrics) RecordQuery(metric QueryMetric) {
	// Increment query count
	atomic.AddInt64(&sm.QueryCount, 1)

	// Type-specific counters
	switch metric.Type {
	case "posts":
		atomic.AddInt64(&sm.PostsSearched, 1)
	case "users":
		atomic.AddInt64(&sm.UsersSearched, 1)
	case "advanced":
		atomic.AddInt64(&sm.AdvancedSearched, 1)
	}

	// Result tracking
	atomic.AddInt64(&sm.TotalResults, int64(metric.ResultCount))

	// Cache tracking
	if metric.CacheHit {
		atomic.AddInt64(&sm.CacheHits, 1)
		SearchCacheHitsTotal.Inc()
	} else {
		atomic.AddInt64(&sm.CacheMisses, 1)
		SearchCacheMissesTotal.Inc()
	}

	// Error tracking
	if metric.Error {
		atomic.AddInt64(&sm.ErrorCount, 1)
		SearchErrorsTotal.WithLabelValues(metric.Type, "query_failed").Inc()
	}

	// Duration tracking (in milliseconds)
	durationMs := metric.Duration.Milliseconds()
	durationSec := float64(durationMs) / 1000.0

	// Update total time
	atomic.AddInt64(&sm.TotalQueryTime, durationMs)

	// Update min/max
	sm.updateMinMax(durationMs)

	// Store for percentile calculation
	sm.mu.Lock()
	if len(sm.queryTimings) < sm.maxTimingsSize {
		sm.queryTimings = append(sm.queryTimings, durationMs)
	}
	sm.mu.Unlock()

	// Export to Prometheus
	SearchQueriesTotal.WithLabelValues(metric.Type).Inc()
	SearchQueryDuration.WithLabelValues(metric.Type).Observe(durationSec)
	SearchResultsTotal.WithLabelValues(metric.Type).Add(float64(metric.ResultCount))
}

// RecordTimeout records a query timeout
func (sm *SearchMetrics) RecordTimeout() {
	atomic.AddInt64(&sm.TimeoutCount, 1)
	atomic.AddInt64(&sm.ErrorCount, 1)
}

// updateMinMax updates min and max query times
func (sm *SearchMetrics) updateMinMax(duration int64) {
	for {
		oldMin := atomic.LoadInt64(&sm.MinQueryTime)
		if oldMin == 0 || duration < oldMin {
			if atomic.CompareAndSwapInt64(&sm.MinQueryTime, oldMin, duration) {
				break
			}
		} else {
			break
		}
	}

	for {
		oldMax := atomic.LoadInt64(&sm.MaxQueryTime)
		if duration > oldMax {
			if atomic.CompareAndSwapInt64(&sm.MaxQueryTime, oldMax, duration) {
				break
			}
		} else {
			break
		}
	}
}

// GetStats returns current metrics as a map
func (sm *SearchMetrics) GetStats() map[string]interface{} {
	queryCount := atomic.LoadInt64(&sm.QueryCount)
	cacheHits := atomic.LoadInt64(&sm.CacheHits)
	cacheMisses := atomic.LoadInt64(&sm.CacheMisses)
	totalTime := atomic.LoadInt64(&sm.TotalQueryTime)

	var avgTime float64
	if queryCount > 0 {
		avgTime = float64(totalTime) / float64(queryCount)
	}

	var cacheHitRate float64
	totalCacheOps := cacheHits + cacheMisses
	if totalCacheOps > 0 {
		cacheHitRate = float64(cacheHits) / float64(totalCacheOps) * 100
	}

	var errorRate float64
	if queryCount > 0 {
		errorRate = float64(atomic.LoadInt64(&sm.ErrorCount)) / float64(queryCount) * 100
	}

	// Calculate percentiles
	sm.mu.RLock()
	p50, p95, p99 := sm.calculatePercentiles()
	sm.mu.RUnlock()

	return map[string]interface{}{
		"total_queries":        queryCount,
		"posts_searched":       atomic.LoadInt64(&sm.PostsSearched),
		"users_searched":       atomic.LoadInt64(&sm.UsersSearched),
		"advanced_searched":    atomic.LoadInt64(&sm.AdvancedSearched),
		"cache_hits":           cacheHits,
		"cache_misses":         cacheMisses,
		"cache_hit_rate":       cacheHitRate,
		"total_results":        atomic.LoadInt64(&sm.TotalResults),
		"error_count":          atomic.LoadInt64(&sm.ErrorCount),
		"error_rate":           errorRate,
		"timeout_count":        atomic.LoadInt64(&sm.TimeoutCount),
		"avg_query_time_ms":    avgTime,
		"min_query_time_ms":    atomic.LoadInt64(&sm.MinQueryTime),
		"max_query_time_ms":    atomic.LoadInt64(&sm.MaxQueryTime),
		"p50_query_time_ms":    p50,
		"p95_query_time_ms":    p95,
		"p99_query_time_ms":    p99,
		"timestamp":            time.Now().Unix(),
	}
}

// calculatePercentiles calculates p50, p95, p99 from recent timings
// Note: assumes mu is already locked
func (sm *SearchMetrics) calculatePercentiles() (p50, p95, p99 int64) {
	if len(sm.queryTimings) == 0 {
		return 0, 0, 0
	}

	// Simple bubble sort for small datasets
	timings := make([]int64, len(sm.queryTimings))
	copy(timings, sm.queryTimings)

	for i := 0; i < len(timings); i++ {
		for j := i + 1; j < len(timings); j++ {
			if timings[j] < timings[i] {
				timings[i], timings[j] = timings[j], timings[i]
			}
		}
	}

	n := len(timings)
	p50 = timings[(n*50)/100]
	p95 = timings[(n*95)/100]
	p99 = timings[(n*99)/100]

	return
}

// Reset clears all metrics
func (sm *SearchMetrics) Reset() {
	atomic.StoreInt64(&sm.QueryCount, 0)
	atomic.StoreInt64(&sm.PostsSearched, 0)
	atomic.StoreInt64(&sm.UsersSearched, 0)
	atomic.StoreInt64(&sm.AdvancedSearched, 0)
	atomic.StoreInt64(&sm.CacheHits, 0)
	atomic.StoreInt64(&sm.CacheMisses, 0)
	atomic.StoreInt64(&sm.TotalQueryTime, 0)
	atomic.StoreInt64(&sm.MaxQueryTime, 0)
	atomic.StoreInt64(&sm.MinQueryTime, 0)
	atomic.StoreInt64(&sm.ErrorCount, 0)
	atomic.StoreInt64(&sm.TimeoutCount, 0)
	atomic.StoreInt64(&sm.TotalResults, 0)

	sm.mu.Lock()
	sm.queryTimings = sm.queryTimings[:0]
	sm.mu.Unlock()
}
