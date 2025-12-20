package middleware

import (
	"github.com/gin-gonic/gin"
	"go.opentelemetry.io/contrib/instrumentation/github.com/gin-gonic/gin/otelgin"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"
)

// TracingMiddleware returns a middleware that traces HTTP requests using OpenTelemetry
// It wraps the official otelgin middleware and adds custom span attributes
func TracingMiddleware(serviceName string) gin.HandlerFunc {
	// Use official otelgin middleware as base
	base := otelgin.Middleware(serviceName)

	return func(c *gin.Context) {
		base(c)

		// Add custom span attributes after otelgin processes
		span := trace.SpanFromContext(c.Request.Context())
		if span.IsRecording() {
			// Add user context if authenticated
			if userID, exists := c.Get("user_id"); exists {
				if userIDStr, ok := userID.(string); ok {
					span.SetAttributes(attribute.String("user.id", userIDStr))
				}
			}

			// Add custom business attributes from query parameters
			if feedType := c.Query("feed_type"); feedType != "" {
				span.SetAttributes(attribute.String("feed.type", feedType))
			}

			if limit := c.Query("limit"); limit != "" {
				span.SetAttributes(attribute.String("query.limit", limit))
			}

			if offset := c.Query("offset"); offset != "" {
				span.SetAttributes(attribute.String("query.offset", offset))
			}

			// Record Gin errors as span events
			if len(c.Errors) > 0 {
				for _, ginErr := range c.Errors {
					if ginErr.Err != nil {
						span.RecordError(ginErr.Err, trace.WithStackTrace(true))
						span.SetStatus(codes.Error, ginErr.Error())
					}
				}
			}
		}
	}
}
