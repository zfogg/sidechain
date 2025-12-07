package handlers

import (
	"fmt"
	"net/http"
	"path/filepath"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"gorm.io/gorm"
)

// TODO: Phase 4.5.1.1 - Test GetTimeline - authenticated user, pagination, empty feed
// TODO: Phase 4.5.1.2 - Test GetGlobalFeed - pagination, ordering
// TODO: Phase 4.5.1.3 - Test CreatePost - valid post, missing fields, audio URL validation
// TODO: Phase 4.5.1.28 - Test GetAggregatedTimeline - grouping
// TODO: Phase 4.5.1.29 - Test GetTrendingFeed - trending algorithm

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
		// MIDI data (R.3.3 Cross-DAW MIDI Collaboration)
		MIDIData *models.MIDIData `json:"midi_data,omitempty"` // Optional inline MIDI data
		MIDIID   string           `json:"midi_id,omitempty"`   // Optional existing MIDI pattern ID
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	postID := uuid.New().String()

	// Handle MIDI data - either use existing pattern or create new one
	var midiPatternID *string
	if req.MIDIID != "" {
		// Verify the MIDI pattern exists and user has access
		var pattern models.MIDIPattern
		if err := database.DB.First(&pattern, "id = ?", req.MIDIID).Error; err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_midi_id"})
			return
		}
		// Check access - must be public or owned by user
		if !pattern.IsPublic && pattern.UserID != userID {
			c.JSON(http.StatusForbidden, gin.H{"error": "midi_access_denied"})
			return
		}
		midiPatternID = &req.MIDIID
	} else if req.MIDIData != nil && len(req.MIDIData.Events) > 0 {
		// Create new MIDI pattern from inline data
		pattern := &models.MIDIPattern{
			UserID:        userID,
			Name:          "MIDI from post",
			Events:        req.MIDIData.Events,
			Tempo:         req.MIDIData.Tempo,
			TimeSignature: req.MIDIData.TimeSignature,
			IsPublic:      true, // Public by default when attached to post
		}
		if err := database.DB.Create(pattern).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_create_midi"})
			return
		}
		midiPatternID = &pattern.ID
	}

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

	// Add MIDI info to activity extra data
	if midiPatternID != nil {
		activity.Extra = map[string]interface{}{
			"has_midi":        true,
			"midi_pattern_id": *midiPatternID,
		}
	}

	if err := h.stream.CreateLoopActivity(userID, activity); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to create post"})
		return
	}

	response := gin.H{
		"post_id":   postID,
		"activity":  activity,
		"timestamp": time.Now().UTC(),
	}
	if midiPatternID != nil {
		response["midi_pattern_id"] = *midiPatternID
		response["has_midi"] = true
	}

	c.JSON(http.StatusCreated, response)
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

// DeletePost deletes a post (soft delete)
// DELETE /api/v1/posts/:id
func (h *Handlers) DeletePost(c *gin.Context) {
	postID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	// Find the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	// Check ownership
	if post.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "not_post_owner"})
		return
	}

	// Soft delete the post
	if err := database.DB.Delete(&post).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_delete_post", "message": err.Error()})
		return
	}

	// Delete from Stream.io if it has a stream activity ID
	// Remove from origin feed (user feed) - this will cascade to all other feeds
	if h.stream != nil && post.StreamActivityID != "" {
		if err := h.stream.DeleteLoopActivity(post.UserID, post.StreamActivityID); err != nil {
			// Log but don't fail - post is already deleted in database
			fmt.Printf("Failed to delete Stream.io activity: %v\n", err)
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "post_deleted",
	})
}

// ReportPost reports a post for moderation
// POST /api/v1/posts/:id/report
func (h *Handlers) ReportPost(c *gin.Context) {
	postID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var req struct {
		Reason      string `json:"reason" binding:"required"`
		Description string `json:"description,omitempty"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	// Validate reason
	validReasons := []string{"spam", "harassment", "inappropriate", "copyright", "violence", "other"}
	valid := false
	for _, r := range validReasons {
		if req.Reason == r {
			valid = true
			break
		}
	}
	if !valid {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_reason", "message": "Reason must be one of: spam, harassment, inappropriate, copyright, violence, other"})
		return
	}

	// Find the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	// Create report
	report := models.Report{
		ReporterID:   userID.(string),
		TargetType:   models.ReportTargetPost,
		TargetID:     postID,
		TargetUserID: &post.UserID,
		Reason:       models.ReportReason(req.Reason),
		Description:  req.Description,
		Status:       models.ReportStatusPending,
	}

	if err := database.DB.Create(&report).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_create_report", "message": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message":   "report_created",
		"report_id": report.ID,
	})
}

// DownloadPost generates a download URL for a post and tracks the download
// POST /api/v1/posts/:id/download
func (h *Handlers) DownloadPost(c *gin.Context) {
	postID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	// Find the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	// Check if post is public or user owns it
	if !post.IsPublic && post.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "post_not_accessible"})
		return
	}

	// Increment download count atomically
	database.DB.Model(&post).UpdateColumn("download_count", gorm.Expr("download_count + 1"))

	// Reload post to get updated download count
	database.DB.First(&post, "id = ?", postID)

	// Generate filename: {username}_{title}_{bpm}bpm.mp3
	// For now, use a simple format since we don't have title field
	var user models.User
	database.DB.First(&user, "id = ?", post.UserID)
	username := user.Username
	if username == "" {
		username = "user"
	}

	// Extract extension from audio URL or default to .mp3
	extension := ".mp3"
	if post.OriginalFilename != "" {
		ext := filepath.Ext(post.OriginalFilename)
		if ext != "" {
			extension = ext
		}
	}

	filename := fmt.Sprintf("%s_%s_%dbpm%s", username, postID[:8], post.BPM, extension)

	// Return download URL and metadata
	// Note: For now, we return the public audio URL directly
	// In the future, we can generate signed URLs with expiration
	c.JSON(http.StatusOK, gin.H{
		"download_url": post.AudioURL,
		"filename":     filename,
		"metadata": gin.H{
			"bpm":      post.BPM,
			"key":      post.Key,
			"duration": post.Duration,
			"genre":    post.Genre,
			"daw":      post.DAW,
		},
		"download_count": post.DownloadCount,
	})
}
