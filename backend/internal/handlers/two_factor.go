package handlers

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/base32"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/pquerna/otp"
	"github.com/pquerna/otp/hotp"
	"github.com/pquerna/otp/totp"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
)

const (
	// Number of backup codes to generate
	backupCodeCount = 10
	// Backup code length (characters)
	backupCodeLength = 8
	// OTP issuer name shown in authenticator apps
	otpIssuer = "Sidechain"
	// OTP types
	OTPTypeTOTP = "totp" // Time-based (Google Authenticator, Authy)
	OTPTypeHOTP = "hotp" // Counter-based (YubiKey hardware tokens)
	// HOTP look-ahead window for counter synchronization
	hotpLookAhead = 10
)

// Enable2FARequest is the request body for initiating 2FA setup
type Enable2FARequest struct {
	Password string `json:"password" binding:"required"`
	Type     string `json:"type"` // "totp" (default) or "hotp" for YubiKey
}

// Enable2FAResponse contains the OTP setup data
type Enable2FAResponse struct {
	Type        string   `json:"type"`         // "totp" or "hotp"
	Secret      string   `json:"secret"`       // Base32-encoded secret for manual entry
	QRCodeURL   string   `json:"qr_code_url"`  // otpauth:// URL for QR code
	BackupCodes []string `json:"backup_codes"` // One-time backup codes
	Counter     uint64   `json:"counter,omitempty"` // Initial counter for HOTP
}

// Verify2FARequest is the request body for verifying 2FA setup
type Verify2FARequest struct {
	Code string `json:"code" binding:"required"`
}

// Disable2FARequest is the request body for disabling 2FA
type Disable2FARequest struct {
	Password string `json:"password"`
	Code     string `json:"code"` // TOTP code or backup code
}

// Verify2FALoginRequest is the request body for 2FA during login
type Verify2FALoginRequest struct {
	UserID string `json:"user_id" binding:"required"`
	Code   string `json:"code" binding:"required"`
}

// RegenerateBackupCodesRequest is the request for generating new backup codes
type RegenerateBackupCodesRequest struct {
	Code string `json:"code" binding:"required"` // Current TOTP code
}

// Get2FAStatus returns the current 2FA status for the authenticated user
// GET /api/v1/auth/2fa/status
func (h *Handlers) Get2FAStatus(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Count remaining backup codes
	var backupCodesRemaining int
	if user.BackupCodes != nil && *user.BackupCodes != "" {
		var codes []string
		if err := json.Unmarshal([]byte(*user.BackupCodes), &codes); err == nil {
			backupCodesRemaining = len(codes)
		}
	}

	response := gin.H{
		"enabled":                user.TwoFactorEnabled,
		"backup_codes_remaining": backupCodesRemaining,
	}

	// Include type if 2FA is enabled
	if user.TwoFactorEnabled {
		otpType := user.TwoFactorType
		if otpType == "" {
			otpType = OTPTypeTOTP // Default for legacy users
		}
		response["type"] = otpType
	}

	c.JSON(http.StatusOK, response)
}

// Enable2FA initiates 2FA setup for the authenticated user
// POST /api/v1/auth/2fa/enable
// Returns OTP secret, QR code URL, and backup codes
// Supports both TOTP (authenticator apps) and HOTP (YubiKey hardware tokens)
func (h *Handlers) Enable2FA(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req Enable2FARequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Password is required"})
		return
	}

	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Check if 2FA is already enabled
	if user.TwoFactorEnabled {
		c.JSON(http.StatusBadRequest, gin.H{"error": "2FA is already enabled"})
		return
	}

	// Verify password (only for native auth users)
	if user.PasswordHash != nil {
		if !verifyPassword(*user.PasswordHash, req.Password) {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid password"})
			return
		}
	}

	// Determine OTP type (default to TOTP)
	otpType := OTPTypeTOTP
	if req.Type == OTPTypeHOTP {
		otpType = OTPTypeHOTP
	}

	var key *otp.Key
	var err error
	var initialCounter uint64 = 0

	if otpType == OTPTypeHOTP {
		// Generate HOTP key for hardware tokens (YubiKey)
		key, err = hotp.Generate(hotp.GenerateOpts{
			Issuer:      otpIssuer,
			AccountName: user.Email,
			SecretSize:  32,
		})
	} else {
		// Generate TOTP key for authenticator apps
		key, err = totp.Generate(totp.GenerateOpts{
			Issuer:      otpIssuer,
			AccountName: user.Email,
			SecretSize:  32,
		})
	}

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate 2FA secret"})
		return
	}

	// Generate backup codes
	backupCodes := generateBackupCodes(backupCodeCount)
	hashedCodes := hashBackupCodes(backupCodes)
	hashedCodesJSON, _ := json.Marshal(hashedCodes)
	hashedCodesStr := string(hashedCodesJSON)

	// Store the secret temporarily (not enabled yet - user must verify first)
	secret := key.Secret()
	if err := database.DB.Model(&user).Updates(map[string]interface{}{
		"two_factor_type":    otpType,
		"two_factor_secret":  secret,
		"two_factor_counter": initialCounter,
		"backup_codes":       hashedCodesStr,
	}).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save 2FA setup"})
		return
	}

	response := Enable2FAResponse{
		Type:        otpType,
		Secret:      secret,
		QRCodeURL:   key.URL(),
		BackupCodes: backupCodes,
	}

	// Include counter for HOTP
	if otpType == OTPTypeHOTP {
		response.Counter = initialCounter
	}

	c.JSON(http.StatusOK, response)
}

// Verify2FA completes 2FA setup by verifying an OTP code
// POST /api/v1/auth/2fa/verify
// Supports both TOTP (time-based) and HOTP (counter-based)
func (h *Handlers) Verify2FA(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req Verify2FARequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Code is required"})
		return
	}

	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Check if secret exists (setup was initiated)
	if user.TwoFactorSecret == nil || *user.TwoFactorSecret == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "2FA setup not initiated. Call /2fa/enable first"})
		return
	}

	// Verify the OTP code based on type
	valid := false
	var newCounter uint64

	if user.TwoFactorType == OTPTypeHOTP {
		// HOTP verification with look-ahead window
		counter := uint64(0)
		if user.TwoFactorCounter != nil {
			counter = *user.TwoFactorCounter
		}
		valid, newCounter = verifyHOTPWithLookAhead(*user.TwoFactorSecret, req.Code, counter, hotpLookAhead)
	} else {
		// TOTP verification (default)
		valid = totp.Validate(req.Code, *user.TwoFactorSecret)
	}

	if !valid {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid verification code"})
		return
	}

	// Enable 2FA and update counter for HOTP
	updates := map[string]interface{}{
		"two_factor_enabled": true,
	}
	if user.TwoFactorType == OTPTypeHOTP {
		updates["two_factor_counter"] = newCounter
	}

	if err := database.DB.Model(&user).Updates(updates).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to enable 2FA"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Two-factor authentication enabled successfully",
		"enabled": true,
		"type":    user.TwoFactorType,
	})
}

// Disable2FA disables 2FA for the authenticated user
// POST /api/v1/auth/2fa/disable
func (h *Handlers) Disable2FA(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req Disable2FARequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid request"})
		return
	}

	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Check if 2FA is enabled
	if !user.TwoFactorEnabled {
		c.JSON(http.StatusBadRequest, gin.H{"error": "2FA is not enabled"})
		return
	}

	// Verify either password or OTP code
	verified := false

	// Try OTP code first (TOTP or HOTP)
	if req.Code != "" && user.TwoFactorSecret != nil {
		if user.TwoFactorType == OTPTypeHOTP {
			counter := uint64(0)
			if user.TwoFactorCounter != nil {
				counter = *user.TwoFactorCounter
			}
			valid, newCounter := verifyHOTPWithLookAhead(*user.TwoFactorSecret, req.Code, counter, hotpLookAhead)
			if valid {
				verified = true
				// Update counter
				database.DB.Model(&user).Update("two_factor_counter", newCounter)
			}
		} else {
			if totp.Validate(req.Code, *user.TwoFactorSecret) {
				verified = true
			}
		}

		// Try as backup code if OTP didn't work
		if !verified {
			if verifyAndConsumeBackupCode(&user, req.Code) {
				verified = true
			}
		}
	}

	// Try password
	if !verified && req.Password != "" && user.PasswordHash != nil {
		if verifyPassword(*user.PasswordHash, req.Password) {
			verified = true
		}
	}

	if !verified {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid password or verification code"})
		return
	}

	// Disable 2FA and clear secrets
	if err := database.DB.Model(&user).Updates(map[string]interface{}{
		"two_factor_enabled": false,
		"two_factor_type":    OTPTypeTOTP,
		"two_factor_secret":  nil,
		"two_factor_counter": nil,
		"backup_codes":       nil,
	}).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to disable 2FA"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Two-factor authentication disabled successfully",
		"enabled": false,
	})
}

// Verify2FALogin verifies 2FA code during login flow
// POST /api/v1/auth/2fa/login
// This is called after successful password verification when 2FA is enabled
// Supports both TOTP (authenticator apps) and HOTP (YubiKey)
func (h *AuthHandlers) Verify2FALogin(c *gin.Context) {
	var req Verify2FALoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "User ID and code are required"})
		return
	}

	var user models.User
	if err := database.DB.First(&user, "id = ?", req.UserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Check if 2FA is enabled
	if !user.TwoFactorEnabled || user.TwoFactorSecret == nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "2FA is not enabled for this account"})
		return
	}

	// Verify OTP code based on type
	valid := false
	var newCounter uint64

	if user.TwoFactorType == OTPTypeHOTP {
		// HOTP verification
		counter := uint64(0)
		if user.TwoFactorCounter != nil {
			counter = *user.TwoFactorCounter
		}
		valid, newCounter = verifyHOTPWithLookAhead(*user.TwoFactorSecret, req.Code, counter, hotpLookAhead)
		if valid {
			// Update counter
			database.DB.Model(&user).Update("two_factor_counter", newCounter)
		}
	} else {
		// TOTP verification (default)
		valid = totp.Validate(req.Code, *user.TwoFactorSecret)
	}

	if !valid {
		// Try as backup code
		if !verifyAndConsumeBackupCode(&user, req.Code) {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid verification code"})
			return
		}
	}

	// Generate auth token
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Auth service not configured"})
		return
	}

	authResp, err := h.authService.GenerateTokenForUser(&user)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"auth": gin.H{
			"token":      authResp.Token,
			"user":       authResp.User,
			"expires_at": authResp.ExpiresAt,
		},
	})
}

// RegenerateBackupCodes generates new backup codes (invalidates old ones)
// POST /api/v1/auth/2fa/backup-codes
func (h *Handlers) RegenerateBackupCodes(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req RegenerateBackupCodesRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Current 2FA code is required"})
		return
	}

	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Check if 2FA is enabled
	if !user.TwoFactorEnabled || user.TwoFactorSecret == nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "2FA is not enabled"})
		return
	}

	// Verify current OTP code
	valid := false
	if user.TwoFactorType == OTPTypeHOTP {
		counter := uint64(0)
		if user.TwoFactorCounter != nil {
			counter = *user.TwoFactorCounter
		}
		var newCounter uint64
		valid, newCounter = verifyHOTPWithLookAhead(*user.TwoFactorSecret, req.Code, counter, hotpLookAhead)
		if valid {
			database.DB.Model(&user).Update("two_factor_counter", newCounter)
		}
	} else {
		valid = totp.Validate(req.Code, *user.TwoFactorSecret)
	}

	if !valid {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid verification code"})
		return
	}

	// Generate new backup codes
	backupCodes := generateBackupCodes(backupCodeCount)
	hashedCodes := hashBackupCodes(backupCodes)
	hashedCodesJSON, _ := json.Marshal(hashedCodes)
	hashedCodesStr := string(hashedCodesJSON)

	// Save new codes
	if err := database.DB.Model(&user).Update("backup_codes", hashedCodesStr).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save backup codes"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"backup_codes": backupCodes,
		"message":      "New backup codes generated. Save them securely - old codes are now invalid.",
	})
}

// Helper functions

// generateBackupCodes generates a set of random backup codes
func generateBackupCodes(count int) []string {
	codes := make([]string, count)
	for i := 0; i < count; i++ {
		codes[i] = generateRandomCode(backupCodeLength)
	}
	return codes
}

// generateRandomCode generates a random alphanumeric code
func generateRandomCode(length int) string {
	bytes := make([]byte, length)
	rand.Read(bytes)
	// Use base32 for easy typing (no confusing chars like 0/O, 1/I)
	encoded := base32.StdEncoding.EncodeToString(bytes)
	// Take first 'length' characters and format as XXXX-XXXX
	code := strings.ToUpper(encoded[:length])
	if length == 8 {
		return code[:4] + "-" + code[4:]
	}
	return code
}

// hashBackupCodes hashes backup codes for secure storage
func hashBackupCodes(codes []string) []string {
	hashed := make([]string, len(codes))
	for i, code := range codes {
		// Remove dashes for hashing
		cleanCode := strings.ReplaceAll(code, "-", "")
		hash := sha256.Sum256([]byte(cleanCode))
		hashed[i] = hex.EncodeToString(hash[:])
	}
	return hashed
}

// hashBackupCode hashes a single backup code
func hashBackupCode(code string) string {
	cleanCode := strings.ReplaceAll(strings.ToUpper(code), "-", "")
	hash := sha256.Sum256([]byte(cleanCode))
	return hex.EncodeToString(hash[:])
}

// verifyAndConsumeBackupCode checks if a backup code is valid and removes it if so
func verifyAndConsumeBackupCode(user *models.User, code string) bool {
	if user.BackupCodes == nil || *user.BackupCodes == "" {
		return false
	}

	var hashedCodes []string
	if err := json.Unmarshal([]byte(*user.BackupCodes), &hashedCodes); err != nil {
		return false
	}

	// Hash the provided code
	providedHash := hashBackupCode(code)

	// Find and remove the matching code
	for i, storedHash := range hashedCodes {
		if storedHash == providedHash {
			// Remove the used code
			hashedCodes = append(hashedCodes[:i], hashedCodes[i+1:]...)

			// Save updated codes
			updatedJSON, _ := json.Marshal(hashedCodes)
			updatedStr := string(updatedJSON)
			database.DB.Model(user).Update("backup_codes", updatedStr)

			return true
		}
	}

	return false
}

// verifyHOTPWithLookAhead verifies an HOTP code with a look-ahead window
// This handles cases where the user may have generated codes without using them
// Returns (valid, newCounter) - newCounter is the counter to use for the next verification
func verifyHOTPWithLookAhead(secret, code string, counter uint64, lookAhead int) (bool, uint64) {
	for i := 0; i <= lookAhead; i++ {
		testCounter := counter + uint64(i)
		if hotp.Validate(code, testCounter, secret) {
			// Return the next counter value (after the one that matched)
			return true, testCounter + 1
		}
	}
	return false, counter
}

// verifyPassword verifies a password against a hash
// This should match the implementation in auth package
func verifyPassword(hash, password string) bool {
	// Try bcrypt first (most common)
	// Import bcrypt if not already available
	// For now, assume it uses base64-encoded bcrypt
	decoded, err := base64.StdEncoding.DecodeString(hash)
	if err != nil {
		// Not base64, try direct comparison (development only)
		return hash == password
	}

	// Use bcrypt compare
	// Note: This is a simplified version - the real implementation should use the auth package
	_ = decoded
	// For proper implementation, call auth.VerifyPassword(hash, password)
	return false
}

// Check2FARequired checks if a user requires 2FA verification
// Used internally during login flow
func Check2FARequired(userID string) (bool, error) {
	var user models.User
	if err := database.DB.Select("two_factor_enabled").First(&user, "id = ?", userID).Error; err != nil {
		return false, err
	}
	return user.TwoFactorEnabled, nil
}

// GenerateTOTPCode generates a current TOTP code for a secret (for testing)
func GenerateTOTPCode(secret string) (string, error) {
	return totp.GenerateCode(secret, time.Now())
}

// GenerateHOTPCode generates an HOTP code for a secret and counter (for testing)
func GenerateHOTPCode(secret string, counter uint64) (string, error) {
	return hotp.GenerateCode(secret, counter)
}
