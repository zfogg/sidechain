package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/util"
	"go.uber.org/zap"
	"gorm.io/gorm"
)

// ============================================================================
// REMIX CHAINS
// ============================================================================

// Allows users to remix MIDI and/or audio from posts and stories.
// Creates a chain of remixes showing the creative lineage.


// CreateRemixPost creates a new post that remixes content from another post or story
// POST /api/v1/posts/:id/remix (remix a post)
// POST /api/v1/stories/:id/remix (remix a story)
func (h *Handlers) CreateRemixPost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	// Determine source type (post or story)
	sourceID := c.Param("id")
	sourceType := c.Param("source_type") // "post" or "story" - set by route
	if sourceType == "" {
		// Try to detect from URL path
		if c.FullPath() == "/api/v1/posts/:id/remix" {
			sourceType = "post"
		} else if c.FullPath() == "/api/v1/stories/:id/remix" {
			sourceType = "story"
		}
	}

	var req struct {
		AudioURL  string `json:"audio_url" binding:"required"`
		BPM       int    `json:"bpm"`
		Key       string `json:"key"`
		DAW       string `json:"daw"`
		Genre     []string `json:"genre"`
		// What content was remixed
		RemixType string `json:"remix_type" binding:"required"` // "audio", "midi", "both"
		// Optional: New MIDI data created for the remix
		MIDIData *models.MIDIData `json:"midi_data,omitempty"`
		MIDIID   string           `json:"midi_id,omitempty"`
		// Title for the remix
		Title string `json:"title"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Validate remix type
	if req.RemixType != "audio" && req.RemixType != "midi" && req.RemixType != "both" {
		util.RespondBadRequest(c, "invalid_remix_type", "remix_type must be 'audio', 'midi', or 'both'")
		return
	}

	var remixOfPostID *string
	var remixOfStoryID *string
	var remixChainDepth int
	var sourceAudioURL string
	var sourceMIDIPatternID *string
	var sourceUserID string

	// Validate source exists and get its data
	if sourceType == "post" {
		var sourcePost models.AudioPost
		if err := database.DB.First(&sourcePost, "id = ?", sourceID).Error; err != nil {
			if err == gorm.ErrRecordNotFound {
				util.RespondNotFound(c, "post_not_found")
				return
			}
			util.RespondInternalError(c, "db_error")
			return
		}
		remixOfPostID = &sourceID
		remixChainDepth = sourcePost.RemixChainDepth + 1
		sourceAudioURL = sourcePost.AudioURL
		sourceMIDIPatternID = sourcePost.MIDIPatternID
		sourceUserID = sourcePost.UserID

		// Validate we can remix what was requested
		if req.RemixType == "midi" || req.RemixType == "both" {
			if sourceMIDIPatternID == nil {
				util.RespondBadRequest(c, "no_midi_available", "Source post does not have MIDI data to remix")
				return
			}
		}
	} else if sourceType == "story" {
		var sourceStory models.Story
		if err := database.DB.First(&sourceStory, "id = ?", sourceID).Error; err != nil {
			if err == gorm.ErrRecordNotFound {
				util.RespondNotFound(c, "story_not_found")
				return
			}
			util.RespondInternalError(c, "db_error")
			return
		}
		// Check if story is expired
		if sourceStory.ExpiresAt.Before(time.Now().UTC()) {
			util.RespondBadRequest(c, "story_expired", "Cannot remix an expired story")
			return
		}
		remixOfStoryID = &sourceID
		remixChainDepth = 1 // Stories are always depth 0 sources
		sourceAudioURL = sourceStory.AudioURL
		sourceMIDIPatternID = sourceStory.MIDIPatternID
		sourceUserID = sourceStory.UserID

		// Validate we can remix what was requested
		if req.RemixType == "midi" || req.RemixType == "both" {
			if sourceMIDIPatternID == nil {
				util.RespondBadRequest(c, "no_midi_available", "Source story does not have MIDI data to remix")
				return
			}
		}
	} else {
		util.RespondBadRequest(c, "invalid_source_type")
		return
	}

	// Enforce max chain depth (prevent infinite chains)
	const maxChainDepth = 10
	if remixChainDepth > maxChainDepth {
		util.RespondBadRequest(c, "chain_depth_exceeded", "Maximum remix chain depth reached")
		return
	}

	postID := uuid.New().String()

	// Handle MIDI data for the remix
	var midiPatternID *string
	if req.MIDIID != "" {
		// Use existing MIDI pattern
		var pattern models.MIDIPattern
		if err := database.DB.First(&pattern, "id = ?", req.MIDIID).Error; err != nil {
			util.RespondBadRequest(c, "invalid_midi_id")
			return
		}
		if !pattern.IsPublic && pattern.UserID != userID {
			util.RespondForbidden(c, "midi_access_denied")
			return
		}
		midiPatternID = &req.MIDIID
	} else if req.MIDIData != nil && len(req.MIDIData.Events) > 0 {
		// Create new MIDI pattern
		pattern := &models.MIDIPattern{
			UserID:        userID,
			Name:          "MIDI from remix",
			Events:        req.MIDIData.Events,
			Tempo:         req.MIDIData.Tempo,
			TimeSignature: req.MIDIData.TimeSignature,
			IsPublic:      true,
		}
		if err := database.DB.Create(pattern).Error; err != nil {
			util.RespondInternalError(c, "failed_to_create_midi")
			return
		}
		midiPatternID = &pattern.ID
	}

	// Create the AudioPost record in database
	post := &models.AudioPost{
		ID:              postID,
		UserID:          userID,
		AudioURL:        req.AudioURL,
		BPM:             req.BPM,
		Key:             req.Key,
		DAW:             req.DAW,
		Genre:           req.Genre,
		MIDIPatternID:   midiPatternID,
		RemixOfPostID:   remixOfPostID,
		RemixOfStoryID:  remixOfStoryID,
		RemixChainDepth: remixChainDepth,
		RemixType:       req.RemixType,
		IsPublic:        true,
	}

	if err := database.DB.Create(post).Error; err != nil {
		util.RespondInternalError(c, "failed_to_create_post")
		return
	}

	// Update remix count on source
	if remixOfPostID != nil {
		if err := database.DB.Model(&models.AudioPost{}).Where("id = ?", *remixOfPostID).
			UpdateColumn("remix_count", gorm.Expr("remix_count + 1")).Error; err != nil {
			logger.WarnWithFields("Failed to increment remix count for post "+*remixOfPostID, err)
		}
	}

	// Create activity in getstream.io
	activity := &stream.Activity{
		Actor:        "user:" + userID,
		Verb:         "remixed",
		Object:       "loop:" + postID,
		ForeignID:    "loop:" + postID,
		AudioURL:     req.AudioURL,
		BPM:          req.BPM,
		Key:          req.Key,
		DAW:          req.DAW,
		Genre:        req.Genre,
		Extra: map[string]interface{}{
			"remix_of_post_id":  remixOfPostID,
			"remix_of_story_id": remixOfStoryID,
			"remix_chain_depth": remixChainDepth,
			"remix_type":        req.RemixType,
			"source_audio_url":  sourceAudioURL,
			"source_user_id":    sourceUserID,
		},
	}
	if midiPatternID != nil {
		activity.Extra["has_midi"] = true
		activity.Extra["midi_pattern_id"] = *midiPatternID
	}
	if sourceMIDIPatternID != nil {
		activity.Extra["source_midi_pattern_id"] = *sourceMIDIPatternID
	}

	if err := h.stream.CreateLoopActivity(userID, activity); err != nil {
		// Log error but don't fail - post is already created
		// TODO: Queue failed activity for retry using work queue (Redis/database)
		logger.Log.Error("Failed to create remix activity, post created but feed won't show activity",
			zap.String("user_id", userID),
			zap.String("post_id", postID),
			zap.Error(err),
		)
	}

	c.JSON(http.StatusCreated, gin.H{
		"post_id":           postID,
		"remix_of_post_id":  remixOfPostID,
		"remix_of_story_id": remixOfStoryID,
		"remix_chain_depth": remixChainDepth,
		"remix_type":        req.RemixType,
		"has_midi":          midiPatternID != nil,
		"timestamp":         time.Now().UTC(),
	})
}

// GetRemixChain returns the full remix chain for a post (original -> remix -> remix of remix)
// GET /api/v1/posts/:id/remix-chain
func (h *Handlers) GetRemixChain(c *gin.Context) {
	postID := c.Param("id")

	var post models.AudioPost
	if err := database.DB.Preload("User").First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			util.RespondNotFound(c, "post_not_found")
			return
		}
		util.RespondInternalError(c, "db_error")
		return
	}

	chain := []gin.H{}

	// Traverse up to find the original
	ancestors := []models.AudioPost{}
	current := &post
	for current.RemixOfPostID != nil {
		var parent models.AudioPost
		if err := database.DB.Preload("User").First(&parent, "id = ?", *current.RemixOfPostID).Error; err != nil {
			break
		}
		ancestors = append([]models.AudioPost{parent}, ancestors...) // Prepend
		current = &parent
	}

	// Check if original source was a story
	var originalStory *models.Story
	if len(ancestors) == 0 && post.RemixOfStoryID != nil {
		var story models.Story
		if err := database.DB.Preload("User").First(&story, "id = ?", *post.RemixOfStoryID).Error; err == nil {
			originalStory = &story
		}
	} else if len(ancestors) > 0 && ancestors[0].RemixOfStoryID != nil {
		var story models.Story
		if err := database.DB.Preload("User").First(&story, "id = ?", *ancestors[0].RemixOfStoryID).Error; err == nil {
			originalStory = &story
		}
	}

	// Add original story if exists
	if originalStory != nil {
		chain = append(chain, gin.H{
			"type":       "story",
			"id":         originalStory.ID,
			"user_id":    originalStory.UserID,
			"username":   originalStory.User.Username,
			"audio_url":  originalStory.AudioURL,
			"has_midi":   originalStory.MIDIPatternID != nil,
			"created_at": originalStory.CreatedAt,
			"depth":      0,
		})
	}

	// Add ancestor posts
	for i, ancestor := range ancestors {
		depth := i
		if originalStory != nil {
			depth = i + 1
		}
		chain = append(chain, gin.H{
			"type":        "post",
			"id":          ancestor.ID,
			"user_id":     ancestor.UserID,
			"username":    ancestor.User.Username,
			"audio_url":   ancestor.AudioURL,
			"has_midi":    ancestor.MIDIPatternID != nil,
			"remix_type":  ancestor.RemixType,
			"remix_count": ancestor.RemixCount,
			"created_at":  ancestor.CreatedAt,
			"depth":       depth,
		})
	}

	// Add current post
	currentDepth := len(ancestors)
	if originalStory != nil {
		currentDepth++
	}
	chain = append(chain, gin.H{
		"type":        "post",
		"id":          post.ID,
		"user_id":     post.UserID,
		"username":    post.User.Username,
		"audio_url":   post.AudioURL,
		"has_midi":    post.MIDIPatternID != nil,
		"remix_type":  post.RemixType,
		"remix_count": post.RemixCount,
		"created_at":  post.CreatedAt,
		"depth":       currentDepth,
		"is_current":  true,
	})

	c.JSON(http.StatusOK, gin.H{
		"chain":       chain,
		"total_depth": currentDepth,
	})
}

// GetPostRemixes returns all direct remixes of a post
// GET /api/v1/posts/:id/remixes
func (h *Handlers) GetPostRemixes(c *gin.Context) {
	postID := c.Param("id")
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Verify post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			util.RespondNotFound(c, "post_not_found")
			return
		}
		util.RespondInternalError(c, "db_error")
		return
	}

	var remixes []models.AudioPost
	var total int64

	if err := database.DB.Model(&models.AudioPost{}).Where("remix_of_post_id = ?", postID).Count(&total).Error; err != nil {
		logger.WarnWithFields("Failed to count remixes for post "+postID, err)
		total = 0
	}
	if err := database.DB.Preload("User").Where("remix_of_post_id = ?", postID).
		Order("created_at DESC").
		Limit(limit).Offset(offset).
		Find(&remixes).Error; err != nil {
		logger.WarnWithFields("Failed to find remixes for post "+postID, err)
	}

	results := make([]gin.H, len(remixes))
	for i, remix := range remixes {
		results[i] = gin.H{
			"id":          remix.ID,
			"user_id":     remix.UserID,
			"username":    remix.User.Username,
			"audio_url":   remix.AudioURL,
			"bpm":         remix.BPM,
			"key":         remix.Key,
			"has_midi":    remix.MIDIPatternID != nil,
			"remix_type":  remix.RemixType,
			"remix_count": remix.RemixCount,
			"created_at":  remix.CreatedAt,
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"remixes": results,
		"total":   total,
		"limit":   limit,
		"offset":  offset,
	})
}

// GetStoryRemixes returns all remixes of a story
// GET /api/v1/stories/:id/remixes
func (h *Handlers) GetStoryRemixes(c *gin.Context) {
	storyID := c.Param("id")
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Verify story exists
	var story models.Story
	if err := database.DB.First(&story, "id = ?", storyID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			util.RespondNotFound(c, "story_not_found")
			return
		}
		util.RespondInternalError(c, "db_error")
		return
	}

	var remixes []models.AudioPost
	var total int64

	if err := database.DB.Model(&models.AudioPost{}).Where("remix_of_story_id = ?", storyID).Count(&total).Error; err != nil {
		logger.WarnWithFields("Failed to count remixes for story "+storyID, err)
		total = 0
	}
	if err := database.DB.Preload("User").Where("remix_of_story_id = ?", storyID).
		Order("created_at DESC").
		Limit(limit).Offset(offset).
		Find(&remixes).Error; err != nil {
		logger.WarnWithFields("Failed to find remixes for story "+storyID, err)
	}

	results := make([]gin.H, len(remixes))
	for i, remix := range remixes {
		results[i] = gin.H{
			"id":          remix.ID,
			"user_id":     remix.UserID,
			"username":    remix.User.Username,
			"audio_url":   remix.AudioURL,
			"bpm":         remix.BPM,
			"key":         remix.Key,
			"has_midi":    remix.MIDIPatternID != nil,
			"remix_type":  remix.RemixType,
			"remix_count": remix.RemixCount,
			"created_at":  remix.CreatedAt,
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"remixes":  results,
		"total":    total,
		"limit":    limit,
		"offset":   offset,
		"story_id": storyID,
	})
}

// GetRemixSource returns the source content (audio URL and MIDI) for remixing
// GET /api/v1/posts/:id/remix-source
// GET /api/v1/stories/:id/remix-source
func (h *Handlers) GetRemixSource(c *gin.Context) {
	sourceID := c.Param("id")
	sourceType := "post"
	if c.FullPath() == "/api/v1/stories/:id/remix-source" {
		sourceType = "story"
	}

	response := gin.H{
		"source_type": sourceType,
		"source_id":   sourceID,
	}

	if sourceType == "post" {
		var post models.AudioPost
		if err := database.DB.Preload("User").Preload("MIDIPattern").First(&post, "id = ?", sourceID).Error; err != nil {
			if err == gorm.ErrRecordNotFound {
				util.RespondNotFound(c, "post_not_found")
				return
			}
			util.RespondInternalError(c, "db_error")
			return
		}

		response["audio_url"] = post.AudioURL
		response["bpm"] = post.BPM
		response["key"] = post.Key
		response["user_id"] = post.UserID
		response["username"] = post.User.Username
		response["has_midi"] = post.MIDIPatternID != nil
		response["is_remix"] = post.IsRemix()
		response["remix_chain_depth"] = post.RemixChainDepth

		if post.MIDIPattern != nil {
			response["midi_pattern_id"] = post.MIDIPattern.ID
			response["midi_data"] = gin.H{
				"events":         post.MIDIPattern.Events,
				"tempo":          post.MIDIPattern.Tempo,
				"time_signature": post.MIDIPattern.TimeSignature,
			}
		}
	} else {
		var story models.Story
		if err := database.DB.Preload("User").Preload("MIDIPattern").First(&story, "id = ?", sourceID).Error; err != nil {
			if err == gorm.ErrRecordNotFound {
				util.RespondNotFound(c, "story_not_found")
				return
			}
			util.RespondInternalError(c, "db_error")
			return
		}

		// Check if story is expired
		if story.ExpiresAt.Before(time.Now().UTC()) {
			util.RespondBadRequest(c, "story_expired", "Cannot remix an expired story")
			return
		}

		response["audio_url"] = story.AudioURL
		response["audio_duration"] = story.AudioDuration
		response["user_id"] = story.UserID
		response["username"] = story.User.Username
		response["has_midi"] = story.MIDIPatternID != nil
		response["expires_at"] = story.ExpiresAt

		if story.BPM != nil {
			response["bpm"] = *story.BPM
		}
		if story.Key != nil {
			response["key"] = *story.Key
		}

		if story.MIDIPattern != nil {
			response["midi_pattern_id"] = story.MIDIPattern.ID
			response["midi_data"] = gin.H{
				"events":         story.MIDIPattern.Events,
				"tempo":          story.MIDIPattern.Tempo,
				"time_signature": story.MIDIPattern.TimeSignature,
			}
// // 		} else if story.MIDIData != nil {
// // 			// Legacy: Use embedded MIDI data
// // 			response["midi_data"] = story.MIDIData
		}
	}

	c.JSON(http.StatusOK, response)
}
