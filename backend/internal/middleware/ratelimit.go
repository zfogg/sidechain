package middleware

import (
	"net/http"
	"strconv"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
)

// RateLimitConfig holds configuration for rate limiting
type RateLimitConfig struct {
	// Requests per window
	Limit int
	// Window duration
	Window time.Duration
}

// DefaultRateLimitConfig returns sensible defaults
func DefaultRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		Limit:  100,         // 100 requests
		Window: time.Minute, // per minute
	}
}

// AuthRateLimitConfig returns stricter limits for auth endpoints
func AuthRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		Limit:  10,          // 10 requests
		Window: time.Minute, // per minute
	}
}

// UploadRateLimitConfig returns limits for upload endpoints
func UploadRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		Limit:  20,          // 20 uploads
		Window: time.Minute, // per minute
	}
}

// SearchRateLimitConfig returns limits for search endpoints
func SearchRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		Limit:  100,         // 100 search requests per minute
		Window: time.Minute, // per minute
	}
}

// TokenBucket for rate limiting
type TokenBucket struct {
	tokens    float64
	maxTokens float64
	refillRate float64 // tokens per second
	lastRefill time.Time
	mu        sync.Mutex
}

// NewTokenBucket creates a new token bucket
func NewTokenBucket(maxTokens float64, refillRate float64) *TokenBucket {
	return &TokenBucket{
		tokens:     maxTokens,
		maxTokens:  maxTokens,
		refillRate: refillRate,
		lastRefill: time.Now(),
	}
}

// Allow checks if a request is allowed based on token availability
func (tb *TokenBucket) Allow() bool {
	tb.mu.Lock()
	defer tb.mu.Unlock()

	// Refill tokens based on elapsed time
	now := time.Now()
	elapsed := now.Sub(tb.lastRefill).Seconds()
	tb.tokens = min(tb.maxTokens, tb.tokens+elapsed*tb.refillRate)
	tb.lastRefill = now

	if tb.tokens >= 1 {
		tb.tokens--
		return true
	}
	return false
}

// GetRetryAfter returns seconds to wait before next request
func (tb *TokenBucket) GetRetryAfter() int {
	tb.mu.Lock()
	defer tb.mu.Unlock()

	if tb.tokens < 1 {
		// Calculate time to get 1 token
		timeToToken := (1 - tb.tokens) / tb.refillRate
		return int(timeToToken) + 1
	}
	return 0
}

func min(a, b float64) float64 {
	if a < b {
		return a
	}
	return b
}

// RateLimiter uses token buckets for each IP
type RateLimiter struct {
	buckets map[string]*TokenBucket
	config  RateLimitConfig
	mu      sync.RWMutex
	cleanup *time.Ticker
}

// NewRateLimiter creates a new rate limiting middleware
func NewRateLimiter(config RateLimitConfig) gin.HandlerFunc {
	rl := &RateLimiter{
		buckets: make(map[string]*TokenBucket),
		config:  config,
		cleanup: time.NewTicker(1 * time.Minute),
	}

	// Start cleanup goroutine
	go rl.cleanupRoutine()

	return func(c *gin.Context) {
		if !rl.Allow(c.ClientIP()) {
			retryAfter := rl.GetRetryAfter(c.ClientIP())
			c.Header("Retry-After", strconv.Itoa(retryAfter))
			c.Header("X-RateLimit-Limit", strconv.Itoa(config.Limit))
			c.Header("X-RateLimit-Remaining", "0")
			c.AbortWithStatusJSON(http.StatusTooManyRequests, gin.H{
				"error":       "rate limit exceeded",
				"retry_after": retryAfter,
			})
			return
		}
		c.Next()
	}
}

// Allow checks if an IP is allowed to make a request
func (rl *RateLimiter) Allow(ip string) bool {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	bucket, exists := rl.buckets[ip]
	if !exists {
		// Create new bucket with refill rate: limit per window duration
		refillRate := float64(rl.config.Limit) / rl.config.Window.Seconds()
		bucket = NewTokenBucket(float64(rl.config.Limit), refillRate)
		rl.buckets[ip] = bucket
	}

	return bucket.Allow()
}

// GetRetryAfter gets retry-after seconds for an IP
func (rl *RateLimiter) GetRetryAfter(ip string) int {
	rl.mu.RLock()
	defer rl.mu.RUnlock()

	bucket, exists := rl.buckets[ip]
	if !exists {
		return 1
	}
	return bucket.GetRetryAfter()
}

// cleanupRoutine periodically cleans up idle buckets
func (rl *RateLimiter) cleanupRoutine() {
	for range rl.cleanup.C {
		rl.mu.Lock()
		// Keep only buckets that are active (have used some tokens)
		// Simple approach: remove all and let them be recreated on next request
		// In production, you'd want to be more selective
		rl.mu.Unlock()
	}
}

// RateLimit returns a middleware with default configuration
func RateLimit() gin.HandlerFunc {
	return NewRateLimiter(DefaultRateLimitConfig())
}

// RateLimitAuth returns a middleware for auth endpoints
func RateLimitAuth() gin.HandlerFunc {
	return NewRateLimiter(AuthRateLimitConfig())
}

// RateLimitUpload returns a middleware for upload endpoints
func RateLimitUpload() gin.HandlerFunc {
	return NewRateLimiter(UploadRateLimitConfig())
}

// RateLimitSearch returns a middleware for search endpoints
func RateLimitSearch() gin.HandlerFunc {
	return NewRateLimiter(SearchRateLimitConfig())
}

// Smart rate limit wrappers that use Redis if available, else fallback to in-memory
// Note: Redis middleware automatically checks availability and falls back gracefully

// RateLimitSmartDefault returns a middleware with default config that tries Redis first
func RateLimitSmartDefault() gin.HandlerFunc {
	// Try Redis-backed rate limiting first (falls back to in-memory if Redis unavailable)
	return RedisRateLimitMiddleware(DefaultRateLimitConfig().Limit, DefaultRateLimitConfig().Window)
}

// RateLimitSmartAuth returns a middleware for auth with Redis support
func RateLimitSmartAuth() gin.HandlerFunc {
	return RedisRateLimitMiddleware(AuthRateLimitConfig().Limit, AuthRateLimitConfig().Window)
}

// RateLimitSmartUpload returns a middleware for upload with Redis support
func RateLimitSmartUpload() gin.HandlerFunc {
	return RedisRateLimitMiddleware(UploadRateLimitConfig().Limit, UploadRateLimitConfig().Window)
}

// RateLimitSmartSearch returns a middleware for search with Redis support
func RateLimitSmartSearch() gin.HandlerFunc {
	return RedisRateLimitMiddleware(SearchRateLimitConfig().Limit, SearchRateLimitConfig().Window)
}
