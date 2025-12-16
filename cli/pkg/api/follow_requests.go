package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// FollowRequest represents a pending follow request
type FollowRequest struct {
	ID          string `json:"id"`
	RequesterID string `json:"requester_id"`
	Username    string `json:"username"`
	DisplayName string `json:"display_name"`
	AvatarURL   string `json:"avatar_url"`
	Bio         string `json:"bio"`
	CreatedAt   string `json:"created_at"`
}

// FollowRequestsResponse lists follow requests
type FollowRequestsResponse struct {
	Requests []FollowRequest `json:"requests"`
	Count    int             `json:"count"`
}

// FollowRequestStatusResponse represents follow request status
type FollowRequestStatusResponse struct {
	Status        string `json:"status"`
	HasRequest    bool   `json:"has_request"`
	RequestID     string `json:"request_id,omitempty"`
	CreatedAt     string `json:"created_at,omitempty"`
}

// GetFollowRequests retrieves pending follow requests for current user
func GetFollowRequests() (*FollowRequestsResponse, error) {
	logger.Debug("Getting follow requests")

	var response FollowRequestsResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get("/api/v1/users/me/follow-requests")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get follow requests: %s", resp.Status())
	}

	return &response, nil
}

// GetPendingFollowRequests retrieves follow requests sent by current user
func GetPendingFollowRequests() (*FollowRequestsResponse, error) {
	logger.Debug("Getting pending follow requests")

	var response FollowRequestsResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get("/api/v1/users/me/pending-requests")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get pending requests: %s", resp.Status())
	}

	return &response, nil
}

// GetFollowRequestCount retrieves count of pending requests
func GetFollowRequestCount() (int, error) {
	logger.Debug("Getting follow request count")

	var response struct {
		Count int `json:"count"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get("/api/v1/users/me/follow-requests/count")

	if err != nil {
		return 0, err
	}

	if !resp.IsSuccess() {
		return 0, fmt.Errorf("failed to get follow request count: %s", resp.Status())
	}

	return response.Count, nil
}

// AcceptFollowRequest accepts a follow request
func AcceptFollowRequest(requestID string) error {
	logger.Debug("Accepting follow request", "request_id", requestID)

	var response struct {
		Status  string `json:"status"`
		Message string `json:"message"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/follow-requests/%s/accept", requestID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to accept follow request: %s", resp.Status())
	}

	return nil
}

// RejectFollowRequest rejects a follow request
func RejectFollowRequest(requestID string) error {
	logger.Debug("Rejecting follow request", "request_id", requestID)

	var response struct {
		Status  string `json:"status"`
		Message string `json:"message"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/follow-requests/%s/reject", requestID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to reject follow request: %s", resp.Status())
	}

	return nil
}

// CancelFollowRequest cancels a follow request sent by current user
func CancelFollowRequest(requestID string) error {
	logger.Debug("Cancelling follow request", "request_id", requestID)

	var response struct {
		Status  string `json:"status"`
		Message string `json:"message"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Delete(fmt.Sprintf("/api/v1/follow-requests/%s", requestID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to cancel follow request: %s", resp.Status())
	}

	return nil
}

// CheckFollowRequestStatus checks status with a specific user
func CheckFollowRequestStatus(userID string) (*FollowRequestStatusResponse, error) {
	logger.Debug("Checking follow request status", "user_id", userID)

	var response FollowRequestStatusResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/users/%s/follow-request-status", userID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to check follow request status: %s", resp.Status())
	}

	return &response, nil
}
