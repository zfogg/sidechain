package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// GetNotifications retrieves notifications with pagination
func GetNotifications(page, pageSize int) (*NotificationListResponse, error) {
	logger.Debug("Fetching notifications", "page", page)

	var response NotificationListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/notifications")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch notifications: %s", resp.Status())
	}

	return &response, nil
}

// GetUnreadCount retrieves the count of unread notifications
func GetUnreadCount() (int, error) {
	logger.Debug("Fetching unread notification count")

	var response struct {
		UnreadCount int `json:"unread_count"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get("/api/v1/notifications/unread/count")

	if err != nil {
		return 0, err
	}

	if !resp.IsSuccess() {
		return 0, fmt.Errorf("failed to fetch unread count: %s", resp.Status())
	}

	return response.UnreadCount, nil
}

// MarkNotificationAsRead marks a single notification as read
func MarkNotificationAsRead(notificationID string) error {
	logger.Debug("Marking notification as read", "notification_id", notificationID)

	resp, err := client.GetClient().
		R().
		Patch(fmt.Sprintf("/api/v1/notifications/%s/read", notificationID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to mark notification as read: %s", resp.Status())
	}

	return nil
}

// MarkAllNotificationsAsRead marks all notifications as read
func MarkAllNotificationsAsRead() error {
	logger.Debug("Marking all notifications as read")

	resp, err := client.GetClient().
		R().
		Patch("/api/v1/notifications/read-all")

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to mark all notifications as read: %s", resp.Status())
	}

	return nil
}

// NotificationPreferences represents user notification preferences
type NotificationPreferences struct {
	PostLikes     bool `json:"post_likes"`
	PostComments  bool `json:"post_comments"`
	PostReposts   bool `json:"post_reposts"`
	Follows       bool `json:"follows"`
	Messages      bool `json:"messages"`
	Mentions      bool `json:"mentions"`
	ReportUpdates bool `json:"report_updates"`
	NewChallenge  bool `json:"new_challenge"`
}

// GetNotificationPreferences retrieves the user's notification preferences
func GetNotificationPreferences() (*NotificationPreferences, error) {
	logger.Debug("Fetching notification preferences")

	var response struct {
		Preferences NotificationPreferences `json:"preferences"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get("/api/v1/notifications/preferences")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch preferences: %s", resp.Status())
	}

	return &response.Preferences, nil
}

// UpdateNotificationPreferences updates the user's notification preferences
func UpdateNotificationPreferences(prefs *NotificationPreferences) error {
	logger.Debug("Updating notification preferences")

	resp, err := client.GetClient().
		R().
		SetBody(prefs).
		Put("/api/v1/notifications/preferences")

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to update preferences: %s", resp.Status())
	}

	return nil
}
