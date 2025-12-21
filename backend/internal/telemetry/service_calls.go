package telemetry

import (
	"context"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"
)

// ============================================================================
// STREAM.IO SERVICE CALLS
// ============================================================================

// TraceStreamIOCall creates a span for Stream.io API calls
// Examples: create_activity, follow, get_feed, get_notifications
func TraceStreamIOCall(ctx context.Context, operation string, attrs map[string]interface{}) (context.Context, trace.Span) {
	ctx, span := otel.Tracer("stream.io").Start(ctx, "stream."+operation,
		trace.WithSpanKind(trace.SpanKindClient),
		trace.WithAttributes(
			attribute.String("stream.operation", operation),
		),
	)

	// Add optional attributes
	if userID, ok := attrs["user_id"].(string); ok && userID != "" {
		span.SetAttributes(attribute.String("stream.user_id", userID))
	}
	if feedID, ok := attrs["feed_id"].(string); ok && feedID != "" {
		span.SetAttributes(attribute.String("stream.feed_id", feedID))
	}
	if activity, ok := attrs["activity"].(string); ok && activity != "" {
		span.SetAttributes(attribute.String("stream.activity", activity))
	}
	if targetID, ok := attrs["target_id"].(string); ok && targetID != "" {
		span.SetAttributes(attribute.String("stream.target_id", targetID))
	}
	if limit, ok := attrs["limit"].(int); ok && limit > 0 {
		span.SetAttributes(attribute.Int("stream.limit", limit))
	}

	return ctx, span
}

// ============================================================================
// GORSE RECOMMENDATION ENGINE CALLS
// ============================================================================

// TraceGorseCall creates a span for Gorse recommendation API calls
// Examples: insert_user_item, get_recommend, insert_feedback
func TraceGorseCall(ctx context.Context, operation string, attrs map[string]interface{}) (context.Context, trace.Span) {
	ctx, span := otel.Tracer("gorse").Start(ctx, "gorse."+operation,
		trace.WithSpanKind(trace.SpanKindClient),
		trace.WithAttributes(
			attribute.String("gorse.operation", operation),
		),
	)

	// Add optional attributes
	if userID, ok := attrs["user_id"].(string); ok && userID != "" {
		span.SetAttributes(attribute.String("gorse.user_id", userID))
	}
	if itemID, ok := attrs["item_id"].(string); ok && itemID != "" {
		span.SetAttributes(attribute.String("gorse.item_id", itemID))
	}
	if feedback, ok := attrs["feedback"].(string); ok && feedback != "" {
		span.SetAttributes(attribute.String("gorse.feedback", feedback))
	}
	if count, ok := attrs["count"].(int); ok && count > 0 {
		span.SetAttributes(attribute.Int("gorse.count", count))
	}
	if batchSize, ok := attrs["batch_size"].(int); ok && batchSize > 0 {
		span.SetAttributes(attribute.Int("gorse.batch_size", batchSize))
	}

	return ctx, span
}

// ============================================================================
// ELASTICSEARCH CALLS
// ============================================================================

// TraceElasticsearchCall creates a span for Elasticsearch operations
// Examples: index, search, delete, bulk
func TraceElasticsearchCall(ctx context.Context, operation string, attrs map[string]interface{}) (context.Context, trace.Span) {
	ctx, span := otel.Tracer("elasticsearch").Start(ctx, "es."+operation,
		trace.WithSpanKind(trace.SpanKindClient),
		trace.WithAttributes(
			attribute.String("es.operation", operation),
		),
	)

	// Add optional attributes
	if index, ok := attrs["index"].(string); ok && index != "" {
		span.SetAttributes(attribute.String("es.index", index))
	}
	if query, ok := attrs["query"].(string); ok && query != "" {
		// Truncate long queries to avoid cardinality explosion
		if len(query) > 200 {
			query = query[:200] + "..."
		}
		span.SetAttributes(attribute.String("es.query", query))
	}
	if docID, ok := attrs["doc_id"].(string); ok && docID != "" {
		span.SetAttributes(attribute.String("es.doc_id", docID))
	}
	if hitCount, ok := attrs["hit_count"].(int); ok {
		span.SetAttributes(attribute.Int("es.hit_count", hitCount))
	}
	if shards, ok := attrs["shards"].(int); ok && shards > 0 {
		span.SetAttributes(attribute.Int("es.shards_queried", shards))
	}
	if bulkSize, ok := attrs["bulk_size"].(int); ok && bulkSize > 0 {
		span.SetAttributes(attribute.Int("es.bulk_size", bulkSize))
	}

	return ctx, span
}

// ============================================================================
// AWS S3 / STORAGE CALLS
// ============================================================================

// TraceS3Call creates a span for AWS S3 operations
// Examples: put_object, get_object, delete_object, list_objects
func TraceS3Call(ctx context.Context, operation string, attrs map[string]interface{}) (context.Context, trace.Span) {
	ctx, span := otel.Tracer("s3").Start(ctx, "s3."+operation,
		trace.WithSpanKind(trace.SpanKindClient),
		trace.WithAttributes(
			attribute.String("s3.operation", operation),
		),
	)

	// Add optional attributes
	if bucket, ok := attrs["bucket"].(string); ok && bucket != "" {
		span.SetAttributes(attribute.String("s3.bucket", bucket))
	}
	if key, ok := attrs["key"].(string); ok && key != "" {
		span.SetAttributes(attribute.String("s3.key", key))
	}
	if contentType, ok := attrs["content_type"].(string); ok && contentType != "" {
		span.SetAttributes(attribute.String("s3.content_type", contentType))
	}
	if sizeBytes, ok := attrs["size_bytes"].(int64); ok && sizeBytes > 0 {
		span.SetAttributes(attribute.Int64("s3.size_bytes", sizeBytes))
	}
	if duration, ok := attrs["duration_ms"].(int64); ok && duration > 0 {
		span.SetAttributes(attribute.Int64("s3.duration_ms", duration))
	}

	return ctx, span
}

// ============================================================================
// CACHE OPERATIONS
// ============================================================================

// TraceCacheCall creates a span for cache (Redis) operations
// Examples: get, set, delete, ttl, incr
func TraceCacheCall(ctx context.Context, operation string, attrs map[string]interface{}) (context.Context, trace.Span) {
	ctx, span := otel.Tracer("cache").Start(ctx, "cache."+operation,
		trace.WithSpanKind(trace.SpanKindClient),
		trace.WithAttributes(
			attribute.String("cache.operation", operation),
		),
	)

	// Add optional attributes
	if key, ok := attrs["key"].(string); ok && key != "" {
		span.SetAttributes(attribute.String("cache.key", key))
	}
	if hit, ok := attrs["hit"].(bool); ok {
		span.SetAttributes(attribute.Bool("cache.hit", hit))
	}
	if sizeBytes, ok := attrs["size_bytes"].(int64); ok && sizeBytes > 0 {
		span.SetAttributes(attribute.Int64("cache.size_bytes", sizeBytes))
	}
	if ttl, ok := attrs["ttl_seconds"].(int); ok && ttl > 0 {
		span.SetAttributes(attribute.Int("cache.ttl_seconds", ttl))
	}
	if keyCount, ok := attrs["key_count"].(int); ok && keyCount > 0 {
		span.SetAttributes(attribute.Int("cache.key_count", keyCount))
	}

	return ctx, span
}

// ============================================================================
// ERROR AND SUCCESS RECORDING
// ============================================================================

// RecordServiceError records a service error in the current span
func RecordServiceError(span trace.Span, service string, err error) {
	if err != nil {
		span.SetStatus(codes.Error, err.Error())
		span.RecordError(err, trace.WithStackTrace(true))
		span.SetAttributes(attribute.String("error.type", "service_error"))
	}
}

// RecordServiceSuccess records success metrics for a service call
func RecordServiceSuccess(span trace.Span, attrs map[string]interface{}) {
	if itemCount, ok := attrs["item_count"].(int); ok {
		span.SetAttributes(attribute.Int("result.item_count", itemCount))
	}
	if durationMs, ok := attrs["duration_ms"].(int64); ok {
		span.SetAttributes(attribute.Int64("result.duration_ms", durationMs))
	}
	if cached, ok := attrs["cached"].(bool); ok && cached {
		span.SetAttributes(attribute.Bool("result.from_cache", true))
	}

	span.SetStatus(codes.Ok, "")
}

// ============================================================================
// CORRELATION ID AND BAGGAGE HELPERS
// ============================================================================

// SetCorrelationID sets a correlation ID in span attributes for tracking across services
func SetCorrelationID(span trace.Span, correlationID string) {
	if correlationID != "" {
		span.SetAttributes(attribute.String("trace.correlation_id", correlationID))
	}
}

// SetUserContext sets user-related attributes for better tracing
func SetUserContext(span trace.Span, userID string, organizationID string) {
	if userID != "" {
		span.SetAttributes(attribute.String("user.id", userID))
	}
	if organizationID != "" {
		span.SetAttributes(attribute.String("organization.id", organizationID))
	}
}

// SetRequestContext sets request-specific attributes
func SetRequestContext(span trace.Span, requestID string, userAgent string) {
	if requestID != "" {
		span.SetAttributes(attribute.String("request.id", requestID))
	}
	if userAgent != "" {
		// Truncate to avoid cardinality explosion
		if len(userAgent) > 200 {
			userAgent = userAgent[:200] + "..."
		}
		span.SetAttributes(attribute.String("http.user_agent", userAgent))
	}
}
