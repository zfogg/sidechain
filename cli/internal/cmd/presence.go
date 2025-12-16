package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var presenceCmd = &cobra.Command{
	Use:   "presence",
	Short: "Manage your online presence",
	Long:  "View and manage your online status and availability",
}

var presenceStatusCmd = &cobra.Command{
	Use:   "status",
	Short: "View your current presence status",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPresenceService()
		return svc.ViewStatus()
	},
}

var presenceSetCmd = &cobra.Command{
	Use:   "set",
	Short: "Update your presence status",
	Long:  "Interactively update your online status and custom status message",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPresenceService()
		return svc.SetStatus()
	},
}

func init() {
	// Add presence subcommands
	presenceCmd.AddCommand(presenceStatusCmd)
	presenceCmd.AddCommand(presenceSetCmd)

	// Add presence command to root
	rootCmd.AddCommand(presenceCmd)
}
