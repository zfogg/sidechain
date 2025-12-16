package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var muteCmd = &cobra.Command{
	Use:   "mute",
	Short: "Manage muted users",
	Long:  "Mute or unmute users and manage your mute list",
}

var muteUserCmd = &cobra.Command{
	Use:   "add",
	Short: "Mute a user",
	Long:  "Mute a user so their posts won't appear in your feed",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewMuteService()
		return svc.MuteUser()
	},
}

var unmuteUserCmd = &cobra.Command{
	Use:   "remove",
	Short: "Unmute a user",
	Long:  "Unmute a user so their posts will appear in your feed again",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewMuteService()
		return svc.UnmuteUser()
	},
}


var listMutedCmd = &cobra.Command{
	Use:   "list",
	Short: "List all muted users",
	Long:  "Display all users you've muted",
	RunE: func(cmd *cobra.Command, args []string) error {
		page, _ := cmd.Flags().GetInt("page")
		limit, _ := cmd.Flags().GetInt("limit")
		svc := service.NewMuteService()
		return svc.ListMutedUsers(page, limit)
	},
}

func init() {
	muteCmd.AddCommand(muteUserCmd)
	muteCmd.AddCommand(unmuteUserCmd)
	muteCmd.AddCommand(listMutedCmd)

	listMutedCmd.Flags().IntP("page", "p", 1, "Page number")
	listMutedCmd.Flags().IntP("limit", "l", 50, "Items per page")
}
