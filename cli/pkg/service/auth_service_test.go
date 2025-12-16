package service

import (
	"testing"
)

func TestNewAuthService(t *testing.T) {
	service := NewAuthService()
	if service == nil {
		t.Error("NewAuthService returned nil")
	}
}

func TestAuthService_Login(t *testing.T) {
	service := NewAuthService()

	// Login requires interactive input
	err := service.Login()
	t.Logf("Login (requires input): %v", err)
}

func TestAuthService_Logout(t *testing.T) {
	service := NewAuthService()

	err := service.Logout()
	t.Logf("Logout: %v", err)
}

func TestAuthService_GetMe(t *testing.T) {
	service := NewAuthService()

	err := service.GetMe()
	t.Logf("GetMe: %v", err)
}

func TestAuthService_RefreshToken(t *testing.T) {
	service := NewAuthService()

	err := service.RefreshToken()
	t.Logf("RefreshToken: %v", err)
}

func TestAuthService_Enable2FA(t *testing.T) {
	service := NewAuthService()

	err := service.Enable2FA()
	t.Logf("Enable2FA: %v", err)
}

func TestAuthService_Disable2FA(t *testing.T) {
	service := NewAuthService()

	// Disable2FA requires interactive input
	err := service.Disable2FA()
	t.Logf("Disable2FA (requires input): %v", err)
}

func TestAuthService_GetBackupCodes(t *testing.T) {
	service := NewAuthService()

	err := service.GetBackupCodes()
	t.Logf("GetBackupCodes: %v", err)
}

func TestAuthService_Register(t *testing.T) {
	service := NewAuthService()

	// Register requires interactive input
	err := service.Register()
	t.Logf("Register (requires input): %v", err)
}

func TestAuthService_RequestPasswordReset(t *testing.T) {
	service := NewAuthService()

	// RequestPasswordReset requires interactive input
	err := service.RequestPasswordReset()
	t.Logf("RequestPasswordReset (requires input): %v", err)
}

func TestAuthService_ResetPassword(t *testing.T) {
	service := NewAuthService()

	// ResetPassword requires interactive input
	err := service.ResetPassword()
	t.Logf("ResetPassword (requires input): %v", err)
}

func TestAuthService_Get2FAStatus(t *testing.T) {
	service := NewAuthService()

	err := service.Get2FAStatus()
	t.Logf("Get2FAStatus: %v", err)
}
