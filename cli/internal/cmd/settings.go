package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var settingsCmd = &cobra.Command{
	Use:   "settings",
	Short: "Manage user settings",
	Long:  "Configure user preferences and account settings",
}

var statusViewCmd = &cobra.Command{
	Use:   "status",
	Short: "View current activity status",
	Long:  "Display your current online status and status message",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewActivityStatusService()
		return svc.ViewStatus()
	},
}

var statusSetCmd = &cobra.Command{
	Use:   "set-status",
	Short: "Change activity status",
	Long:  "Update your current activity status (online, away, do-not-disturb, offline)",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewActivityStatusService()
		return svc.SetStatus()
	},
}

var statusTextCmd = &cobra.Command{
	Use:   "set-message",
	Short: "Set custom status message",
	Long:  "Add a custom message to your activity status (max 100 characters)",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewActivityStatusService()
		return svc.SetStatusText()
	},
}

var statusTextClearCmd = &cobra.Command{
	Use:   "clear-message",
	Short: "Clear status message",
	Long:  "Remove your custom status message",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewActivityStatusService()
		return svc.ClearStatusText()
	},
}

var statusVisibilityCmd = &cobra.Command{
	Use:   "visibility",
	Short: "Configure status visibility",
	Long:  "Set whether your status is visible to other users",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewActivityStatusService()
		return svc.SetStatusVisibility()
	},
}

var autoOfflineCmd = &cobra.Command{
	Use:   "auto-offline",
	Short: "Configure auto-offline timeout",
	Long:  "Set how long before you're automatically marked as offline",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewActivityStatusService()
		return svc.SetAutoOffline()
	},
}

var statusResetCmd = &cobra.Command{
	Use:   "reset-online",
	Short: "Reset status to online",
	Long:  "Quickly set your status back to online",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewActivityStatusService()
		return svc.ResetToOnline()
	},
}

var statusAllCmd = &cobra.Command{
	Use:   "all",
	Short: "View all status settings",
	Long:  "Display all activity status configuration",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewActivityStatusService()
		return svc.ViewStatusSettings()
	},
}

func init() {
	settingsCmd.AddCommand(statusViewCmd)
	settingsCmd.AddCommand(statusSetCmd)
	settingsCmd.AddCommand(statusTextCmd)
	settingsCmd.AddCommand(statusTextClearCmd)
	settingsCmd.AddCommand(statusVisibilityCmd)
	settingsCmd.AddCommand(autoOfflineCmd)
	settingsCmd.AddCommand(statusResetCmd)
	settingsCmd.AddCommand(statusAllCmd)
}
