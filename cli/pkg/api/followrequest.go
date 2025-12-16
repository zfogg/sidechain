package api

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// FollowRequest represents a follow request
type FollowRequest struct {
	ID        string    `json:"id"`
	RequesterID string  `json:"requester_id"`
	TargetID  string    `json:"target_id"`
	Status    string    `json:"status"` // pending, accepted, rejected
	CreatedAt time.Time `json:"created_at"`
	UpdatedAt time.Time `json:"updated_at"`
}

type FollowRequestListResponse struct {
	Requests   []FollowRequest `json:"requests"`
	TotalCount int             `json:"total_count"`
	Page       int             `json:"page"`
	PageSize   int             `json:"page_size"`
}

// GetFollowRequests retrieves pending follow requests
func GetFollowRequests(page, pageSize int) (*FollowRequestListResponse, error) {
	logger.Debug("Fetching follow requests", "page", page)

	var response FollowRequestListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/follow-requests")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch follow requests: %s", resp.Status())
	}

	return &response, nil
}

// AcceptFollowRequest accepts a follow request
func AcceptFollowRequest(username string) error {
	logger.Debug("Accepting follow request", "username", username)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/follow-requests/%s/accept", username))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to accept follow request: %s", resp.Status())
	}

	return nil
}

// RejectFollowRequest rejects a follow request
func RejectFollowRequest(username string) error {
	logger.Debug("Rejecting follow request", "username", username)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/follow-requests/%s/reject", username))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to reject follow request: %s", resp.Status())
	}

	return nil
}

// CancelFollowRequest cancels a follow request sent by the user
func CancelFollowRequest(username string) error {
	logger.Debug("Cancelling follow request", "username", username)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/follow-requests/%s", username))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to cancel follow request: %s", resp.Status())
	}

	return nil
}
