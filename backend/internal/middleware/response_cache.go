package middleware

import (
	"bytes"
	"fmt"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/cache"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
)

// ResponseCacheMiddleware caches successful GET responses with configurable TTL
// Only caches 2xx responses
// Adds X-Cache: HIT/MISS header for debugging
// Cache key is: response:{path}:{query_string}:{user_id}
func ResponseCacheMiddleware(ttl time.Duration) gin.HandlerFunc {
	return func(c *gin.Context) {
		// Only cache GET requests
		if c.Request.Method != "GET" {
			c.Next()
			return
		}

		redisClient := cache.GetRedisClient()
		if redisClient == nil {
			// Redis not available, skip caching
			c.Next()
			return
		}

		// Extract user ID for user-specific caching
		userID := ""
		if user, ok := c.Get("user_id"); ok {
			if uid, ok := user.(string); ok {
				userID = uid
			}
		}

		// Generate cache key from route + query params + user
		cacheKey := generateCacheKey(c.Request.URL.Path, c.Request.URL.RawQuery, userID)
		ctx := c.Request.Context()

		// Try to get from cache
		startTime := time.Now()
		cachedData, err := redisClient.Get(ctx, cacheKey)
		getDuration := time.Since(startTime)

		if err == nil {
			logger.Log.Debug("Cache hit",
				zap.String("key", cacheKey),
				zap.Duration("ttl", ttl),
			)
			// Record cache metrics
			RecordCacheHit("response_cache")
			RecordCacheOperation("GET", "response_cache", getDuration)
			c.Data(http.StatusOK, "application/json", []byte(cachedData))
			c.Header("X-Cache", "HIT")
			c.Header("Cache-Control", fmt.Sprintf("public, max-age=%d", int(ttl.Seconds())))
			return
		}

		// Record cache miss
		RecordCacheMiss("response_cache")
		RecordCacheOperation("GET", "response_cache", getDuration)

		// Cache miss - capture response and cache it
		writer := &cachedResponseWriter{
			ResponseWriter: c.Writer,
			statusCode:     http.StatusOK,
			body:           &bytes.Buffer{},
		}
		c.Writer = writer

		c.Next()

		// Only cache successful responses (200-299)
		if writer.statusCode >= 200 && writer.statusCode < 300 {
			bodyStr := writer.body.String()

			// Only cache if response has content
			if bodyStr != "" {
				setStartTime := time.Now()
				if err := redisClient.SetEx(ctx, cacheKey, bodyStr, ttl); err != nil {
					logger.Log.Debug("Failed to write response to cache",
						zap.String("key", cacheKey),
						zap.Error(err),
					)
				} else {
					setDuration := time.Since(setStartTime)
					RecordCacheOperation("SET", "response_cache", setDuration)
					logger.Log.Debug("Response cached",
						zap.String("key", cacheKey),
						zap.Duration("ttl", ttl),
						zap.Int("size_bytes", len(bodyStr)),
					)
				}
			}
		}

		c.Header("X-Cache", "MISS")
		c.Header("Cache-Control", fmt.Sprintf("public, max-age=%d", int(ttl.Seconds())))
	}
}

// generateCacheKey creates a cache key from request path, query params, and user ID
func generateCacheKey(path, query, userID string) string {
	key := fmt.Sprintf("response:%s", path)

	if query != "" {
		key = fmt.Sprintf("%s:%s", key, query)
	}

	if userID != "" {
		key = fmt.Sprintf("%s:%s", key, userID)
	}

	return key
}

// cachedResponseWriter intercepts response writes to capture the response body
type cachedResponseWriter struct {
	gin.ResponseWriter
	statusCode int
	body       *bytes.Buffer
}

// Write writes data to the response while capturing it for caching
func (w *cachedResponseWriter) Write(data []byte) (int, error) {
	w.body.Write(data)
	return w.ResponseWriter.Write(data)
}

// WriteHeader records the HTTP status code
func (w *cachedResponseWriter) WriteHeader(statusCode int) {
	w.statusCode = statusCode
	w.ResponseWriter.WriteHeader(statusCode)
}

// CacheInvalidationMiddleware invalidates cache on POST/PUT/DELETE requests
// Use on mutation endpoints to clear related caches
func CacheInvalidationMiddleware(invalidationPatterns ...string) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Next()

		// Only invalidate cache on successful mutations
		if c.Request.Method != "POST" && c.Request.Method != "PUT" && c.Request.Method != "DELETE" {
			return
		}

		if c.Writer.Status() < 200 || c.Writer.Status() >= 400 {
			return // Only clear cache on successful mutations
		}

		redisClient := cache.GetRedisClient()
		if redisClient == nil {
			return
		}

		ctx := c.Request.Context()

		// Invalidate specified cache patterns
		for _, pattern := range invalidationPatterns {
			keys, err := redisClient.Keys(ctx, pattern)
			if err != nil {
				logger.Log.Debug("Failed to find cache keys for invalidation",
					zap.String("pattern", pattern),
					zap.Error(err),
				)
				continue
			}

			if len(keys) > 0 {
				if err := redisClient.Del(ctx, keys...); err != nil {
					logger.Log.Warn("Failed to invalidate cache",
						zap.Strings("keys", keys),
						zap.Error(err),
					)
				} else {
					logger.Log.Debug("Cache invalidated",
						zap.Strings("keys", keys),
					)
				}
			}
		}
	}
}
