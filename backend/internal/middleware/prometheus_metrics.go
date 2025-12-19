package middleware

import (
	"bytes"
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"go.uber.org/zap"
)

// MetricsMiddleware collects HTTP metrics for Prometheus
func MetricsMiddleware() gin.HandlerFunc {
	m := metrics.Get()

	return func(c *gin.Context) {
		// Track active connections
		method := c.Request.Method
		path := c.Request.URL.Path
		m.HTTPActiveConnections.WithLabelValues(method, path).Inc()
		defer m.HTTPActiveConnections.WithLabelValues(method, path).Dec()

		// Capture request size
		contentLength := c.Request.ContentLength
		if contentLength > 0 {
			m.HTTPRequestSize.WithLabelValues(method, path).Observe(float64(contentLength))
		}

		// Wrap response writer to capture response size and status
		writer := &metricsResponseWriter{
			ResponseWriter: c.Writer,
			statusCode:     http.StatusOK,
			body:           &bytes.Buffer{},
		}
		c.Writer = writer

		// Record start time
		startTime := time.Now()

		// Process request
		c.Next()

		// Record metrics
		duration := time.Since(startTime).Seconds()
		status := c.Writer.Status()
		// Use numeric status code as string (e.g., "200", "500") for Prometheus label
		// This allows Grafana queries like status=~"5.." to match 5xx errors
		statusStr := strconv.Itoa(status)

		// Record request count and latency
		m.HTTPRequestsTotal.WithLabelValues(method, path, statusStr).Inc()
		m.HTTPRequestDuration.WithLabelValues(method, path, statusStr).Observe(duration)

		// Record response size
		responseSize := writer.body.Len()
		if responseSize > 0 {
			m.HTTPResponseSize.WithLabelValues(method, path, statusStr).Observe(float64(responseSize))
		}

		logger.Log.Debug("HTTP request recorded",
			zap.String("method", method),
			zap.String("path", path),
			zap.Int("status", status),
			zap.Float64("duration_sec", duration),
			zap.Int("response_size", responseSize),
		)
	}
}

// CacheMetricsMiddleware records cache operations
func RecordCacheHit(cacheName string) {
	m := metrics.Get()
	m.CacheHitsTotal.WithLabelValues(cacheName).Inc()
}

func RecordCacheMiss(cacheName string) {
	m := metrics.Get()
	m.CacheMissesTotal.WithLabelValues(cacheName).Inc()
}

func RecordCacheOperation(operation, cacheName string, duration time.Duration) {
	m := metrics.Get()
	m.CacheOperationsTotal.WithLabelValues(operation, cacheName).Inc()
	m.CacheOperationDuration.WithLabelValues(operation, cacheName).Observe(duration.Seconds())
}

func RecordCacheEviction(cacheName string, count int64) {
	m := metrics.Get()
	m.CacheEvictionsTotal.WithLabelValues(cacheName).Add(float64(count))
}

// RateLimitMetrics records rate limiting events
func RecordRateLimitExceeded(endpoint, method string) {
	m := metrics.Get()
	m.RateLimitExceededTotal.WithLabelValues(endpoint, method).Inc()
}

func RecordRateLimitBucketUsage(endpoint, clientIP string, tokensUsed float64) {
	m := metrics.Get()
	m.RateLimitBucketUsage.WithLabelValues(endpoint, clientIP).Set(tokensUsed)
}

// DatabaseMetrics records database operations
func RecordDatabaseQuery(queryType, table string, duration time.Duration, err error) {
	m := metrics.Get()
	status := "success"
	if err != nil {
		status = "error"
	}
	m.DatabaseQueryDuration.WithLabelValues(queryType, table).Observe(duration.Seconds())
	m.DatabaseQueriesTotal.WithLabelValues(queryType, table, status).Inc()
}

func SetDatabaseConnections(database string, count int) {
	m := metrics.Get()
	m.DatabaseConnectionsOpen.WithLabelValues(database).Set(float64(count))
}

// RedisMetrics records Redis operations
func RecordRedisOperation(operation, keyPattern string, duration time.Duration, err error) {
	m := metrics.Get()
	status := "success"
	if err != nil {
		status = "error"
	}
	m.RedisOperationDuration.WithLabelValues(operation, keyPattern).Observe(duration.Seconds())
	m.RedisOperationsTotal.WithLabelValues(operation, status).Inc()
}

func SetRedisConnections(instance string, count int) {
	m := metrics.Get()
	m.RedisConnectionsOpen.WithLabelValues(instance).Set(float64(count))
}

// FeedMetrics records feed generation and recommendations
func RecordFeedGeneration(feedType string, duration time.Duration) {
	m := metrics.Get()
	m.FeedGenerationTime.WithLabelValues(feedType).Observe(duration.Seconds())
}

func RecordGorseRecommendation(recommendationType string) {
	m := metrics.Get()
	m.GorseRecommendations.WithLabelValues(recommendationType).Inc()
}

func RecordGorseError(errorType string) {
	m := metrics.Get()
	m.GorseErrors.WithLabelValues(errorType).Inc()
}

// ErrorMetrics records errors
func RecordError(errorType, endpoint string) {
	m := metrics.Get()
	m.ErrorsTotal.WithLabelValues(errorType, endpoint).Inc()
}

// metricsResponseWriter intercepts response writes to capture size and status
type metricsResponseWriter struct {
	gin.ResponseWriter
	statusCode int
	body       *bytes.Buffer
}

func (w *metricsResponseWriter) Write(data []byte) (int, error) {
	w.body.Write(data)
	return w.ResponseWriter.Write(data)
}

func (w *metricsResponseWriter) WriteHeader(statusCode int) {
	w.statusCode = statusCode
	w.ResponseWriter.WriteHeader(statusCode)
}
