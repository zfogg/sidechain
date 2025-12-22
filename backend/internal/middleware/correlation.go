package middleware

import (
	"context"

	"github.com/gin-gonic/gin"
	"go.opentelemetry.io/otel/attribute"
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
				if s, ok := reqID.(string); ok {
					correlationID = s
				}
			}
		}

		c.Set("correlation_id", correlationID)
		c.Header("X-Correlation-ID", correlationID)

		// Get current span and add correlation metadata
		span := trace.SpanFromContext(c.Request.Context())
		if span.IsRecording() && correlationID != "" {
			span.SetAttributes(
				attribute.String("trace.correlation_id", correlationID),
			)
		}

		// Create new context with correlation ID in baggage
		// This allows it to propagate to background operations
		ctx := c.Request.Context()
		if correlationID != "" {
			member, err := baggage.NewMember("correlation_id", correlationID)
			if err == nil {
				newBaggage, err := baggage.New(member)
				if err == nil {
					ctx = baggage.ContextWithBaggage(ctx, newBaggage)
				}
			}
		}

		// Add sampling hint if provided (for adaptive sampling)
		if samplingHint := c.GetHeader("X-Sampling-Hint"); samplingHint != "" {
			c.Set("sampling_hint", samplingHint)
			member, err := baggage.NewMember("sampling_hint", samplingHint)
			if err == nil {
				newBaggage, err := baggage.New(member)
				if err == nil {
					ctx = baggage.ContextWithBaggage(ctx, newBaggage)
				}
			}
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
				span.SetAttributes(
					attribute.Int64("http.response.size_bytes", int64(responseSize)),
				)
			}

			// Record cache-related headers if present
			if cacheControl := c.GetHeader("Cache-Control"); cacheControl != "" {
				span.SetAttributes(
					attribute.String("http.cache_control", cacheControl),
				)
			}
		}
	}
}

// GetCorrelationIDFromContext extracts correlation ID from context
// Useful in background tasks that need to maintain correlation
func GetCorrelationIDFromContext(ctx context.Context) string {
	b := baggage.FromContext(ctx)

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

	for _, member := range b.Members() {
		if member.Key() == "sampling_hint" {
			return member.Value()
		}
	}

	return ""
}
