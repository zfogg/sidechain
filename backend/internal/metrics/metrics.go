package metrics

import (
	"sync"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
)

// Metrics holds all Prometheus metrics for the application
type Metrics struct {
	// HTTP metrics
	HTTPRequestsTotal     prometheus.CounterVec
	HTTPRequestDuration   prometheus.HistogramVec
	HTTPRequestSize       prometheus.HistogramVec
	HTTPResponseSize      prometheus.HistogramVec
	HTTPActiveConnections prometheus.GaugeVec

	// Cache metrics
	CacheHitsTotal         prometheus.CounterVec
	CacheMissesTotal       prometheus.CounterVec
	CacheOperationsTotal   prometheus.CounterVec
	CacheOperationDuration prometheus.HistogramVec
	CacheEvictionsTotal    prometheus.CounterVec

	// Rate limiting metrics
	RateLimitExceededTotal prometheus.CounterVec
	RateLimitBucketUsage   prometheus.GaugeVec

	// Database metrics
	DatabaseQueryDuration   prometheus.HistogramVec
	DatabaseQueriesTotal    prometheus.CounterVec
	DatabaseConnectionsOpen prometheus.GaugeVec

	// Redis metrics
	RedisOperationDuration prometheus.HistogramVec
	RedisOperationsTotal   prometheus.CounterVec
	RedisConnectionsOpen   prometheus.GaugeVec

	// Feed/recommendation metrics
	FeedGenerationTime   prometheus.HistogramVec
	GorseRecommendations prometheus.CounterVec
	GorseErrors          prometheus.CounterVec

	// Error metrics
	ErrorsTotal prometheus.CounterVec
}

var (
	instance *Metrics
	once     sync.Once
)

// Initialize creates and registers all Prometheus metrics
func Initialize() *Metrics {
	once.Do(func() {
		instance = &Metrics{
			// HTTP metrics
			HTTPRequestsTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "http_requests_total",
					Help: "Total number of HTTP requests",
				},
				[]string{"method", "path", "status"},
			),
			HTTPRequestDuration: *promauto.NewHistogramVec(
				prometheus.HistogramOpts{
					Name:    "http_request_duration_seconds",
					Help:    "HTTP request latency in seconds",
					Buckets: []float64{.001, .005, .01, .025, .05, .1, .25, .5, 1, 2.5, 5},
				},
				[]string{"method", "path", "status"},
			),
			HTTPRequestSize: *promauto.NewHistogramVec(
				prometheus.HistogramOpts{
					Name:    "http_request_size_bytes",
					Help:    "HTTP request body size in bytes",
					Buckets: prometheus.ExponentialBuckets(100, 10, 7),
				},
				[]string{"method", "path"},
			),
			HTTPResponseSize: *promauto.NewHistogramVec(
				prometheus.HistogramOpts{
					Name:    "http_response_size_bytes",
					Help:    "HTTP response size in bytes",
					Buckets: prometheus.ExponentialBuckets(100, 10, 7),
				},
				[]string{"method", "path", "status"},
			),
			HTTPActiveConnections: *promauto.NewGaugeVec(
				prometheus.GaugeOpts{
					Name: "http_active_connections",
					Help: "Number of currently active HTTP connections",
				},
				[]string{"method", "path"},
			),

			// Cache metrics
			CacheHitsTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "cache_hits_total",
					Help: "Total number of cache hits",
				},
				[]string{"cache_name"},
			),
			CacheMissesTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "cache_misses_total",
					Help: "Total number of cache misses",
				},
				[]string{"cache_name"},
			),
			CacheOperationsTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "cache_operations_total",
					Help: "Total number of cache operations",
				},
				[]string{"operation", "cache_name"},
			),
			CacheOperationDuration: *promauto.NewHistogramVec(
				prometheus.HistogramOpts{
					Name:    "cache_operation_duration_seconds",
					Help:    "Cache operation latency in seconds",
					Buckets: []float64{.0001, .0005, .001, .005, .01, .05, .1},
				},
				[]string{"operation", "cache_name"},
			),
			CacheEvictionsTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "cache_evictions_total",
					Help: "Total number of cache evictions",
				},
				[]string{"cache_name"},
			),

			// Rate limiting metrics
			RateLimitExceededTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "rate_limit_exceeded_total",
					Help: "Total number of rate limit violations",
				},
				[]string{"endpoint", "method"},
			),
			RateLimitBucketUsage: *promauto.NewGaugeVec(
				prometheus.GaugeOpts{
					Name: "rate_limit_bucket_usage",
					Help: "Current rate limit bucket usage (tokens used)",
				},
				[]string{"endpoint", "client_ip"},
			),

			// Database metrics
			DatabaseQueryDuration: *promauto.NewHistogramVec(
				prometheus.HistogramOpts{
					Name:    "database_query_duration_seconds",
					Help:    "Database query latency in seconds",
					Buckets: []float64{.001, .005, .01, .05, .1, .25, .5, 1, 2.5, 5},
				},
				[]string{"query_type", "table"},
			),
			DatabaseQueriesTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "database_queries_total",
					Help: "Total number of database queries",
				},
				[]string{"query_type", "table", "status"},
			),
			DatabaseConnectionsOpen: *promauto.NewGaugeVec(
				prometheus.GaugeOpts{
					Name: "database_connections_open",
					Help: "Number of currently open database connections",
				},
				[]string{"database"},
			),

			// Redis metrics
			RedisOperationDuration: *promauto.NewHistogramVec(
				prometheus.HistogramOpts{
					Name:    "redis_operation_duration_seconds",
					Help:    "Redis operation latency in seconds",
					Buckets: []float64{.0001, .0005, .001, .005, .01, .05, .1},
				},
				[]string{"operation", "key_pattern"},
			),
			RedisOperationsTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "redis_operations_total",
					Help: "Total number of Redis operations",
				},
				[]string{"operation", "status"},
			),
			RedisConnectionsOpen: *promauto.NewGaugeVec(
				prometheus.GaugeOpts{
					Name: "redis_connections_open",
					Help: "Number of currently open Redis connections",
				},
				[]string{"instance"},
			),

			// Feed/recommendation metrics
			FeedGenerationTime: *promauto.NewHistogramVec(
				prometheus.HistogramOpts{
					Name:    "feed_generation_duration_seconds",
					Help:    "Time to generate feed in seconds",
					Buckets: []float64{.01, .05, .1, .25, .5, 1, 2.5, 5},
				},
				[]string{"feed_type"},
			),
			GorseRecommendations: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "gorse_recommendations_total",
					Help: "Total number of recommendations fetched from Gorse",
				},
				[]string{"recommendation_type"},
			),
			GorseErrors: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "gorse_errors_total",
					Help: "Total number of Gorse errors",
				},
				[]string{"error_type"},
			),

			// Error metrics
			ErrorsTotal: *promauto.NewCounterVec(
				prometheus.CounterOpts{
					Name: "errors_total",
					Help: "Total number of errors by type",
				},
				[]string{"error_type", "endpoint"},
			),
		}
	})
	return instance
}

// Get returns the global metrics instance
func Get() *Metrics {
	if instance == nil {
		return Initialize()
	}
	return instance
}
