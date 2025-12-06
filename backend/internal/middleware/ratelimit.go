package middleware

import (
	"net/http"
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
	// Key function to identify clients (default: IP address)
	KeyFunc func(*gin.Context) string
}

// DefaultRateLimitConfig returns sensible defaults
func DefaultRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		Limit:  100,            // 100 requests
		Window: time.Minute,    // per minute
		KeyFunc: func(c *gin.Context) string {
			return c.ClientIP()
		},
	}
}

// AuthRateLimitConfig returns stricter limits for auth endpoints
func AuthRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		Limit:  10,             // 10 requests
		Window: time.Minute,    // per minute
		KeyFunc: func(c *gin.Context) string {
			return c.ClientIP()
		},
	}
}

// UploadRateLimitConfig returns limits for upload endpoints
func UploadRateLimitConfig() RateLimitConfig {
	return RateLimitConfig{
		Limit:  20,             // 20 uploads
		Window: time.Minute,    // per minute
		KeyFunc: func(c *gin.Context) string {
			// Use user ID if available, otherwise IP
			if userID := c.GetString("user_id"); userID != "" {
				return "user:" + userID
			}
			return c.ClientIP()
		},
	}
}

type rateLimiter struct {
	config   RateLimitConfig
	clients  map[string]*clientState
	mu       sync.RWMutex
	stopChan chan struct{}
}

type clientState struct {
	requests  int
	windowEnd time.Time
}

// NewRateLimiter creates a new rate limiting middleware
func NewRateLimiter(config RateLimitConfig) gin.HandlerFunc {
	rl := &rateLimiter{
		config:   config,
		clients:  make(map[string]*clientState),
		stopChan: make(chan struct{}),
	}

	// Start cleanup goroutine
	go rl.cleanup()

	return rl.handle
}

func (rl *rateLimiter) handle(c *gin.Context) {
	key := rl.config.KeyFunc(c)
	now := time.Now()

	rl.mu.Lock()
	state, exists := rl.clients[key]
	if !exists || now.After(state.windowEnd) {
		// New window
		rl.clients[key] = &clientState{
			requests:  1,
			windowEnd: now.Add(rl.config.Window),
		}
		rl.mu.Unlock()
		c.Next()
		return
	}

	if state.requests >= rl.config.Limit {
		rl.mu.Unlock()
		retryAfter := int(state.windowEnd.Sub(now).Seconds())
		if retryAfter < 1 {
			retryAfter = 1
		}
		c.Header("Retry-After", string(rune(retryAfter)))
		c.Header("X-RateLimit-Limit", string(rune(rl.config.Limit)))
		c.Header("X-RateLimit-Remaining", "0")
		c.AbortWithStatusJSON(http.StatusTooManyRequests, gin.H{
			"error":       "rate limit exceeded",
			"retry_after": retryAfter,
		})
		return
	}

	state.requests++
	rl.mu.Unlock()
	c.Next()
}

func (rl *rateLimiter) cleanup() {
	ticker := time.NewTicker(time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			now := time.Now()
			rl.mu.Lock()
			for key, state := range rl.clients {
				if now.After(state.windowEnd) {
					delete(rl.clients, key)
				}
			}
			rl.mu.Unlock()
		case <-rl.stopChan:
			return
		}
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
