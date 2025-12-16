package service

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/credentials"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
	"github.com/zfogg/sidechain/cli/pkg/logger"
	"github.com/zfogg/sidechain/cli/pkg/prompter"
)

type AuthService struct{}

// NewAuthService creates a new auth service
func NewAuthService() *AuthService {
	return &AuthService{}
}

// Login handles user login
func (s *AuthService) Login() error {
	// Check if already logged in
	creds, err := credentials.Load()
	if err != nil {
		logger.Error("Failed to load credentials", err)
		return err
	}

	if creds != nil && creds.IsValid() {
		formatter.PrintWarning("Already logged in as %s", creds.Username)
		confirm, err := prompter.PromptConfirm("Continue with new login?")
		if err != nil {
			return err
		}
		if !confirm {
			return nil
		}
	}

	// Prompt for email and password
	email, err := prompter.PromptString("Email: ")
	if err != nil {
		return err
	}

	if email == "" {
		return fmt.Errorf("email cannot be empty")
	}

	password, err := prompter.PromptPassword("Password: ")
	if err != nil {
		return err
	}

	if password == "" {
		return fmt.Errorf("password cannot be empty")
	}

	// Initialize HTTP client
	client.Init()

	// Call login API
	formatter.PrintInfo("Authenticating...")
	loginResp, err := api.Login(email, password)
	if err != nil {
		formatter.PrintError("Login failed: %v", err)
		return err
	}

	// Set auth token for subsequent requests
	client.SetAuthToken(loginResp.AccessToken)

	// Calculate expiry time
	expiresAt := time.Now().Add(time.Duration(loginResp.ExpiresIn) * time.Second)

	// Save credentials
	creds = &credentials.Credentials{
		AccessToken:  loginResp.AccessToken,
		RefreshToken: loginResp.RefreshToken,
		ExpiresAt:    expiresAt,
		UserID:       loginResp.User.ID,
		Username:     loginResp.User.Username,
		Email:        loginResp.User.Email,
		IsAdmin:      loginResp.User.IsAdmin,
	}

	if err := credentials.Save(creds); err != nil {
		formatter.PrintError("Failed to save credentials: %v", err)
		return err
	}

	// Display success message
	formatter.PrintSuccess("✓ Login successful!")
	if loginResp.User.IsAdmin {
		formatter.PrintInfo("Logged in as %s (ADMIN)", formatter.Bold.Sprint(loginResp.User.Username))
	} else {
		formatter.PrintInfo("Logged in as %s", formatter.Bold.Sprint(loginResp.User.Username))
	}
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Username":       loginResp.User.Username,
		"Email":          loginResp.User.Email,
		"Display Name":   loginResp.User.DisplayName,
		"Followers":      loginResp.User.FollowerCount,
		"Following":      loginResp.User.FollowingCount,
		"Posts":          loginResp.User.PostCount,
		"Email Verified": loginResp.User.EmailVerified,
	}
	if loginResp.User.IsAdmin {
		keyValues["Admin"] = "✓ YES"
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// Logout handles user logout
func (s *AuthService) Logout() error {
	// Check if logged in
	creds, err := credentials.Load()
	if err != nil {
		logger.Error("Failed to load credentials", err)
		return err
	}

	if creds == nil {
		formatter.PrintWarning("Not logged in")
		return nil
	}

	// Confirm logout
	confirm, err := prompter.PromptConfirm("Logout?")
	if err != nil {
		return err
	}

	if !confirm {
		return nil
	}

	// Delete credentials
	if err := credentials.Delete(); err != nil {
		formatter.PrintError("Failed to delete credentials: %v", err)
		return err
	}

	// Clear auth token
	client.ClearAuthToken()

	formatter.PrintSuccess("✓ Logged out successfully")
	return nil
}

// GetMe gets current user information
func (s *AuthService) GetMe() error {
	// Check if logged in
	creds, err := credentials.Load()
	if err != nil {
		logger.Error("Failed to load credentials", err)
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	// Initialize HTTP client with auth token
	client.Init()
	client.SetAuthToken(creds.AccessToken)

	// Refresh token if expired
	if creds.IsExpired() {
		if err := s.RefreshToken(); err != nil {
			formatter.PrintError("Failed to refresh token. Please login again.")
			return err
		}

		// Reload credentials
		creds, _ = credentials.Load()
		client.SetAuthToken(creds.AccessToken)
	}

	// Call API
	formatter.PrintInfo("Fetching user information...")
	user, err := api.GetCurrentUser()
	if err != nil {
		if api.IsUnauthorized(err) {
			formatter.PrintError("Session expired. Please login again.")
			credentials.Delete()
			return fmt.Errorf("unauthorized")
		}
		formatter.PrintError("Failed to fetch user: %v", err)
		return err
	}

	// Display user information
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Username":       user.Username,
		"Email":          user.Email,
		"Display Name":   user.DisplayName,
		"Bio":            user.Bio,
		"Location":       user.Location,
		"Followers":      user.FollowerCount,
		"Following":      user.FollowingCount,
		"Posts":          user.PostCount,
		"Email Verified": user.EmailVerified,
		"2FA Enabled":    user.TwoFactorEnabled,
		"Private":        user.IsPrivate,
		"Created":        user.CreatedAt.Format("2006-01-02 15:04:05"),
	}
	if user.IsAdmin {
		keyValues["Admin"] = "✓ YES"
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// RefreshToken refreshes the access token
func (s *AuthService) RefreshToken() error {
	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil {
		return fmt.Errorf("not logged in")
	}

	client.Init()

	logger.Debug("Refreshing token")
	refreshResp, err := api.Refresh(creds.RefreshToken)
	if err != nil {
		if api.IsUnauthorized(err) {
			credentials.Delete()
			return fmt.Errorf("refresh token expired, please login again")
		}
		return err
	}

	// Update credentials
	creds.AccessToken = refreshResp.AccessToken
	creds.ExpiresAt = time.Now().Add(time.Duration(refreshResp.ExpiresIn) * time.Second)

	if err := credentials.Save(creds); err != nil {
		return err
	}

	logger.Debug("Token refreshed successfully")
	return nil
}

// Enable2FA enables two-factor authentication
func (s *AuthService) Enable2FA() error {
	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	formatter.PrintInfo("Enabling two-factor authentication...")

	result, err := api.Enable2FA()
	if err != nil {
		formatter.PrintError("Failed to enable 2FA: %v", err)
		return err
	}

	formatter.PrintSuccess("✓ Two-factor authentication enabled")
	fmt.Printf("\n")

	// Display QR code and setup info if available
	if qrCode, ok := result["qr_code"].(string); ok {
		formatter.PrintInfo("QR Code:")
		fmt.Println(qrCode)
	}

	if secret, ok := result["secret"].(string); ok {
		formatter.PrintInfo("Secret: %s", secret)
		fmt.Println("Save this secret in a secure location.")
	}

	return nil
}

// Disable2FA disables two-factor authentication
func (s *AuthService) Disable2FA() error {
	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	// Prompt for 2FA code
	code, err := prompter.PromptString("Enter your 2FA code: ")
	if err != nil {
		return err
	}

	if code == "" {
		return fmt.Errorf("2FA code cannot be empty")
	}

	formatter.PrintInfo("Disabling two-factor authentication...")

	if err := api.Disable2FA(code); err != nil {
		formatter.PrintError("Failed to disable 2FA: %v", err)
		return err
	}

	formatter.PrintSuccess("✓ Two-factor authentication disabled")
	return nil
}

// GetBackupCodes gets backup codes for 2FA
func (s *AuthService) GetBackupCodes() error {
	creds, err := credentials.Load()
	if err != nil {
		return err
	}

	if creds == nil || !creds.IsValid() {
		formatter.PrintError("Not logged in. Please run 'sidechain-cli auth login'")
		return fmt.Errorf("not authenticated")
	}

	client.Init()
	client.SetAuthToken(creds.AccessToken)

	formatter.PrintInfo("Regenerating backup codes...")

	codes, err := api.GetBackupCodes()
	if err != nil {
		formatter.PrintError("Failed to get backup codes: %v", err)
		return err
	}

	formatter.PrintSuccess("✓ Backup codes regenerated")
	fmt.Printf("\n")
	formatter.PrintWarning("Save these codes in a secure location. You'll need one if you lose access to your 2FA device.")
	fmt.Printf("\n")

	for i, code := range codes {
		fmt.Printf("%d. %s\n", i+1, code)
	}

	return nil
}
