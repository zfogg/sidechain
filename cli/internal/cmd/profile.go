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

func init() {
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

	// Add subcommands
	profilePictureCmd.AddCommand(profilePictureUploadCmd)
}
