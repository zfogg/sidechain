package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
	"gorm.io/gorm"
)

// MuteUser mutes a user for the current user
// Muting hides the muted user's posts from the feed without unfollowing
// POST /api/v1/users/:id/mute
func (h *Handlers) MuteUser(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	targetUserID := c.Param("id")
	if targetUserID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "User ID is required"})
		return
	}

	// Can't mute yourself
	if userID == targetUserID {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Cannot mute yourself"})
		return
	}

	// Check if target user exists
	var targetUser models.User
	if err := database.DB.First(&targetUser, "id = ?", targetUserID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to find user"})
		return
	}

	// Check if already muted
	var existingMute models.MutedUser
	err := database.DB.Where("user_id = ? AND muted_user_id = ?", userID, targetUserID).First(&existingMute).Error
	if err == nil {
		// Already muted
		c.JSON(http.StatusOK, gin.H{
			"message": "User already muted",
			"muted":   true,
		})
		return
	}

	// Create muted user record
	mutedUser := models.MutedUser{
		UserID:      userID,
		MutedUserID: targetUserID,
	}

	if err := database.DB.Create(&mutedUser).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to mute user"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":  "User muted successfully",
		"muted":    true,
		"muted_at": mutedUser.CreatedAt,
	})
}

// UnmuteUser unmutes a user for the current user
// DELETE /api/v1/users/:id/mute
func (h *Handlers) UnmuteUser(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	targetUserID := c.Param("id")
	if targetUserID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "User ID is required"})
		return
	}

	// Find and delete the muted user record
	result := database.DB.Where("user_id = ? AND muted_user_id = ?", userID, targetUserID).Delete(&models.MutedUser{})
	if result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to unmute user"})
		return
	}

	if result.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "User was not muted"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "User unmuted successfully",
		"muted":   false,
	})
}

// GetMutedUsers returns the current user's muted users list
// GET /api/v1/users/me/muted
func (h *Handlers) GetMutedUsers(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	// Parse pagination
	limit := util.ParseInt(c.DefaultQuery("limit", "50"), 50)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	if limit > 100 {
		limit = 100
	}

	// Get muted users with user details
	var mutedUsers []models.MutedUser
	err := database.DB.
		Preload("MutedUser").
		Where("user_id = ?", userID).
		Order("created_at DESC").
		Limit(limit).
		Offset(offset).
		Find(&mutedUsers).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get muted users"})
		return
	}

	// Get total count
	var totalCount int64
	if err := database.DB.Model(&models.MutedUser{}).Where("user_id = ?", userID).Count(&totalCount).Error; err != nil {
		logger.WarnWithFields("Failed to count muted users for "+userID, err)
		totalCount = 0 // Default to 0 on error
	}

	// Transform to response format
	users := make([]gin.H, len(mutedUsers))
	for i, mu := range mutedUsers {
		users[i] = gin.H{
			"id":           mu.MutedUser.ID,
			"username":     mu.MutedUser.Username,
			"display_name": mu.MutedUser.DisplayName,
			"avatar_url":   mu.MutedUser.GetAvatarURL(),
			"muted_at":     mu.CreatedAt,
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"users":       users,
		"total_count": totalCount,
		"limit":       limit,
		"offset":      offset,
		"has_more":    offset+len(users) < int(totalCount),
	})
}

// IsUserMuted checks if the current user has muted a specific user
// GET /api/v1/users/:id/muted
func (h *Handlers) IsUserMuted(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	targetUserID := c.Param("id")
	if targetUserID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "User ID is required"})
		return
	}

	var mutedUser models.MutedUser
	err := database.DB.Where("user_id = ? AND muted_user_id = ?", userID, targetUserID).First(&mutedUser).Error

	// Return muted_at as null if user is not muted
	var mutedAt *time.Time
	if err == nil {
		mutedAt = &mutedUser.CreatedAt
	}

	c.JSON(http.StatusOK, gin.H{
		"muted":    err == nil,
		"muted_at": mutedAt,
	})
}

// GetMutedUserIDs returns just the IDs of muted users for feed filtering
// This is an internal helper used by feed queries
func GetMutedUserIDs(userID string) ([]string, error) {
	var mutedUserIDs []string
	err := database.DB.Model(&models.MutedUser{}).
		Where("user_id = ?", userID).
		Pluck("muted_user_id", &mutedUserIDs).Error
	return mutedUserIDs, err
}
