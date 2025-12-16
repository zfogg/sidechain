package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
	"github.com/zfogg/sidechain/cli/pkg/api"
)

var (
	adminReportPage     int
	adminReportPageSize int
)

var adminCmd = &cobra.Command{
	Use:   "admin",
	Short: "Admin commands",
	Long:  "Administrative and moderation commands (admin-only)",
}

// Reports
var adminReportsCmd = &cobra.Command{
	Use:   "reports",
	Short: "Manage reports",
}

var adminReportsListCmd = &cobra.Command{
	Use:   "list [status]",
	Short: "List all reports",
	Long:  "List reports, optionally filtered by status (pending, reviewing, resolved, dismissed)",
	RunE: func(cmd *cobra.Command, args []string) error {
		status := ""
		if len(args) > 0 {
			status = args[0]
		}

		reports, err := api.GetReports(status, adminReportPage, adminReportPageSize)
		if err != nil {
			return err
		}

		if len(reports.Reports) == 0 {
			fmt.Println("No reports found.")
			return nil
		}

		displayReports(reports)
		return nil
	},
}

var adminReportsViewCmd = &cobra.Command{
	Use:   "view <report-id>",
	Short: "View report details",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		report, err := api.GetReport(args[0])
		if err != nil {
			return err
		}

		displayReportDetail(report)
		return nil
	},
}

var adminReportsResolveCmd = &cobra.Command{
	Use:   "resolve <report-id> <action>",
	Short: "Resolve a report",
	Long:  "Resolve a report with action: dismiss, remove_content, suspend_user",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.ResolveReport(args[0], args[1]); err != nil {
			return err
		}

		fmt.Printf("✓ Report resolved.\n")
		return nil
	},
}

var adminReportsDismissCmd = &cobra.Command{
	Use:   "dismiss <report-id>",
	Short: "Dismiss a report",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.DismissReport(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Report dismissed.\n")
		return nil
	},
}

// Posts
var adminPostsCmd = &cobra.Command{
	Use:   "posts",
	Short: "Manage posts",
}

var adminPostsDeleteCmd = &cobra.Command{
	Use:   "delete <post-id>",
	Short: "Delete a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.AdminDeletePost(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Post deleted.\n")
		return nil
	},
}

var adminPostsHideCmd = &cobra.Command{
	Use:   "hide <post-id>",
	Short: "Hide a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.AdminHidePost(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Post hidden.\n")
		return nil
	},
}

var adminPostsUnhideCmd = &cobra.Command{
	Use:   "unhide <post-id>",
	Short: "Unhide a post",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.AdminUnhidePost(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Post unhidden.\n")
		return nil
	},
}

var adminPostsListCmd = &cobra.Command{
	Use:   "list",
	Short: "List flagged posts",
	RunE: func(cmd *cobra.Command, args []string) error {
		posts, err := api.GetFlaggedPosts(adminReportPage, adminReportPageSize)
		if err != nil {
			return err
		}

		if len(posts.Posts) == 0 {
			fmt.Println("No flagged posts.")
			return nil
		}

		displayFlaggedPosts(posts)
		return nil
	},
}

var adminPostsViewCmd = &cobra.Command{
	Use:   "view <post-id>",
	Short: "View post with context",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		post, err := api.GetPost(args[0])
		if err != nil {
			return err
		}

		displayPostDetail(post)
		return nil
	},
}

// Comments
var adminCommentsCmd = &cobra.Command{
	Use:   "comments",
	Short: "Manage comments",
}

var adminCommentsDeleteCmd = &cobra.Command{
	Use:   "delete <comment-id>",
	Short: "Delete a comment",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.AdminDeleteComment(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Comment deleted.\n")
		return nil
	},
}

var adminCommentsHideCmd = &cobra.Command{
	Use:   "hide <comment-id>",
	Short: "Hide a comment",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.AdminHideComment(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ Comment hidden.\n")
		return nil
	},
}

var adminCommentsListCmd = &cobra.Command{
	Use:   "list",
	Short: "List flagged comments",
	RunE: func(cmd *cobra.Command, args []string) error {
		comments, err := api.GetFlaggedComments(adminReportPage, adminReportPageSize)
		if err != nil {
			return err
		}

		if len(comments) == 0 {
			fmt.Println("No flagged comments.")
			return nil
		}

		displayFlaggedComments(comments)
		return nil
	},
}

// Users
var adminUsersCmd = &cobra.Command{
	Use:   "users",
	Short: "Manage users",
}

var adminUsersListCmd = &cobra.Command{
	Use:   "list",
	Short: "List users",
	RunE: func(cmd *cobra.Command, args []string) error {
		verified, _ := cmd.Flags().GetBool("verified")
		twoFA, _ := cmd.Flags().GetBool("two-fa")

		users, err := api.GetUsers(adminReportPage, adminReportPageSize, verified, twoFA)
		if err != nil {
			return err
		}

		if len(users.Users) == 0 {
			fmt.Println("No users found.")
			return nil
		}

		displayUsers(users)
		return nil
	},
}

var adminUsersViewCmd = &cobra.Command{
	Use:   "view <username>",
	Short: "View user details",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		user, err := api.GetUser(args[0])
		if err != nil {
			return err
		}

		displayUserDetail(user)
		return nil
	},
}

var adminUsersSuspendCmd = &cobra.Command{
	Use:   "suspend <username>",
	Short: "Suspend a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		reason, _ := cmd.Flags().GetString("reason")
		if reason == "" {
			reason = "No reason provided"
		}

		if err := api.SuspendUser(args[0], reason); err != nil {
			return err
		}

		fmt.Printf("✓ User @%s has been suspended.\n", args[0])
		return nil
	},
}

var adminUsersUnsuspendCmd = &cobra.Command{
	Use:   "unsuspend <username>",
	Short: "Unsuspend a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		if err := api.UnsuspendUser(args[0]); err != nil {
			return err
		}

		fmt.Printf("✓ User @%s has been unsuspended.\n", args[0])
		return nil
	},
}

var adminUsersWarnCmd = &cobra.Command{
	Use:   "warn <username>",
	Short: "Issue a warning to a user",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		reason, _ := cmd.Flags().GetString("reason")
		if reason == "" {
			reason = "No reason provided"
		}

		if err := api.WarnUser(args[0], reason); err != nil {
			return err
		}

		fmt.Printf("✓ Warning issued to user @%s.\n", args[0])
		return nil
	},
}

// Dashboard
var adminDashboardCmd = &cobra.Command{
	Use:   "dashboard",
	Short: "View moderation dashboard",
	RunE: func(cmd *cobra.Command, args []string) error {
		// TODO: Implement admin dashboard
		return nil
	},
}

var adminStatsCmd = &cobra.Command{
	Use:   "stats",
	Short: "View platform statistics",
	RunE: func(cmd *cobra.Command, args []string) error {
		// TODO: Implement admin stats
		return nil
	},
}

var adminHealthCmd = &cobra.Command{
	Use:   "health",
	Short: "Check server health",
	RunE: func(cmd *cobra.Command, args []string) error {
		// TODO: Implement admin health
		return nil
	},
}

func init() {
	// Pagination flags
	adminReportsListCmd.Flags().IntVar(&adminReportPage, "page", 1, "Page number")
	adminReportsListCmd.Flags().IntVar(&adminReportPageSize, "page-size", 10, "Results per page")
	adminCommentsListCmd.Flags().IntVar(&adminReportPage, "page", 1, "Page number")
	adminCommentsListCmd.Flags().IntVar(&adminReportPageSize, "page-size", 10, "Results per page")
	adminUsersListCmd.Flags().IntVar(&adminReportPage, "page", 1, "Page number")
	adminUsersListCmd.Flags().IntVar(&adminReportPageSize, "page-size", 10, "Results per page")

	// User list filter flags
	adminUsersListCmd.Flags().Bool("verified", false, "Filter by verified users")
	adminUsersListCmd.Flags().Bool("two-fa", false, "Filter by 2FA enabled users")

	// User action flags
	adminUsersSuspendCmd.Flags().String("reason", "", "Reason for suspension")
	adminUsersWarnCmd.Flags().String("reason", "", "Reason for warning")

	adminCmd.AddCommand(adminReportsCmd)
	adminCmd.AddCommand(adminPostsCmd)
	adminCmd.AddCommand(adminCommentsCmd)
	adminCmd.AddCommand(adminUsersCmd)
	adminCmd.AddCommand(adminDashboardCmd)
	adminCmd.AddCommand(adminStatsCmd)
	adminCmd.AddCommand(adminHealthCmd)

	adminReportsCmd.AddCommand(adminReportsListCmd)
	adminReportsCmd.AddCommand(adminReportsViewCmd)
	adminReportsCmd.AddCommand(adminReportsResolveCmd)
	adminReportsCmd.AddCommand(adminReportsDismissCmd)

	adminPostsCmd.AddCommand(adminPostsDeleteCmd)
	adminPostsCmd.AddCommand(adminPostsHideCmd)
	adminPostsCmd.AddCommand(adminPostsUnhideCmd)
	adminPostsCmd.AddCommand(adminPostsListCmd)
	adminPostsCmd.AddCommand(adminPostsViewCmd)

	adminCommentsCmd.AddCommand(adminCommentsDeleteCmd)
	adminCommentsCmd.AddCommand(adminCommentsHideCmd)
	adminCommentsCmd.AddCommand(adminCommentsListCmd)

	adminUsersCmd.AddCommand(adminUsersListCmd)
	adminUsersCmd.AddCommand(adminUsersViewCmd)
	adminUsersCmd.AddCommand(adminUsersSuspendCmd)
	adminUsersCmd.AddCommand(adminUsersUnsuspendCmd)
	adminUsersCmd.AddCommand(adminUsersWarnCmd)
}

// Helper display functions for reports
func displayReports(reports *api.ReportListResponse) {
	fmt.Printf("\n🚨 Reports (%d)\n\n", len(reports.Reports))

	for i, report := range reports.Reports {
		statusEmoji := "🟡"
		if report.Status == "resolved" {
			statusEmoji = "✓"
		} else if report.Status == "reviewing" {
			statusEmoji = "🔍"
		} else if report.Status == "dismissed" {
			statusEmoji = "✗"
		}

		fmt.Printf("%d. %s [%s] %s\n", i+1, statusEmoji, report.Type, report.ID)
		fmt.Printf("   Reason: %s\n", report.Reason)
		fmt.Printf("   Target: %s\n", report.TargetID)
		fmt.Printf("   Status: %s | %s\n", report.Status, report.CreatedAt.Format("2006-01-02 15:04:05"))
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d reports (Page %d)\n\n", len(reports.Reports), reports.TotalCount, reports.Page)
}

func displayReportDetail(report *api.Report) {
	fmt.Printf("\n🚨 Report Details\n")
	fmt.Printf("ID:           %s\n", report.ID)
	fmt.Printf("Type:         %s\n", report.Type)
	fmt.Printf("Target ID:    %s\n", report.TargetID)
	fmt.Printf("Reporter ID:  %s\n", report.ReporterID)
	fmt.Printf("Reason:       %s\n", report.Reason)
	if report.Description != "" {
		fmt.Printf("Description:  %s\n", report.Description)
	}
	fmt.Printf("Status:       %s\n", report.Status)
	if report.AssignedToID != "" {
		fmt.Printf("Assigned To:  %s\n", report.AssignedToID)
	}
	fmt.Printf("Created:      %s\n", report.CreatedAt.Format("2006-01-02 15:04:05"))
	if report.ResolvedAt != nil {
		fmt.Printf("Resolved:     %s\n", report.ResolvedAt.Format("2006-01-02 15:04:05"))
	}
	fmt.Printf("\n")
}

// Helper display functions for posts
func displayFlaggedPosts(posts *api.FeedResponse) {
	fmt.Printf("\n🚩 Flagged Posts (%d)\n\n", len(posts.Posts))

	for i, post := range posts.Posts {
		fmt.Printf("%d. %s\n", i+1, post.ID)
		fmt.Printf("   Author: @%s\n", post.AuthorUsername)
		fmt.Printf("   Title: %s\n", post.Title)
		if post.Description != "" {
			fmt.Printf("   Description: %s\n", post.Description)
		}
		fmt.Printf("   Posted: %s\n", post.CreatedAt.Format("2006-01-02 15:04:05"))
		fmt.Printf("   Reports: %d\n", post.ReportCount)
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d posts (Page %d)\n\n", len(posts.Posts), posts.TotalCount, posts.Page)
}

func displayPostDetail(post *api.Post) {
	fmt.Printf("\n📝 Post Details\n")
	fmt.Printf("ID:           %s\n", post.ID)
	fmt.Printf("Author:       @%s\n", post.AuthorUsername)
	fmt.Printf("Title:        %s\n", post.Title)
	if post.Description != "" {
		fmt.Printf("Description:  %s\n", post.Description)
	}
	fmt.Printf("Posted:       %s\n", post.CreatedAt.Format("2006-01-02 15:04:05"))
	fmt.Printf("Likes:        %d\n", post.LikeCount)
	fmt.Printf("Comments:     %d\n", post.CommentCount)
	fmt.Printf("Reposts:      %d\n", post.RepostCount)
	if post.ReportCount > 0 {
		fmt.Printf("Reports:      %d 🚩\n", post.ReportCount)
	}
	fmt.Printf("\n")
}

// Helper display function for comments
func displayFlaggedComments(comments []api.AdminComment) {
	fmt.Printf("\n💬 Flagged Comments (%d)\n\n", len(comments))

	for i, comment := range comments {
		fmt.Printf("%d. %s\n", i+1, comment.ID)
		fmt.Printf("   Author: @%s\n", comment.AuthorUsername)
		fmt.Printf("   Content: %s\n", comment.Content)
		fmt.Printf("   Posted: %s\n", comment.CreatedAt)
		fmt.Printf("   Likes: %d\n", comment.LikeCount)
		fmt.Printf("\n")
	}
}

// Helper display function for users list
func displayUsers(users *api.UserListResponse) {
	fmt.Printf("\n👥 Users (%d)\n\n", len(users.Users))

	for i, user := range users.Users {
		status := "Active"
		if user.IsSuspended {
			status = "🚫 Suspended"
		} else if user.IsVerified {
			status = "✓ Verified"
		}

		fmt.Printf("%d. @%s\n", i+1, user.Username)
		fmt.Printf("   Email: %s\n", user.Email)
		if user.FirstName != "" || user.LastName != "" {
			fmt.Printf("   Name: %s %s\n", user.FirstName, user.LastName)
		}
		fmt.Printf("   Status: %s\n", status)
		fmt.Printf("   2FA: %v\n", user.TwoFAEnabled)
		fmt.Printf("   Posts: %d | Followers: %d | Following: %d\n", user.PostCount, user.FollowersCount, user.FollowingCount)
		fmt.Printf("   Joined: %s\n", user.CreatedAt.Format("2006-01-02"))
		if user.Warnings > 0 {
			fmt.Printf("   ⚠️  Warnings: %d\n", user.Warnings)
		}
		fmt.Printf("\n")
	}

	fmt.Printf("Showing %d of %d users (Page %d)\n\n", len(users.Users), users.TotalCount, users.Page)
}

// Helper display function for user details
func displayUserDetail(user *api.User) {
	fmt.Printf("\n👤 User Details\n")
	fmt.Printf("ID:           %s\n", user.ID)
	fmt.Printf("Username:     @%s\n", user.Username)
	fmt.Printf("Email:        %s\n", user.Email)

	if user.FirstName != "" {
		fmt.Printf("First Name:   %s\n", user.FirstName)
	}
	if user.LastName != "" {
		fmt.Printf("Last Name:    %s\n", user.LastName)
	}
	if user.Bio != "" {
		fmt.Printf("Bio:          %s\n", user.Bio)
	}

	fmt.Printf("Verification: ")
	if user.IsVerified {
		fmt.Printf("✓ Verified\n")
	} else {
		fmt.Printf("Unverified\n")
	}

	fmt.Printf("2FA Enabled:  %v\n", user.TwoFAEnabled)

	if user.IsSuspended {
		fmt.Printf("Status:       🚫 SUSPENDED\n")
		if user.SuspendedAt != nil {
			fmt.Printf("Suspended:    %s\n", user.SuspendedAt.Format("2006-01-02 15:04:05"))
		}
		if user.SuspensionReason != "" {
			fmt.Printf("Reason:       %s\n", user.SuspensionReason)
		}
	} else {
		fmt.Printf("Status:       Active\n")
	}

	fmt.Printf("\nActivity:\n")
	fmt.Printf("  Posts:      %d\n", user.PostCount)
	fmt.Printf("  Followers:  %d\n", user.FollowersCount)
	fmt.Printf("  Following:  %d\n", user.FollowingCount)

	fmt.Printf("\nAccount Info:\n")
	fmt.Printf("  Joined:     %s\n", user.CreatedAt.Format("2006-01-02 15:04:05"))
	fmt.Printf("  Last Update: %s\n", user.UpdatedAt.Format("2006-01-02 15:04:05"))

	if user.LastLoginAt != nil {
		fmt.Printf("  Last Login: %s\n", user.LastLoginAt.Format("2006-01-02 15:04:05"))
	}

	if user.Warnings > 0 {
		fmt.Printf("\n⚠️  Warnings: %d\n", user.Warnings)
	}

	fmt.Printf("\n")
}
