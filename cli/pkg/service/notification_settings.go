package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// NotificationSettingsService manages notification preferences
type NotificationSettingsService struct{}

// NewNotificationSettingsService creates a new notification settings service
func NewNotificationSettingsService() *NotificationSettingsService {
	return &NotificationSettingsService{}
}

// ViewPreferences displays the user's notification preferences
func (ns *NotificationSettingsService) ViewPreferences() error {
	logger.Debug("Viewing notification preferences")

	prefs, err := api.GetNotificationPreferences()
	if err != nil {
		return err
	}

	ns.displayPreferences(prefs)
	return nil
}

// ManagePreferences interactively allows user to update notification preferences
func (ns *NotificationSettingsService) ManagePreferences() error {
	logger.Debug("Managing notification preferences")

	// Get current preferences
	prefs, err := api.GetNotificationPreferences()
	if err != nil {
		return err
	}

	formatter.PrintInfo("🔔 Notification Settings")
	fmt.Println("\nConfigure which notifications you want to receive:")

	reader := bufio.NewReader(os.Stdin)

	// Helper function to get yes/no input
	getToggle := func(prompt string, current bool) bool {
		fmt.Printf("%s [%s]: ", prompt, boolToString(current))
		response, _ := reader.ReadString('\n')
		response = strings.TrimSpace(strings.ToLower(response))
		if response == "" {
			return current
		}
		return response == "y" || response == "yes"
	}

	// Update preferences based on user input
	prefs.PostLikes = getToggle("Receive notifications for post likes?", prefs.PostLikes)
	prefs.PostComments = getToggle("Receive notifications for post comments?", prefs.PostComments)
	prefs.PostReposts = getToggle("Receive notifications for post reposts?", prefs.PostReposts)
	prefs.Follows = getToggle("Receive notifications when someone follows you?", prefs.Follows)
	prefs.Messages = getToggle("Receive notifications for new messages?", prefs.Messages)
	prefs.Mentions = getToggle("Receive notifications when mentioned?", prefs.Mentions)
	prefs.ReportUpdates = getToggle("Receive notifications for report updates?", prefs.ReportUpdates)
	prefs.NewChallenge = getToggle("Receive notifications for new challenges?", prefs.NewChallenge)

	// Confirm update
	fmt.Print("\nSave these preferences? [y/n]: ")
	confirm, _ := reader.ReadString('\n')
	confirm = strings.TrimSpace(strings.ToLower(confirm))

	if confirm != "y" && confirm != "yes" {
		fmt.Println("Preferences not saved.")
		return nil
	}

	// Update preferences
	err = api.UpdateNotificationPreferences(prefs)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Notification preferences updated!")
	ns.displayPreferences(prefs)
	return nil
}

// Helper functions

func (ns *NotificationSettingsService) displayPreferences(prefs *api.NotificationPreferences) {
	fmt.Printf("\n🔔 Your Notification Preferences\n")
	fmt.Println(strings.Repeat("=", 40))

	settings := []struct {
		name    string
		emoji   string
		enabled bool
	}{
		{"Post Likes", "❤️", prefs.PostLikes},
		{"Post Comments", "💬", prefs.PostComments},
		{"Post Reposts", "🔄", prefs.PostReposts},
		{"New Followers", "👥", prefs.Follows},
		{"Messages", "💌", prefs.Messages},
		{"Mentions", "@️", prefs.Mentions},
		{"Report Updates", "📋", prefs.ReportUpdates},
		{"New Challenges", "🎹", prefs.NewChallenge},
	}

	for _, setting := range settings {
		status := "✅ ON "
		if !setting.enabled {
			status = "❌ OFF"
		}
		fmt.Printf("%s %s %s\n", setting.emoji, status, setting.name)
	}

	fmt.Printf("\n")
}

func boolToString(b bool) string {
	if b {
		return "currently ON"
	}
	return "currently OFF"
}
