package handlers

import (
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

// CreateMIDIPattern creates a new MIDI pattern
// POST /api/v1/midi
func (h *Handlers) CreateMIDIPattern(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	var req struct {
		Name          string             `json:"name"`
		Description   string             `json:"description"`
		Events        []models.MIDIEvent `json:"events" binding:"required"`
		TotalTime     float64            `json:"total_time" binding:"required"`
		Tempo         int                `json:"tempo" binding:"required,min=1"`
		TimeSignature []int              `json:"time_signature" binding:"required"`
		Key           string             `json:"key"`
		IsPublic      *bool              `json:"is_public"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	// Validate time signature
	if len(req.TimeSignature) != 2 || req.TimeSignature[0] <= 0 || req.TimeSignature[1] <= 0 {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_time_signature",
			"message": "time_signature must be [numerator, denominator] with positive values",
		})
		return
	}

	// Default to public if not specified
	isPublic := true
	if req.IsPublic != nil {
		isPublic = *req.IsPublic
	}

	pattern := &models.MIDIPattern{
		UserID:        currentUser.ID,
		Name:          req.Name,
		Description:   req.Description,
		Events:        req.Events,
		TotalTime:     req.TotalTime,
		Tempo:         req.Tempo,
		TimeSignature: req.TimeSignature,
		Key:           req.Key,
		IsPublic:      isPublic,
	}

	// Validate the pattern
	if err := pattern.Validate(); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "validation_failed",
			"message": err.Error(),
		})
		return
	}

	if err := database.DB.Create(pattern).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "creation_failed",
			"message": "Failed to create MIDI pattern",
		})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"id":           pattern.ID,
		"name":         pattern.Name,
		"download_url": "/api/v1/midi/" + pattern.ID + "/file",
		"message":      "MIDI pattern created successfully",
	})
}

// GetMIDIPattern gets a MIDI pattern by ID
// GET /api/v1/midi/:id
func (h *Handlers) GetMIDIPattern(c *gin.Context) {
	patternID := c.Param("id")
	userID := c.GetString("user_id")

	var pattern models.MIDIPattern
	if err := database.DB.Preload("User").First(&pattern, "id = ?", patternID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "midi_pattern_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_pattern"})
		return
	}

	// Check permissions: must be public or owned by requesting user
	if !pattern.IsPublic && pattern.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "access_denied"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"id":             pattern.ID,
		"user_id":        pattern.UserID,
		"user":           pattern.User,
		"name":           pattern.Name,
		"description":    pattern.Description,
		"events":         pattern.Events,
		"total_time":     pattern.TotalTime,
		"tempo":          pattern.Tempo,
		"time_signature": pattern.TimeSignature,
		"key":            pattern.Key,
		"is_public":      pattern.IsPublic,
		"download_count": pattern.DownloadCount,
		"created_at":     pattern.CreatedAt,
	})
}

// GetMIDIPatternFile downloads a MIDI pattern as a .mid file
// GET /api/v1/midi/:id/file
func (h *Handlers) GetMIDIPatternFile(c *gin.Context) {
	patternID := c.Param("id")
	userID := c.GetString("user_id")

	var pattern models.MIDIPattern
	if err := database.DB.First(&pattern, "id = ?", patternID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "midi_pattern_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_pattern"})
		return
	}

	// Check permissions
	if !pattern.IsPublic && pattern.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "access_denied"})
		return
	}

	// Increment download count
	database.DB.Model(&pattern).UpdateColumn("download_count", gorm.Expr("download_count + 1"))

	// Generate MIDI file
	midiData := pattern.ToMIDIData()
	midiBytes, err := GenerateMIDIFile(midiData, pattern.Name)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "midi_generation_failed",
			"message": err.Error(),
		})
		return
	}

	// Determine filename
	filename := pattern.Name
	if filename == "" {
		filename = "pattern_" + pattern.ID[:8]
	}
	filename += ".mid"

	c.Header("Content-Type", "audio/midi")
	c.Header("Content-Disposition", "attachment; filename=\""+filename+"\"")
	c.Header("Content-Length", strconv.Itoa(len(midiBytes)))
	c.Data(http.StatusOK, "audio/midi", midiBytes)
}

// ListMIDIPatterns lists MIDI patterns for the current user
// GET /api/v1/midi
func (h *Handlers) ListMIDIPatterns(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))
	publicOnly := c.Query("public") == "true"
	sortBy := c.DefaultQuery("sort", "created_at")

	query := database.DB.Where("user_id = ?", currentUser.ID)

	if publicOnly {
		query = query.Where("is_public = true")
	}

	// Apply sorting
	switch sortBy {
	case "download_count":
		query = query.Order("download_count DESC")
	case "name":
		query = query.Order("name ASC")
	default:
		query = query.Order("created_at DESC")
	}

	var patterns []models.MIDIPattern
	var total int64

	query.Model(&models.MIDIPattern{}).Count(&total)
	query.Limit(limit).Offset(offset).Find(&patterns)

	c.JSON(http.StatusOK, gin.H{
		"patterns": patterns,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"total":  total,
		},
	})
}

// DeleteMIDIPattern soft-deletes a MIDI pattern
// DELETE /api/v1/midi/:id
func (h *Handlers) DeleteMIDIPattern(c *gin.Context) {
	patternID := c.Param("id")
	userID := c.GetString("user_id")

	var pattern models.MIDIPattern
	if err := database.DB.First(&pattern, "id = ?", patternID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "midi_pattern_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_pattern"})
		return
	}

	// Check ownership
	if pattern.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "not_pattern_owner"})
		return
	}

	// Check if pattern is referenced by posts or stories
	var postCount, storyCount int64
	database.DB.Model(&models.AudioPost{}).Where("midi_pattern_id = ?", patternID).Count(&postCount)
	database.DB.Model(&models.Story{}).Where("midi_pattern_id = ?", patternID).Count(&storyCount)

	if postCount > 0 || storyCount > 0 {
		c.JSON(http.StatusConflict, gin.H{
			"error":   "pattern_in_use",
			"message": "MIDI pattern is referenced by posts or stories",
			"references": gin.H{
				"posts":   postCount,
				"stories": storyCount,
			},
		})
		return
	}

	// Soft delete
	if err := database.DB.Delete(&pattern).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_delete_pattern"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "midi_pattern_deleted"})
}

// UpdateMIDIPattern updates a MIDI pattern's metadata
// PATCH /api/v1/midi/:id
func (h *Handlers) UpdateMIDIPattern(c *gin.Context) {
	patternID := c.Param("id")
	userID := c.GetString("user_id")

	var pattern models.MIDIPattern
	if err := database.DB.First(&pattern, "id = ?", patternID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "midi_pattern_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_pattern"})
		return
	}

	// Check ownership
	if pattern.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "not_pattern_owner"})
		return
	}

	var req struct {
		Name        *string `json:"name"`
		Description *string `json:"description"`
		Key         *string `json:"key"`
		IsPublic    *bool   `json:"is_public"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	updates := map[string]interface{}{}
	if req.Name != nil {
		updates["name"] = *req.Name
	}
	if req.Description != nil {
		updates["description"] = *req.Description
	}
	if req.Key != nil {
		updates["key"] = *req.Key
	}
	if req.IsPublic != nil {
		updates["is_public"] = *req.IsPublic
	}

	if len(updates) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "no_updates_provided"})
		return
	}

	if err := database.DB.Model(&pattern).Updates(updates).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_update_pattern"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "midi_pattern_updated",
		"pattern": pattern,
	})
}
