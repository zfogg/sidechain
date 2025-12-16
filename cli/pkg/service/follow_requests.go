package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// FollowRequestService provides operations for managing follow requests
type FollowRequestService struct{}

// NewFollowRequestService creates a new follow request service
func NewFollowRequestService() *FollowRequestService {
	return &FollowRequestService{}
}

// GetFollowRequests displays incoming follow requests
func (frs *FollowRequestService) GetFollowRequests() error {
	resp, err := api.GetFollowRequests()
	if err != nil {
		return fmt.Errorf("failed to fetch follow requests: %w", err)
	}

	if len(resp.Requests) == 0 {
		fmt.Println("No pending follow requests")
		return nil
	}

	formatter.PrintInfo("📬 Incoming Follow Requests (%d total)", resp.Count)
	fmt.Printf("\n")

	// Display requests
	for i, req := range resp.Requests {
		fmt.Printf("[%d] %s (@%s)\n", i+1, req.DisplayName, req.Username)
		if req.Bio != "" {
			fmt.Printf("    Bio: %s\n", truncate(req.Bio, 60))
		}
		fmt.Printf("    Followers: %d | Requested: %s\n\n", len(resp.Requests), req.CreatedAt)
	}

	return nil
}

// GetPendingFollowRequests displays outgoing follow requests
func (frs *FollowRequestService) GetPendingFollowRequests() error {
	resp, err := api.GetPendingFollowRequests()
	if err != nil {
		return fmt.Errorf("failed to fetch pending requests: %w", err)
	}

	if len(resp.Requests) == 0 {
		fmt.Println("No pending follow requests sent")
		return nil
	}

	formatter.PrintInfo("⏳ Pending Follow Requests (%d total)", resp.Count)
	fmt.Printf("\n")

	// Display requests
	for i, req := range resp.Requests {
		fmt.Printf("[%d] %s (@%s)\n", i+1, req.DisplayName, req.Username)
		if req.Bio != "" {
			fmt.Printf("    Bio: %s\n", truncate(req.Bio, 60))
		}
		fmt.Printf("    Sent: %s\n\n", req.CreatedAt)
	}

	return nil
}

// AcceptFollowRequest accepts an incoming follow request
func (frs *FollowRequestService) AcceptFollowRequest() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Follow Request ID: ")
	requestID, _ := reader.ReadString('\n')
	requestID = strings.TrimSpace(requestID)

	if requestID == "" {
		return fmt.Errorf("follow request ID cannot be empty")
	}

	// Accept request
	err := api.AcceptFollowRequest(requestID)
	if err != nil {
		return fmt.Errorf("failed to accept follow request: %w", err)
	}

	formatter.PrintSuccess("✓ Follow request accepted!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Request ID": requestID,
		"Status":     "Accepted",
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// RejectFollowRequest rejects an incoming follow request
func (frs *FollowRequestService) RejectFollowRequest() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Follow Request ID: ")
	requestID, _ := reader.ReadString('\n')
	requestID = strings.TrimSpace(requestID)

	if requestID == "" {
		return fmt.Errorf("follow request ID cannot be empty")
	}

	// Reject request
	err := api.RejectFollowRequest(requestID)
	if err != nil {
		return fmt.Errorf("failed to reject follow request: %w", err)
	}

	formatter.PrintSuccess("✓ Follow request rejected!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Request ID": requestID,
		"Status":     "Rejected",
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// CancelFollowRequest cancels an outgoing follow request
func (frs *FollowRequestService) CancelFollowRequest() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Follow Request ID: ")
	requestID, _ := reader.ReadString('\n')
	requestID = strings.TrimSpace(requestID)

	if requestID == "" {
		return fmt.Errorf("follow request ID cannot be empty")
	}

	// Confirm cancellation
	fmt.Print("Are you sure you want to cancel this follow request? (yes/no): ")
	confirm, _ := reader.ReadString('\n')
	confirm = strings.TrimSpace(strings.ToLower(confirm))

	if confirm != "yes" && confirm != "y" {
		fmt.Println("Cancellation cancelled")
		return nil
	}

	// Cancel request
	err := api.CancelFollowRequest(requestID)
	if err != nil {
		return fmt.Errorf("failed to cancel follow request: %w", err)
	}

	formatter.PrintSuccess("✓ Follow request cancelled!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Request ID": requestID,
		"Status":     "Cancelled",
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// CheckFollowRequestStatus checks if follow request exists with a user
func (frs *FollowRequestService) CheckFollowRequestStatus() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("User ID: ")
	userID, _ := reader.ReadString('\n')
	userID = strings.TrimSpace(userID)

	if userID == "" {
		return fmt.Errorf("user ID cannot be empty")
	}

	// Check status
	statusResp, err := api.CheckFollowRequestStatus(userID)
	if err != nil {
		return fmt.Errorf("failed to check follow request status: %w", err)
	}

	formatter.PrintInfo("Status with user:")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"User ID":          userID,
		"Has Request":      fmt.Sprintf("%v", statusResp.HasRequest),
		"Status":           statusResp.Status,
	}
	if statusResp.RequestID != "" {
		keyValues["Request ID"] = statusResp.RequestID
	}
	if statusResp.CreatedAt != "" {
		keyValues["Created At"] = statusResp.CreatedAt
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// GetFollowRequestCount displays count of pending requests
func (frs *FollowRequestService) GetFollowRequestCount() error {
	count, err := api.GetFollowRequestCount()
	if err != nil {
		return fmt.Errorf("failed to get follow request count: %w", err)
	}

	formatter.PrintInfo("📬 Follow Requests")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Pending Requests": count,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}
