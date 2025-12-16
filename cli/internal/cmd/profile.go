package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var (
	followReqPage     int
	followReqPageSize int
)

var profileCmd = &cobra.Command{
	Use:   "profile",
	Short: "User profile commands",
	Long:  "View and manage user profiles",
}

var profileViewCmd = &cobra.Command{
	Use:   "view [username]",
	Short: "View a user profile",
	RunE: func(cmd *cobra.Command, args []string) error {
		username := ""
		if len(args) > 0 {
			username = args[0]
		}
		svc := service.NewProfileService()
		return svc.ViewProfile(username)
	},
}

var profileEditCmd = &cobra.Command{
	Use:   "edit",
	Short: "Edit your profile",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewProfileService()
		return svc.EditProfile()
	},
}

var profileFollowCmd = &cobra.Command{
	Use:   "follow <username>",
	Short: "Follow a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewProfileService()
		return svc.FollowUser(args[0])
	},
}

var profileUnfollowCmd = &cobra.Command{
	Use:   "unfollow <username>",
	Short: "Unfollow a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewProfileService()
		return svc.UnfollowUser(args[0])
	},
}

var profileMuteCmd = &cobra.Command{
	Use:   "mute <username>",
	Short: "Mute a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewProfileService()
		return svc.MuteUser(args[0])
	},
}

var profileUnmuteCmd = &cobra.Command{
	Use:   "unmute <username>",
	Short: "Unmute a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewProfileService()
		return svc.UnmuteUser(args[0])
	},
}

var profileListMutedCmd = &cobra.Command{
	Use:   "list-muted",
	Short: "List muted users",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewProfileService()
		return svc.ListMutedUsers(1)
	},
}

var profileActivityCmd = &cobra.Command{
	Use:   "activity [username]",
	Short: "View user activity",
	RunE: func(cmd *cobra.Command, args []string) error {
		username := ""
		if len(args) > 0 {
			username = args[0]
		}
		svc := service.NewProfileService()
		return svc.GetActivity(username)
	},
}

var profileFollowersCmd = &cobra.Command{
	Use:   "followers [username]",
	Short: "List followers of a user",
	RunE: func(cmd *cobra.Command, args []string) error {
		username := ""
		if len(args) > 0 {
			username = args[0]
		}
		svc := service.NewProfileService()
		return svc.GetFollowers(username, 1)
	},
}

var profileFollowingCmd = &cobra.Command{
	Use:   "following [username]",
	Short: "List users that someone is following",
	RunE: func(cmd *cobra.Command, args []string) error {
		username := ""
		if len(args) > 0 {
			username = args[0]
		}
		svc := service.NewProfileService()
		return svc.GetFollowing(username, 1)
	},
}

var profilePictureCmd = &cobra.Command{
	Use:   "picture",
	Short: "Manage profile picture",
}

var profilePictureUploadCmd = &cobra.Command{
	Use:   "upload <filepath>",
	Short: "Upload a profile picture",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		// TODO: Implement picture upload with file handling
		return nil
	},
}

// Follow Request Commands
var followRequestCmd = &cobra.Command{
	Use:   "follow-request",
	Short: "Manage follow requests",
	Long:  "Manage incoming and outgoing follow requests",
}

var followRequestListCmd = &cobra.Command{
	Use:   "list",
	Short: "List pending follow requests",
	RunE: func(cmd *cobra.Command, args []string) error {
		requests, err := api.GetFollowRequests(followReqPage, followReqPageSize)
		if err != nil {
			return err
		}

		if len(requests.Requests) == 0 {
			fmt.Println("No pending follow requests.")
			return nil
		}

		displayFollowRequests(requests)
		return nil
	},
}

var followRequestAcceptCmd = &cobra.Command{
	Use:   "accept <username>",
	Short: "Accept a follow request",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.AcceptFollowRequest(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Follow request from @%s accepted.\n", args[0])
		return nil
	},
}

var followRequestRejectCmd = &cobra.Command{
	Use:   "reject <username>",
	Short: "Reject a follow request",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.RejectFollowRequest(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Follow request from @%s rejected.\n", args[0])
		return nil
	},
}

var followRequestCancelCmd = &cobra.Command{
	Use:   "cancel <username>",
	Short: "Cancel a follow request you sent",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.CancelFollowRequest(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Follow request to @%s cancelled.\n", args[0])
		return nil
	},
}

func displayFollowRequests(requests *api.FollowRequestListResponse) {
	fmt.Printf("\n👥 Follow Requests (%d)\n\n", len(requests.Requests))

	for i, req := range requests.Requests {
		fmt.Printf("%d. Request from user ID: %s\n", i+1, req.RequesterID)
		fmt.Printf("   Status: %s\n", req.Status)
		fmt.Printf("   Requested: %s\n", req.CreatedAt.Format("2006-01-02 15:04:05"))
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d requests (Page %d)\n\n", len(requests.Requests), requests.TotalCount, requests.Page)
}

func init() {
	// Follow request pagination
	followRequestListCmd.Flags().IntVar(&followReqPage, "page", 1, "Page number")
	followRequestListCmd.Flags().IntVar(&followReqPageSize, "page-size", 10, "Results per page")

	// Add main subcommands
	profileCmd.AddCommand(profileViewCmd)
	profileCmd.AddCommand(profileEditCmd)
	profileCmd.AddCommand(profileFollowCmd)
	profileCmd.AddCommand(profileUnfollowCmd)
	profileCmd.AddCommand(profileMuteCmd)
	profileCmd.AddCommand(profileUnmuteCmd)
	profileCmd.AddCommand(profileListMutedCmd)
	profileCmd.AddCommand(profileActivityCmd)
	profileCmd.AddCommand(profileFollowersCmd)
	profileCmd.AddCommand(profileFollowingCmd)
	profileCmd.AddCommand(profilePictureCmd)
	profileCmd.AddCommand(followRequestCmd)

	// Add subcommands
	profilePictureCmd.AddCommand(profilePictureUploadCmd)

	// Follow request subcommands
	followRequestCmd.AddCommand(followRequestListCmd)
	followRequestCmd.AddCommand(followRequestAcceptCmd)
	followRequestCmd.AddCommand(followRequestRejectCmd)
	followRequestCmd.AddCommand(followRequestCancelCmd)
}
