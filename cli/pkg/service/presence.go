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

// PresenceService manages user presence and online status
type PresenceService struct{}

// NewPresenceService creates a new presence service
func NewPresenceService() *PresenceService {
	return &PresenceService{}
}

// ViewStatus displays the current user's presence and activity status
func (ps *PresenceService) ViewStatus() error {
	logger.Debug("Viewing user presence status")

	status, err := api.GetActivityStatus()
	if err != nil {
		return err
	}

	ps.displayStatus(status)
	return nil
}

// SetStatus interactively updates the user's online status
func (ps *PresenceService) SetStatus() error {
	logger.Debug("Setting user presence status")

	formatter.PrintInfo("📍 Update Presence Status")
	fmt.Println("\nAvailable statuses:")
	fmt.Println("  1. online       - Currently active")
	fmt.Println("  2. away         - Away but may return soon")
	fmt.Println("  3. offline      - Not available")
	fmt.Println("  4. do-not-disturb - Busy, do not disturb")

	reader := bufio.NewReader(os.Stdin)

	fmt.Print("\nSelect status (1-4) or enter custom: ")
	choice, _ := reader.ReadString('\n')
	choice = strings.TrimSpace(choice)

	statusMap := map[string]string{
		"1": "online",
		"2": "away",
		"3": "offline",
		"4": "do-not-disturb",
	}

	statusValue := statusMap[choice]
	if statusValue == "" {
		// If it's not a number, treat it as custom status
		statusValue = choice
	}

	if statusValue == "" {
		return fmt.Errorf("status is required")
	}

	fmt.Print("\nCustom status message (optional, press Enter to skip): ")
	statusText, _ := reader.ReadString('\n')
	statusText = strings.TrimSpace(statusText)

	// Update the status
	result, err := api.SetActivityStatus(statusValue)
	if err != nil {
		return err
	}

	// If a custom status text was provided, update it
	if statusText != "" {
		result, err = api.SetStatusText(statusText)
		if err != nil {
			return err
		}
	}

	formatter.PrintSuccess("Presence status updated!")
	ps.displayStatus(result)
	return nil
}

// Helper functions

func (ps *PresenceService) displayStatus(status *api.ActivityStatus) {
	fmt.Printf("\n📍 Your Presence Status\n")
	fmt.Println(strings.Repeat("=", 35))

	statusEmoji := ps.getStatusEmoji(status.Status)
	fmt.Printf("\nStatus: %s %s\n", statusEmoji, strings.ToUpper(status.Status))

	if status.StatusText != "" {
		fmt.Printf("Message: \"%s\"\n", status.StatusText)
	}

	fmt.Printf("Visible to others: %v\n", status.IsVisible)
	fmt.Printf("Last seen: %s\n", status.LastSeen)

	if status.AutoOffline > 0 {
		fmt.Printf("Auto-offline timeout: %d minutes\n", status.AutoOffline)
	}

	fmt.Printf("Updated: %s\n\n", status.UpdatedAt)
}

func (ps *PresenceService) getStatusEmoji(status string) string {
	switch strings.ToLower(status) {
	case "online":
		return "🟢"
	case "away":
		return "🟡"
	case "offline":
		return "⚫"
	case "do-not-disturb", "dnd":
		return "🔴"
	default:
		return "❓"
	}
}
