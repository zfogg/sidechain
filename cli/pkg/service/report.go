package service

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
)

// ReportService handles user reporting operations
type ReportService struct{}

// NewReportService creates a new report service
func NewReportService() *ReportService {
	return &ReportService{}
}

// ReportReasons defines common report reasons
var ReportReasons = map[string][]string{
	"post": {
		"inappropriate_content",
		"harassment_or_bullying",
		"hate_speech",
		"spam_or_scam",
		"copyright_violation",
		"misinformation",
		"other",
	},
	"comment": {
		"inappropriate_content",
		"harassment_or_bullying",
		"hate_speech",
		"spam_or_scam",
		"other",
	},
	"user": {
		"harassment",
		"hate_speech",
		"spam_or_scam",
		"impersonation",
		"underage_account",
		"other",
	},
}

// ReportPost submits a report for a post
func (s *ReportService) ReportPost(postID string) error {
	fmt.Printf("\n📋 Report Post %s\n\n", postID)

	// Select reason
	reasons := ReportReasons["post"]
	reasonIdx, _ := prompter.PromptSelect("Select reason for report:", reasons)
	reason := reasons[reasonIdx]

	// Get optional description
	description, _ := prompter.PromptString("Additional details (optional): ")

	// Confirm before submitting
	fmt.Printf("\nReport Details:\n")
	fmt.Printf("  Post ID: %s\n", postID)
	fmt.Printf("  Reason: %s\n", reason)
	if description != "" {
		fmt.Printf("  Details: %s\n", description)
	}

	confirmed, _ := prompter.PromptConfirm("\nSubmit report?")
	if !confirmed {
		fmt.Println("Report cancelled.")
		return nil
	}

	// Submit report
	if err := api.ReportContent("post", postID, reason, description); err != nil {
		return fmt.Errorf("failed to report post: %w", err)
	}

	fmt.Printf("\n✓ Post reported successfully. Thank you for helping keep Sidechain safe.\n\n")
	return nil
}

// ReportComment submits a report for a comment
func (s *ReportService) ReportComment(commentID string) error {
	fmt.Printf("\n📋 Report Comment %s\n\n", commentID)

	// Select reason
	reasons := ReportReasons["comment"]
	reasonIdx, _ := prompter.PromptSelect("Select reason for report:", reasons)
	reason := reasons[reasonIdx]

	// Get optional description
	description, _ := prompter.PromptString("Additional details (optional): ")

	// Confirm before submitting
	fmt.Printf("\nReport Details:\n")
	fmt.Printf("  Comment ID: %s\n", commentID)
	fmt.Printf("  Reason: %s\n", reason)
	if description != "" {
		fmt.Printf("  Details: %s\n", description)
	}

	confirmed, _ := prompter.PromptConfirm("\nSubmit report?")
	if !confirmed {
		fmt.Println("Report cancelled.")
		return nil
	}

	// Submit report
	if err := api.ReportContent("comment", commentID, reason, description); err != nil {
		return fmt.Errorf("failed to report comment: %w", err)
	}

	fmt.Printf("\n✓ Comment reported successfully. Thank you for helping keep Sidechain safe.\n\n")
	return nil
}

// ReportUser submits a report for a user
func (s *ReportService) ReportUser(username string) error {
	fmt.Printf("\n📋 Report User @%s\n\n", username)

	// Select reason
	reasons := ReportReasons["user"]
	reasonIdx, _ := prompter.PromptSelect("Select reason for report:", reasons)
	reason := reasons[reasonIdx]

	// Get optional description
	description, _ := prompter.PromptString("Additional details (optional): ")

	// Confirm before submitting
	fmt.Printf("\nReport Details:\n")
	fmt.Printf("  Username: @%s\n", username)
	fmt.Printf("  Reason: %s\n", reason)
	if description != "" {
		fmt.Printf("  Details: %s\n", description)
	}

	confirmed, _ := prompter.PromptConfirm("\nSubmit report?")
	if !confirmed {
		fmt.Println("Report cancelled.")
		return nil
	}

	// Submit report
	if err := api.ReportContent("user", username, reason, description); err != nil {
		return fmt.Errorf("failed to report user: %w", err)
	}

	fmt.Printf("\n✓ User reported successfully. Thank you for helping keep Sidechain safe.\n\n")
	return nil
}

