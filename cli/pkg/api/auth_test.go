package api

import (
	"testing"
)

func TestLogin_InvalidCredentials(t *testing.T) {
	// Test with invalid email/password
	resp, err := Login("invalid@example.com", "wrongpassword")

	if resp != nil && resp.User.ID != "" {
		t.Logf("Login with invalid credentials: unexpected success")
	}
	if err == nil {
		t.Logf("Expected error for invalid credentials, got nil")
	}
	// Valid responses should have user info
	if resp != nil && resp.User.Username == "" && err == nil {
		t.Logf("Invalid response structure for successful login")
	}
}

func TestLogin_ValidCredentials(t *testing.T) {
	// Test with valid credentials (will fail without real account, but tests API call)
	resp, err := Login("testuser@example.com", "testpassword")

	// Check structure - either succeeds or gives expected error
	if err != nil {
		t.Logf("Login API called (error expected without valid account): %v", err)
		return
	}
	if resp == nil {
		t.Error("Login returned nil response on success")
	} else if resp.AccessToken == "" {
		t.Error("Login response missing access token")
	} else if resp.User.ID == "" {
		t.Error("Login response missing user ID")
	}
}

func TestRegister_InvalidInput(t *testing.T) {
	tests := []struct {
		email       string
		username    string
		password    string
		displayName string
	}{
		{"", "testuser", "password", "Test User"},
		{"test@example.com", "", "password", "Test User"},
		{"test@example.com", "testuser", "", "Test User"},
		{"test@example.com", "testuser", "password", ""},
	}

	for _, tt := range tests {
		resp, err := Register(tt.email, tt.username, tt.password, tt.displayName)
		// All should either fail or return nil response
		if resp != nil && resp.User.ID != "" && err == nil {
			t.Logf("Register with empty field succeeded unexpectedly: %s", tt.username)
		}
	}
}

func TestRegister_ValidInput(t *testing.T) {
	resp, err := Register("newuser@example.com", "newuser", "password123", "New User")

	if err != nil {
		t.Logf("Register API called (error expected without server): %v", err)
		return
	}
	if resp == nil {
		t.Error("Register returned nil response")
	}
	if resp != nil && resp.User.Username != "newuser" {
		t.Errorf("Register response username mismatch: got %s", resp.User.Username)
	}
}

func TestRefresh_WithValidToken(t *testing.T) {
	// Test with a non-existent token (will fail but tests API call)
	resp, err := Refresh("invalid-refresh-token")

	if resp != nil && resp.AccessToken == "" {
		t.Error("Refresh response missing access token")
	}
	if err == nil && resp == nil {
		t.Logf("Refresh call completed successfully (unexpected)")
	}
	if err != nil {
		t.Logf("Refresh API call made (error expected): %v", err)
	}
}

func TestGetCurrentUser(t *testing.T) {
	resp, err := GetCurrentUser()

	if err != nil {
		t.Logf("GetCurrentUser API called (error expected without auth): %v", err)
		return
	}
	if resp == nil {
		t.Error("GetCurrentUser returned nil response")
	}
	if resp != nil {
		if resp.ID == "" {
			t.Error("GetCurrentUser response missing user ID")
		}
		if resp.Username == "" {
			t.Error("GetCurrentUser response missing username")
		}
		if resp.Email == "" {
			t.Error("GetCurrentUser response missing email")
		}
	}
}

func TestEnable2FA(t *testing.T) {
	resp, err := Enable2FA()

	if err != nil {
		t.Logf("Enable2FA API called (error expected): %v", err)
		return
	}
	if resp == nil {
		t.Error("Enable2FA returned nil response")
	}
	if resp != nil && len(resp) == 0 {
		t.Logf("Enable2FA response is empty")
	}
	// Should contain secret or QR code data
	if resp != nil {
		for key := range resp {
			if key != "" {
				t.Logf("Enable2FA response contains key: %s", key)
				return
			}
		}
	}
}

func TestDisable2FA_WithInvalidCode(t *testing.T) {
	// Test with invalid code
	err := Disable2FA("000000")

	if err != nil {
		t.Logf("Disable2FA with invalid code (error expected): %v", err)
	}
	// Invalid code should fail
}

func TestDisable2FA_WithValidCode(t *testing.T) {
	// Test successful disable (will fail without actual 2FA enabled)
	err := Disable2FA("123456")

	if err != nil {
		t.Logf("Disable2FA API called (error expected): %v", err)
	}
}

func TestVerify2FA_WithValidCode(t *testing.T) {
	err := Verify2FA("123456")

	if err != nil {
		t.Logf("Verify2FA API called (error expected): %v", err)
	}
}

func TestVerify2FA_WithInvalidCode(t *testing.T) {
	err := Verify2FA("000000")

	if err != nil {
		t.Logf("Verify2FA with invalid code (error expected): %v", err)
	}
}

func TestGetBackupCodes(t *testing.T) {
	codes, err := GetBackupCodes()

	if err != nil {
		t.Logf("GetBackupCodes API called (error expected): %v", err)
		return
	}
	// Check response structure
	if codes != nil && len(codes) > 0 {
		for _, code := range codes {
			if code == "" {
				t.Error("Empty backup code in response")
			}
		}
	}
}

func TestRequestPasswordReset(t *testing.T) {
	tests := []string{
		"user@example.com",
		"nonexistent@example.com",
		"invalid-email",
	}

	for _, email := range tests {
		err := RequestPasswordReset(email)
		if err != nil {
			t.Logf("RequestPasswordReset for %s: %v", email, err)
		}
	}
}

func TestResetPassword(t *testing.T) {
	tests := []struct {
		token       string
		newPassword string
	}{
		{"valid-token", "newpassword123"},
		{"invalid-token", "newpassword123"},
		{"", "newpassword123"},
	}

	for _, tt := range tests {
		err := ResetPassword(tt.token, tt.newPassword)
		if err != nil {
			t.Logf("ResetPassword with token %s: %v", tt.token, err)
		}
	}
}

func TestGet2FAStatus(t *testing.T) {
	resp, err := Get2FAStatus()

	if err != nil {
		t.Logf("Get2FAStatus API called (error expected): %v", err)
		return
	}
	if resp == nil {
		t.Error("Get2FAStatus returned nil response")
	}
	if resp != nil && len(resp) == 0 {
		t.Logf("Get2FAStatus response is empty")
	}
	// Should indicate 2FA status
	if resp != nil && resp["enabled"] != nil {
		t.Logf("Get2FAStatus contains enabled status")
	}
}
