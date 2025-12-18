package handlers

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
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
	"github.com/zfogg/sidechain/backend/internal/search"
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
	search       *search.Client
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

// SetSearchClient sets the Elasticsearch search client
func (h *AuthHandlers) SetSearchClient(searchClient *search.Client) {
	h.search = searchClient
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

	// Index new user in Elasticsearch (Phase 0.3)
	if h.search != nil {
		go func() {
			userDoc := search.UserToSearchDoc(authResp.User)
			if err := h.search.IndexUser(c.Request.Context(), authResp.User.ID, userDoc); err != nil {
				// Log but don't fail - search is non-critical
				fmt.Printf("Warning: Failed to index user %s in Elasticsearch: %v\n", authResp.User.ID, err)
			}
		}()
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
// GET /api/v1/auth/google?session_id=...&state=...&redirect_uri=...
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

	// If redirect_uri is provided (web flow), encode it in the state
	redirectURI := c.Query("redirect_uri")
	if redirectURI != "" {
		stateData := map[string]string{
			"session_id":   state,
			"redirect_uri": redirectURI,
		}
		stateJSON, _ := json.Marshal(stateData)
		state = url.QueryEscape(string(stateJSON))
	}

	oauthURL := h.authService.GetGoogleOAuthURL(state)
	c.Redirect(http.StatusTemporaryRedirect, oauthURL)
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

	// Handle Google OAuth callback
	authResp, err := h.authService.HandleGoogleCallback(code)
	if err != nil {
		// Extract redirect_uri if present
		stateParam := c.Query("state")
		var redirectURI string
		stateData := decodeStateParameter(stateParam)
		if stateData != nil {
			redirectURI = stateData["redirect_uri"]
		}

		// If redirect_uri provided (web flow), redirect with error
		if redirectURI != "" {
			redirectURL := fmt.Sprintf("%s?error=%s", redirectURI, url.QueryEscape(err.Error()))
			c.Redirect(http.StatusTemporaryRedirect, redirectURL)
			return
		}

		// Otherwise show error page to browser (VST flow)
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

	// Extract session_id and redirect_uri from state parameter
	stateParam := c.Query("state")
	stateData := decodeStateParameter(stateParam)

	var redirectURI string
	var sessionID string

	if stateData != nil {
		redirectURI = stateData["redirect_uri"]
		sessionID = stateData["session_id"]
	} else {
		// Fallback: use state as session_id if not JSON encoded
		sessionID = stateParam
	}

	// If redirect_uri provided (web flow), redirect back with token and user
	if redirectURI != "" {
		userJSON, _ := json.Marshal(authResp.User)
		userJSONEncoded := url.QueryEscape(string(userJSON))
		redirectURL := fmt.Sprintf("%s?token=%s&user=%s", redirectURI, authResp.Token, userJSONEncoded)
		c.Redirect(http.StatusTemporaryRedirect, redirectURL)
		return
	}

	// VST flow: Store session for polling (expires in 5 minutes)
	if sessionID != "" {
		h.oauthMutex.Lock()
		h.oauthSessions[sessionID] = &OAuthSession{
			AuthResponse: authResp,
			CreatedAt:    time.Now(),
			ExpiresAt:    time.Now().Add(5 * time.Minute),
		}
		h.oauthMutex.Unlock()
	}

	// Show success page to browser (plugin will poll for auth if sessionID was provided)
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
}

// DiscordOAuth initiates Discord OAuth flow
// GET /api/v1/auth/discord?session_id=...&state=...&redirect_uri=...
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

	// If redirect_uri is provided (web flow), encode it in the state
	redirectURI := c.Query("redirect_uri")
	if redirectURI != "" {
		stateData := map[string]string{
			"session_id":   state,
			"redirect_uri": redirectURI,
		}
		stateJSON, _ := json.Marshal(stateData)
		state = url.QueryEscape(string(stateJSON))
	}

	oauthURL := h.authService.GetDiscordOAuthURL(state)
	c.Redirect(http.StatusTemporaryRedirect, oauthURL)
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

	// Handle Discord OAuth callback
	authResp, err := h.authService.HandleDiscordCallback(code)
	if err != nil {
		// Extract redirect_uri if present
		stateParam := c.Query("state")
		var redirectURI string
		stateData := decodeStateParameter(stateParam)
		if stateData != nil {
			redirectURI = stateData["redirect_uri"]
		}

		// If redirect_uri provided (web flow), redirect with error
		if redirectURI != "" {
			redirectURL := fmt.Sprintf("%s?error=%s", redirectURI, url.QueryEscape(err.Error()))
			c.Redirect(http.StatusTemporaryRedirect, redirectURL)
			return
		}

		// Otherwise show error page to browser (VST flow)
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

	// Extract session_id and redirect_uri from state parameter
	stateParam := c.Query("state")
	stateData := decodeStateParameter(stateParam)

	var redirectURI string
	var sessionID string

	if stateData != nil {
		redirectURI = stateData["redirect_uri"]
		sessionID = stateData["session_id"]
	} else {
		// Fallback: use state as session_id if not JSON encoded
		sessionID = stateParam
	}

	// If redirect_uri provided (web flow), redirect back with token and user
	if redirectURI != "" {
		userJSON, _ := json.Marshal(authResp.User)
		userJSONEncoded := url.QueryEscape(string(userJSON))
		redirectURL := fmt.Sprintf("%s?token=%s&user=%s", redirectURI, authResp.Token, userJSONEncoded)
		c.Redirect(http.StatusTemporaryRedirect, redirectURL)
		return
	}

	// VST flow: Store session for polling (expires in 5 minutes)
	if sessionID != "" {
		h.oauthMutex.Lock()
		h.oauthSessions[sessionID] = &OAuthSession{
			AuthResponse: authResp,
			CreatedAt:    time.Now(),
			ExpiresAt:    time.Now().Add(5 * time.Minute),
		}
		h.oauthMutex.Unlock()
	}

	// Show success page to browser (plugin will poll for auth if sessionID was provided)
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

	// Use database user ID consistently for Stream Chat (frontend expects this)
	// This ensures the token's user_id claim matches the frontend's user.id
	streamUserID := user.ID

	// Create user in getstream.io if needed
	if err := h.stream.CreateUser(streamUserID, user.Username); err != nil {
		// Log but continue - user might already exist
		fmt.Printf("[Auth] Warning: failed to create getstream.io user: %v\n", err)
	}

	// Generate token (24 hour expiration)
	expiration := time.Now().Add(24 * time.Hour)
	token, err := h.stream.CreateToken(streamUserID, expiration)
	if err != nil {
		fmt.Printf("[Auth] Error creating Stream token: %v\n", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "token_generation_failed", "message": err.Error()})
		return
	}

	// Get API key from stream client (needed for plugin)
	apiKey := os.Getenv("STREAM_API_KEY")
	if apiKey == "" {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "stream_api_key_not_configured"})
		return
	}

	fmt.Printf("[Auth] Generated Stream token for user:%s (userID=%s)\n", user.Username, streamUserID)

	c.JSON(http.StatusOK, gin.H{
		"token":      token,
		"api_key":    apiKey,
		"user_id":    streamUserID,
		"expires_at": expiration,
	})
}

// SyncUserToStream syncs a user to getstream.io Chat (for DM creation)
// POST /api/v1/auth/sync-user/:user_id
// This endpoint is called when creating a DM with a user who may not have authenticated yet.
// It uses the backend's admin credentials to create/sync the user in Stream Chat.
func (h *AuthHandlers) SyncUserToStream(c *gin.Context) {
	// Require authentication - only authenticated users can initiate sync
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}
	_ = userID // We have the current user, but we're syncing a target user

	if h.stream == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "stream_client_not_configured"})
		return
	}

	// Get target user ID from URL param
	targetUserID := c.Param("user_id")
	if targetUserID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "user_id_required"})
		return
	}

	// Look up target user in database
	var user models.User
	if err := database.DB.Where("id = ?", targetUserID).First(&user).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Create/sync user in getstream.io with admin credentials
	if err := h.stream.CreateUser(user.ID, user.Username); err != nil {
		// Log the error but check if it's a "user already exists" error (which is OK)
		fmt.Printf("[Auth] Warning: failed to sync user to getstream.io: %v\n", err)
		// We don't return an error here - the user might already exist in Stream Chat
		// Continue and return success since the user is now syncable
	}

	fmt.Printf("[Auth] Successfully synced user:%s (userID=%s) to getstream.io\n", user.Username, user.ID)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"message": "user synced to stream chat",
		"user_id": user.ID,
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

// AuthMiddlewareOptional validates token if provided, but allows requests without auth (Phase 5.1)
func (h *AuthHandlers) AuthMiddlewareOptional() gin.HandlerFunc {
	return func(c *gin.Context) {
		// Extract token from Authorization header (Bearer token)
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			// No auth provided - continue without setting user context
			c.Next()
			return
		}

		// Remove "Bearer " prefix if present
		tokenString := strings.TrimPrefix(authHeader, "Bearer ")
		tokenString = strings.TrimSpace(tokenString)

		if tokenString == "" {
			// Invalid format but don't reject - continue without user context
			c.Next()
			return
		}

		// Validate token using auth service
		if h.authService == nil {
			// Auth service not configured - continue without validation
			c.Next()
			return
		}

		user, err := h.authService.ValidateToken(tokenString)
		if err != nil {
			// Invalid token but don't reject - log and continue
			// User context won't be set, so handlers can check with c.GetString("user_id")
			c.Next()
			return
		}

		// Token is valid - set user info in context
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

// decodeStateParameter decodes the state parameter which may contain JSON with session_id and redirect_uri
func decodeStateParameter(state string) map[string]string {
	if state == "" {
		return nil
	}

	// Try to URL decode first
	decoded, err := url.QueryUnescape(state)
	if err != nil {
		return nil
	}

	// Try to unmarshal as JSON
	var stateData map[string]string
	if err := json.Unmarshal([]byte(decoded), &stateData); err != nil {
		// If not JSON, it's a plain state value (for VST flow)
		return nil
	}

	return stateData
}
