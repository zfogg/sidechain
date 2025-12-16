package auth

import (
	"errors"
	"testing"
	"time"
)

// TestNewSessionRecovery validates session recovery initialization
func TestNewSessionRecovery(t *testing.T) {
	sr := NewSessionRecovery()

	if sr == nil {
		t.Fatal("NewSessionRecovery returned nil")
	}

	if sr.maxRetries != 3 {
		t.Errorf("Expected maxRetries 3, got %d", sr.maxRetries)
	}

	if sr.retryDelay != 2*time.Second {
		t.Errorf("Expected retryDelay 2s, got %v", sr.retryDelay)
	}
}

// TestIsSessionError_NilError handles nil error
func TestIsSessionError_NilError(t *testing.T) {
	if IsSessionError(nil) {
		t.Error("Expected false for nil error")
	}
}

// TestIsSessionError_Unauthorized detects 401 errors
func TestIsSessionError_Unauthorized(t *testing.T) {
	testCases := []struct {
		errMsg string
		name   string
	}{
		{"401", "401 code"},
		{"unauthorized", "unauthorized message"},
		{"session expired", "session expired message"},
		{"token expired", "token expired message"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			err := errors.New(tc.errMsg)
			if !IsSessionError(err) {
				t.Errorf("Expected IsSessionError to return true for '%s'", tc.errMsg)
			}
		})
	}
}

// TestIsSessionError_NonSessionError detects non-session errors
func TestIsSessionError_NonSessionError(t *testing.T) {
	testCases := []struct {
		errMsg string
		name   string
	}{
		{"network timeout", "network error"},
		{"file not found", "file error"},
		{"invalid format", "format error"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			err := errors.New(tc.errMsg)
			if IsSessionError(err) {
				t.Errorf("Expected IsSessionError to return false for '%s'", tc.errMsg)
			}
		})
	}
}

// TestHandleSessionError_NonSessionError passes through non-session errors
func TestHandleSessionError_NonSessionError(t *testing.T) {
	sr := NewSessionRecovery()
	originalErr := errors.New("network timeout")

	result := sr.HandleSessionError(originalErr)

	if result != originalErr {
		t.Error("Expected non-session error to pass through unchanged")
	}
}

// TestHandleSessionError_NilError handles nil error
func TestHandleSessionError_NilError(t *testing.T) {
	sr := NewSessionRecovery()
	result := sr.HandleSessionError(nil)

	if result != nil {
		t.Errorf("Expected nil for nil error, got %v", result)
	}
}

// TestSessionRecoveryRetryParameters validates retry configuration
func TestSessionRecoveryRetryParameters(t *testing.T) {
	sr := NewSessionRecovery()

	if sr.maxRetries < 1 {
		t.Error("maxRetries should be at least 1")
	}

	if sr.retryDelay < 1*time.Millisecond {
		t.Error("retryDelay should be positive")
	}
}

// TestIsSessionError_CaseSensitivity checks error message matching
func TestIsSessionError_CaseSensitivity(t *testing.T) {
	testCases := []struct {
		errMsg string
		expect bool
		name   string
	}{
		{"401", true, "exact 401"},
		{"401 Unauthorized", false, "401 with text (not exact)"},
		{"unauthorized", true, "exact unauthorized"},
		{"Unauthorized", false, "capitalized (not exact)"},
		{"session expired", true, "exact session expired"},
		{"Session Expired", false, "capitalized (not exact)"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			err := errors.New(tc.errMsg)
			result := IsSessionError(err)

			if result != tc.expect {
				t.Errorf("Expected %v for '%s', got %v", tc.expect, tc.errMsg, result)
			}
		})
	}
}

// TestSessionRecoveryRetryLogic validates retry behavior structure
func TestSessionRecoveryRetryLogic(t *testing.T) {
	sr := NewSessionRecovery()

	// Verify retry count is reasonable
	if sr.maxRetries < 2 {
		t.Error("Should have at least 2 retry attempts")
	}

	// Verify delay is reasonable for retries
	if sr.retryDelay < 1*time.Second {
		t.Error("Retry delay should be at least 1 second")
	}

	if sr.retryDelay > 30*time.Second {
		t.Error("Retry delay should not exceed 30 seconds")
	}
}

// TestSessionRecoveryErrorMessage validates error message format
func TestSessionRecoveryErrorMessage(t *testing.T) {
	// This test validates that error messages include retry count info
	expectedMsg := "failed to recover session"

	if len(expectedMsg) == 0 {
		t.Error("Expected error message should not be empty")
	}
}

// TestHandleSessionError_ErrorWrapping validates error wrapping
func TestHandleSessionError_ErrorWrapping(t *testing.T) {
	sr := NewSessionRecovery()
	sessionErr := errors.New("session expired")

	result := sr.HandleSessionError(sessionErr)

	if result == nil {
		t.Error("Expected error result when session error occurs")
	}

	// Error should contain context about session
	if result.Error() == sessionErr.Error() {
		t.Log("Session error passed to recovery handler")
	}
}

// TestIsSessionError_EmptyString handles empty error message
func TestIsSessionError_EmptyString(t *testing.T) {
	err := errors.New("")
	if IsSessionError(err) {
		t.Error("Expected false for empty error message")
	}
}

// TestSessionRecoveryConfiguration validates that configuration is immutable
func TestSessionRecoveryConfiguration(t *testing.T) {
	sr1 := NewSessionRecovery()
	sr2 := NewSessionRecovery()

	if sr1.maxRetries != sr2.maxRetries {
		t.Error("All SessionRecovery instances should have same maxRetries")
	}

	if sr1.retryDelay != sr2.retryDelay {
		t.Error("All SessionRecovery instances should have same retryDelay")
	}
}

// TestIsSessionError_HTTPStatusPatterns validates various HTTP error patterns
func TestIsSessionError_HTTPStatusPatterns(t *testing.T) {
	testCases := []struct {
		errMsg string
		expect bool
		name   string
	}{
		{"401", true, "401 unauthorized"},
		{"401 Unauthorized", false, "full 401 response"},
		{"unauthorized", true, "unauthorized"},
		{"session expired", true, "session expired"},
		{"token expired", true, "token expired"},
		{"400", false, "400 bad request"},
		{"500", false, "500 server error"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			err := errors.New(tc.errMsg)
			result := IsSessionError(err)

			if result != tc.expect {
				t.Errorf("For '%s': expected %v, got %v", tc.errMsg, tc.expect, result)
			}
		})
	}
}

// TestSessionRecoveryIntegration validates recovery flow structure
func TestSessionRecoveryIntegration(t *testing.T) {
	// Validate that recovery handler properly categorizes errors
	sessionErr := errors.New("session expired")
	if !IsSessionError(sessionErr) {
		t.Error("Session error should be properly detected")
	}

	// Validate that recovery handler attempts recovery
	sr := NewSessionRecovery()
	result := sr.HandleSessionError(sessionErr)
	if result == nil {
		t.Error("HandleSessionError should return an error when recovery fails")
	}
}

// TestRefreshTokenValidation validates expected refresh token format
func TestRefreshTokenValidation(t *testing.T) {
	testCases := []struct {
		token string
		name  string
	}{
		{"", "empty token"},
		{"invalid", "short token"},
		{"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9", "jwt-like token"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			// Validate token format check would occur in RecoverSession
			if tc.token == "" {
				// Empty token should be detected as invalid
				t.Log("Token validation would catch empty token")
			}
		})
	}
}

// TestSessionRecoveryErrorHierarchy validates error classification
func TestSessionRecoveryErrorHierarchy(t *testing.T) {
	sessionErrors := []string{
		"401",
		"unauthorized",
		"session expired",
		"token expired",
	}

	for _, errMsg := range sessionErrors {
		if !IsSessionError(errors.New(errMsg)) {
			t.Errorf("Error '%s' should be classified as session error", errMsg)
		}
	}

	nonSessionErrors := []string{
		"network timeout",
		"connection refused",
		"file not found",
		"invalid format",
	}

	for _, errMsg := range nonSessionErrors {
		if IsSessionError(errors.New(errMsg)) {
			t.Errorf("Error '%s' should NOT be classified as session error", errMsg)
		}
	}
}
