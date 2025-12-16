package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var (
	postTitle       string
	postDescription string
	postBPM         string
	postKey         string
	postPage        int
	postPageSize    int
)

var postCmd = &cobra.Command{
	Use:   "post",
	Short: "Post management commands",
	Long:  "Create, view, and manage posts",
}

var postUploadCmd = &cobra.Command{
	Use:   "upload <filepath>",
	Short: "Upload audio and create a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.UploadPost(args[0], postTitle, postDescription, postBPM, postKey)
	},
}

var postListCmd = &cobra.Command{
	Use:   "list",
	Short: "List your posts",
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.ListPosts(postPage, postPageSize)
	},
}

var postViewCmd = &cobra.Command{
	Use:   "view <post-id>",
	Short: "View post details",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.ViewPost(args[0])
	},
}

var postDeleteCmd = &cobra.Command{
	Use:   "delete <post-id>",
	Short: "Delete a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.DeletePost(args[0])
	},
}

var postArchiveCmd = &cobra.Command{
	Use:   "archive <post-id>",
	Short: "Archive a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.ArchivePost(args[0])
	},
}

var postUnarchiveCmd = &cobra.Command{
	Use:   "unarchive <post-id>",
	Short: "Unarchive a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.UnarchivePost(args[0])
	},
}

var postLikeCmd = &cobra.Command{
	Use:   "like <post-id>",
	Short: "Like a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.LikePost(args[0])
	},
}

var postUnlikeCmd = &cobra.Command{
	Use:   "unlike <post-id>",
	Short: "Unlike a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.UnlikePost(args[0])
	},
}

var postReactCmd = &cobra.Command{
	Use:   "react <post-id> <emoji>",
	Short: "Add an emoji reaction to a post",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.ReactToPost(args[0], args[1])
	},
}

var postCommentCmd = &cobra.Command{
	Use:   "comment <post-id>",
	Short: "Add a comment to a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()

		// Prompt user for comment content
		content, err := prompter.PromptString("Comment: ")
		if err != nil {
			return err
		}

		if content == "" {
			fmt.Fprintf(os.Stderr, "Comment cannot be empty.\n")
			os.Exit(1)
		}

		return postService.CommentOnPost(args[0], content)
	},
}

var postCommentsCmd = &cobra.Command{
	Use:   "comments <post-id>",
	Short: "List comments on a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.ListComments(args[0], postPage, postPageSize)
	},
}

var postSaveCmd = &cobra.Command{
	Use:   "save <post-id>",
	Short: "Save a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.SavePost(args[0])
	},
}

var postUnsaveCmd = &cobra.Command{
	Use:   "unsave <post-id>",
	Short: "Unsave a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.UnsavePost(args[0])
	},
}

var postSavedListCmd = &cobra.Command{
	Use:   "saved-list",
	Short: "List your saved posts",
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.ListSavedPosts(postPage, postPageSize)
	},
}

var postPinCmd = &cobra.Command{
	Use:   "pin <post-id>",
	Short: "Pin a post to your profile",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.PinPost(args[0])
	},
}

var postUnpinCmd = &cobra.Command{
	Use:   "unpin <post-id>",
	Short: "Unpin a post from your profile",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.UnpinPost(args[0])
	},
}

var postRepostCmd = &cobra.Command{
	Use:   "repost <post-id>",
	Short: "Repost a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.RepostPost(args[0])
	},
}

var postUnrepostCmd = &cobra.Command{
	Use:   "unrepost <post-id>",
	Short: "Undo a repost",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.UnrepostPost(args[0])
	},
}

var commentEditCmd = &cobra.Command{
	Use:   "comment-edit <comment-id>",
	Short: "Edit a comment",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()

		// Prompt user for new comment content
		content, err := prompter.PromptString("New comment content: ")
		if err != nil {
			return err
		}

		if content == "" {
			fmt.Fprintf(os.Stderr, "Comment cannot be empty.\n")
			os.Exit(1)
		}

		return postService.EditComment(args[0], content)
	},
}

var commentDeleteCmd = &cobra.Command{
	Use:   "comment-delete <comment-id>",
	Short: "Delete a comment",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.DeleteComment(args[0])
	},
}

var commentLikeCmd = &cobra.Command{
	Use:   "comment-like <comment-id>",
	Short: "Like a comment",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.LikeComment(args[0])
	},
}

var commentUnlikeCmd = &cobra.Command{
	Use:   "comment-unlike <comment-id>",
	Short: "Unlike a comment",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.UnlikeComment(args[0])
	},
}

func init() {
	// Upload command flags
	postUploadCmd.Flags().StringVarP(&postTitle, "title", "t", "", "Post title")
	postUploadCmd.Flags().StringVarP(&postDescription, "description", "d", "", "Post description")
	postUploadCmd.Flags().StringVar(&postBPM, "bpm", "", "BPM (beats per minute)")
	postUploadCmd.Flags().StringVar(&postKey, "key", "", "Musical key")

	// List/pagination flags
	postListCmd.Flags().IntVar(&postPage, "page", 1, "Page number")
	postListCmd.Flags().IntVar(&postPageSize, "page-size", 10, "Results per page")

	postCommentsCmd.Flags().IntVar(&postPage, "page", 1, "Page number")
	postCommentsCmd.Flags().IntVar(&postPageSize, "page-size", 10, "Results per page")

	postSavedListCmd.Flags().IntVar(&postPage, "page", 1, "Page number")
	postSavedListCmd.Flags().IntVar(&postPageSize, "page-size", 10, "Results per page")

	// Add subcommands
	postCmd.AddCommand(postUploadCmd)
	postCmd.AddCommand(postListCmd)
	postCmd.AddCommand(postViewCmd)
	postCmd.AddCommand(postDeleteCmd)
	postCmd.AddCommand(postArchiveCmd)
	postCmd.AddCommand(postUnarchiveCmd)
	postCmd.AddCommand(postLikeCmd)
	postCmd.AddCommand(postUnlikeCmd)
	postCmd.AddCommand(postReactCmd)
	postCmd.AddCommand(postCommentCmd)
	postCmd.AddCommand(postCommentsCmd)
	postCmd.AddCommand(postSaveCmd)
	postCmd.AddCommand(postUnsaveCmd)
	postCmd.AddCommand(postSavedListCmd)
	postCmd.AddCommand(postPinCmd)
	postCmd.AddCommand(postUnpinCmd)
	postCmd.AddCommand(postRepostCmd)
	postCmd.AddCommand(postUnrepostCmd)
	postCmd.AddCommand(commentEditCmd)
	postCmd.AddCommand(commentDeleteCmd)
	postCmd.AddCommand(commentLikeCmd)
	postCmd.AddCommand(commentUnlikeCmd)
}
