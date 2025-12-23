package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/util"
	"go.uber.org/zap"
)

// GetActivityStatusSettings gets the current user's activity status privacy settings
// GET /api/v1/settings/activity-status
func (h *Handlers) GetActivityStatusSettings(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"show_activity_status": currentUser.ShowActivityStatus,
		"show_last_active":     currentUser.ShowLastActive,
	})
}

// UpdateActivityStatusSettingsRequest is the request body for updating activity status settings
type UpdateActivityStatusSettingsRequest struct {
	ShowActivityStatus *bool `json:"show_activity_status"`
	ShowLastActive     *bool `json:"show_last_active"`
}

// UpdateActivityStatusSettings updates the current user's activity status privacy settings
// PUT /api/v1/settings/activity-status
func (h *Handlers) UpdateActivityStatusSettings(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var req UpdateActivityStatusSettingsRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", "Invalid request body")
		return
	}

	// Build update map for only provided fields
	updates := make(map[string]interface{})
	if req.ShowActivityStatus != nil {
		updates["show_activity_status"] = *req.ShowActivityStatus
	}
	if req.ShowLastActive != nil {
		updates["show_last_active"] = *req.ShowLastActive
	}

	if len(updates) == 0 {
		util.RespondBadRequest(c, "no_fields", "No fields to update")
		return
	}

	// Update the user
	if err := database.DB.Model(&currentUser).Updates(updates).Error; err != nil {
		util.RespondInternalError(c, "failed_to_update", err.Error())
		return
	}

	// Reload user to get updated values
	if err := database.DB.First(&currentUser, "id = ?", currentUser.ID).Error; err != nil {
		logger.Log.Warn("Failed to reload user after activity status update", zap.Error(err))
	}

	c.JSON(http.StatusOK, gin.H{
		"status":               "success",
		"message":              "activity_status_settings_updated",
		"show_activity_status": currentUser.ShowActivityStatus,
		"show_last_active":     currentUser.ShowLastActive,
	})
}

// ShouldShowActivityStatus checks if a user's activity status should be visible
// Used when returning user profiles to other users
func ShouldShowActivityStatus(userID string) bool {
	var showStatus bool
	err := database.DB.Model(&struct{}{}).Table("users").
		Select("show_activity_status").
		Where("id = ?", userID).
		Scan(&showStatus).Error
	if err != nil {
		return true // Default to showing on error
	}
	return showStatus
}

// ShouldShowLastActive checks if a user's last active time should be visible
// Used when returning user profiles to other users
func ShouldShowLastActive(userID string) bool {
	var showLastActive bool
	err := database.DB.Model(&struct{}{}).Table("users").
		Select("show_last_active").
		Where("id = ?", userID).
		Scan(&showLastActive).Error
	if err != nil {
		return true // Default to showing on error
	}
	return showLastActive
}
