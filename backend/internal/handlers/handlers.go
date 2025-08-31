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

	// Submit to background processing queue
	job, err := h.audioProcessor.SubmitProcessingJob(currentUser.ID, tempFilePath, file.Filename, metadata)
	if err != nil {
		os.Remove(tempFilePath)
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "queue_failed",
			"message": "Failed to queue audio for processing",
		})
		return
	}

	// Create pending audio post in database
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
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "database_failed",
			"message": "Failed to save audio post",
		})
		return
	}

	// Return immediate response - processing happens in background
	c.JSON(http.StatusAccepted, gin.H{
		"message":    "Audio upload received - processing in background",
		"post_id":    audioPost.ID,
		"job_id":     job.ID,
		"status":     "processing",
		"eta_seconds": 10, // Estimate 10 seconds for processing
		"poll_url":   fmt.Sprintf("/api/v1/audio/status/%s", job.ID),
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
		"id":        audioID,
		"url":       "https://cdn.sidechain.app/audio/" + audioID + ".mp3",
		"duration":  32.5,
		"waveform":  "data:image/svg+xml;base64,PHN2Zz48L3N2Zz4=",
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
		"status":        "following",
		"target_user":   req.TargetUserID,
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

// GetProfile gets user profile information
func (h *Handlers) GetProfile(c *gin.Context) {
	userID := c.GetString("user_id")
	
	c.JSON(http.StatusOK, gin.H{
		"user_id":   userID,
		"username":  "producer_" + userID,
		"followers": 42,
		"following": 17,
		"posts":     23,
		"bio":       "Making beats at 3am",
	})
}

// UpdateProfile updates user profile information
func (h *Handlers) UpdateProfile(c *gin.Context) {
	userID := c.GetString("user_id")
	
	var req struct {
		Username string `json:"username"`
		Bio      string `json:"bio"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// In production, update database
	c.JSON(http.StatusOK, gin.H{
		"user_id":  userID,
		"username": req.Username,
		"bio":      req.Bio,
		"updated":  time.Now().UTC(),
	})
}

// EmojiReact adds an emoji reaction to a post or message
func (h *Handlers) EmojiReact(c *gin.Context) {
	userID := c.GetString("user_id")
	
	var req struct {
		ActivityID string `json:"activity_id" binding:"required"`
		Emoji      string `json:"emoji" binding:"required"`          // Required emoji like 🎵, ❤️, 🔥, 😍, 🚀, 💯
		Type       string `json:"type,omitempty"`                   // Optional: "like", "love", "fire", etc.
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