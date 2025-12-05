package handlers

import (
	"fmt"
	"mime/multipart"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/websocket"
	"gorm.io/gorm"
)

// Handlers contains all HTTP handlers for the API
type Handlers struct {
	stream         *stream.Client
	audioProcessor *audio.Processor
	wsHandler      *websocket.Handler
}

// NewHandlers creates a new handlers instance
func NewHandlers(streamClient *stream.Client, audioProcessor *audio.Processor) *Handlers {
	return &Handlers{
		stream:         streamClient,
		audioProcessor: audioProcessor,
	}
}

// SetWebSocketHandler sets the WebSocket handler for real-time notifications
func (h *Handlers) SetWebSocketHandler(ws *websocket.Handler) {
	h.wsHandler = ws
}

// RegisterDevice creates a new device ID for VST authentication
func (h *Handlers) RegisterDevice(c *gin.Context) {
	deviceID := uuid.New().String()

	c.JSON(http.StatusOK, gin.H{
		"device_id": deviceID,
		"status":    "pending_claim",
		"claim_url": "/auth/claim/" + deviceID,
	})
}

// ClaimDevice allows a user to claim a device through web authentication
func (h *Handlers) ClaimDevice(c *gin.Context) {
	var req struct {
		DeviceID string `json:"device_id" binding:"required"`
		UserID   string `json:"user_id" binding:"required"`
		Username string `json:"username" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Create Stream.io user if doesn't exist
	if err := h.stream.CreateUser(req.UserID, req.Username); err != nil {
		// User might already exist, continue
	}

	// In a real implementation, you'd store this mapping in a database
	// For now, we'll generate a JWT token
	token := "jwt_token_placeholder_" + req.UserID

	c.JSON(http.StatusOK, gin.H{
		"token":    token,
		"user_id":  req.UserID,
		"username": req.Username,
		"status":   "claimed",
	})
}

// VerifyToken verifies a JWT token
func (h *Handlers) VerifyToken(c *gin.Context) {
	token := c.GetHeader("Authorization")
	if token == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "no token provided"})
		return
	}

	// Simple token validation - in production use proper JWT
	if len(token) > 20 {
		c.JSON(http.StatusOK, gin.H{"valid": true})
	} else {
		c.JSON(http.StatusUnauthorized, gin.H{"valid": false})
	}
}

// AuthMiddleware validates requests with JWT tokens
func (h *Handlers) AuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		token := c.GetHeader("Authorization")
		if token == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "no token provided"})
			c.Abort()
			return
		}

		// Simple validation - in production use proper JWT parsing
		if len(token) < 20 {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid token"})
			c.Abort()
			return
		}

		// Extract user ID from token (simplified)
		userID := "user_123" // In production, extract from JWT
		c.Set("user_id", userID)
		c.Next()
	}
}

// UploadAudio handles audio file uploads with async processing
func (h *Handlers) UploadAudio(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}

	currentUser := user.(*models.User)

	// Get uploaded file
	file, err := c.FormFile("audio")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "no_audio_file",
			"message": "No audio file provided in 'audio' field",
		})
		return
	}

	// Validate file size (max 50MB)
	const maxFileSize = 50 * 1024 * 1024
	if file.Size > maxFileSize {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "file_too_large",
			"message": "Audio file must be under 50MB",
		})
		return
	}

	// Validate file type
	if !isValidAudioFile(file.Filename) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_file_type",
			"message": "Only .mp3, .wav, .aiff, .m4a files are supported",
		})
		return
	}

	// Parse metadata from form
	metadata := map[string]interface{}{
		"bpm":           parseInt(c.PostForm("bpm"), 120),
		"key":           c.DefaultPostForm("key", "C major"),
		"duration_bars": parseInt(c.PostForm("duration_bars"), 8),
		"daw":           c.DefaultPostForm("daw", "Unknown"),
		"genre":         c.DefaultPostForm("genre", "Electronic"),
		"sample_rate":   parseFloat(c.PostForm("sample_rate"), 44100.0),
	}

	// Save uploaded file temporarily
	tempFilePath, err := saveUploadedFile(file)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "upload_failed",
			"message": "Failed to save uploaded file",
		})
		return
	}

	// Create pending audio post in database FIRST to get the postID
	// This allows the background job to update this record when complete
	audioPost := &models.AudioPost{
		ID:               uuid.New().String(),
		UserID:           currentUser.ID,
		OriginalFilename: file.Filename,
		FileSize:         file.Size,
		BPM:              parseInt(c.PostForm("bpm"), 120),
		Key:              c.DefaultPostForm("key", "C major"),
		DurationBars:     parseInt(c.PostForm("duration_bars"), 8),
		DAW:              c.DefaultPostForm("daw", "Unknown"),
		Genre:            parseGenreArray(c.PostForm("genre")),
		ProcessingStatus: "pending",
		IsPublic:         true,
	}

	err = database.DB.Create(audioPost).Error
	if err != nil {
		os.Remove(tempFilePath)
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "database_failed",
			"message": "Failed to save audio post",
		})
		return
	}

	// Submit to background processing queue with postID so it can update the record
	job, err := h.audioProcessor.SubmitProcessingJob(currentUser.ID, audioPost.ID, tempFilePath, file.Filename, metadata)
	if err != nil {
		os.Remove(tempFilePath)
		// Update the post to failed status
		database.DB.Model(audioPost).Update("processing_status", "failed")
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "queue_failed",
			"message": "Failed to queue audio for processing",
		})
		return
	}

	// Return immediate response - processing happens in background
	c.JSON(http.StatusAccepted, gin.H{
		"message":     "Audio upload received - processing in background",
		"post_id":     audioPost.ID,
		"job_id":      job.ID,
		"status":      "processing",
		"eta_seconds": 10, // Estimate 10 seconds for processing
		"poll_url":    fmt.Sprintf("/api/v1/audio/status/%s", job.ID),
	})
}

// GetAudioProcessingStatus returns the status of an audio processing job
func (h *Handlers) GetAudioProcessingStatus(c *gin.Context) {
	jobID := c.Param("job_id")

	status, err := h.audioProcessor.GetJobStatus(jobID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{
			"error":   "job_not_found",
			"message": "Processing job not found",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"job_id": jobID,
		"status": status.Status,
		"result": status.Result,
		"error":  status.ErrorMessage,
	})
}

// Helper functions
func isValidAudioFile(filename string) bool {
	ext := strings.ToLower(filepath.Ext(filename))
	validExts := []string{".mp3", ".wav", ".aiff", ".aif", ".m4a", ".flac", ".ogg"}

	for _, validExt := range validExts {
		if ext == validExt {
			return true
		}
	}
	return false
}

func saveUploadedFile(file *multipart.FileHeader) (string, error) {
	src, err := file.Open()
	if err != nil {
		return "", err
	}
	defer src.Close()

	// Create temp file
	tempDir := "/tmp/sidechain_uploads"
	os.MkdirAll(tempDir, 0755)

	tempFilePath := filepath.Join(tempDir, uuid.New().String()+filepath.Ext(file.Filename))

	// Read and save file data
	fileData := make([]byte, file.Size)
	_, err = src.Read(fileData)
	if err != nil {
		return "", err
	}

	err = os.WriteFile(tempFilePath, fileData, 0644)
	if err != nil {
		return "", err
	}

	return tempFilePath, nil
}

// Helper functions for parsing form data
func parseInt(s string, defaultValue int) int {
	if val, err := strconv.Atoi(s); err == nil {
		return val
	}
	return defaultValue
}

func parseFloat(s string, defaultValue float64) float64 {
	if val, err := strconv.ParseFloat(s, 64); err == nil {
		return val
	}
	return defaultValue
}

func parseGenreArray(s string) []string {
	if s == "" {
		return []string{}
	}
	// Simple comma-separated parsing - could be improved
	return []string{s}
}

// GetAudio retrieves audio file metadata
func (h *Handlers) GetAudio(c *gin.Context) {
	audioID := c.Param("id")

	// In production, fetch from database
	c.JSON(http.StatusOK, gin.H{
		"id":       audioID,
		"url":      "https://cdn.sidechain.app/audio/" + audioID + ".mp3",
		"duration": 32.5,
		"waveform": "data:image/svg+xml;base64,PHN2Zz48L3N2Zz4=",
		"metadata": gin.H{
			"bpm":           128,
			"key":           "F minor",
			"duration_bars": 8,
		},
	})
}

// GetTimeline gets the user's timeline feed
func (h *Handlers) GetTimeline(c *gin.Context) {
	userID := c.GetString("user_id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	activities, err := h.stream.GetUserTimeline(userID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to get timeline"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"total":  len(activities),
		},
	})
}

// GetGlobalFeed gets the global feed
func (h *Handlers) GetGlobalFeed(c *gin.Context) {
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	activities, err := h.stream.GetGlobalFeed(limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to get global feed"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"total":  len(activities),
		},
	})
}

// CreatePost creates a new post (alternative to upload)
func (h *Handlers) CreatePost(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		AudioURL     string   `json:"audio_url" binding:"required"`
		BPM          int      `json:"bpm"`
		Key          string   `json:"key"`
		DAW          string   `json:"daw"`
		DurationBars int      `json:"duration_bars"`
		Genre        []string `json:"genre"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	postID := uuid.New().String()

	activity := &stream.Activity{
		Actor:        "user:" + userID,
		Verb:         "posted",
		Object:       "loop:" + postID,
		ForeignID:    "loop:" + postID,
		AudioURL:     req.AudioURL,
		BPM:          req.BPM,
		Key:          req.Key,
		DAW:          req.DAW,
		DurationBars: req.DurationBars,
		Genre:        req.Genre,
	}

	if err := h.stream.CreateLoopActivity(userID, activity); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to create post"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"post_id":   postID,
		"activity":  activity,
		"timestamp": time.Now().UTC(),
	})
}

// FollowUser follows another user
func (h *Handlers) FollowUser(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		TargetUserID string `json:"target_user_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.stream.FollowUser(userID, req.TargetUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to follow user"})
		return
	}

	// Send real-time WebSocket notification to the target user
	if h.wsHandler != nil {
		// Fetch follower and followee info for the notification
		var follower, followee models.User
		database.DB.Select("id, username, display_name, profile_picture_url").First(&follower, "id = ?", userID)
		database.DB.Select("id, username, display_name").First(&followee, "id = ?", req.TargetUserID)

		// Get updated follower count for the target user
		followerCount := 0
		if stats, err := h.stream.GetFollowStats(req.TargetUserID); err == nil {
			followerCount = stats.FollowerCount
		}

		followerName := follower.DisplayName
		if followerName == "" {
			followerName = follower.Username
		}
		followeeName := followee.DisplayName
		if followeeName == "" {
			followeeName = followee.Username
		}

		h.wsHandler.NotifyFollow(req.TargetUserID, &websocket.FollowPayload{
			FollowerID:     userID,
			FollowerName:   followerName,
			FollowerAvatar: follower.ProfilePictureURL,
			FolloweeID:     req.TargetUserID,
			FolloweeName:   followeeName,
			FollowerCount:  followerCount,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"status":         "following",
		"target_user":    req.TargetUserID,
		"following_user": userID,
	})
}

// UnfollowUser unfollows a user
func (h *Handlers) UnfollowUser(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		TargetUserID string `json:"target_user_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.stream.UnfollowUser(userID, req.TargetUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to unfollow user"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "unfollowed",
		"target_user": req.TargetUserID,
	})
}

// LikePost likes a post with optional emoji
func (h *Handlers) LikePost(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		ActivityID string `json:"activity_id" binding:"required"`
		Emoji      string `json:"emoji,omitempty"` // Optional emoji like ❤️, 🔥, 🎵
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Use emoji reaction if provided, otherwise default like
	if req.Emoji != "" {
		if err := h.stream.AddReactionWithEmoji("like", userID, req.ActivityID, req.Emoji); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to add emoji reaction"})
			return
		}
	} else {
		if err := h.stream.AddReaction("like", userID, req.ActivityID); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to like post"})
			return
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "liked",
		"activity_id": req.ActivityID,
		"user_id":     userID,
		"emoji":       req.Emoji,
	})
}

// UnlikePost removes a like from a post
func (h *Handlers) UnlikePost(c *gin.Context) {
	// In production, you'd need to find the reaction ID first
	c.JSON(http.StatusOK, gin.H{
		"status": "unliked",
	})
}

// GetUserProfile gets a user's profile by ID (public endpoint)
// GET /api/users/:id/profile
func (h *Handlers) GetUserProfile(c *gin.Context) {
	targetUserID := c.Param("id")
	currentUserID := c.GetString("user_id") // May be empty if not authenticated

	// Fetch user from database
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found", "message": "User not found"})
		return
	}

	// Fetch follow stats from Stream.io (source of truth)
	var followStats *stream.FollowStats
	var followStatsErr error
	if h.stream != nil {
		followStats, followStatsErr = h.stream.GetFollowStats(user.StreamUserID)
		if followStatsErr != nil {
			// Log but don't fail - use cached values from DB
			fmt.Printf("Warning: Failed to get follow stats from Stream.io: %v\n", followStatsErr)
		}
	}

	// Use Stream.io stats if available, otherwise fall back to cached DB values
	followerCount := user.FollowerCount
	followingCount := user.FollowingCount
	if followStats != nil {
		followerCount = followStats.FollowerCount
		followingCount = followStats.FollowingCount
		// Optionally update cache in background
		go func() {
			database.DB.Model(&user).Updates(map[string]interface{}{
				"follower_count":  followerCount,
				"following_count": followingCount,
			})
		}()
	}

	// Check if current user follows this profile
	var isFollowing bool
	var isFollowedBy bool
	if currentUserID != "" && currentUserID != user.ID && h.stream != nil {
		isFollowing, _ = h.stream.CheckIsFollowing(currentUserID, user.StreamUserID)
		isFollowedBy, _ = h.stream.CheckIsFollowing(user.StreamUserID, currentUserID)
	}

	// Count posts
	var postCount int64
	database.DB.Model(&models.AudioPost{}).Where("user_id = ? AND is_public = true", user.ID).Count(&postCount)

	c.JSON(http.StatusOK, gin.H{
		"id":                  user.ID,
		"username":            user.Username,
		"display_name":        user.DisplayName,
		"bio":                 user.Bio,
		"location":            user.Location,
		"avatar_url":          user.AvatarURL,
		"profile_picture_url": user.ProfilePictureURL,
		"daw_preference":      user.DAWPreference,
		"genre":               user.Genre,
		"social_links":        user.SocialLinks,
		"follower_count":      followerCount,
		"following_count":     followingCount,
		"post_count":          postCount,
		"is_following":        isFollowing,
		"is_followed_by":      isFollowedBy,
		"created_at":          user.CreatedAt,
	})
}

// GetMyProfile gets the authenticated user's own profile
// GET /api/profile
func (h *Handlers) GetMyProfile(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	// Fetch follow stats from Stream.io
	var followStats *stream.FollowStats
	if h.stream != nil {
		followStats, _ = h.stream.GetFollowStats(currentUser.StreamUserID)
	}

	followerCount := currentUser.FollowerCount
	followingCount := currentUser.FollowingCount
	if followStats != nil {
		followerCount = followStats.FollowerCount
		followingCount = followStats.FollowingCount
	}

	// Count posts
	var postCount int64
	database.DB.Model(&models.AudioPost{}).Where("user_id = ?", currentUser.ID).Count(&postCount)

	c.JSON(http.StatusOK, gin.H{
		"id":                  currentUser.ID,
		"email":               currentUser.Email,
		"username":            currentUser.Username,
		"display_name":        currentUser.DisplayName,
		"bio":                 currentUser.Bio,
		"location":            currentUser.Location,
		"avatar_url":          currentUser.AvatarURL,
		"profile_picture_url": currentUser.ProfilePictureURL,
		"daw_preference":      currentUser.DAWPreference,
		"genre":               currentUser.Genre,
		"social_links":        currentUser.SocialLinks,
		"follower_count":      followerCount,
		"following_count":     followingCount,
		"post_count":          postCount,
		"email_verified":      currentUser.EmailVerified,
		"created_at":          currentUser.CreatedAt,
	})
}

// UpdateMyProfile updates the authenticated user's profile
// PUT /api/profile
func (h *Handlers) UpdateMyProfile(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	var req struct {
		DisplayName       *string             `json:"display_name"`
		Bio               *string             `json:"bio"`
		Location          *string             `json:"location"`
		DAWPreference     *string             `json:"daw_preference"`
		Genre             []string            `json:"genre"`
		SocialLinks       *models.SocialLinks `json:"social_links"`
		ProfilePictureURL *string             `json:"profile_picture_url"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Build update map for non-nil fields
	updates := make(map[string]interface{})
	if req.DisplayName != nil {
		updates["display_name"] = *req.DisplayName
	}
	if req.Bio != nil {
		updates["bio"] = *req.Bio
	}
	if req.Location != nil {
		updates["location"] = *req.Location
	}
	if req.DAWPreference != nil {
		updates["daw_preference"] = *req.DAWPreference
	}
	if req.Genre != nil {
		updates["genre"] = req.Genre
	}
	if req.SocialLinks != nil {
		updates["social_links"] = req.SocialLinks
	}
	if req.ProfilePictureURL != nil {
		updates["profile_picture_url"] = *req.ProfilePictureURL
	}

	if len(updates) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "no_fields_to_update"})
		return
	}

	// Update in database
	if err := database.DB.Model(currentUser).Updates(updates).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "update_failed", "message": err.Error()})
		return
	}

	// Reload user
	database.DB.First(currentUser, "id = ?", currentUser.ID)

	c.JSON(http.StatusOK, gin.H{
		"message": "profile_updated",
		"user": gin.H{
			"id":                  currentUser.ID,
			"display_name":        currentUser.DisplayName,
			"bio":                 currentUser.Bio,
			"location":            currentUser.Location,
			"daw_preference":      currentUser.DAWPreference,
			"genre":               currentUser.Genre,
			"social_links":        currentUser.SocialLinks,
			"profile_picture_url": currentUser.ProfilePictureURL,
		},
		"updated_at": time.Now().UTC(),
	})
}

// ChangeUsername allows users to change their username with uniqueness validation
// PUT /api/v1/users/username
func (h *Handlers) ChangeUsername(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	var req struct {
		Username string `json:"username" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "username_required"})
		return
	}

	newUsername := strings.TrimSpace(req.Username)

	// Validate username format
	if len(newUsername) < 3 {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "username_too_short",
			"message": "Username must be at least 3 characters",
		})
		return
	}

	if len(newUsername) > 30 {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "username_too_long",
			"message": "Username must be at most 30 characters",
		})
		return
	}

	// Only allow alphanumeric, underscores, and hyphens
	validUsername := regexp.MustCompile(`^[a-zA-Z0-9_-]+$`)
	if !validUsername.MatchString(newUsername) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "username_invalid_chars",
			"message": "Username can only contain letters, numbers, underscores, and hyphens",
		})
		return
	}

	// Check if username is unchanged
	if strings.EqualFold(newUsername, currentUser.Username) {
		c.JSON(http.StatusOK, gin.H{
			"message":  "username_unchanged",
			"username": currentUser.Username,
		})
		return
	}

	// Check uniqueness (case-insensitive)
	var existingUser models.User
	err := database.DB.Where("LOWER(username) = LOWER(?) AND id != ?", newUsername, currentUser.ID).First(&existingUser).Error
	if err == nil {
		// User found with this username
		c.JSON(http.StatusConflict, gin.H{
			"error":   "username_taken",
			"message": "This username is already taken",
		})
		return
	}
	if err != gorm.ErrRecordNotFound {
		// Database error
		c.JSON(http.StatusInternalServerError, gin.H{"error": "database_error"})
		return
	}

	// Update username
	oldUsername := currentUser.Username
	if err := database.DB.Model(currentUser).Update("username", newUsername).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "update_failed",
			"message": err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":      "username_changed",
		"username":     newUsername,
		"old_username": oldUsername,
		"updated_at":   time.Now().UTC(),
	})
}

// GetUserFollowers gets the list of users who follow a user
// GET /api/users/:id/followers
func (h *Handlers) GetUserFollowers(c *gin.Context) {
	targetUserID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Validate user exists
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Get followers from Stream.io
	followers, err := h.stream.GetFollowers(user.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_followers", "message": err.Error()})
		return
	}

	// Enrich with user data from database
	followerUsers := make([]gin.H, 0, len(followers))
	for _, f := range followers {
		var followerUser models.User
		if err := database.DB.First(&followerUser, "id = ? OR stream_user_id = ?", f.UserID, f.UserID).Error; err == nil {
			followerUsers = append(followerUsers, gin.H{
				"id":           followerUser.ID,
				"username":     followerUser.Username,
				"display_name": followerUser.DisplayName,
				"avatar_url":   followerUser.AvatarURL,
				"bio":          followerUser.Bio,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"followers": followerUsers,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(followerUsers),
		},
	})
}

// GetUserFollowing gets the list of users a user follows
// GET /api/users/:id/following
func (h *Handlers) GetUserFollowing(c *gin.Context) {
	targetUserID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Validate user exists
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Get following from Stream.io
	following, err := h.stream.GetFollowing(user.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_following", "message": err.Error()})
		return
	}

	// Enrich with user data from database
	followingUsers := make([]gin.H, 0, len(following))
	for _, f := range following {
		var followedUser models.User
		if err := database.DB.First(&followedUser, "id = ? OR stream_user_id = ?", f.UserID, f.UserID).Error; err == nil {
			followingUsers = append(followingUsers, gin.H{
				"id":           followedUser.ID,
				"username":     followedUser.Username,
				"display_name": followedUser.DisplayName,
				"avatar_url":   followedUser.AvatarURL,
				"bio":          followedUser.Bio,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"following": followingUsers,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(followingUsers),
		},
	})
}

// GetUserPosts gets a user's posts with enriched data (likes, etc.)
// GET /api/users/:id/posts
func (h *Handlers) GetUserPosts(c *gin.Context) {
	targetUserID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Validate user exists
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Get enriched activities from Stream.io
	activities, err := h.stream.GetEnrichedUserFeed(user.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_posts", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"posts": activities,
		"user": gin.H{
			"id":           user.ID,
			"username":     user.Username,
			"display_name": user.DisplayName,
			"avatar_url":   user.AvatarURL,
		},
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
		},
	})
}

// GetEnrichedTimeline gets the user's timeline with reaction counts
// GET /api/feed/timeline/enriched
func (h *Handlers) GetEnrichedTimeline(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	activities, err := h.stream.GetEnrichedTimeline(currentUser.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_timeline", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
		},
	})
}

// GetEnrichedGlobalFeed gets the global feed with reaction counts
// GET /api/feed/global/enriched
func (h *Handlers) GetEnrichedGlobalFeed(c *gin.Context) {
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	activities, err := h.stream.GetEnrichedGlobalFeed(limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_global_feed", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
		},
	})
}

// FollowUserByID follows a user by ID (path param version)
// POST /api/users/:id/follow
func (h *Handlers) FollowUserByID(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)
	targetUserID := c.Param("id")

	// Validate target user exists
	var targetUser models.User
	if err := database.DB.First(&targetUser, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Can't follow yourself
	if currentUser.ID == targetUser.ID {
		c.JSON(http.StatusBadRequest, gin.H{"error": "cannot_follow_self"})
		return
	}

	// Follow via Stream.io
	if err := h.stream.FollowUser(currentUser.StreamUserID, targetUser.StreamUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "follow_failed", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "following",
		"target_user": targetUser.ID,
		"username":    targetUser.Username,
	})
}

// UnfollowUserByID unfollows a user by ID (path param version)
// DELETE /api/users/:id/follow
func (h *Handlers) UnfollowUserByID(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)
	targetUserID := c.Param("id")

	// Validate target user exists
	var targetUser models.User
	if err := database.DB.First(&targetUser, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Unfollow via Stream.io
	if err := h.stream.UnfollowUser(currentUser.StreamUserID, targetUser.StreamUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "unfollow_failed", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "unfollowed",
		"target_user": targetUser.ID,
	})
}

// Legacy placeholder handlers for backwards compatibility
// GetProfile gets user profile information (deprecated - use GetMyProfile)
func (h *Handlers) GetProfile(c *gin.Context) {
	h.GetMyProfile(c)
}

// UpdateProfile updates user profile information (deprecated - use UpdateMyProfile)
func (h *Handlers) UpdateProfile(c *gin.Context) {
	h.UpdateMyProfile(c)
}

// =============================================================================
// NOTIFICATION ENDPOINTS
// =============================================================================

// GetNotifications gets the user's notifications with unseen/unread counts
// GET /api/notifications
func (h *Handlers) GetNotifications(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	notifs, err := h.stream.GetNotifications(currentUser.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_notifications", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"groups": notifs.Groups,
		"unseen": notifs.Unseen,
		"unread": notifs.Unread,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(notifs.Groups),
		},
	})
}

// GetNotificationCounts gets just the unseen/unread counts for badge display
// GET /api/notifications/counts
func (h *Handlers) GetNotificationCounts(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	unseen, unread, err := h.stream.GetNotificationCounts(currentUser.StreamUserID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_notification_counts", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"unseen": unseen,
		"unread": unread,
	})
}

// MarkNotificationsRead marks all notifications as read
// POST /api/notifications/read
func (h *Handlers) MarkNotificationsRead(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	if err := h.stream.MarkNotificationsRead(currentUser.StreamUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_mark_read", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "success",
		"message": "all_notifications_marked_read",
	})
}

// MarkNotificationsSeen marks all notifications as seen (clears badge)
// POST /api/notifications/seen
func (h *Handlers) MarkNotificationsSeen(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	if err := h.stream.MarkNotificationsSeen(currentUser.StreamUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_mark_seen", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "success",
		"message": "all_notifications_marked_seen",
	})
}

// =============================================================================
// AGGREGATED FEED ENDPOINTS
// =============================================================================

// GetAggregatedTimeline gets the user's aggregated timeline (grouped by user+day)
// GET /api/feed/timeline/aggregated
func (h *Handlers) GetAggregatedTimeline(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	resp, err := h.stream.GetAggregatedTimeline(currentUser.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_aggregated_timeline", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"groups": resp.Groups,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(resp.Groups),
		},
	})
}

// GetTrendingFeed gets trending loops - returns flat activities for the plugin feed
// Activities are extracted from aggregated groups and sorted by engagement score
// GET /api/feed/trending
func (h *Handlers) GetTrendingFeed(c *gin.Context) {
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	resp, err := h.stream.GetTrendingFeed(limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_trending_feed", "message": err.Error()})
		return
	}

	// Flatten groups into activities for the plugin's feed display
	// Each group contains activities - we extract them for flat display
	var activities []*stream.Activity
	for _, group := range resp.Groups {
		activities = append(activities, group.Activities...)
	}

	// Limit to requested amount after flattening
	if len(activities) > limit {
		activities = activities[:limit]
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
		},
	})
}

// GetTrendingFeedGrouped gets trending loops grouped by genre/time
// Returns aggregated groups for advanced display ("5 new electronic loops this week")
// GET /api/feed/trending/grouped
func (h *Handlers) GetTrendingFeedGrouped(c *gin.Context) {
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	resp, err := h.stream.GetTrendingFeed(limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_trending_feed", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"groups": resp.Groups,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(resp.Groups),
		},
	})
}

// GetUserActivitySummary gets aggregated activity for a user's profile
// GET /api/users/:id/activity
func (h *Handlers) GetUserActivitySummary(c *gin.Context) {
	targetUserID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "10"))

	// Validate user exists
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	resp, err := h.stream.GetUserActivitySummary(user.StreamUserID, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_activity_summary", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"user_id": user.ID,
		"groups":  resp.Groups,
		"meta": gin.H{
			"limit": limit,
			"count": len(resp.Groups),
		},
	})
}

// =============================================================================
// LEGACY EMOJI/REACTION HANDLERS
// =============================================================================

// EmojiReact adds an emoji reaction to a post or message
func (h *Handlers) EmojiReact(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		ActivityID string `json:"activity_id" binding:"required"`
		Emoji      string `json:"emoji" binding:"required"` // Required emoji like 🎵, ❤️, 🔥, 😍, 🚀, 💯
		Type       string `json:"type,omitempty"`           // Optional: "like", "love", "fire", etc.
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Default reaction type based on emoji or use provided type
	reactionType := req.Type
	if reactionType == "" {
		// Map common emojis to reaction types
		switch req.Emoji {
		case "❤️", "💕", "💖":
			reactionType = "love"
		case "🔥":
			reactionType = "fire"
		case "🎵", "🎶":
			reactionType = "music"
		case "😍":
			reactionType = "wow"
		case "🚀":
			reactionType = "hype"
		case "💯":
			reactionType = "perfect"
		default:
			reactionType = "react"
		}
	}

	// Add the emoji reaction
	if err := h.stream.AddReactionWithEmoji(reactionType, userID, req.ActivityID, req.Emoji); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to add emoji reaction"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "reacted",
		"activity_id": req.ActivityID,
		"user_id":     userID,
		"emoji":       req.Emoji,
		"type":        reactionType,
		"timestamp":   time.Now().UTC(),
	})
}

// =============================================================================
// USER DISCOVERY & SEARCH ENDPOINTS
// =============================================================================

// SearchUsers searches for users by username or display name
// GET /api/search/users?q=query&limit=20&offset=0
func (h *Handlers) SearchUsers(c *gin.Context) {
	query := c.Query("q")
	if query == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "search_query_required"})
		return
	}

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Sanitize and prepare search pattern
	searchPattern := "%" + query + "%"

	var users []models.User
	result := database.DB.Where(
		"username ILIKE ? OR display_name ILIKE ?",
		searchPattern, searchPattern,
	).Order("follower_count DESC").Limit(limit).Offset(offset).Find(&users)

	if result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "search_failed", "message": result.Error.Error()})
		return
	}

	// Convert to response format
	userResults := make([]gin.H, 0, len(users))
	for _, user := range users {
		userResults = append(userResults, gin.H{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.AvatarURL,
			"bio":            user.Bio,
			"follower_count": user.FollowerCount,
			"genre":          user.Genre,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"meta": gin.H{
			"query":  query,
			"limit":  limit,
			"offset": offset,
			"count":  len(userResults),
		},
	})
}

// GetTrendingUsers returns users trending based on recent activity
// GET /api/discover/trending?limit=20&period=week
func (h *Handlers) GetTrendingUsers(c *gin.Context) {
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	period := c.DefaultQuery("period", "week")

	// Determine time window based on period
	var since time.Time
	switch period {
	case "day":
		since = time.Now().AddDate(0, 0, -1)
	case "week":
		since = time.Now().AddDate(0, 0, -7)
	case "month":
		since = time.Now().AddDate(0, -1, 0)
	default:
		since = time.Now().AddDate(0, 0, -7)
	}

	// Find users with most activity (posts) in the time period
	// Also factor in follower growth and engagement
	// Use distinct alias 'recent_post_count' to avoid conflict with users.post_count column
	var users []models.User
	result := database.DB.
		Select("users.*, COUNT(audio_posts.id) as recent_post_count").
		Joins("LEFT JOIN audio_posts ON audio_posts.user_id = users.id AND audio_posts.created_at > ?", since).
		Group("users.id").
		Order("recent_post_count DESC, users.follower_count DESC").
		Limit(limit).
		Find(&users)

	if result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_trending_users", "message": result.Error.Error()})
		return
	}

	userResults := make([]gin.H, 0, len(users))
	for _, user := range users {
		userResults = append(userResults, gin.H{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.AvatarURL,
			"bio":            user.Bio,
			"follower_count": user.FollowerCount,
			"post_count":     user.PostCount,
			"genre":          user.Genre,
			"daw_preference": user.DAWPreference,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users":  userResults,
		"period": period,
		"meta": gin.H{
			"limit": limit,
			"count": len(userResults),
		},
	})
}

// GetFeaturedProducers returns curated/featured producers
// GET /api/discover/featured?limit=10
func (h *Handlers) GetFeaturedProducers(c *gin.Context) {
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "10"))

	// Featured producers: high follower count + active recently + has posts
	var users []models.User
	oneWeekAgo := time.Now().AddDate(0, 0, -7)

	result := database.DB.
		Where("follower_count > ? AND last_active_at > ? AND post_count > ?", 10, oneWeekAgo, 0).
		Order("follower_count DESC").
		Limit(limit).
		Find(&users)

	if result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_featured", "message": result.Error.Error()})
		return
	}

	// If not enough users meet criteria, fall back to most followed
	if len(users) < limit {
		database.DB.
			Where("post_count > ?", 0).
			Order("follower_count DESC").
			Limit(limit).
			Find(&users)
	}

	userResults := make([]gin.H, 0, len(users))
	for _, user := range users {
		userResults = append(userResults, gin.H{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.AvatarURL,
			"bio":            user.Bio,
			"follower_count": user.FollowerCount,
			"post_count":     user.PostCount,
			"genre":          user.Genre,
			"daw_preference": user.DAWPreference,
			"featured":       true,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"meta": gin.H{
			"limit": limit,
			"count": len(userResults),
		},
	})
}

// GetUsersByGenre returns users who produce in a specific genre
// GET /api/discover/genre/:genre?limit=20&offset=0
func (h *Handlers) GetUsersByGenre(c *gin.Context) {
	genre := c.Param("genre")
	if genre == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "genre_required"})
		return
	}

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Find users with this genre in their genre array
	var users []models.User
	result := database.DB.
		Where("? = ANY(genre)", genre).
		Order("follower_count DESC").
		Limit(limit).
		Offset(offset).
		Find(&users)

	if result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_users_by_genre", "message": result.Error.Error()})
		return
	}

	userResults := make([]gin.H, 0, len(users))
	for _, user := range users {
		userResults = append(userResults, gin.H{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.AvatarURL,
			"bio":            user.Bio,
			"follower_count": user.FollowerCount,
			"genre":          user.Genre,
			"daw_preference": user.DAWPreference,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"genre": genre,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(userResults),
		},
	})
}

// GetSuggestedUsers returns personalized user suggestions based on who you follow
// GET /api/discover/suggested?limit=10
func (h *Handlers) GetSuggestedUsers(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "10"))

	// Get who the current user follows
	following, err := h.stream.GetFollowing(currentUser.StreamUserID, 100, 0)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_following", "message": err.Error()})
		return
	}

	// Extract followed user IDs
	followedIDs := make([]string, 0, len(following))
	for _, f := range following {
		followedIDs = append(followedIDs, f.UserID)
	}

	// Find users in similar genres who the user doesn't already follow
	var suggestedUsers []models.User

	if len(currentUser.Genre) > 0 {
		// Users with overlapping genres, not already followed, not self
		query := database.DB.
			Where("id != ? AND id NOT IN ?", currentUser.ID, append(followedIDs, currentUser.ID))

		// Build genre overlap condition
		for _, g := range currentUser.Genre {
			query = query.Or("? = ANY(genre)", g)
		}

		query.Order("follower_count DESC").Limit(limit).Find(&suggestedUsers)
	}

	// If not enough suggestions, add popular users
	if len(suggestedUsers) < limit {
		remaining := limit - len(suggestedUsers)
		var popularUsers []models.User

		excludeIDs := append(followedIDs, currentUser.ID)
		for _, u := range suggestedUsers {
			excludeIDs = append(excludeIDs, u.ID)
		}

		database.DB.
			Where("id NOT IN ?", excludeIDs).
			Order("follower_count DESC").
			Limit(remaining).
			Find(&popularUsers)

		suggestedUsers = append(suggestedUsers, popularUsers...)
	}

	userResults := make([]gin.H, 0, len(suggestedUsers))
	for _, u := range suggestedUsers {
		// Calculate shared genres
		sharedGenres := []string{}
		for _, g1 := range currentUser.Genre {
			for _, g2 := range u.Genre {
				if g1 == g2 {
					sharedGenres = append(sharedGenres, g1)
				}
			}
		}

		userResults = append(userResults, gin.H{
			"id":             u.ID,
			"username":       u.Username,
			"display_name":   u.DisplayName,
			"avatar_url":     u.AvatarURL,
			"bio":            u.Bio,
			"follower_count": u.FollowerCount,
			"genre":          u.Genre,
			"shared_genres":  sharedGenres,
			"daw_preference": u.DAWPreference,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"meta": gin.H{
			"limit": limit,
			"count": len(userResults),
		},
	})
}

// GetSimilarUsers returns users with similar BPM and key preferences
// GET /api/users/:id/similar?limit=10
func (h *Handlers) GetSimilarUsers(c *gin.Context) {
	targetUserID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "10"))

	// Get target user
	var targetUser models.User
	if err := database.DB.First(&targetUser, "id = ?", targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Get target user's posts to analyze their BPM/key preferences
	var targetPosts []models.AudioPost
	database.DB.Where("user_id = ?", targetUserID).Limit(20).Find(&targetPosts)

	if len(targetPosts) == 0 {
		// If no posts, fall back to genre-based similarity
		c.JSON(http.StatusOK, gin.H{
			"users":  []gin.H{},
			"reason": "no_posts_to_analyze",
			"meta": gin.H{
				"limit": limit,
				"count": 0,
			},
		})
		return
	}

	// Calculate average BPM and most common keys
	var totalBPM int
	keyCount := make(map[string]int)
	for _, post := range targetPosts {
		totalBPM += post.BPM
		if post.Key != "" {
			keyCount[post.Key]++
		}
	}
	avgBPM := totalBPM / len(targetPosts)

	// Find the most common key
	var commonKey string
	maxKeyCount := 0
	for key, count := range keyCount {
		if count > maxKeyCount {
			commonKey = key
			maxKeyCount = count
		}
	}

	// Find users with similar posts (within 20 BPM range and/or same key)
	var similarUserIDs []string
	bpmRange := 20

	subQuery := database.DB.Model(&models.AudioPost{}).
		Select("DISTINCT user_id").
		Where("user_id != ?", targetUserID).
		Where("(bpm BETWEEN ? AND ?) OR key = ?", avgBPM-bpmRange, avgBPM+bpmRange, commonKey)

	subQuery.Pluck("user_id", &similarUserIDs)

	if len(similarUserIDs) == 0 {
		c.JSON(http.StatusOK, gin.H{
			"users":        []gin.H{},
			"analyzed_bpm": avgBPM,
			"analyzed_key": commonKey,
			"reason":       "no_similar_users_found",
			"meta": gin.H{
				"limit": limit,
				"count": 0,
			},
		})
		return
	}

	// Get user details
	var similarUsers []models.User
	database.DB.
		Where("id IN ?", similarUserIDs).
		Order("follower_count DESC").
		Limit(limit).
		Find(&similarUsers)

	userResults := make([]gin.H, 0, len(similarUsers))
	for _, u := range similarUsers {
		userResults = append(userResults, gin.H{
			"id":             u.ID,
			"username":       u.Username,
			"display_name":   u.DisplayName,
			"avatar_url":     u.AvatarURL,
			"bio":            u.Bio,
			"follower_count": u.FollowerCount,
			"genre":          u.Genre,
			"daw_preference": u.DAWPreference,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users":        userResults,
		"analyzed_bpm": avgBPM,
		"analyzed_key": commonKey,
		"meta": gin.H{
			"limit": limit,
			"count": len(userResults),
		},
	})
}

// GetAvailableGenres returns all genres with user counts
// GET /api/discover/genres
func (h *Handlers) GetAvailableGenres(c *gin.Context) {
	// Get unique genres and their counts using unnest for PostgreSQL array
	type GenreCount struct {
		Genre string `json:"genre"`
		Count int    `json:"count"`
	}

	var genreCounts []GenreCount
	result := database.DB.Raw(`
		SELECT unnest(genre) as genre, COUNT(*) as count
		FROM users
		WHERE genre IS NOT NULL AND array_length(genre, 1) > 0
		GROUP BY unnest(genre)
		ORDER BY count DESC
	`).Scan(&genreCounts)

	if result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_genres", "message": result.Error.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"genres": genreCounts,
		"meta": gin.H{
			"count": len(genreCounts),
		},
	})
}

//==============================================================================
// Comment Handlers
//==============================================================================

// CreateComment creates a new comment on a post
// POST /api/v1/posts/:id/comments
func (h *Handlers) CreateComment(c *gin.Context) {
	postID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var req struct {
		Content  string  `json:"content" binding:"required,min=1,max=2000"`
		ParentID *string `json:"parent_id,omitempty"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	// Verify the post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	// If replying to a comment, verify the parent exists and belongs to the same post
	if req.ParentID != nil && *req.ParentID != "" {
		var parentComment models.Comment
		if err := database.DB.First(&parentComment, "id = ? AND post_id = ?", *req.ParentID, postID).Error; err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "parent_comment_not_found"})
			return
		}
		// Only allow 1 level of nesting - if parent has a parent, use the parent's parent
		if parentComment.ParentID != nil {
			req.ParentID = parentComment.ParentID
		}
	}

	// Create the comment
	comment := models.Comment{
		PostID:   postID,
		UserID:   userID.(string),
		Content:  req.Content,
		ParentID: req.ParentID,
	}

	if err := database.DB.Create(&comment).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_create_comment", "message": err.Error()})
		return
	}

	// Increment post comment count
	database.DB.Model(&post).UpdateColumn("comment_count", gorm.Expr("comment_count + 1"))

	// Load the user for response
	database.DB.Preload("User").First(&comment, "id = ?", comment.ID)

	// Extract mentions and create notifications
	mentions := extractMentions(req.Content)
	go h.processMentions(comment.ID, mentions, userID.(string), postID)

	// Notify post owner (if not commenting on own post)
	if post.UserID != userID.(string) {
		go h.notifyCommentOnPost(comment, post)
	}

	c.JSON(http.StatusCreated, gin.H{
		"comment": comment,
	})
}

// GetComments retrieves comments for a post with pagination
// GET /api/v1/posts/:id/comments
func (h *Handlers) GetComments(c *gin.Context) {
	postID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))
	parentID := c.Query("parent_id") // Optional: get replies to specific comment

	if limit > 100 {
		limit = 100
	}

	// Verify the post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	var comments []models.Comment
	query := database.DB.
		Preload("User").
		Where("post_id = ? AND is_deleted = false", postID).
		Order("created_at DESC").
		Limit(limit).
		Offset(offset)

	if parentID != "" {
		// Get replies to a specific comment
		query = query.Where("parent_id = ?", parentID)
	} else {
		// Get top-level comments only (no parent)
		query = query.Where("parent_id IS NULL")
	}

	if err := query.Find(&comments).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_comments", "message": err.Error()})
		return
	}

	// Get total count for pagination
	var total int64
	countQuery := database.DB.Model(&models.Comment{}).Where("post_id = ? AND is_deleted = false", postID)
	if parentID != "" {
		countQuery = countQuery.Where("parent_id = ?", parentID)
	} else {
		countQuery = countQuery.Where("parent_id IS NULL")
	}
	countQuery.Count(&total)

	// For top-level comments, also get reply counts
	if parentID == "" {
		for i := range comments {
			var replyCount int64
			database.DB.Model(&models.Comment{}).
				Where("parent_id = ? AND is_deleted = false", comments[i].ID).
				Count(&replyCount)
			// We can't add to struct, but we'll include in response below
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"comments": comments,
		"meta": gin.H{
			"total":  total,
			"limit":  limit,
			"offset": offset,
		},
	})
}

// GetCommentReplies retrieves replies to a specific comment
// GET /api/v1/comments/:id/replies
func (h *Handlers) GetCommentReplies(c *gin.Context) {
	commentID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	if limit > 100 {
		limit = 100
	}

	// Verify the comment exists
	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	var replies []models.Comment
	err := database.DB.
		Preload("User").
		Where("parent_id = ? AND is_deleted = false", commentID).
		Order("created_at ASC").
		Limit(limit).
		Offset(offset).
		Find(&replies).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_replies", "message": err.Error()})
		return
	}

	var total int64
	database.DB.Model(&models.Comment{}).Where("parent_id = ? AND is_deleted = false", commentID).Count(&total)

	c.JSON(http.StatusOK, gin.H{
		"replies": replies,
		"meta": gin.H{
			"total":  total,
			"limit":  limit,
			"offset": offset,
		},
	})
}

// UpdateComment updates a comment (only within 5 minutes of creation)
// PUT /api/v1/comments/:id
func (h *Handlers) UpdateComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var req struct {
		Content string `json:"content" binding:"required,min=1,max=2000"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	// Check ownership
	if comment.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "not_comment_owner"})
		return
	}

	// Check if comment was deleted
	if comment.IsDeleted {
		c.JSON(http.StatusBadRequest, gin.H{"error": "comment_deleted"})
		return
	}

	// Check 5-minute edit window
	editWindow := 5 * time.Minute
	if time.Since(comment.CreatedAt) > editWindow {
		c.JSON(http.StatusForbidden, gin.H{
			"error":   "edit_window_expired",
			"message": "Comments can only be edited within 5 minutes of creation",
		})
		return
	}

	// Update the comment
	now := time.Now()
	comment.Content = req.Content
	comment.IsEdited = true
	comment.EditedAt = &now

	if err := database.DB.Save(&comment).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_update_comment", "message": err.Error()})
		return
	}

	// Reload with user
	database.DB.Preload("User").First(&comment, "id = ?", comment.ID)

	c.JSON(http.StatusOK, gin.H{
		"comment": comment,
	})
}

// DeleteComment soft-deletes a comment
// DELETE /api/v1/comments/:id
func (h *Handlers) DeleteComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	// Check ownership
	if comment.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "not_comment_owner"})
		return
	}

	// Soft delete - mark as deleted but keep for threading
	comment.IsDeleted = true
	comment.Content = "[Comment deleted]"

	if err := database.DB.Save(&comment).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_delete_comment", "message": err.Error()})
		return
	}

	// Decrement post comment count
	database.DB.Model(&models.AudioPost{}).Where("id = ?", comment.PostID).UpdateColumn("comment_count", gorm.Expr("GREATEST(comment_count - 1, 0)"))

	c.JSON(http.StatusOK, gin.H{
		"message": "comment_deleted",
	})
}

// LikeComment adds a like to a comment
// POST /api/v1/comments/:id/like
func (h *Handlers) LikeComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	if comment.IsDeleted {
		c.JSON(http.StatusBadRequest, gin.H{"error": "cannot_like_deleted_comment"})
		return
	}

	// Add reaction via Stream.io if comment has a stream activity ID
	if h.stream != nil && comment.StreamActivityID != "" {
		err := h.stream.AddReactionWithEmoji("like", userID.(string), comment.StreamActivityID, "")
		if err != nil {
			// Log but don't fail - update local count anyway
			fmt.Printf("Failed to add Stream.io reaction: %v\n", err)
		}
	}

	// Increment like count
	database.DB.Model(&comment).UpdateColumn("like_count", gorm.Expr("like_count + 1"))

	c.JSON(http.StatusOK, gin.H{
		"message":    "comment_liked",
		"like_count": comment.LikeCount + 1,
	})
}

// UnlikeComment removes a like from a comment
// DELETE /api/v1/comments/:id/like
func (h *Handlers) UnlikeComment(c *gin.Context) {
	commentID := c.Param("id")
	_, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	// Decrement like count (don't go below 0)
	database.DB.Model(&comment).UpdateColumn("like_count", gorm.Expr("GREATEST(like_count - 1, 0)"))

	newCount := comment.LikeCount - 1
	if newCount < 0 {
		newCount = 0
	}

	c.JSON(http.StatusOK, gin.H{
		"message":    "comment_unliked",
		"like_count": newCount,
	})
}

//==============================================================================
// Comment Helper Functions
//==============================================================================

// extractMentions extracts @username mentions from comment content
func extractMentions(content string) []string {
	var mentions []string
	words := strings.Fields(content)
	seen := make(map[string]bool)

	for _, word := range words {
		if strings.HasPrefix(word, "@") && len(word) > 1 {
			// Clean the username (remove trailing punctuation)
			username := strings.TrimPrefix(word, "@")
			username = strings.TrimRight(username, ".,!?;:")
			username = strings.ToLower(username)

			if !seen[username] && len(username) >= 3 && len(username) <= 30 {
				seen[username] = true
				mentions = append(mentions, username)
			}
		}
	}
	return mentions
}

// processMentions creates mention records and sends notifications
func (h *Handlers) processMentions(commentID string, usernames []string, authorID string, postID string) {
	if len(usernames) == 0 {
		return
	}

	for _, username := range usernames {
		var user models.User
		if err := database.DB.Where("LOWER(username) = ?", username).First(&user).Error; err != nil {
			continue // User doesn't exist, skip
		}

		// Don't notify yourself
		if user.ID == authorID {
			continue
		}

		// Create mention record
		mention := models.CommentMention{
			CommentID:       commentID,
			MentionedUserID: user.ID,
		}
		database.DB.Create(&mention)

		// Send notification via Stream.io
		if h.stream != nil {
			h.stream.NotifyMention(authorID, user.ID, postID, commentID)
		}

		// Mark notification as sent
		mention.NotificationSent = true
		database.DB.Save(&mention)
	}
}

// notifyCommentOnPost sends a notification to the post owner when someone comments
func (h *Handlers) notifyCommentOnPost(comment models.Comment, post models.AudioPost) {
	if h.stream == nil {
		return
	}

	// Get commenter info for the notification
	var commenter models.User
	database.DB.First(&commenter, "id = ?", comment.UserID)

	// NotifyComment(actorUserID, targetUserID, loopID, commentText)
	// Pass a preview of the comment content for the notification
	h.stream.NotifyComment(comment.UserID, post.UserID, post.ID, comment.Content)
}
