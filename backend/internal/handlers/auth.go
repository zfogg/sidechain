package handlers

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/auth"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/email"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/storage"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

// Note: Password reset endpoints are implemented (RequestPasswordReset, ResetPassword)
// Note: Password reset email sending is now implemented using AWS SES
// Configure SES_FROM_EMAIL, SES_FROM_NAME, and BASE_URL in .env to enable
// Note: "Forgot Password" is now fully wired up in plugin (Auth.cpp) - calls backend API

// OAuthSession stores OAuth authentication data for polling
type OAuthSession struct {
	AuthResponse *auth.AuthResponse
	CreatedAt    time.Time
	ExpiresAt    time.Time
}

// AuthHandlers wraps the auth service and provides HTTP handlers
type AuthHandlers struct {
	authService  *auth.Service
	uploader     storage.ProfilePictureUploader
	stream       *stream.Client
	emailService *email.EmailService
	jwtSecret    []byte
	// OAuth session storage for polling (in-memory, thread-safe)
	oauthSessions map[string]*OAuthSession
	oauthMutex    sync.RWMutex
}

// NewAuthHandlers creates a new auth handlers instance
func NewAuthHandlers(authService *auth.Service, uploader storage.ProfilePictureUploader, streamClient *stream.Client) *AuthHandlers {
	var jwtSecret []byte
	if authService != nil {
		// Extract JWT secret from auth service if possible
		// For now, we'll need to pass it separately or extract from env
		jwtSecret = []byte("default-secret") // This should come from config
	}
	ah := &AuthHandlers{
		authService:   authService,
		uploader:      uploader,
		stream:        streamClient,
		jwtSecret:     jwtSecret,
		oauthSessions: make(map[string]*OAuthSession),
	}

	// Start cleanup goroutine to remove expired sessions
	go ah.cleanupExpiredSessions()

	return ah
}

// cleanupExpiredSessions periodically removes expired OAuth sessions
func (h *AuthHandlers) cleanupExpiredSessions() {
	ticker := time.NewTicker(1 * time.Minute) // Run every minute
	defer ticker.Stop()

	for range ticker.C {
		now := time.Now()
		h.oauthMutex.Lock()
		for sessionID, session := range h.oauthSessions {
			if now.After(session.ExpiresAt) {
				delete(h.oauthSessions, sessionID)
			}
		}
		h.oauthMutex.Unlock()
	}
}

// SetJWTSecret sets the JWT secret for token validation
func (h *AuthHandlers) SetJWTSecret(secret []byte) {
	h.jwtSecret = secret
}

// SetEmailService sets the email service for sending emails
func (h *AuthHandlers) SetEmailService(service *email.EmailService) {
	h.emailService = service
}

// Register handles user registration
// POST /api/v1/auth/register
func (h *AuthHandlers) Register(c *gin.Context) {
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
		return
	}

	var req auth.RegisterRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	authResp, err := h.authService.RegisterNativeUser(req)
	if err != nil {
		if err == auth.ErrUserExists {
			c.JSON(http.StatusConflict, gin.H{"error": "user_exists"})
			return
		}
		if err == auth.ErrUsernameExists {
			c.JSON(http.StatusConflict, gin.H{"error": "username_taken"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "registration_failed", "message": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"auth": gin.H{
			"token":      authResp.Token,
			"user":       authResp.User,
			"expires_at": authResp.ExpiresAt,
		},
	})
}

// Login handles user login
// POST /api/v1/auth/login
// If 2FA is enabled, returns requires_2fa: true instead of token
func (h *AuthHandlers) Login(c *gin.Context) {
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
		return
	}

	var req auth.LoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	authResp, err := h.authService.LoginNativeUser(req)
	if err != nil {
		if err == auth.ErrUserNotFound || err == auth.ErrInvalidCredentials {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid_credentials"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "login_failed", "message": err.Error()})
		return
	}

	// Check if 2FA is enabled for this user
	if authResp.User.TwoFactorEnabled {
		// Return 2FA required response instead of token
		c.JSON(http.StatusOK, gin.H{
			"requires_2fa": true,
			"user_id":      authResp.User.ID,
			"two_factor_type": authResp.User.TwoFactorType,
		})
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

// GoogleOAuth initiates Google OAuth flow
// GET /api/v1/auth/google?session_id=...&state=...
func (h *AuthHandlers) GoogleOAuth(c *gin.Context) {
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
		return
	}

	// Use session_id as state if provided, otherwise generate new state
	state := c.Query("session_id")
	if state == "" {
		state = c.Query("state")
		if state == "" {
			state = uuid.New().String()
		}
	}

	url := h.authService.GetGoogleOAuthURL(state)
	c.Redirect(http.StatusTemporaryRedirect, url)
}

// GoogleCallback handles Google OAuth callback
// GET /api/v1/auth/google/callback?code=...&state=...
func (h *AuthHandlers) GoogleCallback(c *gin.Context) {
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
		return
	}

	code := c.Query("code")
	if code == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "missing_code"})
		return
	}

	authResp, err := h.authService.HandleGoogleCallback(code)
	if err != nil {
		// Show error page to browser
		errorHTML := fmt.Sprintf(`
<!DOCTYPE html>
<html>
<head>
	<title>Authentication Failed</title>
	<style>
		body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
		.container { text-align: center; background: white; padding: 40px; border-radius: 8px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); }
		h1 { color: #d32f2f; margin: 0 0 10px 0; }
		p { color: #666; margin: 5px 0; }
		.error { color: #d32f2f; background: #ffebee; padding: 10px; border-radius: 4px; margin: 20px 0; }
	</style>
</head>
<body>
	<div class="container">
		<h1>Authentication Failed</h1>
		<p>There was an error logging in with Google.</p>
		<div class="error">%s</div>
		<p>Please try again or use a different login method.</p>
	</div>
</body>
</html>`, err.Error())
		c.Header("Content-Type", "text/html; charset=utf-8")
		c.String(http.StatusInternalServerError, errorHTML)
		return
	}

	// Extract session_id from state parameter (OAuth providers return state in callback)
	sessionID := c.Query("state")
	if sessionID != "" {
		// Store session for polling (expires in 5 minutes)
		h.oauthMutex.Lock()
		h.oauthSessions[sessionID] = &OAuthSession{
			AuthResponse: authResp,
			CreatedAt:    time.Now(),
			ExpiresAt:    time.Now().Add(5 * time.Minute),
		}
		h.oauthMutex.Unlock()

		// Show success page to browser (plugin will poll for auth)
		successHTML := `
<!DOCTYPE html>
<html>
<head>
	<title>Authentication Successful</title>
	<style>
		body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
		.container { text-align: center; background: white; padding: 40px; border-radius: 8px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); max-width: 400px; }
		h1 { color: #2e7d32; margin: 0 0 10px 0; }
		p { color: #666; margin: 5px 0; }
		.checkmark { font-size: 48px; margin: 20px 0; }
	</style>
</head>
<body>
	<div class="container">
		<div class="checkmark">✓</div>
		<h1>You're logged in!</h1>
		<p>Return to the Sidechain plugin to continue.</p>
		<p style="font-size: 12px; color: #999; margin-top: 20px;">You can close this window.</p>
	</div>
</body>
</html>`
		c.Header("Content-Type", "text/html; charset=utf-8")
		c.String(http.StatusOK, successHTML)
		return
	}

	// Fallback: Try to redirect to plugin callback URL (for backwards compatibility)
	// This will fail gracefully and browser will show blank page
	redirectURL := fmt.Sprintf("sidechain://auth/callback?token=%s&user_id=%s", authResp.Token, authResp.User.ID)
	c.Redirect(http.StatusTemporaryRedirect, redirectURL)
}

// DiscordOAuth initiates Discord OAuth flow
// GET /api/v1/auth/discord?session_id=...&state=...
func (h *AuthHandlers) DiscordOAuth(c *gin.Context) {
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
		return
	}

	// Use session_id as state if provided, otherwise generate new state
	state := c.Query("session_id")
	if state == "" {
		state = c.Query("state")
		if state == "" {
			state = uuid.New().String()
		}
	}

	url := h.authService.GetDiscordOAuthURL(state)
	c.Redirect(http.StatusTemporaryRedirect, url)
}

// DiscordCallback handles Discord OAuth callback
// GET /api/v1/auth/discord/callback?code=...&state=...
func (h *AuthHandlers) DiscordCallback(c *gin.Context) {
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
		return
	}

	code := c.Query("code")
	if code == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "missing_code"})
		return
	}

	authResp, err := h.authService.HandleDiscordCallback(code)
	if err != nil {
		// Show error page to browser
		errorHTML := fmt.Sprintf(`
<!DOCTYPE html>
<html>
<head>
	<title>Authentication Failed</title>
	<style>
		body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
		.container { text-align: center; background: white; padding: 40px; border-radius: 8px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); }
		h1 { color: #d32f2f; margin: 0 0 10px 0; }
		p { color: #666; margin: 5px 0; }
		.error { color: #d32f2f; background: #ffebee; padding: 10px; border-radius: 4px; margin: 20px 0; }
	</style>
</head>
<body>
	<div class="container">
		<h1>Authentication Failed</h1>
		<p>There was an error logging in with Discord.</p>
		<div class="error">%s</div>
		<p>Please try again or use a different login method.</p>
	</div>
</body>
</html>`, err.Error())
		c.Header("Content-Type", "text/html; charset=utf-8")
		c.String(http.StatusInternalServerError, errorHTML)
		return
	}

	// Extract session_id from state parameter (OAuth providers return state in callback)
	sessionID := c.Query("state")
	if sessionID != "" {
		// Store session for polling (expires in 5 minutes)
		h.oauthMutex.Lock()
		h.oauthSessions[sessionID] = &OAuthSession{
			AuthResponse: authResp,
			CreatedAt:    time.Now(),
			ExpiresAt:    time.Now().Add(5 * time.Minute),
		}
		h.oauthMutex.Unlock()

		// Show success page to browser (plugin will poll for auth)
		successHTML := `
<!DOCTYPE html>
<html>
<head>
	<title>Authentication Successful</title>
	<style>
		body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
		.container { text-align: center; background: white; padding: 40px; border-radius: 8px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); max-width: 400px; }
		h1 { color: #2e7d32; margin: 0 0 10px 0; }
		p { color: #666; margin: 5px 0; }
		.checkmark { font-size: 48px; margin: 20px 0; }
	</style>
</head>
<body>
	<div class="container">
		<div class="checkmark">✓</div>
		<h1>You're logged in!</h1>
		<p>Return to the Sidechain plugin to continue.</p>
		<p style="font-size: 12px; color: #999; margin-top: 20px;">You can close this window.</p>
	</div>
</body>
</html>`
		c.Header("Content-Type", "text/html; charset=utf-8")
		c.String(http.StatusOK, successHTML)
		return
	}

	// Fallback: Try to redirect to plugin callback URL (for backwards compatibility)
	// This will fail gracefully and browser will show blank page
	redirectURL := fmt.Sprintf("sidechain://auth/callback?token=%s&user_id=%s", authResp.Token, authResp.User.ID)
	c.Redirect(http.StatusTemporaryRedirect, redirectURL)
}

// OAuthPoll polls for OAuth completion (for plugin flow)
// GET /api/v1/auth/oauth/poll?session_id=...
func (h *AuthHandlers) OAuthPoll(c *gin.Context) {
	sessionID := c.Query("session_id")
	if sessionID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "missing_session_id"})
		return
	}

	h.oauthMutex.RLock()
	session, exists := h.oauthSessions[sessionID]
	h.oauthMutex.RUnlock()

	if !exists {
		c.JSON(http.StatusOK, gin.H{
			"status": "pending",
		})
		return
	}

	// Check if session expired
	if time.Now().After(session.ExpiresAt) {
		// Clean up expired session
		h.oauthMutex.Lock()
		delete(h.oauthSessions, sessionID)
		h.oauthMutex.Unlock()

		c.JSON(http.StatusOK, gin.H{
			"status": "expired",
		})
		return
	}

	// Session found and valid - return auth data
	// Remove session after retrieval (one-time use)
	h.oauthMutex.Lock()
	delete(h.oauthSessions, sessionID)
	h.oauthMutex.Unlock()

	c.JSON(http.StatusOK, gin.H{
		"status": "complete",
		"auth": gin.H{
			"token":      session.AuthResponse.Token,
			"user":       session.AuthResponse.User,
			"expires_at": session.AuthResponse.ExpiresAt,
		},
	})
}

// Me returns current user info
// GET /api/v1/auth/me
func (h *AuthHandlers) Me(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var user models.User
	if err := database.DB.Where("id = ?", userID).First(&user).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"user": user,
	})
}

// GetStreamToken generates a getstream.io Chat token
// GET /api/v1/auth/stream-token
func (h *AuthHandlers) GetStreamToken(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	if h.stream == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "stream_client_not_configured"})
		return
	}

	// Ensure user exists in getstream.io
	var user models.User
	if err := database.DB.Where("id = ?", userID).First(&user).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Create user in getstream.io if needed
	if err := h.stream.CreateUser(user.StreamUserID, user.Username); err != nil {
		// Log but continue - user might already exist
		fmt.Printf("Warning: failed to create/getstream.io user: %v\n", err)
	}

	// Generate token (24 hour expiration)
	expiration := time.Now().Add(24 * time.Hour)
	token, err := h.stream.CreateToken(user.StreamUserID, expiration)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "token_generation_failed", "message": err.Error()})
		return
	}

	// Get API key from stream client (needed for plugin)
	apiKey := os.Getenv("STREAM_API_KEY")
	if apiKey == "" {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "stream_api_key_not_configured"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token":      token,
		"api_key":    apiKey,
		"user_id":    user.StreamUserID,
		"expires_at": expiration,
	})
}

// UploadProfilePicture handles profile picture upload
// POST /api/v1/users/upload-profile-picture
func (h *AuthHandlers) UploadProfilePicture(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	if h.uploader == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "uploader_not_configured"})
		return
	}

	file, header, err := c.Request.FormFile("file")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "no_file_provided"})
		return
	}
	defer file.Close()

	result, err := h.uploader.UploadProfilePicture(context.Background(), file, header, userID.(string))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "upload_failed", "message": err.Error()})
		return
	}

	// Update user's profile picture URL in database (user-uploaded picture)
	database.DB.Model(&models.User{}).Where("id = ?", userID).Update("profile_picture_url", result.URL)

	c.JSON(http.StatusOK, gin.H{
		"url": result.URL,
	})
}

// GetProfilePictureURL returns the profile picture URL for a user
// GET /api/v1/users/:id/profile-picture
func (h *AuthHandlers) GetProfilePictureURL(c *gin.Context) {
	userID := c.Param("id")

	var user models.User
	if err := database.DB.Where("id = ?", userID).First(&user).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Prefer user-uploaded picture, fallback to OAuth picture
	avatarURL := user.ProfilePictureURL
	if avatarURL == "" {
		avatarURL = user.OAuthProfilePictureURL
	}
	if avatarURL == "" {
		c.JSON(http.StatusNotFound, gin.H{"error": "no_profile_picture"})
		return
	}

	// Check if client wants the actual image data (proxy mode)
	// This is indicated by Accept header containing image/* or query param ?proxy=true
	acceptHeader := c.GetHeader("Accept")
	proxyMode := c.Query("proxy") == "true" || strings.Contains(acceptHeader, "image/")

	if proxyMode {
		// Proxy the actual image data from S3/external URL
		client := &http.Client{Timeout: 10 * time.Second}
		resp, err := client.Get(avatarURL)
		if err != nil {
			c.JSON(http.StatusBadGateway, gin.H{"error": "failed_to_fetch_image"})
			return
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			c.JSON(http.StatusBadGateway, gin.H{"error": "image_not_found"})
			return
		}

		// Forward content type and stream the image
		contentType := resp.Header.Get("Content-Type")
		if contentType == "" {
			contentType = "image/jpeg" // Default fallback
		}
		c.Header("Content-Type", contentType)
		c.Header("Cache-Control", "public, max-age=3600") // Cache for 1 hour
		c.Status(http.StatusOK)
		io.Copy(c.Writer, resp.Body)
		return
	}

	// Default: Return the URL for the plugin to download directly
	c.JSON(http.StatusOK, gin.H{
		"url":     avatarURL,
		"user_id": userID,
	})
}

// RegisterDevice creates a new device ID for VST authentication
func (h *Handlers) RegisterDevice(c *gin.Context) {
	deviceID := uuid.New().String()

	c.JSON(http.StatusOK, gin.H{
		"device_id": deviceID,
		"status":    "pending_claim",
		"claim_url": "/auth/claim/" + deviceID,
	})
}

// ClaimDevice allows a user to claim a device through web authentication
func (h *Handlers) ClaimDevice(c *gin.Context) {
	var req struct {
		DeviceID string `json:"device_id" binding:"required"`
		UserID   string `json:"user_id" binding:"required"`
		Username string `json:"username" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Create Stream.io user if doesn't exist
	if err := h.stream.CreateUser(req.UserID, req.Username); err != nil {
		// User might already exist, continue
	}

	// In a real implementation, you'd store this mapping in a database
	// For now, we'll generate a JWT token
	token := "jwt_token_placeholder_" + req.UserID

	c.JSON(http.StatusOK, gin.H{
		"token":    token,
		"user_id":  req.UserID,
		"username": req.Username,
		"status":   "claimed",
	})
}

// VerifyToken verifies a JWT token
func (h *Handlers) VerifyToken(c *gin.Context) {
	token := c.GetHeader("Authorization")
	if token == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "no token provided"})
		return
	}

	// Simple token validation - in production use proper JWT
	if len(token) > 20 {
		c.JSON(http.StatusOK, gin.H{"valid": true})
	} else {
		c.JSON(http.StatusUnauthorized, gin.H{"valid": false})
	}
}

// AuthMiddleware validates requests with JWT tokens
func (h *AuthHandlers) AuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		// Extract token from Authorization header (Bearer token)
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "no_token_provided"})
			c.Abort()
			return
		}

		// Remove "Bearer " prefix if present
		tokenString := strings.TrimPrefix(authHeader, "Bearer ")
		tokenString = strings.TrimSpace(tokenString)

		if tokenString == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid_token_format"})
			c.Abort()
			return
		}

		// Validate token using auth service
		if h.authService == nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
			c.Abort()
			return
		}

		user, err := h.authService.ValidateToken(tokenString)
		if err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid_token", "message": err.Error()})
			c.Abort()
			return
		}

		// Set user info in context
		c.Set("user_id", user.ID)
		c.Set("user", user)
		c.Next()
	}
}

// RequestPasswordReset creates a password reset token
// POST /api/v1/auth/reset-password
func (h *AuthHandlers) RequestPasswordReset(c *gin.Context) {
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
		return
	}

	var req struct {
		Email string `json:"email" binding:"required,email"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	// Create reset token (don't reveal if user exists for security)
	token, err := h.authService.RequestPasswordReset(req.Email)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "reset_request_failed", "message": err.Error()})
		return
	}

	// Always return success (don't reveal if user exists)
	if token != nil {
		// Send password reset email if email service is configured
		if h.emailService != nil {
			err := h.emailService.SendPasswordResetEmail(c.Request.Context(), req.Email, token.Token)
			if err != nil {
				// Log error but don't fail the request (security: don't reveal if email was sent)
				fmt.Printf("Warning: Failed to send password reset email to %s: %v\n", req.Email, err)
				// In development, still return token for testing
				if os.Getenv("ENVIRONMENT") == "development" {
					c.JSON(http.StatusOK, gin.H{
						"message": "password_reset_email_sent",
						"token":   token.Token, // Dev mode only
					})
					return
				}
			}
		} else {
			// Email service not configured - return token in dev mode only
			if os.Getenv("ENVIRONMENT") == "development" {
				c.JSON(http.StatusOK, gin.H{
					"message": "password_reset_email_sent",
					"token":   token.Token, // Dev mode only
				})
				return
			}
		}

		// Production mode or email sent successfully - don't return token
		c.JSON(http.StatusOK, gin.H{
			"message": "password_reset_email_sent",
		})
	} else {
		// User doesn't exist or has no password - still return success
		c.JSON(http.StatusOK, gin.H{
			"message": "password_reset_email_sent",
		})
	}
}

// ResetPassword validates token and updates password
// POST /api/v1/auth/reset-password/confirm
func (h *AuthHandlers) ResetPassword(c *gin.Context) {
	if h.authService == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "auth_service_not_configured"})
		return
	}

	var req struct {
		Token       string `json:"token" binding:"required"`
		NewPassword string `json:"new_password" binding:"required,min=8"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	err := h.authService.ResetPassword(req.Token, req.NewPassword)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "reset_failed", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "password_reset_successful",
	})
}
