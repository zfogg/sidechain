package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/container"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// SoundHandlers handles Sound-related API endpoints.
// Uses dependency injection via container for service dependencies.
type SoundHandlers struct {
	container *container.Container
}

// NewSoundHandlers creates a new SoundHandlers instance.
// All dependencies are accessed through the container.
func NewSoundHandlers(c *container.Container) *SoundHandlers {
	return &SoundHandlers{
		container: c,
	}
}

// SoundResponse represents a sound in API responses
type SoundResponse struct {
	ID              string              `json:"id"`
	Name            string              `json:"name"`
	Description     string              `json:"description"`
	Duration        float64             `json:"duration"`
	CreatorID       string              `json:"creator_id"`
	Creator         *UserBriefResponse  `json:"creator,omitempty"`
	OriginalPostID  *string             `json:"original_post_id,omitempty"`
	UsageCount      int                 `json:"usage_count"`
	IsTrending      bool                `json:"is_trending"`
	TrendingRank    int                 `json:"trending_rank"`
	IsPublic        bool                `json:"is_public"`
	CreatedAt       string              `json:"created_at"`
}

// UserBriefResponse is a brief user info for sound responses
type UserBriefResponse struct {
	ID          string `json:"id"`
	Username    string `json:"username"`
	DisplayName string `json:"display_name"`
	AvatarURL   string `json:"avatar_url"`
}

// GetSound retrieves a sound by ID
// GET /api/v1/sounds/:id
func (h *SoundHandlers) GetSound(c *gin.Context) {
	soundID := c.Param("id")
	if soundID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Sound ID is required"})
		return
	}

	var sound models.Sound
	err := database.DB.
		Preload("Creator").
		Where("id = ?", soundID).
		First(&sound).Error

	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Sound not found"})
		return
	}

	response := soundToResponse(&sound)
	c.JSON(http.StatusOK, response)
}

// GetSoundPosts retrieves posts using a specific sound
// GET /api/v1/sounds/:id/posts
func (h *SoundHandlers) GetSoundPosts(c *gin.Context) {
	soundID := c.Param("id")
	if soundID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Sound ID is required"})
		return
	}

	// Get pagination params
	limit := 20
	offset := 0
	if l := c.Query("limit"); l != "" {
		if _, err := parseIntParam(l); err == nil {
			limit = min(50, max(1, mustParseInt(l)))
		}
	}
	if o := c.Query("offset"); o != "" {
		if _, err := parseIntParam(o); err == nil {
			offset = max(0, mustParseInt(o))
		}
	}

	// Get sound usages with posts
	var usages []models.SoundUsage
	err := database.DB.
		Preload("AudioPost").
		Preload("AudioPost.User").
		Where("sound_id = ?", soundID).
		Order("created_at DESC").
		Limit(limit).
		Offset(offset).
		Find(&usages).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch posts"})
		return
	}

	// Get total count
	var total int64
	database.DB.Model(&models.SoundUsage{}).
		Where("sound_id = ?", soundID).
		Count(&total)

	// Convert to response format
	var posts []gin.H
	for _, usage := range usages {
		if usage.AudioPost != nil {
			posts = append(posts, gin.H{
				"id":           usage.AudioPost.ID,
				"audio_url":    usage.AudioPost.AudioURL,
				"duration":     usage.AudioPost.Duration,
				"bpm":          usage.AudioPost.BPM,
				"key":          usage.AudioPost.Key,
				"waveform_url": usage.AudioPost.WaveformURL,
				"like_count":   usage.AudioPost.LikeCount,
				"play_count":   usage.AudioPost.PlayCount,
				"created_at":   usage.AudioPost.CreatedAt,
				"user": gin.H{
					"id":           usage.AudioPost.User.ID,
					"username":     usage.AudioPost.User.Username,
					"display_name": usage.AudioPost.User.DisplayName,
					"avatar_url":   usage.AudioPost.User.GetAvatarURL(),
				},
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"posts":  posts,
		"total":  total,
		"limit":  limit,
		"offset": offset,
	})
}

// GetTrendingSounds retrieves trending sounds
// GET /api/v1/sounds/trending
func (h *SoundHandlers) GetTrendingSounds(c *gin.Context) {
	limit := 20
	if l := c.Query("limit"); l != "" {
		if _, err := parseIntParam(l); err == nil {
			limit = min(50, max(1, mustParseInt(l)))
		}
	}

	var sounds []models.Sound
	err := database.DB.
		Preload("Creator").
		Where("is_public = ?", true).
		Order("usage_count DESC, created_at DESC").
		Limit(limit).
		Find(&sounds).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch trending sounds"})
		return
	}

	var response []SoundResponse
	for _, sound := range sounds {
		response = append(response, *soundToResponse(&sound))
	}

	c.JSON(http.StatusOK, gin.H{
		"sounds": response,
		"total":  len(response),
	})
}

// GetPostSound retrieves the sound associated with a post
// GET /api/v1/posts/:id/sound
func (h *SoundHandlers) GetPostSound(c *gin.Context) {
	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Find the fingerprint for this post
	var fingerprint models.AudioFingerprint
	err := database.DB.
		Preload("Sound").
		Preload("Sound.Creator").
		Where("audio_post_id = ?", postID).
		First(&fingerprint).Error

	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "No sound found for this post"})
		return
	}

	if fingerprint.Sound == nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Sound not yet processed"})
		return
	}

	response := soundToResponse(fingerprint.Sound)
	c.JSON(http.StatusOK, response)
}

// SearchSounds searches for sounds by name
// GET /api/v1/sounds/search
func (h *SoundHandlers) SearchSounds(c *gin.Context) {
	query := c.Query("q")
	if query == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Search query is required"})
		return
	}

	limit := 20
	if l := c.Query("limit"); l != "" {
		if _, err := parseIntParam(l); err == nil {
			limit = min(50, max(1, mustParseInt(l)))
		}
	}

	var sounds []models.Sound
	err := database.DB.
		Preload("Creator").
		Where("is_public = ? AND name ILIKE ?", true, "%"+query+"%").
		Order("usage_count DESC").
		Limit(limit).
		Find(&sounds).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Search failed"})
		return
	}

	var response []SoundResponse
	for _, sound := range sounds {
		response = append(response, *soundToResponse(&sound))
	}

	c.JSON(http.StatusOK, gin.H{
		"sounds": response,
		"total":  len(response),
	})
}

// UpdateSoundName allows the creator to update the sound name
// PATCH /api/v1/sounds/:id
func (h *SoundHandlers) UpdateSound(c *gin.Context) {
	soundID := c.Param("id")
	userID := c.GetString("user_id")

	if soundID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Sound ID is required"})
		return
	}

	var sound models.Sound
	err := database.DB.Where("id = ?", soundID).First(&sound).Error
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "Sound not found"})
		return
	}

	// Only creator can update
	if sound.CreatorID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Only the creator can update this sound"})
		return
	}

	var input struct {
		Name        *string `json:"name"`
		Description *string `json:"description"`
		IsPublic    *bool   `json:"is_public"`
	}

	if err := c.ShouldBindJSON(&input); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid request body"})
		return
	}

	updates := make(map[string]interface{})
	if input.Name != nil && *input.Name != "" {
		updates["name"] = *input.Name
	}
	if input.Description != nil {
		updates["description"] = *input.Description
	}
	if input.IsPublic != nil {
		updates["is_public"] = *input.IsPublic
	}

	if len(updates) > 0 {
		err = database.DB.Model(&sound).Updates(updates).Error
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update sound"})
			return
		}
	}

	// Reload and return updated sound
	database.DB.Preload("Creator").First(&sound, "id = ?", soundID)
	c.JSON(http.StatusOK, soundToResponse(&sound))
}

// Helper functions

func soundToResponse(sound *models.Sound) *SoundResponse {
	response := &SoundResponse{
		ID:             sound.ID,
		Name:           sound.Name,
		Description:    sound.Description,
		Duration:       sound.Duration,
		CreatorID:      sound.CreatorID,
		OriginalPostID: sound.OriginalPostID,
		UsageCount:     sound.UsageCount,
		IsTrending:     sound.IsTrending,
		TrendingRank:   sound.TrendingRank,
		IsPublic:       sound.IsPublic,
		CreatedAt:      sound.CreatedAt.Format("2006-01-02T15:04:05Z"),
	}

	if sound.Creator.ID != "" {
		response.Creator = &UserBriefResponse{
			ID:          sound.Creator.ID,
			Username:    sound.Creator.Username,
			DisplayName: sound.Creator.DisplayName,
			AvatarURL:   sound.Creator.GetAvatarURL(),
		}
	}

	return response
}

func parseIntParam(s string) (int, error) {
	var i int
	_, err := parseIntSafe(s, &i)
	return i, err
}

func parseIntSafe(s string, target *int) (bool, error) {
	if s == "" {
		return false, nil
	}
	n := 0
	for _, c := range s {
		if c < '0' || c > '9' {
			return false, nil
		}
		n = n*10 + int(c-'0')
	}
	*target = n
	return true, nil
}

func mustParseInt(s string) int {
	n := 0
	for _, c := range s {
		if c >= '0' && c <= '9' {
			n = n*10 + int(c-'0')
		}
	}
	return n
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}
