package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var recommendCmd = &cobra.Command{
	Use:   "recommend",
	Short: "Manage recommendation feedback",
	Long:  "Record feedback on recommendations to improve your feed",
}

var dislikeCmd = &cobra.Command{
	Use:   "dislike",
	Short: "Mark a post as not interested",
	Long:  "Tell the algorithm you're not interested in a post",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationFeedbackService()
		return svc.DislikePost()
	},
}

var skipCmd = &cobra.Command{
	Use:   "skip",
	Short: "Record a skipped post",
	Long:  "Record that you skipped past a post",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationFeedbackService()
		return svc.SkipPost()
	},
}

var hideCmd = &cobra.Command{
	Use:   "hide",
	Short: "Hide a post from recommendations",
	Long:  "Hide a post so it won't appear in your recommendations",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationFeedbackService()
		return svc.HidePost()
	},
}

var metricsCmd = &cobra.Command{
	Use:   "metrics",
	Short: "View recommendation metrics",
	Long:  "View your click-through rate and engagement metrics",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewRecommendationFeedbackService()
		return svc.GetCTRMetrics()
	},
}

func init() {
	recommendCmd.AddCommand(dislikeCmd)
	recommendCmd.AddCommand(skipCmd)
	recommendCmd.AddCommand(hideCmd)
	recommendCmd.AddCommand(metricsCmd)
}
