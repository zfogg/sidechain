package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var reportCmd = &cobra.Command{
	Use:   "report",
	Short: "Report inappropriate content",
	Long:  "Report posts, comments, or users that violate community guidelines",
}

var reportPostCmd = &cobra.Command{
	Use:   "post <post-id>",
	Short: "Report a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewReportService()
		return svc.ReportPost(args[0])
	},
}

var reportUserCmd = &cobra.Command{
	Use:   "user <username>",
	Short: "Report a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewReportService()
		return svc.ReportUser(args[0])
	},
}

func init() {
	reportCmd.AddCommand(reportPostCmd)
	reportCmd.AddCommand(reportUserCmd)
}
