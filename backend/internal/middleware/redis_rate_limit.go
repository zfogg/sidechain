package middleware

import (
	"context"
	"fmt"
	"net"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/cache"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
)

// RateLimiterConfig holds rate limiting configuration
type RateLimiterConfig struct {
	MaxRequests int
	Window      time.Duration
}

// RedisRateLimitMiddleware creates a distributed rate limiter using Redis
// This works across multiple instances and provides fair access control
func RedisRateLimitMiddleware(maxRequests int, window time.Duration) gin.HandlerFunc {
	return func(c *gin.Context) {
		redisClient := cache.GetRedisClient()
		if redisClient == nil {
			// Fallback: If Redis isn't available, let request through but log warning
			logger.Log.Warn("Redis rate limiter unavailable, allowing request")
			c.Next()
			return
		}

		clientIP := getClientIP(c.Request.RemoteAddr)
		key := fmt.Sprintf("rate_limit:%s", clientIP)
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()

		// Get current count
		val, err := redisClient.GetInt(ctx, key)
		if err != nil && err.Error() != "redis: nil" {
			// On Redis error, reject request to maintain security
			// Allowing requests through when rate limiter is broken opens API to DoS
			logger.Log.Error("Rate limit check failed - rejecting request for security",
				zap.String("client_ip", clientIP),
				zap.Error(err),
			)
			c.JSON(503, gin.H{"error": "Service temporarily unavailable"})
			c.Abort()
			return
		}

		if val >= int64(maxRequests) {
			logger.Log.Warn("Rate limit exceeded",
				logger.WithIP(clientIP),
				zap.Int("max_requests", maxRequests),
				zap.Int64("current_requests", val),
			)
			c.JSON(429, gin.H{
				"error":       "rate limit exceeded",
				"retry_after": window.Seconds(),
			})
			c.Abort()
			return
		}

		// Increment counter
		newVal, err := redisClient.IncrBy(ctx, key, 1)
		if err != nil {
			logger.Log.Error("Rate limit increment failed - rejecting request for security",
				zap.String("client_ip", clientIP),
				zap.Error(err),
			)
			c.JSON(503, gin.H{"error": "Service temporarily unavailable"})
			c.Abort()
			return
		}

		// Set expiration on first request in this window
		if newVal == 1 {
			if err := redisClient.Expire(ctx, key, window); err != nil {
				logger.Log.Warn("Failed to set rate limit expiration",
					zap.String("client_ip", clientIP),
					zap.Error(err),
				)
			}
		}

		c.Next()
	}
}

// getClientIP extracts the client IP from RemoteAddr
func getClientIP(remoteAddr string) string {
	if ip, _, err := net.SplitHostPort(remoteAddr); err == nil {
		return ip
	}
	return remoteAddr
}
