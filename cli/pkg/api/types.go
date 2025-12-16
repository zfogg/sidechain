package api

import "time"

// Auth Response Types
type LoginRequest struct {
	Email    string `json:"email"`
	Password string `json:"password"`
}

type RegisterRequest struct {
	Email       string `json:"email"`
	Username    string `json:"username"`
	Password    string `json:"password"`
	DisplayName string `json:"display_name"`
}

type LoginResponse struct {
	AccessToken  string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
	ExpiresIn    int    `json:"expires_in"`
	User         User   `json:"user"`
}

type RefreshRequest struct {
	RefreshToken string `json:"refresh_token"`
}

type RefreshResponse struct {
	AccessToken string `json:"access_token"`
	ExpiresIn   int    `json:"expires_in"`
}

type User struct {
	ID               string     `json:"id"`
	Email            string     `json:"email"`
	Username         string     `json:"username"`
	DisplayName      string     `json:"display_name"`
	FirstName        string     `json:"first_name,omitempty"`
	LastName         string     `json:"last_name,omitempty"`
	Bio              string     `json:"bio"`
	Location         string     `json:"location"`
	ProfilePicture   string     `json:"profile_picture"`
	FollowerCount    int        `json:"follower_count"`
	FollowersCount   int        `json:"followers_count"`
	FollowingCount   int        `json:"following_count"`
	PostCount        int        `json:"post_count"`
	SaveCount        int        `json:"save_count"`
	PlayCount        int        `json:"play_count"`
	EmailVerified    bool       `json:"email_verified"`
	IsVerified       bool       `json:"is_verified"`
	TwoFactorEnabled bool       `json:"two_factor_enabled"`
	TwoFAEnabled     bool       `json:"two_fa_enabled"`
	IsAdmin          bool       `json:"is_admin"`
	IsPrivate        bool       `json:"is_private"`
	IsSuspended      bool       `json:"is_suspended"`
	CreatedAt        time.Time  `json:"created_at"`
	UpdatedAt        time.Time  `json:"updated_at"`
	LastLoginAt      *time.Time `json:"last_login_at"`
	SuspendedAt      *time.Time `json:"suspended_at"`
	SuspensionReason string     `json:"suspension_reason,omitempty"`
	Warnings         int        `json:"warnings"`
	BPM              int        `json:"bpm,omitempty"`
	Duration         int        `json:"duration,omitempty"`
	Key              string     `json:"key,omitempty"`
	Genre            []string   `json:"genre,omitempty"`
	ReportCount      int        `json:"report_count"`
	LikeCount        int        `json:"like_count"`
	CommentCount     int        `json:"comment_count"`
	RepostCount      int        `json:"repost_count"`
	AuthorUsername   string     `json:"author_username"`
	Title            string     `json:"title"`
	Description      string     `json:"description"`
}

// Profile Response Types
type ProfileResponse struct {
	User User `json:"user"`
}

type UpdateProfileRequest struct {
	DisplayName string   `json:"display_name,omitempty"`
	Bio         string   `json:"bio,omitempty"`
	Location    string   `json:"location,omitempty"`
	Genre       []string `json:"genre,omitempty"`
	DAW         string   `json:"daw,omitempty"`
}

// Post Response Types
type Post struct {
	ID              string    `json:"id"`
	UserID          string    `json:"user_id"`
	AuthorUsername  string    `json:"author_username,omitempty"`
	Title           string    `json:"title,omitempty"`
	Description     string    `json:"description,omitempty"`
	AudioURL        string    `json:"audio_url"`
	Duration        int       `json:"duration"`
	BPM             int       `json:"bpm,omitempty"`
	Key             string    `json:"key,omitempty"`
	Genre           []string  `json:"genre,omitempty"`
	DAW             string    `json:"daw,omitempty"`
	LikeCount       int       `json:"like_count"`
	PlayCount       int       `json:"play_count"`
	CommentCount    int       `json:"comment_count"`
	SaveCount       int       `json:"save_count"`
	RepostCount     int       `json:"repost_count"`
	ReportCount     int       `json:"report_count,omitempty"`
	IsPinned        bool      `json:"is_pinned"`
	IsArchived      bool      `json:"is_archived"`
	CreatedAt       time.Time `json:"created_at"`
	UpdatedAt       time.Time `json:"updated_at"`
}

type PostListResponse struct {
	Posts      []Post `json:"posts"`
	TotalCount int    `json:"total_count"`
	Page       int    `json:"page"`
	PageSize   int    `json:"page_size"`
}

// Feed Response Types
type FeedResponse struct {
	Posts      []Post `json:"posts"`
	TotalCount int    `json:"total_count"`
	Page       int    `json:"page"`
	PageSize   int    `json:"page_size"`
}

// Error Response
type ErrorResponse struct {
	Code    string `json:"code"`
	Message string `json:"message"`
	Details map[string]interface{} `json:"details,omitempty"`
}

// Generic Paginated Response
type PaginatedResponse struct {
	Data       interface{} `json:"data"`
	TotalCount int         `json:"total_count"`
	Page       int         `json:"page"`
	PageSize   int         `json:"page_size"`
}

// Notification Response
type Notification struct {
	ID        string    `json:"id"`
	Type      string    `json:"type"` // like, comment, follow, mention
	ActorID   string    `json:"actor_id"`
	PostID    string    `json:"post_id,omitempty"`
	CommentID string    `json:"comment_id,omitempty"`
	Read      bool      `json:"read"`
	CreatedAt time.Time `json:"created_at"`
}

type NotificationListResponse struct {
	Notifications []Notification `json:"notifications"`
	UnreadCount   int            `json:"unread_count"`
	TotalCount    int            `json:"total_count"`
	Page          int            `json:"page"`
	PageSize      int            `json:"page_size"`
}
