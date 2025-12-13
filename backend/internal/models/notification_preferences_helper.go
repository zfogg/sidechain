package models

import (
	"gorm.io/gorm"
)

// NotificationPreferencesChecker provides methods to check notification preferences
// This is used by the stream client to check if notifications should be sent
type NotificationPreferencesChecker struct {
	db *gorm.DB
}

// NewNotificationPreferencesChecker creates a new checker with the given database
func NewNotificationPreferencesChecker(db *gorm.DB) *NotificationPreferencesChecker {
	return &NotificationPreferencesChecker{db: db}
}

// GetOrCreate gets or creates notification preferences for a user
func (c *NotificationPreferencesChecker) GetOrCreate(userID string) (*NotificationPreferences, error) {
	var prefs NotificationPreferences

	err := c.db.Where("user_id = ?", userID).First(&prefs).Error
	if err != nil {
		// Not found, create with defaults (all enabled)
		prefs = NotificationPreferences{
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

		if err := c.db.Create(&prefs).Error; err != nil {
			return nil, err
		}
	}

	return &prefs, nil
}

// IsEnabled checks if a specific notification type is enabled for a user
// verb should be one of: "like", "comment", "follow", "mention", "repost", "challenge_created", etc.
func (c *NotificationPreferencesChecker) IsEnabled(userID string, verb string) bool {
	prefs, err := c.GetOrCreate(userID)
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
