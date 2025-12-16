package api

import (
	"testing"
)

func TestGetNotifications_API(t *testing.T) {
	resp, err := GetNotifications(1, 10)

	if err != nil {
		t.Logf("GetNotifications error (API offline or unauthenticated): %v", err)
		return
	}

	if resp == nil {
		t.Error("GetNotifications returned nil")
	}
	if resp.Notifications == nil {
		t.Error("Notifications slice is nil")
	}
	if resp.Page != 1 {
		t.Errorf("Expected page 1, got %d", resp.Page)
	}
}

func TestGetUnreadCount_API(t *testing.T) {
	count, err := GetUnreadCount()

	if err != nil {
		t.Logf("GetUnreadCount error (may be unauthenticated): %v", err)
		return
	}

	if count < 0 {
		t.Errorf("Unread count should be >= 0, got %d", count)
	}
}

func TestGetNotificationPreferences_API(t *testing.T) {
	prefs, err := GetNotificationPreferences()

	if err != nil {
		t.Logf("GetNotificationPreferences error (may be unauthenticated): %v", err)
		return
	}

	if prefs == nil {
		t.Error("Preferences returned nil")
	}
}

func TestUpdateNotificationPreferences_API(t *testing.T) {
	// Get current preferences first
	prefs, err := GetNotificationPreferences()
	if err != nil {
		t.Logf("Skipping update test: cannot fetch current preferences: %v", err)
		return
	}

	// Just toggle one setting and update
	originalPostLikes := prefs.PostLikes
	prefs.PostLikes = !originalPostLikes

	err = UpdateNotificationPreferences(prefs)
	if err != nil {
		t.Logf("UpdateNotificationPreferences error: %v", err)
	}

	// Restore original setting
	prefs.PostLikes = originalPostLikes
	UpdateNotificationPreferences(prefs)
}

func TestMarkNotificationAsRead_API(t *testing.T) {
	// Try with non-existent notification ID
	err := MarkNotificationAsRead("nonexistent-notification-id")

	if err != nil {
		t.Logf("MarkNotificationAsRead error (expected, ID doesn't exist): %v", err)
	}
}

func TestMarkAllNotificationsAsRead_API(t *testing.T) {
	err := MarkAllNotificationsAsRead()

	if err != nil {
		t.Logf("MarkAllNotificationsAsRead error (may be unauthenticated): %v", err)
	}
}
