package credentials

import (
	"testing"
	"time"
)

// TestCredentialsIsExpired validates token expiration check
func TestCredentialsIsExpired(t *testing.T) {
	testCases := []struct {
		expiresAt time.Time
		expect    bool
		name      string
	}{
		{time.Now().Add(-1 * time.Hour), true, "past expiration"},
		{time.Now().Add(1 * time.Hour), false, "future expiration"},
		{time.Now().Add(-1 * time.Minute), true, "recently expired"},
		{time.Now().Add(1 * time.Minute), false, "expiring soon"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			creds := &Credentials{
				AccessToken: "test_token",
				ExpiresAt:   tc.expiresAt,
			}

			result := creds.IsExpired()
			if result != tc.expect {
				t.Errorf("Expected IsExpired=%v, got %v", tc.expect, result)
			}
		})
	}
}

// TestCredentialsIsValid validates credential validity check
func TestCredentialsIsValid(t *testing.T) {
	testCases := []struct {
		accessToken string
		expiresAt   time.Time
		expect      bool
		name        string
	}{
		{"valid_token", time.Now().Add(1 * time.Hour), true, "valid credentials"},
		{"", time.Now().Add(1 * time.Hour), false, "empty access token"},
		{"valid_token", time.Now().Add(-1 * time.Hour), false, "expired token"},
		{"", time.Now().Add(-1 * time.Hour), false, "empty and expired"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			creds := &Credentials{
				AccessToken: tc.accessToken,
				ExpiresAt:   tc.expiresAt,
			}

			result := creds.IsValid()
			if result != tc.expect {
				t.Errorf("Expected IsValid=%v, got %v", tc.expect, result)
			}
		})
	}
}

// TestCredentialsStructure validates credentials struct has required fields
func TestCredentialsStructure(t *testing.T) {
	creds := &Credentials{
		AccessToken:  "access_123",
		RefreshToken: "refresh_123",
		ExpiresAt:    time.Now().Add(1 * time.Hour),
		UserID:       "user_id_123",
		Username:     "testuser",
		Email:        "test@example.com",
		IsAdmin:      false,
	}

	if creds.AccessToken != "access_123" {
		t.Error("AccessToken not set correctly")
	}

	if creds.RefreshToken != "refresh_123" {
		t.Error("RefreshToken not set correctly")
	}

	if creds.UserID != "user_id_123" {
		t.Error("UserID not set correctly")
	}

	if creds.Username != "testuser" {
		t.Error("Username not set correctly")
	}

	if creds.Email != "test@example.com" {
		t.Error("Email not set correctly")
	}

	if creds.IsAdmin != false {
		t.Error("IsAdmin not set correctly")
	}

	if creds.ExpiresAt.IsZero() {
		t.Error("ExpiresAt should be set")
	}
}

// TestCredentialsExpirationBoundary validates expiration at exact boundary
func TestCredentialsExpirationBoundary(t *testing.T) {
	// Just past expiration (should be expired)
	now := time.Now()
	creds := &Credentials{
		AccessToken: "token",
		ExpiresAt:   now.Add(-1 * time.Millisecond),
	}

	if !creds.IsExpired() {
		t.Error("Token just past expiration should be expired")
	}

	// Just before expiration (should not be expired)
	creds.ExpiresAt = now.Add(1 * time.Second)
	if creds.IsExpired() {
		t.Error("Token just before expiration should not be expired")
	}
}

// TestCredentialsValidityChain validates that IsValid depends on both token and expiration
func TestCredentialsValidityChain(t *testing.T) {
	// Valid credentials
	creds := &Credentials{
		AccessToken: "valid_token",
		ExpiresAt:   time.Now().Add(1 * time.Hour),
	}

	if !creds.IsValid() {
		t.Error("Valid credentials should pass IsValid check")
	}

	// Remove token
	creds.AccessToken = ""
	if creds.IsValid() {
		t.Error("Empty token should fail IsValid check")
	}

	// Restore token but expire it
	creds.AccessToken = "valid_token"
	creds.ExpiresAt = time.Now().Add(-1 * time.Hour)
	if creds.IsValid() {
		t.Error("Expired token should fail IsValid check")
	}
}

// TestCredentialsAdminField validates admin flag
func TestCredentialsAdminField(t *testing.T) {
	adminCreds := &Credentials{
		IsAdmin: true,
	}

	if !adminCreds.IsAdmin {
		t.Error("Admin flag should be set")
	}

	userCreds := &Credentials{
		IsAdmin: false,
	}

	if userCreds.IsAdmin {
		t.Error("Admin flag should not be set for regular user")
	}
}

// TestCredentialsZeroValues handles zero-valued credentials
func TestCredentialsZeroValues(t *testing.T) {
	creds := &Credentials{}

	if !creds.IsExpired() {
		t.Error("Zero-value credentials should be expired (ExpiresAt is zero)")
	}

	if creds.IsValid() {
		t.Error("Zero-value credentials should be invalid")
	}
}

// TestCredentialsExpirationTimezone validates expiration works across timezones
func TestCredentialsExpirationTimezone(t *testing.T) {
	// Create time with specific timezone
	futureTime := time.Now().Add(24 * time.Hour)
	creds := &Credentials{
		AccessToken: "token",
		ExpiresAt:   futureTime,
	}

	if creds.IsExpired() {
		t.Error("Future time should not be expired regardless of timezone")
	}

	pastTime := time.Now().Add(-24 * time.Hour)
	creds.ExpiresAt = pastTime
	if !creds.IsExpired() {
		t.Error("Past time should be expired regardless of timezone")
	}
}

// TestCredentialsFieldTypes validates that field types are correct
func TestCredentialsFieldTypes(t *testing.T) {
	creds := &Credentials{
		AccessToken:  "string_value",
		RefreshToken: "string_value",
		UserID:       "string_value",
		Username:     "string_value",
		Email:        "string_value",
		IsAdmin:      true,
		ExpiresAt:    time.Now(),
	}

	// Test string fields
	if _, ok := interface{}(creds.AccessToken).(string); !ok {
		t.Error("AccessToken should be string")
	}

	if _, ok := interface{}(creds.RefreshToken).(string); !ok {
		t.Error("RefreshToken should be string")
	}

	if _, ok := interface{}(creds.IsAdmin).(bool); !ok {
		t.Error("IsAdmin should be bool")
	}

	if _, ok := interface{}(creds.ExpiresAt).(time.Time); !ok {
		t.Error("ExpiresAt should be time.Time")
	}
}

// TestCredentialsImmutability validates credentials don't modify unexpectedly
func TestCredentialsImmutability(t *testing.T) {
	expiresAt := time.Now().Add(1 * time.Hour)
	creds := &Credentials{
		AccessToken: "token1",
		ExpiresAt:   expiresAt,
	}

	// Call IsExpired multiple times, should have same result
	firstResult := creds.IsExpired()
	secondResult := creds.IsExpired()

	if firstResult != secondResult {
		t.Error("IsExpired should return consistent results")
	}

	// Verify token wasn't modified
	if creds.AccessToken != "token1" {
		t.Error("AccessToken should not be modified by IsExpired")
	}
}

// TestCredentialsRefreshTokenHandling validates refresh token field
func TestCredentialsRefreshTokenHandling(t *testing.T) {
	testCases := []struct {
		refreshToken string
		name         string
	}{
		{"valid_refresh_token", "valid refresh token"},
		{"", "empty refresh token"},
		{"long_token_" + string(make([]byte, 100)), "long refresh token"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			creds := &Credentials{
				RefreshToken: tc.refreshToken,
			}

			if creds.RefreshToken != tc.refreshToken {
				t.Errorf("RefreshToken not set correctly")
			}
		})
	}
}

// TestCredentialsUserInfo validates user info fields
func TestCredentialsUserInfo(t *testing.T) {
	creds := &Credentials{
		UserID:   "user_123",
		Username: "john_doe",
		Email:    "john@example.com",
	}

	if creds.UserID != "user_123" {
		t.Error("UserID should be accessible")
	}

	if creds.Username != "john_doe" {
		t.Error("Username should be accessible")
	}

	if creds.Email != "john@example.com" {
		t.Error("Email should be accessible")
	}
}

// TestCredentialsExpiredValidation validates expiration in IsValid
func TestCredentialsExpiredValidation(t *testing.T) {
	creds := &Credentials{
		AccessToken: "token",
		ExpiresAt:   time.Now().Add(-1 * time.Second),
	}

	if creds.IsValid() {
		t.Error("Expired credentials should not be valid")
	}

	if !creds.IsExpired() {
		t.Error("Expired credentials should be marked as expired")
	}
}

// TestCredentialsExpirationEdgeCases handles edge cases
func TestCredentialsExpirationEdgeCases(t *testing.T) {
	// Time in far future
	creds := &Credentials{
		AccessToken: "token",
		ExpiresAt:   time.Now().AddDate(100, 0, 0),
	}

	if creds.IsExpired() {
		t.Error("Token expiring in 100 years should not be expired")
	}

	// Time in very distant past
	creds.ExpiresAt = time.Now().AddDate(-100, 0, 0)
	if !creds.IsExpired() {
		t.Error("Token expired 100 years ago should be expired")
	}
}
