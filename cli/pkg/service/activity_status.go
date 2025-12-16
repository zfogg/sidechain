package service

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// ActivityStatusService provides activity status operations
type ActivityStatusService struct{}

// NewActivityStatusService creates a new activity status service
func NewActivityStatusService() *ActivityStatusService {
	return &ActivityStatusService{}
}

// ViewStatus displays the current user's activity status
func (ats *ActivityStatusService) ViewStatus() error {
	status, err := api.GetActivityStatus()
	if err != nil {
		return fmt.Errorf("failed to get activity status: %w", err)
	}

	formatter.PrintInfo("📊 Activity Status")
	fmt.Printf("\n")

	statusMap := map[string]interface{}{
		"Status":       ats.formatStatusBadge(status.Status),
		"Status Text":  status.StatusText,
		"Last Seen":    status.LastSeen,
		"Visible":      ats.formatBool(status.IsVisible),
		"Auto Offline": fmt.Sprintf("%d minutes", status.AutoOffline),
	}

	formatter.PrintKeyValue(statusMap)
	return nil
}

// SetStatus changes the user's activity status
func (ats *ActivityStatusService) SetStatus() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Println("\nAvailable statuses:")
	fmt.Println("  1. online         - Currently active")
	fmt.Println("  2. away           - Away but might return soon")
	fmt.Println("  3. do-not-disturb - Busy, do not disturb")
	fmt.Println("  4. offline        - Not currently available")
	fmt.Printf("\nSelect status (1-4): ")

	choice, _ := reader.ReadString('\n')
	choice = strings.TrimSpace(choice)

	statusMap := map[string]string{
		"1": "online",
		"2": "away",
		"3": "do-not-disturb",
		"4": "offline",
	}

	status, ok := statusMap[choice]
	if !ok {
		return fmt.Errorf("invalid status selection")
	}

	result, err := api.SetActivityStatus(status)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Status updated to: %s", ats.formatStatusBadge(result.Status))
	return nil
}

// SetStatusText updates the user's custom status message
func (ats *ActivityStatusService) SetStatusText() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Enter custom status message (max 100 chars, press Enter to skip): ")
	statusText, _ := reader.ReadString('\n')
	statusText = strings.TrimSpace(statusText)

	if statusText == "" {
		formatter.PrintInfo("No status text provided")
		return nil
	}

	if len(statusText) > 100 {
		return fmt.Errorf("status text is too long (max 100 characters)")
	}

	result, err := api.SetStatusText(statusText)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Status text updated: %s", result.StatusText)
	return nil
}

// ClearStatusText removes the custom status message
func (ats *ActivityStatusService) ClearStatusText() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Are you sure you want to clear your status text? (y/n): ")
	confirm, _ := reader.ReadString('\n')
	confirm = strings.TrimSpace(strings.ToLower(confirm))

	if confirm != "y" {
		formatter.PrintInfo("Cancelled")
		return nil
	}

	_, err := api.ClearStatusText()
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Status text cleared")
	return nil
}

// SetStatusVisibility configures whether the status is visible to others
func (ats *ActivityStatusService) SetStatusVisibility() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Println("\nVisibility options:")
	fmt.Println("  1. Visible    - Others can see your status")
	fmt.Println("  2. Hidden     - Only you can see your status")
	fmt.Printf("\nSelect visibility (1-2): ")

	choice, _ := reader.ReadString('\n')
	choice = strings.TrimSpace(choice)

	var isVisible bool
	switch choice {
	case "1":
		isVisible = true
	case "2":
		isVisible = false
	default:
		return fmt.Errorf("invalid visibility selection")
	}

	result, err := api.SetStatusVisibility(isVisible)
	if err != nil {
		return err
	}

	visibilityText := "hidden"
	if result.IsVisible {
		visibilityText = "visible"
	}
	formatter.PrintSuccess("Status visibility set to: %s", visibilityText)
	return nil
}

// SetAutoOffline configures auto-offline timeout
func (ats *ActivityStatusService) SetAutoOffline() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Println("\nAuto-offline options:")
	fmt.Println("  1. 5 minutes")
	fmt.Println("  2. 15 minutes")
	fmt.Println("  3. 30 minutes")
	fmt.Println("  4. 1 hour (60 minutes)")
	fmt.Println("  5. 2 hours (120 minutes)")
	fmt.Println("  6. Never (0 minutes)")
	fmt.Printf("\nSelect timeout (1-6): ")

	choice, _ := reader.ReadString('\n')
	choice = strings.TrimSpace(choice)

	minutesMap := map[string]int{
		"1": 5,
		"2": 15,
		"3": 30,
		"4": 60,
		"5": 120,
		"6": 0,
	}

	minutes, ok := minutesMap[choice]
	if !ok {
		return fmt.Errorf("invalid timeout selection")
	}

	result, err := api.SetAutoOfflineMinutes(minutes)
	if err != nil {
		return err
	}

	var timeoutText string
	if minutes == 0 {
		timeoutText = "disabled (never auto-offline)"
	} else {
		timeoutText = fmt.Sprintf("%d minutes", result.AutoOffline)
	}
	formatter.PrintSuccess("Auto-offline timeout set to: %s", timeoutText)
	return nil
}

// ViewStatusSettings displays all status-related settings
func (ats *ActivityStatusService) ViewStatusSettings() error {
	status, err := api.GetActivityStatus()
	if err != nil {
		return fmt.Errorf("failed to get activity status: %w", err)
	}

	formatter.PrintInfo("⚙️  Activity Status Settings")
	fmt.Printf("\n")

	settings := map[string]interface{}{
		"Current Status":    ats.formatStatusBadge(status.Status),
		"Status Message":    status.StatusText,
		"Visibility":        ats.formatBool(status.IsVisible),
		"Auto-Offline (min)": status.AutoOffline,
		"Last Updated":      status.UpdatedAt,
	}

	formatter.PrintKeyValue(settings)
	return nil
}

// ResetToOnline resets status to online
func (ats *ActivityStatusService) ResetToOnline() error {
	result, err := api.SetActivityStatus("online")
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Status reset to: %s", ats.formatStatusBadge(result.Status))
	return nil
}

// Helper functions

func (ats *ActivityStatusService) formatStatusBadge(status string) string {
	badgeMap := map[string]string{
		"online":         "🟢 Online",
		"away":           "🟡 Away",
		"do-not-disturb": "🔴 Do Not Disturb",
		"offline":        "⚫ Offline",
	}

	if badge, ok := badgeMap[status]; ok {
		return badge
	}
	return status
}

func (ats *ActivityStatusService) formatBool(b bool) string {
	if b {
		return "✓ Yes"
	}
	return "✗ No"
}

// Parse custom status timeout from user input
func (ats *ActivityStatusService) parseTimeoutMinutes(input string) (int, error) {
	minutes, err := strconv.Atoi(strings.TrimSpace(input))
	if err != nil || minutes < 0 {
		return 0, fmt.Errorf("invalid timeout value")
	}
	return minutes, nil
}
