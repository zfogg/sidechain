package api

import (
	json "github.com/json-iterator/go"
	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// Login authenticates user with email and password
func Login(email, password string) (*LoginResponse, error) {
	logger.Debug("Attempting login", "email", email)

	req := LoginRequest{
		Email:    email,
		Password: password,
	}

	reqBody, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}

	resp, err := client.GetClient().
		R().
		SetHeader("Content-Type", "application/json").
		SetBody(reqBody).
		Post("/api/v1/auth/login")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var loginResp LoginResponse
	if err := json.Unmarshal(resp.Body(), &loginResp); err != nil {
		return nil, err
	}

	logger.Debug("Login successful", "username", loginResp.User.Username)
	return &loginResp, nil
}

// Refresh refreshes the access token using refresh token
func Refresh(refreshToken string) (*RefreshResponse, error) {
	logger.Debug("Refreshing access token")

	req := RefreshRequest{
		RefreshToken: refreshToken,
	}

	reqBody, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}

	resp, err := client.GetClient().
		R().
		SetHeader("Content-Type", "application/json").
		SetBody(reqBody).
		Post("/api/v1/auth/refresh")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var refreshResp RefreshResponse
	if err := json.Unmarshal(resp.Body(), &refreshResp); err != nil {
		return nil, err
	}

	logger.Debug("Access token refreshed")
	return &refreshResp, nil
}

// GetCurrentUser gets the current authenticated user
func GetCurrentUser() (*User, error) {
	logger.Debug("Fetching current user")

	resp, err := client.GetClient().
		R().
		Get("/api/v1/auth/me")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var profileResp ProfileResponse
	if err := json.Unmarshal(resp.Body(), &profileResp); err != nil {
		return nil, err
	}

	logger.Debug("Current user fetched", "username", profileResp.User.Username)
	return &profileResp.User, nil
}

// Enable2FA enables two-factor authentication
func Enable2FA() (map[string]interface{}, error) {
	logger.Debug("Enabling 2FA")

	resp, err := client.GetClient().
		R().
		Post("/api/v1/auth/2fa/enable")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var result map[string]interface{}
	if err := json.Unmarshal(resp.Body(), &result); err != nil {
		return nil, err
	}

	return result, nil
}

// Verify2FA verifies a 2FA code
func Verify2FA(code string) error {
	logger.Debug("Verifying 2FA code")

	reqBody := map[string]string{
		"code": code,
	}

	data, err := json.Marshal(reqBody)
	if err != nil {
		return err
	}

	resp, err := client.GetClient().
		R().
		SetHeader("Content-Type", "application/json").
		SetBody(data).
		Post("/api/v1/auth/2fa/verify")

	return CheckResponse(resp, err)
}

// Disable2FA disables two-factor authentication
func Disable2FA(code string) error {
	logger.Debug("Disabling 2FA")

	reqBody := map[string]string{
		"code": code,
	}

	data, err := json.Marshal(reqBody)
	if err != nil {
		return err
	}

	resp, err := client.GetClient().
		R().
		SetHeader("Content-Type", "application/json").
		SetBody(data).
		Post("/api/v1/auth/2fa/disable")

	return CheckResponse(resp, err)
}

// GetBackupCodes regenerates backup codes
func GetBackupCodes() ([]string, error) {
	logger.Debug("Getting backup codes")

	resp, err := client.GetClient().
		R().
		Post("/api/v1/auth/2fa/backup-codes")

	if err := CheckResponse(resp, err); err != nil {
		return nil, err
	}

	var result map[string]interface{}
	if err := json.Unmarshal(resp.Body(), &result); err != nil {
		return nil, err
	}

	// Extract codes from response
	codes, ok := result["backup_codes"].([]interface{})
	if !ok {
		return nil, nil
	}

	var backupCodes []string
	for _, code := range codes {
		if codeStr, ok := code.(string); ok {
			backupCodes = append(backupCodes, codeStr)
		}
	}

	return backupCodes, nil
}
