package middleware

import (
	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
)

// RequestIDMiddleware adds a unique request ID to each request
// If X-Request-ID header is present, it will be used; otherwise a new UUID is generated
func RequestIDMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		// Get or generate request ID
		requestID := c.GetHeader("X-Request-ID")
		if requestID == "" {
			requestID = uuid.New().String()
		}

		// Store in context for later use
		c.Set("request_id", requestID)

		// Add to response header for tracing
		c.Header("X-Request-ID", requestID)

		// Add request ID to logger context
		withRequestID := logger.WithRequestID(requestID)
		clientIP := c.ClientIP()
		method := c.Request.Method
		path := c.Request.URL.Path

		// Log request start
		logger.Log.Debug("request started",
			withRequestID,
			logger.WithIP(clientIP),
			zap.String("method", method),
			zap.String("path", path),
		)

		// Call next handler
		c.Next()

		// Log response
		statusCode := c.Writer.Status()
		logger.Log.Debug("request completed",
			withRequestID,
			logger.WithStatus(statusCode),
			zap.String("method", method),
			zap.String("path", path),
		)
	}
}
