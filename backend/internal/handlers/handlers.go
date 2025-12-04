package handlers

import (
	"fmt"
	"mime/multipart"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/audio"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

// Handlers contains all HTTP handlers for the API
type Handlers struct {
	stream         *stream.Client
	audioProcessor *audio.Processor
}

// NewHandlers creates a new handlers instance
func NewHandlers(streamClient *stream.Client, audioProcessor *audio.Processor) *Handlers {
	return &Handlers{
		stream:         streamClient,
		audioProcessor: audioProcessor,
	}
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
		DisplayName   *string              `json:"display_name"`
		Bio           *string              `json:"bio"`
		Location      *string              `json:"location"`
		DAWPreference *string              `json:"daw_preference"`
		Genre         []string             `json:"genre"`
		SocialLinks   *models.SocialLinks  `json:"social_links"`
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
			"id":             currentUser.ID,
			"display_name":   currentUser.DisplayName,
			"bio":            currentUser.Bio,
			"location":       currentUser.Location,
			"daw_preference": currentUser.DAWPreference,
			"genre":          currentUser.Genre,
			"social_links":   currentUser.SocialLinks,
		},
		"updated_at": time.Now().UTC(),
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

// GetTrendingFeed gets trending loops grouped by genre/time
// GET /api/feed/trending
func (h *Handlers) GetTrendingFeed(c *gin.Context) {
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
