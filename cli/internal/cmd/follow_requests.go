package cmd

import (
	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/service"
)

var followCmd = &cobra.Command{
	Use:   "follow",
	Short: "Manage follow requests",
	Long:  "View, accept, reject, and cancel follow requests",
}

var getFollowRequestsCmd = &cobra.Command{
	Use:   "requests",
	Short: "View incoming follow requests",
	Long:  "Display all pending follow requests you have received",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewFollowRequestService()
		return svc.GetFollowRequests()
	},
}

var getPendingFollowRequestsCmd = &cobra.Command{
	Use:   "pending",
	Short: "View pending follow requests sent",
	Long:  "Display all follow requests you have sent that are pending",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewFollowRequestService()
		return svc.GetPendingFollowRequests()
	},
}

var acceptFollowRequestCmd = &cobra.Command{
	Use:   "accept",
	Short: "Accept a follow request",
	Long:  "Accept an incoming follow request from another user",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewFollowRequestService()
		return svc.AcceptFollowRequest()
	},
}

var rejectFollowRequestCmd = &cobra.Command{
	Use:   "reject",
	Short: "Reject a follow request",
	Long:  "Reject an incoming follow request from another user",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewFollowRequestService()
		return svc.RejectFollowRequest()
	},
}

var cancelFollowRequestCmd = &cobra.Command{
	Use:   "cancel",
	Short: "Cancel a follow request",
	Long:  "Cancel a follow request you sent to another user",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewFollowRequestService()
		return svc.CancelFollowRequest()
	},
}

var checkFollowRequestStatusCmd = &cobra.Command{
	Use:   "check",
	Short: "Check follow request status with a user",
	Long:  "Check if there's a pending follow request or relationship with a specific user",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewFollowRequestService()
		return svc.CheckFollowRequestStatus()
	},
}

var getFollowRequestCountCmd = &cobra.Command{
	Use:   "count",
	Short: "Get follow request count",
	Long:  "Display the number of pending follow requests",
	RunE: func(cmd *cobra.Command, args []string) error {
		svc := service.NewFollowRequestService()
		return svc.GetFollowRequestCount()
	},
}

func init() {
	followCmd.AddCommand(getFollowRequestsCmd)
	followCmd.AddCommand(getPendingFollowRequestsCmd)
	followCmd.AddCommand(acceptFollowRequestCmd)
	followCmd.AddCommand(rejectFollowRequestCmd)
	followCmd.AddCommand(cancelFollowRequestCmd)
	followCmd.AddCommand(checkFollowRequestStatusCmd)
	followCmd.AddCommand(getFollowRequestCountCmd)
}
