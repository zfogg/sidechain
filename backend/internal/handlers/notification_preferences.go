package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
)

// GetNotificationPreferences gets the current user's notification preferences
// GET /api/v1/notifications/preferences
func (h *Handlers) GetNotificationPreferences(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	// Get or create notification preferences
	prefs, err := getOrCreateNotificationPreferences(currentUser.ID)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_preferences", err.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"preferences": prefs,
	})
}

// UpdateNotificationPreferencesRequest is the request body for updating notification preferences
type UpdateNotificationPreferencesRequest struct {
	LikesEnabled      *bool `json:"likes_enabled"`
	CommentsEnabled   *bool `json:"comments_enabled"`
	FollowsEnabled    *bool `json:"follows_enabled"`
	MentionsEnabled   *bool `json:"mentions_enabled"`
	DMsEnabled        *bool `json:"dms_enabled"`
	StoriesEnabled    *bool `json:"stories_enabled"`
	RepostsEnabled    *bool `json:"reposts_enabled"`
	ChallengesEnabled *bool `json:"challenges_enabled"`
}

// UpdateNotificationPreferences updates the current user's notification preferences
// PUT /api/v1/notifications/preferences
func (h *Handlers) UpdateNotificationPreferences(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var req UpdateNotificationPreferencesRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", "Invalid request body")
		return
	}

	// Get or create notification preferences
	prefs, err := getOrCreateNotificationPreferences(currentUser.ID)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_preferences", err.Error())
		return
	}

	// Update only the fields that were provided
	if req.LikesEnabled != nil {
		prefs.LikesEnabled = *req.LikesEnabled
	}
	if req.CommentsEnabled != nil {
		prefs.CommentsEnabled = *req.CommentsEnabled
	}
	if req.FollowsEnabled != nil {
		prefs.FollowsEnabled = *req.FollowsEnabled
	}
	if req.MentionsEnabled != nil {
		prefs.MentionsEnabled = *req.MentionsEnabled
	}
	if req.DMsEnabled != nil {
		prefs.DMsEnabled = *req.DMsEnabled
	}
	if req.StoriesEnabled != nil {
		prefs.StoriesEnabled = *req.StoriesEnabled
	}
	if req.RepostsEnabled != nil {
		prefs.RepostsEnabled = *req.RepostsEnabled
	}
	if req.ChallengesEnabled != nil {
		prefs.ChallengesEnabled = *req.ChallengesEnabled
	}

	// Save the updated preferences
	if err := database.DB.Save(prefs).Error; err != nil {
		util.RespondInternalError(c, "failed_to_save_preferences", err.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "success",
		"message":     "notification_preferences_updated",
		"preferences": prefs,
	})
}

// getOrCreateNotificationPreferences gets or creates notification preferences for a user
// Returns a preferences object with all defaults set to true if not found
func getOrCreateNotificationPreferences(userID string) (*models.NotificationPreferences, error) {
	var prefs models.NotificationPreferences

	err := database.DB.Where("user_id = ?", userID).First(&prefs).Error
	if err != nil {
		// Not found, create with defaults
		prefs = models.NotificationPreferences{
			UserID:            userID,
			LikesEnabled:      true,
			CommentsEnabled:   true,
			FollowsEnabled:    true,
			MentionsEnabled:   true,
			DMsEnabled:        true,
			StoriesEnabled:    true,
			RepostsEnabled:    true,
			ChallengesEnabled: true,
		}

		if err := database.DB.Create(&prefs).Error; err != nil {
			return nil, err
		}
	}

	return &prefs, nil
}

// GetNotificationPreferencesForUser gets notification preferences for a specific user (internal use)
// Used by the stream client to check if notifications should be sent
func GetNotificationPreferencesForUser(userID string) (*models.NotificationPreferences, error) {
	return getOrCreateNotificationPreferences(userID)
}

// IsNotificationEnabled checks if a specific notification type is enabled for a user
// verb should be one of: "like", "comment", "follow", "mention", "repost", "challenge_created", etc.
func IsNotificationEnabled(userID string, verb string) bool {
	prefs, err := getOrCreateNotificationPreferences(userID)
	if err != nil {
		// On error, default to allowing notifications
		return true
	}

	switch verb {
	case "like":
		return prefs.LikesEnabled
	case "comment":
		return prefs.CommentsEnabled
	case "follow":
		return prefs.FollowsEnabled
	case "mention":
		return prefs.MentionsEnabled
	case "repost":
		return prefs.RepostsEnabled
	case "challenge_created", "challenge_deadline", "challenge_voting", "challenge_ended":
		return prefs.ChallengesEnabled
	default:
		// Unknown verb types are allowed by default
		return true
	}
}
