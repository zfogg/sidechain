package middleware

import (
	"context"

	"github.com/gin-gonic/gin"
	"go.opentelemetry.io/otel/baggage"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"
)

// CorrelationMiddleware enriches the request context with correlation metadata
// This middleware should run early in the chain, after RequestIDMiddleware
//
// It handles:
// - Correlation ID propagation (X-Correlation-ID header)
// - Trace baggage for non-sensitive metadata
// - User context enrichment
// - Request sampling hints
func CorrelationMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		// Get or create correlation ID (different from request ID)
		// Correlation ID is used to trace business transactions across multiple requests
		correlationID := c.GetHeader("X-Correlation-ID")
		if correlationID == "" {
			// Fall back to request ID if correlation ID not provided
			if reqID, exists := c.Get("request_id"); exists {
				correlationID = reqID.(string)
			}
		}

		c.Set("correlation_id", correlationID)
		c.Header("X-Correlation-ID", correlationID)

		// Extract baggage from incoming request headers
		// Baggage is lighter than trace context and good for non-sensitive metadata
		baggage := baggage.FromContext(c.Request.Context())

		// Get current span and add correlation metadata
		span := trace.SpanFromContext(c.Request.Context())
		if span.IsRecording() && correlationID != "" {
			span.SetAttributes(map[string]interface{}{
				"trace.correlation_id": correlationID,
			})
		}

		// Create new context with correlation ID in baggage
		// This allows it to propagate to background operations
		b, _ := baggage.NewMember("correlation_id", correlationID)
		newBaggage, _ := baggage.New(b)
		ctx := baggage.ContextWithBaggage(c.Request.Context(), newBaggage)

		// Add sampling hint if provided (for adaptive sampling)
		if samplingHint := c.GetHeader("X-Sampling-Hint"); samplingHint != "" {
			c.Set("sampling_hint", samplingHint)
			b, _ := baggage.NewMember("sampling_hint", samplingHint)
			newBaggage, _ := baggage.New(b)
			ctx = baggage.ContextWithBaggage(ctx, newBaggage)
		}

		// Update context
		c.Request = c.Request.WithContext(ctx)

		c.Next()
	}
}

// SpanEnrichmentMiddleware enriches spans with additional context after request processing
// Should be added after all other handlers to capture final state
func SpanEnrichmentMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Next()

		// Enrich span with response metadata
		span := trace.SpanFromContext(c.Request.Context())
		if span.IsRecording() {
			statusCode := c.Writer.Status()

			// Set span status based on HTTP status code
			if statusCode >= 400 {
				if statusCode >= 500 {
					span.SetStatus(codes.Error, "Server error")
				} else if statusCode == 404 {
					span.SetStatus(codes.Unset, "Not found")
				} else {
					span.SetStatus(codes.Error, "Client error")
				}
			} else {
				span.SetStatus(codes.Ok, "")
			}

			// Record response size if available
			if responseSize := c.Writer.Size(); responseSize > 0 {
				span.SetAttributes(map[string]interface{}{
					"http.response.size_bytes": int64(responseSize),
				})
			}

			// Record cache-related headers if present
			if cacheControl := c.GetHeader("Cache-Control"); cacheControl != "" {
				span.SetAttributes(map[string]interface{}{
					"http.cache_control": cacheControl,
				})
			}
		}
	}
}

// GetCorrelationIDFromContext extracts correlation ID from context
// Useful in background tasks that need to maintain correlation
func GetCorrelationIDFromContext(ctx context.Context) string {
	b := baggage.FromContext(ctx)
	if b == nil {
		return ""
	}

	// Look for correlation_id in baggage
	for _, member := range b.Members() {
		if member.Key() == "correlation_id" {
			return member.Value()
		}
	}

	return ""
}

// GetSamplingHintFromContext extracts sampling hint from context
// Useful for adaptive sampling decisions
func GetSamplingHintFromContext(ctx context.Context) string {
	b := baggage.FromContext(ctx)
	if b == nil {
		return ""
	}

	for _, member := range b.Members() {
		if member.Key() == "sampling_hint" {
			return member.Value()
		}
	}

	return ""
}
