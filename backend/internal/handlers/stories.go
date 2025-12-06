package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

// TODO: Phase 7.5.7.1 - Write backend tests for story endpoints
// TODO: Phase 7.5.8.1 - Optimize MIDI data storage
// TODO: Phase 7.5.8.2 - Optimize story feed loading

// CreateStory handles story creation (7.5.1.2.1)
func (h *Handlers) CreateStory(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	var req struct {
		AudioURL      string             `json:"audio_url" binding:"required"`
		AudioDuration float64            `json:"audio_duration" binding:"required"`
		MIDIData      *models.MIDIData   `json:"midi_data,omitempty"`
		WaveformData  string             `json:"waveform_data,omitempty"`
		BPM           *int               `json:"bpm,omitempty"`
		Key           *string            `json:"key,omitempty"`
		Genre         models.StringArray `json:"genre,omitempty"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	// Validate duration (5-60 seconds)
	if req.AudioDuration < 5 || req.AudioDuration > 60 {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_duration",
			"message": "Story duration must be between 5 and 60 seconds",
		})
		return
	}

	// Create story with 24-hour expiration
	story := &models.Story{
		UserID:        currentUser.ID,
		AudioURL:      req.AudioURL,
		AudioDuration: req.AudioDuration,
		MIDIData:      req.MIDIData,
		WaveformData:  req.WaveformData,
		BPM:           req.BPM,
		Key:           req.Key,
		Genre:         req.Genre,
		ExpiresAt:     time.Now().UTC().Add(24 * time.Hour),
		ViewCount:     0,
	}

	if err := database.DB.Create(story).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "creation_failed",
			"message": "Failed to create story",
		})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"story_id":   story.ID,
		"expires_at": story.ExpiresAt.Format(time.RFC3339),
		"message":    "Story created successfully",
	})
}

// GetStories returns active stories from followed users + own stories (7.5.1.3.1)
func (h *Handlers) GetStories(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	// Get list of followed user IDs from getstream.io
	var followedUserIDs []string
	if h.stream != nil {
		following, err := h.stream.GetFollowing(currentUser.ID, 100, 0)
		if err == nil {
			for _, f := range following {
				followedUserIDs = append(followedUserIDs, f.UserID)
			}
		}
	}

	// Include current user's own stories
	followedUserIDs = append(followedUserIDs, currentUser.ID)

	// Query active stories (not expired) from followed users
	var stories []models.Story
	query := database.DB.Where("user_id IN ? AND expires_at > ?", followedUserIDs, time.Now().UTC()).
		Preload("User").
		Order("created_at DESC")

	// Check if user has viewed each story
	var viewedStoryIDs []string
	database.DB.Model(&models.StoryView{}).
		Where("viewer_id = ? AND story_id IN (SELECT id FROM stories WHERE user_id IN ? AND expires_at > ?)",
			currentUser.ID, followedUserIDs, time.Now().UTC()).
		Pluck("story_id", &viewedStoryIDs)

	viewedMap := make(map[string]bool)
	for _, id := range viewedStoryIDs {
		viewedMap[id] = true
	}

	if err := query.Find(&stories).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch stories",
		})
		return
	}

	// Format response with viewed status
	type StoryResponse struct {
		models.Story
		Viewed bool `json:"viewed"`
	}

	response := make([]StoryResponse, len(stories))
	for i, story := range stories {
		response[i] = StoryResponse{
			Story:  story,
			Viewed: viewedMap[story.ID],
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"stories": response,
		"count":   len(response),
	})
}

// GetStory returns a single story by ID (7.5.1.3.2)
func (h *Handlers) GetStory(c *gin.Context) {
	storyID := c.Param("id")

	var story models.Story
	if err := database.DB.Preload("User").First(&story, "id = ?", storyID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{
				"error":   "not_found",
				"message": "Story not found",
			})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch story",
		})
		return
	}

	// Check if story has expired
	if story.ExpiresAt.Before(time.Now().UTC()) {
		c.JSON(http.StatusGone, gin.H{
			"error":   "expired",
			"message": "Story has expired",
		})
		return
	}

	// Get view count and viewer list (if story owner)
	user, exists := c.Get("user")
	if exists {
		currentUser := user.(*models.User)
		if story.UserID == currentUser.ID {
			var views []models.StoryView
			database.DB.Where("story_id = ?", storyID).
				Preload("Viewer").
				Order("viewed_at DESC").
				Find(&views)

			viewers := make([]map[string]interface{}, len(views))
			for i, v := range views {
				viewers[i] = map[string]interface{}{
					"user": map[string]interface{}{
						"id":           v.Viewer.ID,
						"username":     v.Viewer.Username,
						"display_name": v.Viewer.DisplayName,
						"avatar_url":   v.Viewer.AvatarURL,
					},
					"viewed_at": v.ViewedAt.Format(time.RFC3339),
				}
			}

			c.JSON(http.StatusOK, gin.H{
				"story":      story,
				"view_count": story.ViewCount,
				"viewers":    viewers,
			})
			return
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"story":      story,
		"view_count": story.ViewCount,
	})
}

// GetUserStories returns all active stories for a specific user (7.5.1.3.3)
func (h *Handlers) GetUserStories(c *gin.Context) {
	userID := c.Param("id")

	var stories []models.Story
	if err := database.DB.Where("user_id = ? AND expires_at > ?", userID, time.Now().UTC()).
		Preload("User").
		Order("created_at DESC").
		Find(&stories).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch user stories",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"stories": stories,
		"count":   len(stories),
	})
}

// ViewStory marks a story as viewed by the current user (7.5.1.4.1)
func (h *Handlers) ViewStory(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	storyID := c.Param("id")

	// Check if story exists and is not expired
	var story models.Story
	if err := database.DB.First(&story, "id = ?", storyID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{
				"error":   "not_found",
				"message": "Story not found",
			})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch story",
		})
		return
	}

	if story.ExpiresAt.Before(time.Now().UTC()) {
		c.JSON(http.StatusGone, gin.H{
			"error":   "expired",
			"message": "Story has expired",
		})
		return
	}

	// Don't count viewing your own story
	if story.UserID == currentUser.ID {
		c.JSON(http.StatusOK, gin.H{
			"viewed":     true,
			"view_count": story.ViewCount,
		})
		return
	}

	// Check if already viewed (using unique constraint)
	var existingView models.StoryView
	err := database.DB.Where("story_id = ? AND viewer_id = ?", storyID, currentUser.ID).First(&existingView).Error

	if err == gorm.ErrRecordNotFound {
		// Create new view
		view := models.StoryView{
			StoryID:  storyID,
			ViewerID: currentUser.ID,
			ViewedAt: time.Now().UTC(),
		}

		if err := database.DB.Create(&view).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{
				"error":   "view_failed",
				"message": "Failed to record view",
			})
			return
		}

		// Increment view count
		database.DB.Model(&story).Update("view_count", gorm.Expr("view_count + 1"))
		story.ViewCount++
	}

	c.JSON(http.StatusOK, gin.H{
		"viewed":     true,
		"view_count": story.ViewCount,
	})
}

// GetStoryViews returns the list of users who viewed a story (7.5.1.4.2)
func (h *Handlers) GetStoryViews(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	storyID := c.Param("id")

	// Verify story ownership
	var story models.Story
	if err := database.DB.First(&story, "id = ?", storyID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{
			"error":   "not_found",
			"message": "Story not found",
		})
		return
	}

	if story.UserID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{
			"error":   "forbidden",
			"message": "Only story owner can view viewer list",
		})
		return
	}

	// Get views
	var views []models.StoryView
	if err := database.DB.Where("story_id = ?", storyID).
		Preload("Viewer").
		Order("viewed_at DESC").
		Find(&views).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch views",
		})
		return
	}

	// Format response
	viewers := make([]map[string]interface{}, len(views))
	for i, v := range views {
		viewers[i] = map[string]interface{}{
			"user": map[string]interface{}{
				"id":           v.Viewer.ID,
				"username":     v.Viewer.Username,
				"display_name": v.Viewer.DisplayName,
				"avatar_url":   v.Viewer.AvatarURL,
			},
			"viewed_at": v.ViewedAt.Format(time.RFC3339),
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"viewers":    viewers,
		"view_count": len(viewers),
	})
}
