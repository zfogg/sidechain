package telemetry

import (
	"context"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"
)

// BusinessEvents provides helper methods for tracing domain-specific operations
// These are higher-level events beyond HTTP/DB/Cache tracing (e.g., "user followed another user", "post was liked")
type BusinessEvents struct {
	tracer trace.Tracer
}

// NewBusinessEvents creates a new business events tracer
func NewBusinessEvents() *BusinessEvents {
	return &BusinessEvents{
		tracer: otel.Tracer("business-events"),
	}
}

// ============================================================================
// FEED OPERATIONS
// ============================================================================

// FeedEvent attributes for feed-related operations
type FeedEventAttrs struct {
	FeedType      string // "timeline", "global", "trending", "unified", "enriched"
	Limit         int64
	Offset        int64
	ItemCount     int64
	EnrichmentType string // "users", "waveforms", "reactions"
	FallbackUsed   bool
}

// TraceGetFeed creates a span for feed retrieval operations
func (be *BusinessEvents) TraceGetFeed(ctx context.Context, feedType string, attrs FeedEventAttrs) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "feed.get",
		trace.WithAttributes(
			attribute.String("feed.type", feedType),
			attribute.Int64("feed.limit", attrs.Limit),
			attribute.Int64("feed.offset", attrs.Offset),
		),
	)

	// Record optional attributes only if set
	if attrs.ItemCount > 0 {
		span.SetAttributes(attribute.Int64("feed.item_count", attrs.ItemCount))
	}
	if attrs.EnrichmentType != "" {
		span.SetAttributes(attribute.String("feed.enrichment_type", attrs.EnrichmentType))
	}
	if attrs.FallbackUsed {
		span.SetAttributes(attribute.Bool("feed.fallback_used", true))
	}

	return ctx, span
}

// TraceCreatePost creates a span for post creation with audio metadata
func (be *BusinessEvents) TraceCreatePost(ctx context.Context, postID string, audioFormat string) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "feed.create_post",
		trace.WithAttributes(
			attribute.String("post.id", postID),
			attribute.String("audio.format", audioFormat),
		),
	)
	return ctx, span
}

// TracePostEnrichment creates a span for enriching posts with user/waveform data
func (be *BusinessEvents) TracePostEnrichment(ctx context.Context, enrichmentType string, count int64) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "data.enrich_posts",
		trace.WithAttributes(
			attribute.String("enrichment.type", enrichmentType),
			attribute.Int64("enrichment.count", count),
		),
	)
	return ctx, span
}

// ============================================================================
// SOCIAL INTERACTIONS
// ============================================================================

// SocialInteractionAttrs attributes for social operations
type SocialInteractionAttrs struct {
	ActionType       string // "follow", "like", "comment", "repost", "save", "react"
	TargetType       string // "post", "user", "comment"
	TargetID         string
	NotificationSent bool
	IsFollowing      bool // For follow operations
}

// TraceFollowUser creates a span for follow operations
func (be *BusinessEvents) TraceFollowUser(ctx context.Context, userID string, targetUserID string) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "social.follow_user",
		trace.WithAttributes(
			attribute.String("user.id", userID),
			attribute.String("target_user.id", targetUserID),
		),
	)
	return ctx, span
}

// TraceSocialInteraction creates a span for generic social interactions
func (be *BusinessEvents) TraceSocialInteraction(ctx context.Context, actionType string, attrs SocialInteractionAttrs) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "social."+actionType,
		trace.WithAttributes(
			attribute.String("action.type", actionType),
			attribute.String("target.type", attrs.TargetType),
			attribute.String("target.id", attrs.TargetID),
		),
	)

	if attrs.NotificationSent {
		span.SetAttributes(attribute.Bool("notification.sent", true))
	}
	if attrs.IsFollowing {
		span.SetAttributes(attribute.Bool("follow.is_following", attrs.IsFollowing))
	}

	return ctx, span
}

// TraceCreateComment creates a span for comment creation
func (be *BusinessEvents) TraceCreateComment(ctx context.Context, postID string, commentID string, hasReply bool) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "social.create_comment",
		trace.WithAttributes(
			attribute.String("post.id", postID),
			attribute.String("comment.id", commentID),
			attribute.Bool("comment.has_reply", hasReply),
		),
	)
	return ctx, span
}

// TraceReaction creates a span for reaction/emoji operations
func (be *BusinessEvents) TraceReaction(ctx context.Context, emoji string, targetID string) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "social.add_reaction",
		trace.WithAttributes(
			attribute.String("reaction.emoji", emoji),
			attribute.String("target.id", targetID),
		),
	)
	return ctx, span
}

// ============================================================================
// AUDIO PROCESSING
// ============================================================================

// AudioEventAttrs attributes for audio operations
type AudioEventAttrs struct {
	FileSizeBytes  int64  // File size in bytes
	AudioFormat    string // "mp3", "wav", "aiff"
	ProcessingStatus string // "pending", "complete", "failed"
	DAW            string // "ableton", "logic", "fl_studio"
}

// TraceAudioUpload creates a span for audio upload operations
func (be *BusinessEvents) TraceAudioUpload(ctx context.Context, audioPostID string, attrs AudioEventAttrs) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "audio.upload",
		trace.WithAttributes(
			attribute.String("audio_post.id", audioPostID),
			attribute.Int64("file.size_bytes", attrs.FileSizeBytes),
			attribute.String("audio.format", attrs.AudioFormat),
		),
	)

	if attrs.DAW != "" {
		span.SetAttributes(attribute.String("audio.daw", attrs.DAW))
	}

	return ctx, span
}

// TraceAudioProcessing creates a span for audio processing status checks
func (be *BusinessEvents) TraceAudioProcessing(ctx context.Context, audioPostID string, status string) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "audio.processing",
		trace.WithAttributes(
			attribute.String("audio_post.id", audioPostID),
			attribute.String("processing.status", status),
		),
	)
	return ctx, span
}

// ============================================================================
// SEARCH & DISCOVERY
// ============================================================================

// SearchEventAttrs attributes for search operations
type SearchEventAttrs struct {
	Query        string // Search query
	Index        string // "users", "posts", "stories"
	ResultCount  int64
	FiltersUsed  []string // ["genre:electronic", "bpm:120-140"]
	FallbackUsed bool
}

// TraceSearch creates a span for search operations
func (be *BusinessEvents) TraceSearch(ctx context.Context, attrs SearchEventAttrs) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "search.query",
		trace.WithAttributes(
			attribute.String("search.query", attrs.Query),
			attribute.String("search.index", attrs.Index),
			attribute.Int64("search.result_count", attrs.ResultCount),
		),
	)

	if len(attrs.FiltersUsed) > 0 {
		span.SetAttributes(attribute.StringSlice("search.filters", attrs.FiltersUsed))
	}
	if attrs.FallbackUsed {
		span.SetAttributes(attribute.Bool("search.fallback_used", true))
	}

	return ctx, span
}

// ============================================================================
// ENGAGEMENT TRACKING
// ============================================================================

// EngagementEventAttrs attributes for engagement operations
type EngagementEventAttrs struct {
	EventType  string // "play", "view", "like", "comment", "share"
	PostID     string
	UserID     string
	PlayCount  int64
	ViewCount  int64
}

// TraceEngagement creates a span for engagement events (plays, views, likes, etc.)
func (be *BusinessEvents) TraceEngagement(ctx context.Context, eventType string, attrs EngagementEventAttrs) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "engagement.track_"+eventType,
		trace.WithAttributes(
			attribute.String("engagement.event_type", eventType),
			attribute.String("post.id", attrs.PostID),
			attribute.String("user.id", attrs.UserID),
		),
	)

	if attrs.PlayCount > 0 {
		span.SetAttributes(attribute.Int64("engagement.play_count", attrs.PlayCount))
	}
	if attrs.ViewCount > 0 {
		span.SetAttributes(attribute.Int64("engagement.view_count", attrs.ViewCount))
	}

	return ctx, span
}

// ============================================================================
// EXTERNAL API CALLS
// ============================================================================

// ExternalAPIEventAttrs attributes for external API operations
type ExternalAPIEventAttrs struct {
	Service   string // "stream.io", "gorse", "elasticsearch"
	Operation string // "create_activity", "follow", "search"
	Status    string // "success", "error", "timeout"
}

// TraceExternalAPI creates a span for external API calls
func (be *BusinessEvents) TraceExternalAPI(ctx context.Context, service string, operation string) (context.Context, trace.Span) {
	ctx, span := be.tracer.Start(ctx, "external."+service+"."+operation,
		trace.WithAttributes(
			attribute.String("external.service", service),
			attribute.String("external.operation", operation),
		),
	)
	return ctx, span
}

// RecordExternalAPIError records an error in an external API span
func RecordExternalAPIError(span trace.Span, err error, retryable bool) {
	if err != nil {
		span.SetStatus(codes.Error, err.Error())
		span.RecordError(err)
		span.SetAttributes(attribute.Bool("external.error.retryable", retryable))
	}
}

// ============================================================================
// HELPER: Global instance for convenient access
// ============================================================================

var globalBusinessEvents *BusinessEvents

// GetBusinessEvents returns the global business events tracer
// Initialize with init or early startup if needed
func GetBusinessEvents() *BusinessEvents {
	if globalBusinessEvents == nil {
		globalBusinessEvents = NewBusinessEvents()
	}
	return globalBusinessEvents
}
