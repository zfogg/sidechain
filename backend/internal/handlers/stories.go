package handlers

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
	"gorm.io/gorm"
)

// TODO: - Write backend tests for story endpoints
// TODO: - Optimize MIDI data storage
// TODO: - Optimize story feed loading

// ============================================================================
// STORIES - PROFESSIONAL ENHANCEMENTS
// ============================================================================

// NOTE: Common enhancements (caching, analytics, rate limiting, moderation,
// search, webhooks, export, performance, anti-abuse) are documented in
// common_todos.go. See that file for shared TODO items.

// TODO: PROFESSIONAL-2.1 - Implement story-specific analytics
// - Track unique viewers (not just view count)
// - Track average watch time (how long users watch before skipping)
// - Track completion rate (% of users who watch full story)
// - Track skip rate (% of users who skip before end)
// - Track interactions (swipes up, reactions, replies)
// - Return analytics to story owner in GetStory endpoint (when owner requests)

// TODO: PROFESSIONAL-2.2 - Add story reactions and replies
// - Allow viewers to react to stories (emoji reactions)
// - Allow viewers to reply to stories (private message to creator)
// - Notification system for story interactions
// - Creator can see all reactions/replies in story viewer list
// - Integration with getstream.io reactions API

// TODO: PROFESSIONAL-2.3 - Implement story scheduling
// - Allow users to schedule stories for future publication
// - Auto-publish stories at scheduled time
// - Queue management for scheduled stories
// - Edit scheduled stories before publication

// TODO: PROFESSIONAL-2.4 - Add story customization features
// - Support text overlays on stories
// - Support image overlays/stickers
// - Support background colors/gradients
// - Support custom fonts for text
// - Support filters/effects (reverb visualization, etc.)

// TODO: PROFESSIONAL-2.5 - Implement story-specific privacy controls
// - Private stories (only followers can view)
// - Close friends stories (subset of followers)
// - Public stories (anyone can view)
// - See common_todos.go for general privacy infrastructure

// TODO: PROFESSIONAL-2.6 - Add story expiration customization
// - Allow users to extend story expiration (beyond 24 hours)
// - Allow users to set custom expiration times (1 hour, 6 hours, 12 hours, 24 hours, 48 hours)
// - Save stories to highlights before expiration
// - Auto-save to highlights if story gets high engagement

// TODO: PROFESSIONAL-2.7 - Enhance story feed algorithm
// - Prioritize stories from users user interacts with most
// - Prioritize unviewed stories over viewed ones
// - Group stories by user (show all stories from one user together)
// - Show "New" badge on unviewed stories
// - Sort by relevance, not just chronological

// TODO: PROFESSIONAL-2.8 - Implement story collections/highlights improvements
// - Multiple highlights collections per user
// - Custom cover images for highlights
// - Reorder stories within highlights
// - Highlights analytics (views, engagement)
// - Public/private highlights settings

// TODO: PROFESSIONAL-2.9 - Add story sharing features
// - Share story link (works even after expiration if saved)
// - Embed story in external websites (if enabled)
// - Share story to other platforms (future: Twitter, Instagram, etc.)
// - Generate QR codes for story links

// TODO: PROFESSIONAL-2.10 - Implement story-specific moderation
// - Content moderation for story audio (explicit content detection)
// - See common_todos.go for general moderation infrastructure

// TODO: PROFESSIONAL-2.11 - Add story playback optimizations
// - Adaptive streaming (adjust quality based on connection)
// - Preload next story in background
// - Cache stories locally (CDN caching)
// - Progressive loading (start playback before full download)

// TODO: PROFESSIONAL-2.12 - Implement story engagement tracking
// - Track which stories drive profile visits
// - Track which stories drive follows
// - Track which stories drive post plays
// - Attribution tracking (did user discover you via story?)

// TODO: PROFESSIONAL-2.13 - Add story templates
// - Pre-made story templates (e.g., "Behind the scenes", "New release")
// - Customizable templates per user
// - Template library for community
// - Easy story creation from templates

// TODO: PROFESSIONAL-2.14 - Implement story collaboration
// - Tag collaborators in stories
// - Shared stories (multiple users contribute)
// - Story takeovers (one user creates story for another)
// - Cross-promotion features

// TODO: PROFESSIONAL-2.15 - Add story discovery features
// - Story search (by user, by genre, by BPM)
// - Story recommendations ("Stories you might like")
// - Trending stories (based on engagement)
// - Featured stories (curated by admins)
// - See common_todos.go for general search infrastructure

// TODO: PROFESSIONAL-2.16 - Enhance story expiration handling
// - Graceful expiration (notify user 1 hour before expiration)
// - Option to extend expiration before it expires
// - Auto-archive expired stories (optional, user preference)
// - Batch expiration processing (efficient cleanup)

// TODO: PROFESSIONAL-2.17 - Implement story performance metrics
// - Story completion rate (% watched to end)
// - Average watch time per story
// - Skip rate (how many skip this story)
// - Interaction rate (reactions/replies per view)
// - Follower growth attributed to stories

// TODO: PROFESSIONAL-2.18 - Add story content warnings
// - Optional content warnings (explicit language, etc.)
// - Age restrictions (18+ stories)
// - Sensitivity labels
// - User preference to hide certain content

// TODO: PROFESSIONAL-2.19 - Implement story-specific localization
// - Support multiple languages in story metadata
// - Auto-translate story text overlays
// - Region-specific story feeds (if location data available)
// - See common_todos.go for general localization infrastructure

// TODO: PROFESSIONAL-2.20 - Add story-specific backup and export
// - Export story as audio file
// - Export story with MIDI data
// - Backup all stories before expiration
// - See common_todos.go for general export/backup infrastructure

// CreateStory handles story creation with multipart file upload
func (h *Handlers) CreateStory(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	// Get audio file from multipart form
	file, err := c.FormFile("audio")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "no_audio_file",
			"message": "No audio file provided in 'audio' field",
		})
		return
	}

	// Validate file size (max 10MB for stories)
	const maxStorySize = 10 * 1024 * 1024
	if file.Size > maxStorySize {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "file_too_large",
			"message": "Story audio must be under 10MB",
		})
		return
	}

	// Parse duration from form
	durationStr := c.PostForm("duration")
	var audioDuration float64
	if durationStr != "" {
		if _, err := fmt.Sscanf(durationStr, "%f", &audioDuration); err != nil {
			audioDuration = 0
		}
	}

	// Validate duration (5-60 seconds) - but allow 0 if not provided
	if audioDuration > 0 && (audioDuration < 5 || audioDuration > 60) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_duration",
			"message": "Story duration must be between 5 and 60 seconds",
		})
		return
	}

	// Parse and validate display filename
	filename := c.PostForm("filename")
	if err := util.ValidateFilename(filename); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_filename",
			"message": err.Error(),
		})
		return
	}
	// Default to original filename if not provided
	if filename == "" {
		filename = file.Filename
	}

	// Parse and validate MIDI filename
	midiFilename := c.PostForm("midi_filename")
	if err := util.ValidateFilename(midiFilename); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_midi_filename",
			"message": err.Error(),
		})
		return
	}

	// Read audio file content
	src, err := file.Open()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "file_open_failed",
			"message": "Failed to open audio file",
		})
		return
	}
	defer src.Close()

	audioData, err := io.ReadAll(src)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "file_read_failed",
			"message": "Failed to read audio file",
		})
		return
	}

	// Upload audio file to S3 using the audio processor
	audioURL, err := h.audioProcessor.UploadStoryAudio(context.Background(), audioData, currentUser.ID, file.Filename)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "upload_failed",
			"message": "Failed to upload audio: " + err.Error(),
		})
		return
	}

	// Generate and upload waveform PNG
	var waveformURL string
	if h.waveformGenerator != nil && h.waveformStorage != nil {
		// Generate waveform from audio data
		waveformPNG, err := h.waveformGenerator.GenerateFromWAV(bytes.NewReader(audioData))
		if err != nil {
			// Log error but don't fail the request - waveform is optional
			fmt.Printf("Warning: Failed to generate waveform for story %s: %v\n", currentUser.ID, err)
		} else {
			// Upload waveform to S3
			waveformURL, err = h.waveformStorage.UploadWaveform(waveformPNG, currentUser.ID, "story-"+currentUser.ID)
			if err != nil {
				// Log error but don't fail the request
				fmt.Printf("Warning: Failed to upload waveform for story %s: %v\n", currentUser.ID, err)
			}
		}
	}

	// Parse optional fields from form
	var bpm *int
	if bpmStr := c.PostForm("bpm"); bpmStr != "" {
		var bpmVal int
		if _, err := fmt.Sscanf(bpmStr, "%d", &bpmVal); err == nil && bpmVal > 0 {
			bpm = &bpmVal
		}
	}

	var key *string
	if keyStr := c.PostForm("key"); keyStr != "" {
		key = &keyStr
	}

	var genre models.StringArray
	if genreStr := c.PostForm("genre"); genreStr != "" {
		genre = models.StringArray{genreStr}
	}

	// Parse MIDI data if provided
	var midiPatternID *string
	if midiDataStr := c.PostForm("midi_data"); midiDataStr != "" {
		var md models.MIDIData
		if err := json.Unmarshal([]byte(midiDataStr), &md); err == nil && len(md.Events) > 0 {
			// Create standalone MIDI pattern with filename
			pattern := &models.MIDIPattern{
				UserID:        currentUser.ID,
				Name:          "MIDI from story",
				Filename:      midiFilename, // User-provided MIDI filename
				Events:        md.Events,
				Tempo:         md.Tempo,
				TimeSignature: md.TimeSignature,
				IsPublic:      true,
			}
			if err := database.DB.Create(pattern).Error; err == nil {
				midiPatternID = &pattern.ID
			}
		}
	}

	// Create story with 24-hour expiration
	story := &models.Story{
		UserID:        currentUser.ID,
		AudioURL:      audioURL,
		AudioDuration: audioDuration,
		Filename:      filename,     // User-provided display filename
		MIDIFilename:  midiFilename, // User-provided MIDI filename
		MIDIPatternID: midiPatternID,
		WaveformURL:   waveformURL, // CDN URL to waveform PNG
		BPM:           bpm,
		Key:           key,
		Genre:         genre,
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

	// Index story to Elasticsearch
	if h.search != nil {
		go func() {
			storyDoc := map[string]interface{}{
				"id":         story.ID,
				"user_id":    story.UserID,
				"username":   currentUser.Username,
				"created_at": story.CreatedAt,
				"expires_at": story.ExpiresAt,
			}
			if err := h.search.IndexStory(c.Request.Context(), story.ID, storyDoc); err != nil {
				fmt.Printf("Warning: Failed to index story %s in Elasticsearch: %v\n", story.ID, err)
			}
		}()
	}

	response := gin.H{
		"story_id":   story.ID,
		"audio_url":  audioURL,
		"expires_at": story.ExpiresAt.Format(time.RFC3339),
		"message":    "Story created successfully",
	}
	if waveformURL != "" {
		response["waveform_url"] = waveformURL
	}
	if midiPatternID != nil {
		response["midi_pattern_id"] = *midiPatternID
	}

	c.JSON(http.StatusCreated, response)
}

// GetStories returns active stories from followed users + own stories
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

// DeleteStory deletes a story
// DELETE /api/v1/stories/:id
func (h *Handlers) DeleteStory(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	storyID := c.Param("id")

	// Find the story
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

	// Check ownership
	if story.UserID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{
			"error":   "forbidden",
			"message": "You can only delete your own stories",
		})
		return
	}

	// Delete associated view records first (foreign key constraint)
	database.DB.Where("story_id = ?", storyID).Delete(&models.StoryView{})

	// Delete audio file from S3 if URL is present
	if story.AudioURL != "" && h.audioProcessor != nil {
		if err := h.audioProcessor.DeleteStoryAudio(context.Background(), story.AudioURL); err != nil {
			// Log but don't fail - database cleanup is more important
			fmt.Printf("Warning: Failed to delete audio from S3 for story %s: %v\n", story.ID, err)
		}
	}

	// Delete the story record
	if err := database.DB.Delete(&story).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "deletion_failed",
			"message": "Failed to delete story",
		})
		return
	}

	// Delete story from Elasticsearch index
	if h.search != nil {
		if err := h.search.DeleteStory(c.Request.Context(), story.ID); err != nil {
			// Log but don't fail - story is already deleted in database
			fmt.Printf("Warning: Failed to delete story %s from Elasticsearch: %v\n", story.ID, err)
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Story deleted successfully",
	})
}

// GetStory returns a single story by ID
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
						"avatar_url":   v.Viewer.GetAvatarURL(),
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

// GetUserStories returns all active stories for a specific user
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

// ViewStory marks a story as viewed by the current user
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

// DownloadStory returns download URLs for story audio and MIDI
// POST /api/v1/stories/:id/download
func (h *Handlers) DownloadStory(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	storyID := c.Param("id")

	// Find the story
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

	// Check if story has expired (but allow owner to still download)
	if story.ExpiresAt.Before(time.Now().UTC()) && story.UserID != currentUser.ID {
		c.JSON(http.StatusGone, gin.H{
			"error":   "expired",
			"message": "Story has expired",
		})
		return
	}

	if err := database.DB.Model(&story).UpdateColumn("download_count", gorm.Expr("COALESCE(download_count, 0) + 1")).Error; err != nil {
		logger.WarnWithFields("Failed to increment story download count for story "+storyID, err)
	}

	// Generate filename: {username}_story_{id}.mp3
	username := story.User.Username
	if username == "" {
		username = "user"
	}

	audioFilename := fmt.Sprintf("%s_story_%s.mp3", username, storyID[:8])

	// Build response
	response := gin.H{
		"audio_url":      story.AudioURL,
		"audio_filename": audioFilename,
		"metadata": gin.H{
			"bpm":      story.BPM,
			"key":      story.Key,
			"duration": story.AudioDuration,
			"genre":    story.Genre,
		},
	}

	// Add MIDI info if available
	if story.MIDIPatternID != nil && *story.MIDIPatternID != "" {
		response["midi_pattern_id"] = *story.MIDIPatternID
		response["midi_download_url"] = fmt.Sprintf("/api/v1/midi/%s/file", *story.MIDIPatternID)
		response["midi_filename"] = fmt.Sprintf("%s_story_%s.mid", username, storyID[:8])
	}

	c.JSON(http.StatusOK, response)
}

// GetStoryViews returns the list of users who viewed a story
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
				"avatar_url":   v.Viewer.GetAvatarURL(),
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
// STORY HIGHLIGHTS
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
			"id":          h.ID,
			"name":        h.Name,
			"description": h.Description,
			"cover_image": h.CoverImage,
			"story_count": h.StoryCount,
			"sort_order":  h.SortOrder,
			"created_at":  h.CreatedAt.Format(time.RFC3339),
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
			"midi_pattern":   hs.Story.MIDIPattern,
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
