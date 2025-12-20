package metrics

import (
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
)

// ApplicationMetrics tracks domain-specific metrics for features (audio processing, social engagement, etc)
type ApplicationMetrics struct {
	// Audio processing
	AudioUploadsTotal            prometheus.CounterVec
	AudioProcessingDuration      prometheus.HistogramVec
	AudioProcessingFailures      prometheus.CounterVec
	AudioQueuePending            prometheus.GaugeVec
	AudioQueueDLQSize            prometheus.GaugeVec

	// Social engagement
	FollowsTotal                 prometheus.CounterVec
	UnfollowsTotal               prometheus.CounterVec
	LikesTotal                   prometheus.CounterVec
	CommentsTotal                prometheus.CounterVec
	PostsCreated                 prometheus.CounterVec

	// Validation metrics
	ValidationFailures           prometheus.CounterVec

	// Search & recommendations
	SearchRequests               prometheus.CounterVec
	GorseSync                    prometheus.CounterVec
}

// InitializeApplicationMetrics creates and registers all application metrics
func InitializeApplicationMetrics() *ApplicationMetrics {
	return &ApplicationMetrics{
		// Audio upload metrics
		AudioUploadsTotal: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "audio_uploads_total",
				Help: "Total number of audio uploads",
			},
			[]string{"status", "daw_type"},
		),
		AudioProcessingDuration: *promauto.NewHistogramVec(
			prometheus.HistogramOpts{
				Name:    "audio_processing_duration_seconds",
				Help:    "Audio processing duration in seconds",
				Buckets: []float64{0.5, 1, 5, 10, 30, 60, 120, 300},
			},
			[]string{"stage"},
		),
		AudioProcessingFailures: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "audio_processing_failures_total",
				Help: "Total audio processing failures",
			},
			[]string{"reason"},
		),
		AudioQueuePending: *promauto.NewGaugeVec(
			prometheus.GaugeOpts{
				Name: "audio_queue_pending_jobs",
				Help: "Number of pending audio processing jobs",
			},
			[]string{},
		),
		AudioQueueDLQSize: *promauto.NewGaugeVec(
			prometheus.GaugeOpts{
				Name: "audio_queue_dlq_size",
				Help: "Number of jobs in dead letter queue",
			},
			[]string{"reason"},
		),

		// Social metrics
		FollowsTotal: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "follows_total",
				Help: "Total number of follows",
			},
			[]string{},
		),
		UnfollowsTotal: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "unfollows_total",
				Help: "Total number of unfollows",
			},
			[]string{},
		),
		LikesTotal: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "likes_total",
				Help: "Total number of likes",
			},
			[]string{},
		),
		CommentsTotal: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "comments_total",
				Help: "Total number of comments",
			},
			[]string{},
		),
		PostsCreated: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "posts_created_total",
				Help: "Total number of posts created",
			},
			[]string{"visibility"},
		),

		// Validation metrics
		ValidationFailures: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "validation_failures_total",
				Help: "Total validation failures",
			},
			[]string{"field", "reason"},
		),

		// Search metrics
		SearchRequests: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "search_requests_total",
				Help: "Total search requests",
			},
			[]string{"backend", "type"},
		),

		// Recommendation metrics
		GorseSync: *promauto.NewCounterVec(
			prometheus.CounterOpts{
				Name: "gorse_sync_total",
				Help: "Total Gorse sync operations",
			},
			[]string{"type", "status"},
		),
	}
}
