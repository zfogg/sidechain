package models

import (
	"time"

	"gorm.io/gorm"
)

// RecommendationImpression tracks when a recommendation is shown to a user
// Used for CTR (Click-Through Rate) tracking and A/B testing
type RecommendationImpression struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"not null;index:idx_user_timestamp" json:"user_id"`
	PostID string `gorm:"not null;index:idx_post_timestamp" json:"post_id"`

	// Recommendation context
	Source   string `gorm:"not null;index:idx_source_timestamp" json:"source"` // "for-you", "popular", "latest", "discovery", "similar"
	Position int    `gorm:"not null" json:"position"`                          // Position in the feed (0-based)

	// Tracking
	Clicked   bool       `gorm:"default:false;index" json:"clicked"` // Set to true when user clicks/plays
	ClickedAt *time.Time `json:"clicked_at,omitempty"`               // When the click happened
	SessionID *string    `gorm:"index" json:"session_id,omitempty"`  // Optional session identifier for grouping

	// Metadata for analysis
	Score  *float64 `json:"score,omitempty"`  // Recommendation score from Gorse
	Reason *string  `json:"reason,omitempty"` // Recommendation reason text

	// GORM fields
	CreatedAt time.Time      `gorm:"index:idx_user_timestamp,idx_post_timestamp,idx_source_timestamp" json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// RecommendationClick tracks when a user clicks on a recommended item
// This is a simpler alternative to updating RecommendationImpression.Clicked
type RecommendationClick struct {
	ID                         string  `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID                     string  `gorm:"not null;index" json:"user_id"`
	PostID                     string  `gorm:"not null;index" json:"post_id"`
	RecommendationImpressionID *string `gorm:"index" json:"recommendation_impression_id,omitempty"` // Link to impression if available

	// Click context
	Source    string  `gorm:"not null;index" json:"source"` // Same as impression source
	Position  *int    `json:"position,omitempty"`           // Position in feed when clicked
	SessionID *string `gorm:"index" json:"session_id,omitempty"`

	// Engagement metrics
	PlayDuration *float64 `json:"play_duration,omitempty"`        // How long they listened (seconds)
	Completed    bool     `gorm:"default:false" json:"completed"` // Did they listen to completion

	// GORM fields
	CreatedAt time.Time      `gorm:"index" json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}
