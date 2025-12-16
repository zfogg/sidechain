package models

import (
	"database/sql/driver"
	"strings"
	"time"

	"github.com/google/uuid"
	"gorm.io/gorm"
)

// StringArray is a custom type for PostgreSQL text[] that implements Scanner and Valuer
type StringArray []string

// Scan implements the sql.Scanner interface for reading from database
func (a *StringArray) Scan(value interface{}) error {
	if value == nil {
		*a = nil
		return nil
	}

	// PostgreSQL returns text[] as a string like "{value1,value2,value3}"
	str, ok := value.(string)
	if !ok {
		// Try []byte
		if bytes, ok := value.([]byte); ok {
			str = string(bytes)
		} else {
			*a = nil
			return nil
		}
	}

	// Remove the curly braces
	str = strings.TrimPrefix(str, "{")
	str = strings.TrimSuffix(str, "}")

	if str == "" {
		*a = []string{}
		return nil
	}

	// Split by comma (simple case - doesn't handle quoted values with commas)
	*a = strings.Split(str, ",")
	return nil
}

// Value implements the driver.Valuer interface for writing to database
func (a StringArray) Value() (driver.Value, error) {
	if a == nil {
		return nil, nil
	}
	if len(a) == 0 {
		return "{}", nil
	}
	return "{" + strings.Join(a, ",") + "}", nil
}

// SocialLinks stores user's external profile links
type SocialLinks struct {
	Instagram  string `json:"instagram,omitempty"`
	SoundCloud string `json:"soundcloud,omitempty"`
	Spotify    string `json:"spotify,omitempty"`
	YouTube    string `json:"youtube,omitempty"`
	Twitter    string `json:"twitter,omitempty"`
	Website    string `json:"website,omitempty"`
}

// User represents a Sidechain producer account with unified auth
type User struct {
	ID          string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	Email       string `gorm:"uniqueIndex;not null" json:"email"`
	Username    string `gorm:"uniqueIndex;not null" json:"username"`
	DisplayName string `gorm:"not null" json:"display_name"`
	Bio         string `gorm:"type:text" json:"bio"`
	Location    string `gorm:"type:text" json:"location"` // City/Country

	// Native auth fields
	PasswordHash     *string `gorm:"type:text" json:"-"`
	EmailVerified    bool    `gorm:"default:false" json:"email_verified"`
	EmailVerifyToken *string `gorm:"type:text" json:"-"`

	// Two-Factor Authentication
	TwoFactorEnabled bool    `gorm:"default:false" json:"two_factor_enabled"`
	TwoFactorType    string  `gorm:"type:varchar(10);default:totp" json:"-"` // "totp" or "hotp"
	TwoFactorSecret  *string `gorm:"type:text" json:"-"`                     // OTP secret
	TwoFactorCounter *uint64 `gorm:"default:0" json:"-"`                     // HOTP counter (increments after each use)
	BackupCodes      *string `gorm:"type:text" json:"-"`                     // JSON array of hashed backup codes

	// OAuth provider IDs (nullable - users can have native accounts)
	GoogleID  *string `gorm:"uniqueIndex" json:"-"`
	DiscordID *string `gorm:"uniqueIndex" json:"-"`

	// Profile data
	OAuthProfilePictureURL string       `gorm:"column:oauth_profile_picture_url" json:"oauth_profile_picture_url"` // From OAuth provider (Google/Discord)
	ProfilePictureURL      string       `json:"profile_picture_url"`       // S3 URL for user-uploaded profile picture
	DAWPreference     string       `json:"daw_preference"`
	Genre             StringArray  `gorm:"type:text[]" json:"genre"`
	SocialLinks       *SocialLinks `gorm:"type:jsonb;serializer:json" json:"social_links"`

	// Social stats (fetched from getstream.io - these are cached values, not source of truth)
	// Use stream.Client.GetFollowStats() for real-time counts
	FollowerCount  int `gorm:"default:0" json:"follower_count"`
	FollowingCount int `gorm:"default:0" json:"following_count"`
	PostCount      int `gorm:"default:0" json:"post_count"`

	// Admin/Moderation
	IsAdmin bool `gorm:"default:false" json:"is_admin"` // Admin can moderate content and view private profiles

	// Privacy settings
	IsPrivate bool `gorm:"default:false" json:"is_private"` // If true, requires follow approval

	// Activity status privacy
	ShowActivityStatus bool `gorm:"default:true" json:"show_activity_status"` // Show online/offline status to others
	ShowLastActive     bool `gorm:"default:true" json:"show_last_active"`     // Show "last active X ago" to others

	// Activity tracking
	LastActiveAt *time.Time `json:"last_active_at"`
	IsOnline     bool       `gorm:"default:false" json:"is_online"`

	// getstream.io integration
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
	AudioURL         string `gorm:"not null" json:"audio_url"`
	OriginalFilename string `json:"original_filename"`         // System filename from upload
	Filename         string `json:"filename"`                  // User-editable display filename
	FileSize         int64  `json:"file_size"`
	Duration         float64 `json:"duration"` // seconds

	// Audio metadata
	BPM          int         `json:"bpm"`
	Key          string      `json:"key"`
	DurationBars int         `json:"duration_bars"`
	DAW          string      `json:"daw"`
	Genre        StringArray `gorm:"type:text[]" json:"genre"`

	// MIDI data (optional - posts can have associated MIDI patterns)
	MIDIPatternID *string      `gorm:"index" json:"midi_pattern_id,omitempty"`
	MIDIPattern   *MIDIPattern `gorm:"foreignKey:MIDIPatternID" json:"midi_pattern,omitempty"`

	// Remix relationship (R.3.2 Remix Chains)
	// A post can be a remix of another post OR a story
	RemixOfPostID  *string    `gorm:"index" json:"remix_of_post_id,omitempty"`
	RemixOfPost    *AudioPost `gorm:"foreignKey:RemixOfPostID" json:"remix_of_post,omitempty"`
	RemixOfStoryID *string    `gorm:"index" json:"remix_of_story_id,omitempty"`
	RemixOfStory   *Story     `gorm:"foreignKey:RemixOfStoryID" json:"remix_of_story,omitempty"`
	RemixChainDepth int       `gorm:"default:0" json:"remix_chain_depth"` // 0=original, 1=remix, 2=remix of remix
	RemixCount     int        `gorm:"default:0" json:"remix_count"`       // How many posts remix this one
	// What was remixed: "audio", "midi", "both"
	RemixType      string     `json:"remix_type,omitempty"`

	// Visual data
	WaveformURL string `json:"waveform_url"` // CDN URL to waveform PNG

	// Engagement metrics (cached from getstream.io)
	LikeCount     int `gorm:"default:0" json:"like_count"`
	PlayCount     int `gorm:"default:0" json:"play_count"`
	CommentCount  int `gorm:"default:0" json:"comment_count"`
	DownloadCount int `gorm:"default:0" json:"download_count"` // Track downloads for analytics
	SaveCount     int `gorm:"default:0" json:"save_count"`     // How many users saved/bookmarked this post
	RepostCount   int `gorm:"default:0" json:"repost_count"`   // How many users reposted this

	// getstream.io integration
	StreamActivityID string `gorm:"uniqueIndex" json:"stream_activity_id"`

	// Status
	ProcessingStatus string `gorm:"default:pending" json:"processing_status"` // pending, processing, complete, failed
	IsPublic         bool   `gorm:"default:true" json:"is_public"`
	IsArchived       bool   `gorm:"default:false" json:"is_archived"` // Hidden from feeds but not deleted

	// Pin to profile
	IsPinned bool `gorm:"default:false;index" json:"is_pinned"` // Show at top of profile
	PinOrder int  `gorm:"default:0" json:"pin_order"`           // Order among pinned posts (1-3)

	// Comment controls
	// CommentAudience controls who can comment: "everyone" (default), "followers", "off"
	CommentAudience string `gorm:"default:everyone" json:"comment_audience"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// HasMIDI returns true if the post has an associated MIDI pattern
func (p *AudioPost) HasMIDI() bool {
	return p.MIDIPatternID != nil
}

// IsRemix returns true if this post is a remix of another post or story
func (p *AudioPost) IsRemix() bool {
	return p.RemixOfPostID != nil || p.RemixOfStoryID != nil
}

// GetRemixSourceType returns "post", "story", or "" if not a remix
func (p *AudioPost) GetRemixSourceType() string {
	if p.RemixOfPostID != nil {
		return "post"
	}
	if p.RemixOfStoryID != nil {
		return "story"
	}
	return ""
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
	OAuthProfilePictureURL string `json:"oauth_profile_picture_url"`

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

// Comment represents a comment on an AudioPost
type Comment struct {
	ID     string    `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	PostID string    `gorm:"not null;index" json:"post_id"`
	Post   AudioPost `gorm:"foreignKey:PostID" json:"-"`
	UserID string    `gorm:"not null;index" json:"user_id"`
	User   User      `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Content
	Content string `gorm:"type:text;not null" json:"content"`

	// Threading - parent_id is null for top-level comments
	ParentID *string    `gorm:"type:uuid;index" json:"parent_id,omitempty"`
	Parent   *Comment   `gorm:"foreignKey:ParentID" json:"-"`
	Replies  []*Comment `gorm:"foreignKey:ParentID" json:"replies,omitempty"`

	// Engagement (cached from getstream.io reactions)
	LikeCount int `gorm:"default:0" json:"like_count"`

	// getstream.io integration for reactions
	StreamActivityID string `gorm:"uniqueIndex" json:"stream_activity_id,omitempty"`

	// Edit tracking
	IsEdited bool       `gorm:"default:false" json:"is_edited"`
	EditedAt *time.Time `json:"edited_at,omitempty"`

	// Moderation
	IsDeleted bool `gorm:"default:false" json:"is_deleted"` // Soft delete for "comment removed"

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// CommentMention tracks @mentions in comments for notifications
type CommentMention struct {
	ID              string  `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	CommentID       string  `gorm:"not null;index" json:"comment_id"`
	Comment         Comment `gorm:"foreignKey:CommentID" json:"-"`
	MentionedUserID string  `gorm:"not null;index" json:"mentioned_user_id"`
	MentionedUser   User    `gorm:"foreignKey:MentionedUserID" json:"mentioned_user,omitempty"`

	// Whether the notification was sent
	NotificationSent bool `gorm:"default:false" json:"notification_sent"`

	// GORM fields
	CreatedAt time.Time `json:"created_at"`
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

// ReportReason represents the reason for a report
type ReportReason string

const (
	ReportReasonSpam          ReportReason = "spam"
	ReportReasonHarassment    ReportReason = "harassment"
	ReportReasonCopyright     ReportReason = "copyright"
	ReportReasonInappropriate ReportReason = "inappropriate"
	ReportReasonViolence      ReportReason = "violence"
	ReportReasonOther         ReportReason = "other"
)

// ReportStatus represents the status of a report
type ReportStatus string

const (
	ReportStatusPending   ReportStatus = "pending"
	ReportStatusReviewing ReportStatus = "reviewing"
	ReportStatusResolved  ReportStatus = "resolved"
	ReportStatusDismissed ReportStatus = "dismissed"
)

// ReportTargetType represents what type of content is being reported
type ReportTargetType string

const (
	ReportTargetPost    ReportTargetType = "post"
	ReportTargetComment ReportTargetType = "comment"
	ReportTargetUser    ReportTargetType = "user"
)

// CommentAudience constants for who can comment on a post
const (
	CommentAudienceEveryone  = "everyone"  // Anyone can comment (default)
	CommentAudienceFollowers = "followers" // Only followers can comment
	CommentAudienceOff       = "off"       // Comments disabled
)

// Report represents a user report for moderation
type Report struct {
	ID         string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	ReporterID string `gorm:"not null;index" json:"reporter_id"`
	Reporter   User   `gorm:"foreignKey:ReporterID" json:"reporter,omitempty"`

	// Target of the report
	TargetType   ReportTargetType `gorm:"not null" json:"target_type"`     // "post", "comment", "user"
	TargetID     string           `gorm:"not null;index" json:"target_id"` // ID of the post/comment/user
	TargetUserID *string          `gorm:"index" json:"target_user_id"`     // User who created the content (for posts/comments)

	// Report details
	Reason      ReportReason `gorm:"not null" json:"reason"`
	Description string       `gorm:"type:text" json:"description"` // Optional additional context
	Status      ReportStatus `gorm:"default:pending" json:"status"`

	// Moderation action
	ModeratorID *string `gorm:"index" json:"moderator_id"` // Admin who reviewed
	Moderator   *User   `gorm:"foreignKey:ModeratorID" json:"moderator,omitempty"`
	ActionTaken string  `gorm:"type:text" json:"action_taken"` // "warned", "removed", "banned", etc.

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// UserBlock represents a user blocking another user
type UserBlock struct {
	ID        string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	BlockerID string `gorm:"not null;index" json:"blocker_id"` // User who is blocking
	Blocker   User   `gorm:"foreignKey:BlockerID" json:"blocker,omitempty"`
	BlockedID string `gorm:"not null;index" json:"blocked_id"` // User who is blocked
	Blocked   User   `gorm:"foreignKey:BlockedID" json:"blocked,omitempty"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// Ensure unique constraint: one block per user pair
func (UserBlock) TableName() string {
	return "user_blocks"
}

// FollowRequestStatus represents the status of a follow request
type FollowRequestStatus string

const (
	FollowRequestStatusPending  FollowRequestStatus = "pending"
	FollowRequestStatusAccepted FollowRequestStatus = "accepted"
	FollowRequestStatusRejected FollowRequestStatus = "rejected"
)

// FollowRequest represents a pending follow request for private accounts
type FollowRequest struct {
	ID          string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	RequesterID string `gorm:"not null;index" json:"requester_id"` // User requesting to follow
	Requester   User   `gorm:"foreignKey:RequesterID" json:"requester,omitempty"`
	TargetID    string `gorm:"not null;index" json:"target_id"` // Private account being requested to follow
	Target      User   `gorm:"foreignKey:TargetID" json:"target,omitempty"`

	Status FollowRequestStatus `gorm:"type:varchar(20);default:pending" json:"status"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName for follow requests
func (FollowRequest) TableName() string {
	return "follow_requests"
}

func (fr *FollowRequest) BeforeCreate(tx *gorm.DB) error {
	if fr.ID == "" {
		fr.ID = generateUUID()
	}
	return nil
}

// SearchQuery represents a tracked search query for analytics (7.1.9)
type SearchQuery struct {
	ID          string  `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID      *string `gorm:"index" json:"user_id"` // Nullable for anonymous searches
	User        *User   `gorm:"foreignKey:UserID" json:"user,omitempty"`
	Query       string  `gorm:"type:text;not null" json:"query"`
	ResultType  string  `gorm:"type:varchar(20);not null" json:"result_type"` // "users", "posts"
	ResultCount int     `gorm:"default:0" json:"result_count"`
	Filters     string  `gorm:"type:jsonb" json:"filters"` // JSON string of filters applied

	// GORM fields
	CreatedAt time.Time `json:"created_at"`
}

// UserPreference tracks user listening preferences for recommendations (7.2.4)
type UserPreference struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"not null;index" json:"user_id"`
	User   User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Preference data (aggregated from play history)
	GenreWeights map[string]float64 `gorm:"type:jsonb;serializer:json" json:"genre_weights"` // Genre -> weight (0-1)
	BPMRange     *struct {
		Min int `json:"min"`
		Max int `json:"max"`
	} `gorm:"type:jsonb;serializer:json" json:"bpm_range"`
	KeyPreferences StringArray `gorm:"type:text[]" json:"key_preferences"` // Most listened keys

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// PlayHistory tracks individual play events for velocity calculation (7.2.1.3, 7.2.4)
type PlayHistory struct {
	ID        string    `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID    string    `gorm:"not null;index" json:"user_id"`
	User      User      `gorm:"foreignKey:UserID" json:"user,omitempty"`
	PostID    string    `gorm:"not null;index" json:"post_id"`
	Post      AudioPost `gorm:"foreignKey:PostID" json:"post,omitempty"`
	Duration  float64   `gorm:"default:0" json:"duration"`      // Seconds listened
	Completed bool      `gorm:"default:false" json:"completed"` // Did they listen to the full track?
	CreatedAt time.Time `json:"created_at"`
}

// Hashtag represents a hashtag used in posts (7.2.5)
type Hashtag struct {
	ID         string    `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	Name       string    `gorm:"uniqueIndex;not null" json:"name"` // e.g., "house", "techno"
	PostCount  int       `gorm:"default:0" json:"post_count"`      // Number of posts using this hashtag
	LastUsedAt time.Time `json:"last_used_at"`
	CreatedAt  time.Time `json:"created_at"`
	UpdatedAt  time.Time `json:"updated_at"`
}

// PostHashtag links posts to hashtags (many-to-many) (7.2.5)
type PostHashtag struct {
	ID        string    `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	PostID    string    `gorm:"not null;index" json:"post_id"`
	Post      AudioPost `gorm:"foreignKey:PostID" json:"post,omitempty"`
	HashtagID string    `gorm:"not null;index" json:"hashtag_id"`
	Hashtag   Hashtag   `gorm:"foreignKey:HashtagID" json:"hashtag,omitempty"`
	CreatedAt time.Time `json:"created_at"`
}

// NOTE: MIDIEvent and MIDIData have been moved to midi.go

// Story represents a short music clip (15-60 seconds) with MIDI visualization (7.5.1.1.1)
type Story struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"not null;index" json:"user_id"`
	User   User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Audio file data
	AudioURL      string  `gorm:"not null" json:"audio_url"`
	AudioDuration float64 `gorm:"not null" json:"audio_duration"` // seconds
	Filename      string  `json:"filename"`                       // Display filename for audio
	MIDIFilename  string  `json:"midi_filename"`                  // Display filename for MIDI if present

	// MIDI data (optional - stories can be audio-only)
	// References standalone MIDIPattern for sharing/downloading
	MIDIPatternID *string      `gorm:"index" json:"midi_pattern_id,omitempty"`
	MIDIPattern   *MIDIPattern `gorm:"foreignKey:MIDIPatternID" json:"midi_pattern,omitempty"`

	// Legacy: Embedded MIDI data for backwards compatibility
	// TODO: Migrate existing data to midi_patterns table and remove this field
	MIDIData *MIDIData `gorm:"type:jsonb;serializer:json" json:"midi_data,omitempty"`

	// Visual data
	WaveformData string `gorm:"type:text" json:"waveform_data"` // Legacy SVG (deprecated)
	WaveformURL  string `json:"waveform_url"`                   // CDN URL to waveform PNG

	// Audio metadata (optional for quick sharing)
	BPM   *int        `json:"bpm,omitempty"`
	Key   *string     `json:"key,omitempty"`
	Genre StringArray `gorm:"type:text[]" json:"genre,omitempty"`

	// Expiration
	ExpiresAt time.Time `gorm:"not null;index" json:"expires_at"` // created_at + 24 hours

	// Analytics
	ViewCount int `gorm:"default:0" json:"view_count"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// GetMIDIData returns the MIDI data, preferring the referenced MIDIPattern over legacy embedded data
func (s *Story) GetMIDIData() *MIDIData {
	if s.MIDIPattern != nil {
		return s.MIDIPattern.ToMIDIData()
	}
	return s.MIDIData
}

// HasMIDI returns true if the story has MIDI data (either referenced or embedded)
func (s *Story) HasMIDI() bool {
	return s.MIDIPatternID != nil || s.MIDIData != nil
}

// StoryView tracks who viewed a story (7.5.1.1.2)
type StoryView struct {
	ID       string    `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	StoryID  string    `gorm:"not null;index" json:"story_id"`
	Story    Story     `gorm:"foreignKey:StoryID" json:"story,omitempty"`
	ViewerID string    `gorm:"not null;index" json:"viewer_id"`
	Viewer   User      `gorm:"foreignKey:ViewerID" json:"viewer,omitempty"`
	ViewedAt time.Time `gorm:"not null;default:now()" json:"viewed_at"`
}

// TableName ensures unique constraint: one view per user per story
func (StoryView) TableName() string {
	return "story_views"
}

// StoryHighlight represents a collection of saved stories (7.5.6)
// Highlights are permanent - they don't expire like regular stories
type StoryHighlight struct {
	ID          string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID      string `gorm:"not null;index" json:"user_id"`
	User        User   `gorm:"foreignKey:UserID" json:"user,omitempty"`
	Name        string `gorm:"not null" json:"name"`                   // e.g., "Jams", "Experiments"
	CoverImage  string `json:"cover_image,omitempty"`                  // Optional cover image URL
	Description string `gorm:"type:text" json:"description,omitempty"` // Optional description
	SortOrder   int    `gorm:"default:0" json:"sort_order"`            // Order on profile
	StoryCount  int    `gorm:"default:0" json:"story_count"`           // Cached count

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// HighlightedStory is the join table between highlights and stories
type HighlightedStory struct {
	ID          string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	HighlightID string `gorm:"not null;index" json:"highlight_id"`
	StoryID     string `gorm:"not null;index" json:"story_id"`
	SortOrder   int    `gorm:"default:0" json:"sort_order"` // Order within highlight

	// Relations
	Highlight StoryHighlight `gorm:"foreignKey:HighlightID" json:"highlight,omitempty"`
	Story     Story          `gorm:"foreignKey:StoryID" json:"story,omitempty"`

	// GORM fields
	CreatedAt time.Time `json:"created_at"`
}

// TableName for highlighted stories
func (HighlightedStory) TableName() string {
	return "highlighted_stories"
}

// SavedPost represents a bookmarked/saved post by a user
type SavedPost struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"not null;index" json:"user_id"`
	PostID string `gorm:"not null;index" json:"post_id"`

	// Relations
	User User      `gorm:"foreignKey:UserID" json:"user,omitempty"`
	Post AudioPost `gorm:"foreignKey:PostID" json:"post,omitempty"`

	// GORM fields
	CreatedAt time.Time `json:"created_at"`
}

// TableName for saved posts
func (SavedPost) TableName() string {
	return "saved_posts"
}

// Repost represents a user sharing another user's post to their feed
type Repost struct {
	ID             string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID         string `gorm:"not null;index" json:"user_id"`          // User who reposted
	OriginalPostID string `gorm:"not null;index" json:"original_post_id"` // Original post being reposted

	// Optional quote/comment when reposting
	Quote string `gorm:"type:text" json:"quote,omitempty"`

	// Relations
	User         User      `gorm:"foreignKey:UserID" json:"user,omitempty"`
	OriginalPost AudioPost `gorm:"foreignKey:OriginalPostID" json:"original_post,omitempty"`

	// getstream.io integration - the activity ID for this repost in the feed
	StreamActivityID string `gorm:"uniqueIndex" json:"stream_activity_id,omitempty"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName for reposts
func (Repost) TableName() string {
	return "reposts"
}

// MutedUser represents a user muting another user
// Muting hides the muted user's posts from the muter's feed without unfollowing
type MutedUser struct {
	ID          string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID      string `gorm:"not null;index" json:"user_id"`       // User who is muting
	MutedUserID string `gorm:"not null;index" json:"muted_user_id"` // User being muted

	// Relations
	User      User `gorm:"foreignKey:UserID" json:"user,omitempty"`
	MutedUser User `gorm:"foreignKey:MutedUserID" json:"muted_user,omitempty"`

	// GORM fields
	CreatedAt time.Time `json:"created_at"`
}

// TableName for muted users
func (MutedUser) TableName() string {
	return "muted_users"
}

// BeforeCreate generates UUID for MutedUser
func (mu *MutedUser) BeforeCreate(tx *gorm.DB) error {
	if mu.ID == "" {
		mu.ID = uuid.New().String()
	}
	return nil
}

// BeforeCreate hooks for GORM
func (u *User) BeforeCreate(tx *gorm.DB) error {
	if u.ID == "" {
		u.ID = generateUUID()
	}
	if u.StreamUserID == "" {
		u.StreamUserID = u.ID // Use same ID for getstream.io
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

func (c *Comment) BeforeCreate(tx *gorm.DB) error {
	if c.ID == "" {
		c.ID = generateUUID()
	}
	// StreamActivityID has a unique index - generate if not provided
	if c.StreamActivityID == "" {
		c.StreamActivityID = generateUUID()
	}
	return nil
}

func (m *CommentMention) BeforeCreate(tx *gorm.DB) error {
	if m.ID == "" {
		m.ID = generateUUID()
	}
	return nil
}

func (r *Report) BeforeCreate(tx *gorm.DB) error {
	if r.ID == "" {
		r.ID = generateUUID()
	}
	return nil
}

func (b *UserBlock) BeforeCreate(tx *gorm.DB) error {
	if b.ID == "" {
		b.ID = generateUUID()
	}
	return nil
}

func (p *UserPreference) BeforeCreate(tx *gorm.DB) error {
	if p.ID == "" {
		p.ID = generateUUID()
	}
	return nil
}

func (ph *PlayHistory) BeforeCreate(tx *gorm.DB) error {
	if ph.ID == "" {
		ph.ID = generateUUID()
	}
	return nil
}

func (h *Hashtag) BeforeCreate(tx *gorm.DB) error {
	if h.ID == "" {
		h.ID = generateUUID()
	}
	return nil
}

func (ph *PostHashtag) BeforeCreate(tx *gorm.DB) error {
	if ph.ID == "" {
		ph.ID = generateUUID()
	}
	return nil
}

func (s *Story) BeforeCreate(tx *gorm.DB) error {
	if s.ID == "" {
		s.ID = generateUUID()
	}
	// Set expires_at to 24 hours from now if not already set
	if s.ExpiresAt.IsZero() {
		s.ExpiresAt = time.Now().UTC().Add(24 * time.Hour)
	}
	return nil
}

func (h *StoryHighlight) BeforeCreate(tx *gorm.DB) error {
	if h.ID == "" {
		h.ID = generateUUID()
	}
	return nil
}

func (hs *HighlightedStory) BeforeCreate(tx *gorm.DB) error {
	if hs.ID == "" {
		hs.ID = generateUUID()
	}
	return nil
}

func (sv *StoryView) BeforeCreate(tx *gorm.DB) error {
	if sv.ID == "" {
		sv.ID = generateUUID()
	}
	return nil
}

func (sp *SavedPost) BeforeCreate(tx *gorm.DB) error {
	if sp.ID == "" {
		sp.ID = generateUUID()
	}
	return nil
}

func (r *Repost) BeforeCreate(tx *gorm.DB) error {
	if r.ID == "" {
		r.ID = generateUUID()
	}
	if r.StreamActivityID == "" {
		r.StreamActivityID = generateUUID()
	}
	return nil
}

// NotificationPreferences stores user preferences for different notification types
type NotificationPreferences struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"uniqueIndex;not null" json:"user_id"`
	User   User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Notification type toggles (all default to true - enabled)
	LikesEnabled    bool `gorm:"default:true" json:"likes_enabled"`    // Notifications when someone likes your post
	CommentsEnabled bool `gorm:"default:true" json:"comments_enabled"` // Notifications when someone comments on your post
	FollowsEnabled  bool `gorm:"default:true" json:"follows_enabled"`  // Notifications when someone follows you
	MentionsEnabled bool `gorm:"default:true" json:"mentions_enabled"` // Notifications when someone mentions you
	DMsEnabled      bool `gorm:"default:true" json:"dms_enabled"`      // Notifications for direct messages
	StoriesEnabled  bool `gorm:"default:true" json:"stories_enabled"`  // Notifications for story-related activity
	RepostsEnabled  bool `gorm:"default:true" json:"reposts_enabled"`  // Notifications when someone reposts your content

	// Challenge notifications (can be spammy, separate control)
	ChallengesEnabled bool `gorm:"default:true" json:"challenges_enabled"` // Notifications for MIDI challenges

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName for notification preferences
func (NotificationPreferences) TableName() string {
	return "notification_preferences"
}

func (np *NotificationPreferences) BeforeCreate(tx *gorm.DB) error {
	if np.ID == "" {
		np.ID = generateUUID()
	}
	return nil
}

// Helper function for UUID generation
func generateUUID() string {
	return uuid.New().String()
}

// GetAvatarURL returns the effective avatar URL for display
// Prefers user-uploaded profile picture, falls back to OAuth provider picture
func (u *User) GetAvatarURL() string {
	if u.ProfilePictureURL != "" {
		return u.ProfilePictureURL
	}
	return u.OAuthProfilePictureURL
}

//==============================================================================
// Sound/Sample Pages (Feature #15)
// Audio fingerprinting to detect same audio used across posts
//==============================================================================

// Sound represents a unique audio signature that can appear in multiple posts
// Similar to TikTok/Instagram sounds - when a sample is used by multiple users
type Sound struct {
	ID string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`

	// Sound metadata
	Name        string `json:"name"`         // Display name (auto-generated or user-provided)
	Description string `json:"description"`  // Optional description
	Duration    float64 `json:"duration"`    // Duration in seconds of the original sample

	// Original source (first post that used this sound)
	OriginalPostID  *string    `gorm:"index" json:"original_post_id,omitempty"`
	OriginalPost    *AudioPost `gorm:"foreignKey:OriginalPostID" json:"original_post,omitempty"`
	OriginalStoryID *string    `gorm:"index" json:"original_story_id,omitempty"`
	OriginalStory   *Story     `gorm:"foreignKey:OriginalStoryID" json:"original_story,omitempty"`

	// Creator (user who first uploaded this sound)
	CreatorID string `gorm:"index" json:"creator_id"`
	Creator   User   `gorm:"foreignKey:CreatorID" json:"creator,omitempty"`

	// Usage statistics (cached, updated periodically)
	UsageCount   int `gorm:"default:1" json:"usage_count"`   // Number of posts using this sound
	TrendingRank int `gorm:"default:0;index" json:"trending_rank"` // For trending sounds discovery

	// Fingerprint data (Shazam-style)
	// The fingerprint hash is used for fast lookups
	FingerprintHash string `gorm:"index;not null" json:"fingerprint_hash"`

	// Status
	IsPublic   bool `gorm:"default:true" json:"is_public"`   // Can others use this sound
	IsTrending bool `gorm:"default:false;index" json:"is_trending"` // Featured in trending

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

func (Sound) TableName() string {
	return "sounds"
}

func (s *Sound) BeforeCreate(tx *gorm.DB) error {
	if s.ID == "" {
		s.ID = generateUUID()
	}
	return nil
}

// AudioFingerprint stores the fingerprint data for an audio post or story
// Links posts/stories to Sounds for "used this sound" discovery
type AudioFingerprint struct {
	ID string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`

	// The post or story this fingerprint belongs to
	AudioPostID *string    `gorm:"index" json:"audio_post_id,omitempty"`
	AudioPost   *AudioPost `gorm:"foreignKey:AudioPostID" json:"audio_post,omitempty"`
	StoryID     *string    `gorm:"index" json:"story_id,omitempty"`
	Story       *Story     `gorm:"foreignKey:StoryID" json:"story,omitempty"`

	// The matched Sound (set after fingerprint matching)
	SoundID *string `gorm:"index" json:"sound_id,omitempty"`
	Sound   *Sound  `gorm:"foreignKey:SoundID" json:"sound,omitempty"`

	// Fingerprint data (stored for re-matching if needed)
	// Hash is the primary identifier for fast lookups
	FingerprintHash string `gorm:"index;not null" json:"fingerprint_hash"`

	// Fingerprint components (stored as JSON for flexibility)
	// These are the raw peak/hash data from the Shazam algorithm
	FingerprintData []byte `gorm:"type:jsonb" json:"-"` // Full fingerprint data (not exposed in API)

	// Match confidence (0.0 to 1.0) when matched to a Sound
	MatchConfidence float64 `gorm:"default:0" json:"match_confidence"`

	// Processing status
	Status string `gorm:"default:'pending';index" json:"status"` // pending, processing, completed, failed

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

func (AudioFingerprint) TableName() string {
	return "audio_fingerprints"
}

func (af *AudioFingerprint) BeforeCreate(tx *gorm.DB) error {
	if af.ID == "" {
		af.ID = generateUUID()
	}
	return nil
}

// SoundUsage tracks when a user uses a sound in their post
// Allows for analytics and "X users used this sound" displays
type SoundUsage struct {
	ID string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`

	SoundID string `gorm:"index;not null" json:"sound_id"`
	Sound   Sound  `gorm:"foreignKey:SoundID" json:"sound,omitempty"`

	UserID string `gorm:"index;not null" json:"user_id"`
	User   User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// What used the sound
	AudioPostID *string    `gorm:"index" json:"audio_post_id,omitempty"`
	AudioPost   *AudioPost `gorm:"foreignKey:AudioPostID" json:"audio_post,omitempty"`
	StoryID     *string    `gorm:"index" json:"story_id,omitempty"`
	Story       *Story     `gorm:"foreignKey:StoryID" json:"story,omitempty"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

func (SoundUsage) TableName() string {
	return "sound_usages"
}

func (su *SoundUsage) BeforeCreate(tx *gorm.DB) error {
	if su.ID == "" {
		su.ID = generateUUID()
	}
	return nil
}
