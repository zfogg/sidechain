package cmd

import (
	"github.com/spf13/cobra"
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

var postUnreactCmd = &cobra.Command{
	Use:   "unreact <post-id> <emoji>",
	Short: "Remove an emoji reaction from a post",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.UnreactPost(args[0], args[1])
	},
}

var postReactionsCmd = &cobra.Command{
	Use:   "reactions <post-id>",
	Short: "View reactions on a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		postService := service.NewPostService()
		return postService.ViewPostReactions(args[0])
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
	postCmd.AddCommand(postUnreactCmd)
	postCmd.AddCommand(postReactionsCmd)
	postCmd.AddCommand(postSaveCmd)
	postCmd.AddCommand(postUnsaveCmd)
	postCmd.AddCommand(postSavedListCmd)
	postCmd.AddCommand(postPinCmd)
	postCmd.AddCommand(postUnpinCmd)
	postCmd.AddCommand(postRepostCmd)
	postCmd.AddCommand(postUnrepostCmd)
}
