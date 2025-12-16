package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var (
	notifPage     int
	notifPageSize int
)

var notificationsCmd = &cobra.Command{
	Use:   "notifications",
	Short: "Notification commands",
	Long:  "View and manage notifications",
}

var notificationsListCmd = &cobra.Command{
	Use:   "list",
	Short: "List notifications",
	RunE: func(cmd *cobra.Command, args []string) error {
		notifService := service.NewNotificationService()
		return notifService.ListNotifications(notifPage, notifPageSize)
	},
}

var notificationsWatchCmd = &cobra.Command{
	Use:   "watch",
	Short: "Watch for real-time notifications",
	Long:  "Stream real-time notifications via WebSocket",
	RunE: func(cmd *cobra.Command, args []string) error {
		watcherService := service.NewNotificationWatcherService()
		return watcherService.WatchNotifications(cmd.Context())
	},
}

var notificationsCountCmd = &cobra.Command{
	Use:   "count",
	Short: "Show unread notification count",
	RunE: func(cmd *cobra.Command, args []string) error {
		notifService := service.NewNotificationService()
		return notifService.GetUnreadCount()
	},
}

var notificationsMarkReadCmd = &cobra.Command{
	Use:   "mark-read [notification-id]",
	Short: "Mark notifications as read",
	RunE: func(cmd *cobra.Command, args []string) error {
		notifService := service.NewNotificationService()

		if len(args) == 0 {
			// Mark all as read
			return notifService.MarkAllAsRead()
		}

		// Mark specific notification as read
		return notifService.MarkNotificationAsRead(args[0])
	},
}

var notificationsSettingsCmd = &cobra.Command{
	Use:   "settings",
	Short: "Manage notification preferences",
	Long:  "View and update your notification settings",
}

var notificationsSettingsViewCmd = &cobra.Command{
	Use:   "view",
	Short: "View your notification preferences",
	RunE: func(cmd *cobra.Command, args []string) error {
		settingsService := service.NewNotificationSettingsService()
		return settingsService.ViewPreferences()
	},
}

var notificationsSettingsManageCmd = &cobra.Command{
	Use:   "manage",
	Short: "Update your notification preferences",
	Long:  "Interactively configure which notifications you want to receive",
	RunE: func(cmd *cobra.Command, args []string) error {
		settingsService := service.NewNotificationSettingsService()
		return settingsService.ManagePreferences()
	},
}

func init() {
	// Pagination flags
	notificationsListCmd.Flags().IntVar(&notifPage, "page", 1, "Page number")
	notificationsListCmd.Flags().IntVar(&notifPageSize, "page-size", 10, "Results per page")

	// Add settings subcommands to settings parent
	notificationsSettingsCmd.AddCommand(notificationsSettingsViewCmd)
	notificationsSettingsCmd.AddCommand(notificationsSettingsManageCmd)

	// Add subcommands
	notificationsCmd.AddCommand(notificationsListCmd)
	notificationsCmd.AddCommand(notificationsWatchCmd)
	notificationsCmd.AddCommand(notificationsCountCmd)
	notificationsCmd.AddCommand(notificationsMarkReadCmd)
	notificationsCmd.AddCommand(notificationsSettingsCmd)
}
