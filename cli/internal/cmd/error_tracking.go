package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var errorCmd = &cobra.Command{
	Use:   "error",
	Short: "Manage error tracking and reporting",
	Long:  "Report errors, view error statistics, and manage error logs",
}

var reportErrorCmd = &cobra.Command{
	Use:   "report",
	Short: "Report an error",
	Long:  "Report an error that occurred in your setup",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewErrorTrackingService()
		return svc.ReportError()
	},
}

var errorStatsCmd = &cobra.Command{
	Use:   "stats",
	Short: "View error statistics",
	Long:  "Display aggregated error statistics and top errors",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewErrorTrackingService()
		return svc.GetErrorStats()
	},
}

var viewErrorCmd = &cobra.Command{
	Use:   "view",
	Short: "View error details",
	Long:  "Display detailed information for a specific error",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewErrorTrackingService()
		return svc.ViewErrorDetails()
	},
}

var resolveErrorCmd = &cobra.Command{
	Use:   "resolve",
	Short: "Mark an error as resolved",
	Long:  "Mark a specific error as resolved",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewErrorTrackingService()
		return svc.ResolveError()
	},
}

func init() {
	errorCmd.AddCommand(reportErrorCmd)
	errorCmd.AddCommand(errorStatsCmd)
	errorCmd.AddCommand(viewErrorCmd)
	errorCmd.AddCommand(resolveErrorCmd)
}
