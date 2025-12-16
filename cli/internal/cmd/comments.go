package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var commentCmd = &cobra.Command{
	Use:   "comment",
	Short: "Manage comments on posts",
	Long:  "Create, view, update, delete, and interact with comments",
}

var createCommentCmd = &cobra.Command{
	Use:   "create",
	Short: "Create a new comment",
	Long:  "Create a new comment on a post (can be a reply to another comment)",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewCommentService()
		return svc.CreateComment()
	},
}

var viewCommentsCmd = &cobra.Command{
	Use:   "view",
	Short: "View comments on a post",
	Long:  "Display all comments on a post with replies",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewCommentService()
		return svc.ViewComments()
	},
}

var updateCommentCmd = &cobra.Command{
	Use:   "update",
	Short: "Update a comment",
	Long:  "Update an existing comment (must be author)",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewCommentService()
		return svc.UpdateComment()
	},
}

var deleteCommentCmd = &cobra.Command{
	Use:   "delete",
	Short: "Delete a comment",
	Long:  "Delete a comment (must be author)",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewCommentService()
		return svc.DeleteComment()
	},
}

var likeCommentCmd = &cobra.Command{
	Use:   "like",
	Short: "Like a comment",
	Long:  "Like a comment to show appreciation",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewCommentService()
		return svc.LikeComment()
	},
}

var unlikeCommentCmd = &cobra.Command{
	Use:   "unlike",
	Short: "Unlike a comment",
	Long:  "Remove your like from a comment",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewCommentService()
		return svc.UnlikeComment()
	},
}

var reportCommentCmd = &cobra.Command{
	Use:   "report",
	Short: "Report a comment",
	Long:  "Report a comment for moderation",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewCommentService()
		return svc.ReportComment()
	},
}

func init() {
	commentCmd.AddCommand(createCommentCmd)
	commentCmd.AddCommand(viewCommentsCmd)
	commentCmd.AddCommand(updateCommentCmd)
	commentCmd.AddCommand(deleteCommentCmd)
	commentCmd.AddCommand(likeCommentCmd)
	commentCmd.AddCommand(unlikeCommentCmd)
	commentCmd.AddCommand(reportCommentCmd)
}
