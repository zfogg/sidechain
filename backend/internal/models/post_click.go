package models

import (
	"time"

	"gorm.io/gorm"
)

// PostClick tracks when a user views/clicks on a post
// Used for engagement analytics and click metrics
// Each click is recorded as a separate entry for full analytics granularity
type PostClick struct {
	ID        string    `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	PostID    string    `gorm:"not null;index:idx_post_timestamp" json:"post_id"`
	UserID    string    `gorm:"not null;index:idx_user_timestamp" json:"user_id"`
	Timestamp time.Time `gorm:"index:idx_post_timestamp,idx_user_timestamp" json:"timestamp"`

	// Optional context
	Source    string `gorm:"index" json:"source,omitempty"`     // "feed", "search", "profile", "web", "plugin"
	SessionID string `gorm:"index" json:"session_id,omitempty"` // Session identifier for grouping

	// GORM fields
	CreatedAt time.Time      `gorm:"index" json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName specifies the table name
func (PostClick) TableName() string {
	return "post_clicks"
}
