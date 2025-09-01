package models

import (
	"time"

	"github.com/google/uuid"
	"gorm.io/gorm"
)

// User represents a Sidechain producer account with unified auth
type User struct {
	ID          string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	Email       string `gorm:"uniqueIndex;not null" json:"email"`
	Username    string `gorm:"uniqueIndex;not null" json:"username"`
	DisplayName string `gorm:"not null" json:"display_name"`
	Bio         string `gorm:"type:text" json:"bio"`

	// Native auth fields
	PasswordHash     *string `gorm:"type:text" json:"-"`
	EmailVerified    bool    `gorm:"default:false" json:"email_verified"`
	EmailVerifyToken *string `gorm:"type:text" json:"-"`

	// OAuth provider IDs (nullable - users can have native accounts)
	GoogleID  *string `gorm:"uniqueIndex" json:"-"`
	DiscordID *string `gorm:"uniqueIndex" json:"-"`

	// Profile data
	AvatarURL     string   `json:"avatar_url"`
	DAWPreference string   `json:"daw_preference"`
	Genre         []string `gorm:"type:text[]" json:"genre"`

	// Social stats (cached from Stream.io)
	FollowerCount  int `gorm:"default:0" json:"follower_count"`
	FollowingCount int `gorm:"default:0" json:"following_count"`
	PostCount      int `gorm:"default:0" json:"post_count"`

	// Activity tracking
	LastActiveAt *time.Time `json:"last_active_at"`
	IsOnline     bool       `gorm:"default:false" json:"is_online"`

	// Stream.io integration
	StreamUserID string  `gorm:"uniqueIndex" json:"stream_user_id"`
	StreamToken  *string `gorm:"type:text" json:"-"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// AudioPost represents a shared loop with metadata
type AudioPost struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"not null;index" json:"user_id"`
	User   User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Audio file data
	AudioURL         string  `gorm:"not null" json:"audio_url"`
	OriginalFilename string  `json:"original_filename"`
	FileSize         int64   `json:"file_size"`
	Duration         float64 `json:"duration"` // seconds

	// Audio metadata
	BPM          int      `json:"bpm"`
	Key          string   `json:"key"`
	DurationBars int      `json:"duration_bars"`
	DAW          string   `json:"daw"`
	Genre        []string `gorm:"type:text[]" json:"genre"`

	// Visual data
	WaveformSVG string `gorm:"type:text" json:"waveform_svg"`

	// Engagement metrics (cached from Stream.io)
	LikeCount    int `gorm:"default:0" json:"like_count"`
	PlayCount    int `gorm:"default:0" json:"play_count"`
	CommentCount int `gorm:"default:0" json:"comment_count"`

	// Stream.io integration
	StreamActivityID string `gorm:"uniqueIndex" json:"stream_activity_id"`

	// Status
	ProcessingStatus string `gorm:"default:pending" json:"processing_status"` // pending, processing, complete, failed
	IsPublic         bool   `gorm:"default:true" json:"is_public"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// Device represents a VST instance that can be authenticated
type Device struct {
	ID     string  `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID *string `gorm:"index" json:"user_id"` // Nullable until claimed
	User   *User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Device info
	DeviceFingerprint string `gorm:"uniqueIndex" json:"device_fingerprint"`
	Platform          string `json:"platform"` // "macOS", "Windows", "Linux"
	DAW               string `json:"daw"`      // "Ableton Live", "FL Studio", etc.
	DAWVersion        string `json:"daw_version"`

	// Status
	LastUsedAt *time.Time `json:"last_used_at"`
	IsActive   bool       `gorm:"default:false" json:"is_active"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// OAuthProvider represents linked OAuth accounts
type OAuthProvider struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"not null;index" json:"user_id"`
	User   User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	Provider       string `gorm:"not null" json:"provider"` // "google", "discord"
	ProviderUserID string `gorm:"not null" json:"provider_user_id"`
	Email          string `gorm:"not null" json:"email"`
	Name           string `json:"name"`
	AvatarURL      string `json:"avatar_url"`

	// OAuth tokens (for API access if needed)
	AccessToken  *string    `gorm:"type:text" json:"-"`
	RefreshToken *string    `gorm:"type:text" json:"-"`
	TokenExpiry  *time.Time `json:"-"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// Ensure unique constraint: one provider per user
func (OAuthProvider) TableName() string {
	return "oauth_providers"
}

// PasswordReset represents password reset tokens
type PasswordReset struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"not null;index" json:"user_id"`
	User   User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	Token     string    `gorm:"uniqueIndex;not null" json:"token"`
	ExpiresAt time.Time `gorm:"not null" json:"expires_at"`
	Used      bool      `gorm:"default:false" json:"used"`

	// GORM fields
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`
}

// BeforeCreate hooks for GORM
func (u *User) BeforeCreate(tx *gorm.DB) error {
	if u.ID == "" {
		u.ID = generateUUID()
	}
	if u.StreamUserID == "" {
		u.StreamUserID = u.ID // Use same ID for Stream.io
	}
	return nil
}

func (p *AudioPost) BeforeCreate(tx *gorm.DB) error {
	if p.ID == "" {
		p.ID = generateUUID()
	}
	return nil
}

func (d *Device) BeforeCreate(tx *gorm.DB) error {
	if d.ID == "" {
		d.ID = generateUUID()
	}
	return nil
}

// Helper function for UUID generation
func generateUUID() string {
	return uuid.New().String()
}
