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

// =============================================================================
// STORY HIGHLIGHTS (7.5.6)
// =============================================================================

// CreateHighlight creates a new story highlight collection
func (h *Handlers) CreateHighlight(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	var req struct {
		Name        string `json:"name" binding:"required,min=1,max=50"`
		Description string `json:"description,omitempty"`
		CoverImage  string `json:"cover_image,omitempty"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	// Get current max sort order for user
	var maxSortOrder int
	database.DB.Model(&models.StoryHighlight{}).
		Where("user_id = ?", currentUser.ID).
		Select("COALESCE(MAX(sort_order), -1)").
		Scan(&maxSortOrder)

	highlight := &models.StoryHighlight{
		UserID:      currentUser.ID,
		Name:        req.Name,
		Description: req.Description,
		CoverImage:  req.CoverImage,
		SortOrder:   maxSortOrder + 1,
	}

	if err := database.DB.Create(highlight).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "creation_failed",
			"message": "Failed to create highlight",
		})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"highlight_id": highlight.ID,
		"message":      "Highlight created successfully",
	})
}

// GetHighlights returns all highlights for a user
func (h *Handlers) GetHighlights(c *gin.Context) {
	userID := c.Param("id")
	if userID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "user_id_required"})
		return
	}

	// Check if user exists
	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	var highlights []models.StoryHighlight
	if err := database.DB.Where("user_id = ?", userID).
		Order("sort_order ASC").
		Find(&highlights).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch highlights",
		})
		return
	}

	// Format response with story count
	result := make([]map[string]interface{}, len(highlights))
	for i, h := range highlights {
		result[i] = map[string]interface{}{
			"id":           h.ID,
			"name":         h.Name,
			"description":  h.Description,
			"cover_image":  h.CoverImage,
			"story_count":  h.StoryCount,
			"sort_order":   h.SortOrder,
			"created_at":   h.CreatedAt.Format(time.RFC3339),
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"highlights": result,
		"count":      len(result),
	})
}

// GetHighlight returns a single highlight with its stories
func (h *Handlers) GetHighlight(c *gin.Context) {
	highlightID := c.Param("id")
	if highlightID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "highlight_id_required"})
		return
	}

	var highlight models.StoryHighlight
	if err := database.DB.First(&highlight, "id = ?", highlightID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "not_found"})
		return
	}

	// Get stories in this highlight
	var highlightedStories []models.HighlightedStory
	if err := database.DB.
		Preload("Story").
		Preload("Story.User").
		Where("highlight_id = ?", highlightID).
		Order("sort_order ASC").
		Find(&highlightedStories).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch stories",
		})
		return
	}

	// Format stories response
	stories := make([]map[string]interface{}, len(highlightedStories))
	for i, hs := range highlightedStories {
		stories[i] = map[string]interface{}{
			"id":             hs.Story.ID,
			"audio_url":      hs.Story.AudioURL,
			"audio_duration": hs.Story.AudioDuration,
			"midi_data":      hs.Story.MIDIData,
			"bpm":            hs.Story.BPM,
			"key":            hs.Story.Key,
			"genre":          hs.Story.Genre,
			"view_count":     hs.Story.ViewCount,
			"created_at":     hs.Story.CreatedAt.Format(time.RFC3339),
			"sort_order":     hs.SortOrder,
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"highlight": map[string]interface{}{
			"id":          highlight.ID,
			"name":        highlight.Name,
			"description": highlight.Description,
			"cover_image": highlight.CoverImage,
			"story_count": highlight.StoryCount,
			"created_at":  highlight.CreatedAt.Format(time.RFC3339),
		},
		"stories": stories,
	})
}

// UpdateHighlight updates a highlight's metadata
func (h *Handlers) UpdateHighlight(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	highlightID := c.Param("id")
	if highlightID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "highlight_id_required"})
		return
	}

	var highlight models.StoryHighlight
	if err := database.DB.First(&highlight, "id = ?", highlightID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "not_found"})
		return
	}

	// Check ownership
	if highlight.UserID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{"error": "forbidden"})
		return
	}

	var req struct {
		Name        *string `json:"name,omitempty"`
		Description *string `json:"description,omitempty"`
		CoverImage  *string `json:"cover_image,omitempty"`
		SortOrder   *int    `json:"sort_order,omitempty"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	updates := make(map[string]interface{})
	if req.Name != nil {
		updates["name"] = *req.Name
	}
	if req.Description != nil {
		updates["description"] = *req.Description
	}
	if req.CoverImage != nil {
		updates["cover_image"] = *req.CoverImage
	}
	if req.SortOrder != nil {
		updates["sort_order"] = *req.SortOrder
	}

	if len(updates) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "no_fields_to_update"})
		return
	}

	if err := database.DB.Model(&highlight).Updates(updates).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "update_failed",
			"message": "Failed to update highlight",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Highlight updated successfully",
	})
}

// DeleteHighlight deletes a highlight (soft delete)
func (h *Handlers) DeleteHighlight(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	highlightID := c.Param("id")
	if highlightID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "highlight_id_required"})
		return
	}

	var highlight models.StoryHighlight
	if err := database.DB.First(&highlight, "id = ?", highlightID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "not_found"})
		return
	}

	// Check ownership
	if highlight.UserID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{"error": "forbidden"})
		return
	}

	// Delete associated highlighted stories first
	database.DB.Where("highlight_id = ?", highlightID).Delete(&models.HighlightedStory{})

	// Delete the highlight
	if err := database.DB.Delete(&highlight).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "deletion_failed",
			"message": "Failed to delete highlight",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Highlight deleted successfully",
	})
}

// AddStoryToHighlight adds a story to a highlight collection
func (h *Handlers) AddStoryToHighlight(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	highlightID := c.Param("id")
	if highlightID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "highlight_id_required"})
		return
	}

	var req struct {
		StoryID string `json:"story_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	// Check highlight exists and user owns it
	var highlight models.StoryHighlight
	if err := database.DB.First(&highlight, "id = ?", highlightID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "highlight_not_found"})
		return
	}

	if highlight.UserID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{"error": "forbidden"})
		return
	}

	// Check story exists and user owns it
	var story models.Story
	if err := database.DB.First(&story, "id = ?", req.StoryID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "story_not_found"})
		return
	}

	if story.UserID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{"error": "can_only_highlight_own_stories"})
		return
	}

	// Check if already in highlight
	var existing models.HighlightedStory
	if err := database.DB.Where("highlight_id = ? AND story_id = ?", highlightID, req.StoryID).First(&existing).Error; err == nil {
		c.JSON(http.StatusConflict, gin.H{"error": "already_highlighted"})
		return
	}

	// Get max sort order for this highlight
	var maxSortOrder int
	database.DB.Model(&models.HighlightedStory{}).
		Where("highlight_id = ?", highlightID).
		Select("COALESCE(MAX(sort_order), -1)").
		Scan(&maxSortOrder)

	// Add to highlight
	highlightedStory := &models.HighlightedStory{
		HighlightID: highlightID,
		StoryID:     req.StoryID,
		SortOrder:   maxSortOrder + 1,
	}

	if err := database.DB.Create(highlightedStory).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "creation_failed",
			"message": "Failed to add story to highlight",
		})
		return
	}

	// Update story count on highlight
	database.DB.Model(&highlight).Update("story_count", gorm.Expr("story_count + 1"))

	c.JSON(http.StatusCreated, gin.H{
		"message": "Story added to highlight",
	})
}

// RemoveStoryFromHighlight removes a story from a highlight
func (h *Handlers) RemoveStoryFromHighlight(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	highlightID := c.Param("id")
	storyID := c.Param("story_id")
	if highlightID == "" || storyID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "highlight_id_and_story_id_required"})
		return
	}

	// Check highlight exists and user owns it
	var highlight models.StoryHighlight
	if err := database.DB.First(&highlight, "id = ?", highlightID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "highlight_not_found"})
		return
	}

	if highlight.UserID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{"error": "forbidden"})
		return
	}

	// Delete the highlighted story entry
	result := database.DB.Where("highlight_id = ? AND story_id = ?", highlightID, storyID).
		Delete(&models.HighlightedStory{})

	if result.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "story_not_in_highlight"})
		return
	}

	// Update story count on highlight
	database.DB.Model(&highlight).Update("story_count", gorm.Expr("story_count - 1"))

	c.JSON(http.StatusOK, gin.H{
		"message": "Story removed from highlight",
	})
}
