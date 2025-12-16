package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// GetUsers retrieves a list of users (admin only)
func GetUsers(page, pageSize int, verified, twoFA bool) (*UserListResponse, error) {
	logger.Debug("Listing users", "page", page, "page_size", pageSize)

	var response UserListResponse

	request := client.GetClient().R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response)

	// Add optional filters
	if verified {
		request.SetQueryParam("verified", "true")
	}
	if twoFA {
		request.SetQueryParam("two_fa_enabled", "true")
	}

	resp, err := request.Get("/api/v1/admin/users")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to list users: %s", resp.Status())
	}

	return &response, nil
}

// GetUser retrieves a user by username (admin only)
func GetUser(username string) (*User, error) {
	logger.Debug("Fetching user", "username", username)

	var response struct {
		User User `json:"user"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/admin/users/%s", username))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch user: %s", resp.Status())
	}

	return &response.User, nil
}

// SuspendUser suspends a user account (admin only)
func SuspendUser(username, reason string) error {
	logger.Debug("Suspending user", "username", username)

	requestBody := map[string]string{
		"reason": reason,
	}

	resp, err := client.GetClient().
		R().
		SetBody(requestBody).
		Patch(fmt.Sprintf("/api/v1/admin/users/%s/suspend", username))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to suspend user: %s", resp.Status())
	}

	return nil
}

// UnsuspendUser lifts a suspension from a user account (admin only)
func UnsuspendUser(username string) error {
	logger.Debug("Unsuspending user", "username", username)

	resp, err := client.GetClient().
		R().
		Patch(fmt.Sprintf("/api/v1/admin/users/%s/unsuspend", username))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to unsuspend user: %s", resp.Status())
	}

	return nil
}

// WarnUser issues a warning to a user (admin only)
func WarnUser(username, reason string) error {
	logger.Debug("Warning user", "username", username)

	requestBody := map[string]string{
		"reason": reason,
	}

	resp, err := client.GetClient().
		R().
		SetBody(requestBody).
		Post(fmt.Sprintf("/api/v1/admin/users/%s/warn", username))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to warn user: %s", resp.Status())
	}

	return nil
}

// VerifyUserEmail manually verifies a user's email (admin only)
func VerifyUserEmail(username string) error {
	logger.Debug("Verifying user email", "username", username)

	resp, err := client.GetClient().
		R().
		Patch(fmt.Sprintf("/api/v1/admin/users/%s/verify-email", username))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to verify user email: %s", resp.Status())
	}

	return nil
}

// ResetUserPassword forces a password reset for a user (admin only)
func ResetUserPassword(username string) error {
	logger.Debug("Resetting user password", "username", username)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/admin/users/%s/reset-password", username))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to reset user password: %s", resp.Status())
	}

	return nil
}
