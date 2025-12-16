package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// ActivityStatus represents the user's current activity status
type ActivityStatus struct {
	Status      string `json:"status"`              // online, away, offline, do-not-disturb
	StatusText  string `json:"status_text"`         // Custom status message
	LastSeen    string `json:"last_seen"`           // ISO 8601 timestamp
	IsVisible   bool   `json:"is_visible"`          // Whether status is visible to others
	UpdatedAt   string `json:"updated_at"`          // ISO 8601 timestamp
	ShowOnline  bool   `json:"show_online"`         // Show online status
	AutoOffline int    `json:"auto_offline_minutes"` // Auto offline after N minutes
}

// StatusResponse wraps the activity status response
type StatusResponse struct {
	Status ActivityStatus `json:"status"`
	Message string         `json:"message"`
}

// GetActivityStatus retrieves the current user's activity status
func GetActivityStatus() (*ActivityStatus, error) {
	logger.Debug("Getting activity status")

	var result ActivityStatus
	resp, err := client.GetClient().
		R().
		SetResult(&result).
		Get("/api/v1/users/me/status")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get activity status: %s", resp.Status())
	}

	return &result, nil
}

// SetActivityStatus updates the user's activity status
func SetActivityStatus(status string) (*ActivityStatus, error) {
	logger.Debug("Setting activity status", "status", status)

	request := map[string]string{
		"status": status, // online, away, offline, do-not-disturb
	}

	var result ActivityStatus
	resp, err := client.GetClient().
		R().
		SetBody(request).
		SetResult(&result).
		Post("/api/v1/users/me/status")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to set activity status: %s", resp.Status())
	}

	return &result, nil
}

// SetStatusText updates the user's custom status message
func SetStatusText(statusText string) (*ActivityStatus, error) {
	logger.Debug("Setting status text", "text", statusText)

	request := map[string]string{
		"status_text": statusText,
	}

	var result ActivityStatus
	resp, err := client.GetClient().
		R().
		SetBody(request).
		SetResult(&result).
		Post("/api/v1/users/me/status/text")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to set status text: %s", resp.Status())
	}

	return &result, nil
}

// SetStatusVisibility configures whether the status is visible to others
func SetStatusVisibility(isVisible bool) (*ActivityStatus, error) {
	logger.Debug("Setting status visibility", "visible", isVisible)

	request := map[string]bool{
		"is_visible": isVisible,
	}

	var result ActivityStatus
	resp, err := client.GetClient().
		R().
		SetBody(request).
		SetResult(&result).
		Post("/api/v1/users/me/status/visibility")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to set status visibility: %s", resp.Status())
	}

	return &result, nil
}

// SetAutoOfflineMinutes sets the auto-offline timeout in minutes
func SetAutoOfflineMinutes(minutes int) (*ActivityStatus, error) {
	logger.Debug("Setting auto offline timeout", "minutes", minutes)

	request := map[string]int{
		"auto_offline_minutes": minutes,
	}

	var result ActivityStatus
	resp, err := client.GetClient().
		R().
		SetBody(request).
		SetResult(&result).
		Post("/api/v1/users/me/status/auto-offline")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to set auto offline timeout: %s", resp.Status())
	}

	return &result, nil
}

// ClearStatusText clears the custom status message
func ClearStatusText() (*ActivityStatus, error) {
	logger.Debug("Clearing status text")

	var result ActivityStatus
	resp, err := client.GetClient().
		R().
		SetBody(map[string]string{"status_text": ""}).
		SetResult(&result).
		Post("/api/v1/users/me/status/text")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to clear status text: %s", resp.Status())
	}

	return &result, nil
}
