package middleware

import (
	"net"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
)

// GinLoggerMiddleware is a Gin middleware that logs HTTP requests with structured fields
// This replaces gin.Logger with structured logging
func GinLoggerMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		// Start time
		startTime := time.Now()

		// Get request ID from context (set by RequestIDMiddleware)
		requestID, _ := c.Get("request_id")
		requestIDStr := ""
		if rID, ok := requestID.(string); ok {
			requestIDStr = rID
		}

		// Request info
		method := c.Request.Method
		path := c.Request.URL.Path
		query := c.Request.URL.RawQuery
		clientIP := c.ClientIP()
		userAgent := c.Request.UserAgent()

		// Call next handler
		c.Next()

		// Response info
		statusCode := c.Writer.Status()
		responseSize := c.Writer.Size()
		latency := time.Since(startTime)

		// Build log fields
		fields := []zap.Field{
			zap.String("method", method),
			zap.String("path", path),
			zap.String("query", query),
			zap.String("client_ip", clientIP),
			zap.Int("status", statusCode),
			zap.Int("response_size", responseSize),
			zap.Duration("latency", latency),
			zap.String("user_agent", userAgent),
		}

		if requestIDStr != "" {
			fields = append(fields, zap.String("request_id", requestIDStr))
		}

		// Determine log level based on status code
		switch {
		case statusCode >= 500:
			logger.Log.Error("HTTP request", fields...)
		case statusCode >= 400:
			logger.Log.Warn("HTTP request", fields...)
		default:
			logger.Log.Info("HTTP request", fields...)
		}
	}
}

// Helper to extract client IP from request
func clientIP(r *http.Request) string {
	// Try X-Forwarded-For header (for reverse proxy)
	if xff := r.Header.Get("X-Forwarded-For"); xff != "" {
		// X-Forwarded-For can be comma-separated, take first one
		if ip, _, err := net.SplitHostPort(xff); err == nil {
			return ip
		}
		return xff
	}

	// Try X-Real-IP header
	if xri := r.Header.Get("X-Real-IP"); xri != "" {
		return xri
	}

	// Fall back to remote address
	if ip, _, err := net.SplitHostPort(r.RemoteAddr); err == nil {
		return ip
	}

	return r.RemoteAddr
}
