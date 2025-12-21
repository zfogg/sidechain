package telemetry

import (
	"context"
	"fmt"
	"net/http"
	"time"

	"go.opentelemetry.io/contrib/instrumentation/net/http/otelhttp"
	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"
)

// HTTPClientConfig holds configuration for an instrumented HTTP client
type HTTPClientConfig struct {
	ServiceName  string        // Name of the external service (e.g., "stream.io", "gorse")
	Timeout      time.Duration // Request timeout
	TraceHeaders bool          // Whether to trace request/response headers
}

// NewInstrumentedHTTPClient creates an HTTP client with automatic tracing
// All requests made with this client will be traced to OpenTelemetry
func NewInstrumentedHTTPClient(cfg HTTPClientConfig) *http.Client {
	if cfg.Timeout == 0 {
		cfg.Timeout = 30 * time.Second
	}

	client := &http.Client{
		Timeout: cfg.Timeout,
		Transport: otelhttp.NewTransport(
			&http.DefaultTransport,
			otelhttp.WithClientTrace(),
			otelhttp.WithSpanOptions(
				trace.WithSpanKind(trace.SpanKindClient),
			),
		),
	}

	return client
}

// ExternalServiceCallAttrs holds attributes for external service calls
type ExternalServiceCallAttrs struct {
	Service         string // Service name (stream.io, gorse, elasticsearch, s3)
	Operation       string // Operation being performed
	ResourceID      string // ID of resource being operated on (optional)
	RetryCount      int
	RetryableError  bool
	CacheHit        bool
	CircuitBreakerTriggered bool
}

// TraceExternalCall creates a span for an external service call with standard attributes
func TraceExternalCall(ctx context.Context, attrs ExternalServiceCallAttrs) (context.Context, trace.Span) {
	tracer := otel.Tracer("external-api")

	spanName := fmt.Sprintf("%s.%s", attrs.Service, attrs.Operation)
	ctx, span := tracer.Start(ctx, spanName,
		trace.WithSpanKind(trace.SpanKindClient),
		trace.WithAttributes(
			attribute.String("external.service", attrs.Service),
			attribute.String("external.operation", attrs.Operation),
		),
	)

	if attrs.ResourceID != "" {
		span.SetAttributes(attribute.String("external.resource_id", attrs.ResourceID))
	}
	if attrs.RetryCount > 0 {
		span.SetAttributes(attribute.Int("external.retry_count", attrs.RetryCount))
	}
	if attrs.RetryableError {
		span.SetAttributes(attribute.Bool("external.error.retryable", true))
	}
	if attrs.CacheHit {
		span.SetAttributes(attribute.Bool("external.cache_hit", true))
	}
	if attrs.CircuitBreakerTriggered {
		span.SetAttributes(attribute.Bool("external.circuit_breaker_triggered", true))
	}

	return ctx, span
}

// RecordExternalCallError records error details in a span
func RecordExternalCallError(span trace.Span, err error, statusCode int, retryable bool) {
	if err != nil {
		span.SetStatus(codes.Error, err.Error())
		span.RecordError(err)
	}

	if statusCode > 0 {
		span.SetAttributes(attribute.Int("http.status_code", statusCode))

		// Classify error as retryable based on status code
		if retryable || (statusCode >= 500) || statusCode == 408 || statusCode == 429 {
			span.SetAttributes(attribute.Bool("external.error.retryable", true))
		}
	}
}

// RecordExternalCallSuccess records success metrics in a span
func RecordExternalCallSuccess(span trace.Span, statusCode int, responseSizeBytes int64) {
	span.SetAttributes(
		attribute.Int("http.status_code", statusCode),
	)

	if responseSizeBytes > 0 {
		span.SetAttributes(attribute.Int64("http.response.size_bytes", responseSizeBytes))
	}

	span.SetStatus(codes.Ok, "")
}

// ContextPropagator helps propagate trace context across service boundaries
type ContextPropagator struct {
	tracer trace.Tracer
}

// NewContextPropagator creates a new context propagator
func NewContextPropagator() *ContextPropagator {
	return &ContextPropagator{
		tracer: otel.Tracer("context-propagation"),
	}
}

// InjectTraceContext injects the current trace context into HTTP headers
// This is useful for outgoing requests to other services
func (cp *ContextPropagator) InjectTraceContext(ctx context.Context, headers http.Header) {
	carrier := NewHTTPHeaderCarrier(headers)
	otel.GetTextMapPropagator().Inject(ctx, carrier)
}

// HTTPHeaderCarrier implements the TextMapCarrier interface for HTTP headers
type HTTPHeaderCarrier struct {
	headers http.Header
}

// NewHTTPHeaderCarrier creates a carrier from HTTP headers
func NewHTTPHeaderCarrier(headers http.Header) *HTTPHeaderCarrier {
	return &HTTPHeaderCarrier{headers: headers}
}

// Get retrieves a single value from the carrier
func (c *HTTPHeaderCarrier) Get(key string) string {
	return c.headers.Get(key)
}

// Set sets a single value in the carrier
func (c *HTTPHeaderCarrier) Set(key string, value string) {
	c.headers.Set(key, value)
}

// Keys returns all keys in the carrier
func (c *HTTPHeaderCarrier) Keys() []string {
	keys := make([]string, 0)
	for k := range c.headers {
		keys = append(keys, k)
	}
	return keys
}
