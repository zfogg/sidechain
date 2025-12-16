package auth

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/credentials"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// SessionRecovery handles automatic session recovery
type SessionRecovery struct {
	maxRetries int
	retryDelay time.Duration
}

// NewSessionRecovery creates a new session recovery handler
func NewSessionRecovery() *SessionRecovery {
	return &SessionRecovery{
		maxRetries: 3,
		retryDelay: 2 * time.Second,
	}
}

// RecoverSession attempts to recover an expired session
func (sr *SessionRecovery) RecoverSession() error {
	logger.Debug("Attempting to recover session")

	creds, err := credentials.Load()
	if err != nil {
		return fmt.Errorf("failed to load credentials: %w", err)
	}

	if creds == nil || creds.RefreshToken == "" {
		return fmt.Errorf("no refresh token available - please log in again")
	}

	// Try to refresh the token
	for attempt := 1; attempt <= sr.maxRetries; attempt++ {
		logger.Debug("Refreshing token", "attempt", attempt)

		refreshResp, err := api.Refresh(creds.RefreshToken)
		if err == nil {
			// Success - update credentials with new token
			creds.AccessToken = refreshResp.AccessToken
			if err := credentials.Save(creds); err != nil {
				logger.Error("Failed to save updated credentials", "error", err)
			}
			return nil
		}

		if attempt < sr.maxRetries {
			time.Sleep(sr.retryDelay)
		}
	}

	return fmt.Errorf("failed to recover session after %d attempts - please log in again", sr.maxRetries)
}

// IsSessionError checks if an error is a session-related error
func IsSessionError(err error) bool {
	if err == nil {
		return false
	}

	errMsg := err.Error()
	return errMsg == "401" ||
		errMsg == "unauthorized" ||
		errMsg == "session expired" ||
		errMsg == "token expired"
}

// HandleSessionError handles session-related errors with recovery
func (sr *SessionRecovery) HandleSessionError(err error) error {
	if !IsSessionError(err) {
		return err
	}

	logger.Debug("Handling session error with recovery")

	// Try to recover the session
	if recoveryErr := sr.RecoverSession(); recoveryErr != nil {
		logger.Error("Session recovery failed", "error", recoveryErr)
		return fmt.Errorf("session expired: %w", recoveryErr)
	}

	return nil
}
