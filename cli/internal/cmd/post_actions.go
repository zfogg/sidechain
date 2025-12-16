package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var postActionsCmd = &cobra.Command{
	Use:   "actions",
	Short: "Manage advanced post actions",
	Long:  "Pin, remix, download, and report posts",
}

var pinPostCmd = &cobra.Command{
	Use:   "pin",
	Short: "Pin a post to your profile",
	Long:  "Pin a post to keep it at the top of your profile",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPostActionsService()
		return svc.PinPost()
	},
}

var unpinPostCmd = &cobra.Command{
	Use:   "unpin",
	Short: "Unpin a post from your profile",
	Long:  "Remove a pinned post from your profile",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPostActionsService()
		return svc.UnpinPost()
	},
}

var createRemixCmd = &cobra.Command{
	Use:   "remix",
	Short: "Create a remix of a post",
	Long:  "Create a remix based on another user's post",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPostActionsService()
		return svc.CreateRemix()
	},
}

var listRemixesCmd = &cobra.Command{
	Use:   "remixes",
	Short: "List remixes of a post",
	Long:  "Display all remixes created from a specific post",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPostActionsService()
		return svc.ListRemixes()
	},
}

var downloadPostCmd = &cobra.Command{
	Use:   "download",
	Short: "Download a post's audio",
	Long:  "Get the download URL and save a post's audio file locally",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPostActionsService()
		return svc.DownloadPost()
	},
}

var actionReportPostCmd = &cobra.Command{
	Use:   "report",
	Short: "Report a post",
	Long:  "Report a post for moderation (spam, harassment, etc.)",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPostActionsService()
		return svc.ReportPost()
	},
}

var postStatsCmd = &cobra.Command{
	Use:   "stats",
	Short: "View post statistics",
	Long:  "Display engagement statistics for a post",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewPostActionsService()
		return svc.GetPostStats()
	},
}

func init() {
	postActionsCmd.AddCommand(pinPostCmd)
	postActionsCmd.AddCommand(unpinPostCmd)
	postActionsCmd.AddCommand(createRemixCmd)
	postActionsCmd.AddCommand(listRemixesCmd)
	postActionsCmd.AddCommand(downloadPostCmd)
	postActionsCmd.AddCommand(actionReportPostCmd)
	postActionsCmd.AddCommand(postStatsCmd)
}
