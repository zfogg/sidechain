package service

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/logger"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
)

// NotificationService provides notification-related operations
type NotificationService struct{}

// NewNotificationService creates a new notification service
func NewNotificationService() *NotificationService {
	return &NotificationService{}
}

// ListNotifications displays the user's notifications
func (ns *NotificationService) ListNotifications(page, pageSize int) error {
	logger.Debug("Listing notifications", "page", page)

	notifications, err := api.GetNotifications(page, pageSize)
	if err != nil {
		return fmt.Errorf("failed to list notifications: %w", err)
	}

	if len(notifications.Notifications) == 0 {
		fmt.Println("No notifications.")
		return nil
	}

	ns.displayNotifications(notifications)
	return nil
}

// GetUnreadCount displays the count of unread notifications
func (ns *NotificationService) GetUnreadCount() error {
	logger.Debug("Getting unread notification count")

	count, err := api.GetUnreadCount()
	if err != nil {
		return fmt.Errorf("failed to get unread count: %w", err)
	}

	if count == 0 {
		fmt.Println("No unread notifications.")
		return nil
	}

	fmt.Printf("📬 %d unread notification%s\n", count, pluralize(count))
	return nil
}

// MarkNotificationAsRead marks a notification as read
func (ns *NotificationService) MarkNotificationAsRead(notificationID string) error {
	logger.Debug("Marking notification as read", "notification_id", notificationID)

	if err := api.MarkNotificationAsRead(notificationID); err != nil {
		return fmt.Errorf("failed to mark notification as read: %w", err)
	}

	fmt.Printf("✓ Notification marked as read.\n")
	return nil
}

// MarkAllAsRead marks all notifications as read
func (ns *NotificationService) MarkAllAsRead() error {
	logger.Debug("Marking all notifications as read")

	// Confirm before marking all
	confirm, err := prompter.PromptConfirm("Mark all notifications as read?")
	if err != nil {
		return err
	}

	if !confirm {
		fmt.Println("Cancelled.")
		return nil
	}

	if err := api.MarkAllNotificationsAsRead(); err != nil {
		return fmt.Errorf("failed to mark all as read: %w", err)
	}

	fmt.Printf("✓ All notifications marked as read.\n")
	return nil
}

// ViewPreferences displays the user's notification preferences
func (ns *NotificationService) ViewPreferences() error {
	logger.Debug("Viewing notification preferences")

	prefs, err := api.GetNotificationPreferences()
	if err != nil {
		return fmt.Errorf("failed to get preferences: %w", err)
	}

	ns.displayPreferences(prefs)
	return nil
}

// EditPreferences interactively edits notification preferences
func (ns *NotificationService) EditPreferences() error {
	logger.Debug("Editing notification preferences")

	// Get current preferences
	prefs, err := api.GetNotificationPreferences()
	if err != nil {
		return fmt.Errorf("failed to get preferences: %w", err)
	}

	// Display current settings
	fmt.Println("\nCurrent Notification Settings:")
	ns.displayPreferences(prefs)

	fmt.Println("\nEdit preferences (y/n for each):")

	// Edit each preference
	prefs.PostLikes, _ = prompter.PromptConfirm("  Post likes")
	prefs.PostComments, _ = prompter.PromptConfirm("  Post comments")
	prefs.PostReposts, _ = prompter.PromptConfirm("  Post reposts")
	prefs.Follows, _ = prompter.PromptConfirm("  New followers")
	prefs.Messages, _ = prompter.PromptConfirm("  Messages")
	prefs.Mentions, _ = prompter.PromptConfirm("  Mentions")
	prefs.ReportUpdates, _ = prompter.PromptConfirm("  Report updates")
	prefs.NewChallenge, _ = prompter.PromptConfirm("  New challenges")

	// Update preferences
	if err := api.UpdateNotificationPreferences(prefs); err != nil {
		return fmt.Errorf("failed to update preferences: %w", err)
	}

	fmt.Printf("✓ Notification preferences updated.\n")
	return nil
}

// Helper functions

func (ns *NotificationService) displayNotifications(notifs *api.NotificationListResponse) {
	fmt.Printf("\n🔔 Notifications (%d)\n\n", len(notifs.Notifications))

	typeEmoji := map[string]string{
		"like":    "❤️",
		"comment": "💬",
		"follow":  "👥",
		"mention": "🏷️",
		"repost":  "🔄",
	}

	for i, notif := range notifs.Notifications {
		emoji := "📧"
		if e, ok := typeEmoji[notif.Type]; ok {
			emoji = e
		}

		readMarker := "  "
		if notif.Read {
			readMarker = "✓ "
		}

		fmt.Printf("%s [%d] %s %s\n", readMarker, i+1, emoji, notif.Type)
		fmt.Printf("     ID: %s\n", notif.ID)
		fmt.Printf("     %s\n", notif.CreatedAt.Format("2006-01-02 15:04:05"))
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d notifications (Page %d)\n\n", len(notifs.Notifications), notifs.TotalCount, notifs.Page)
}

func (ns *NotificationService) displayPreferences(prefs *api.NotificationPreferences) {
	fmt.Printf("\n🔔 Notification Settings\n")
	fmt.Printf("  Post Likes:        %s\n", boolToStatus(prefs.PostLikes))
	fmt.Printf("  Post Comments:     %s\n", boolToStatus(prefs.PostComments))
	fmt.Printf("  Post Reposts:      %s\n", boolToStatus(prefs.PostReposts))
	fmt.Printf("  New Followers:     %s\n", boolToStatus(prefs.Follows))
	fmt.Printf("  Messages:          %s\n", boolToStatus(prefs.Messages))
	fmt.Printf("  Mentions:          %s\n", boolToStatus(prefs.Mentions))
	fmt.Printf("  Report Updates:    %s\n", boolToStatus(prefs.ReportUpdates))
	fmt.Printf("  New Challenges:    %s\n\n", boolToStatus(prefs.NewChallenge))
}

func boolToStatus(b bool) string {
	if b {
		return "✓ ON"
	}
	return "✗ OFF"
}

func pluralize(count int) string {
	if count == 1 {
		return ""
	}
	return "s"
}
