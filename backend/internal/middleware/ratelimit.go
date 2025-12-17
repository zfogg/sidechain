package middleware

import (
	"time"

	"github.com/didip/tollbooth"
	"github.com/didip/tollbooth_gin"
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

// SearchRateLimitConfig returns limits for search endpoints (Phase 6.3)
func SearchRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		Limit:  100,         // 100 search requests per minute
		Window: time.Minute, // per minute
	}
}

// NewRateLimiter creates a new rate limiting middleware using tollbooth
func NewRateLimiter(config RateLimitConfig) gin.HandlerFunc {
	limiter := tollbooth.NewLimiter(
		float64(config.Limit)/60.0, // Convert to per-second rate (60 seconds in a minute)
		&tollbooth.LimitCounter{
			TokenBucket: tollbooth.NewTokenBucket(config.Window),
		},
	)
	return tollbooth_gin.LimitHandler(limiter)
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

// RateLimitSearch returns a middleware for search endpoints (Phase 6.3)
func RateLimitSearch() gin.HandlerFunc {
	return NewRateLimiter(SearchRateLimitConfig())
}
