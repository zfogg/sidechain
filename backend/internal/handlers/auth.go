package handlers

import (
	"context"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/auth"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/search"
	"github.com/zfogg/sidechain/backend/internal/storage"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

// OAuthSession stores pending OAuth session data
type OAuthSession struct {
	Provider  string
	State     string
	AuthResp  *auth.AuthResponse
	Error     string
	CreatedAt time.Time
}

// In-memory OAuth session storage (consider Redis for production)
var (
	oauthSessions     = make(map[string]*OAuthSession)
	oauthSessionMutex sync.RWMutex
)

// AuthHandlers contains authentication-related HTTP handlers
type AuthHandlers struct {
	authService  *auth.Service
	s3Uploader   storage.ProfilePictureUploader
	search       *search.Client
	streamClient *stream.Client
}

// NewAuthHandlers creates new auth handlers
func NewAuthHandlers(authService *auth.Service, s3Uploader storage.ProfilePictureUploader, streamClient *stream.Client) *AuthHandlers {
	return &AuthHandlers{
		authService:  authService,
		s3Uploader:   s3Uploader,
		streamClient: streamClient,
	}
}

// SetSearchClient sets the search client for indexing users
func (h *AuthHandlers) SetSearchClient(searchClient *search.Client) {
	h.search = searchClient
}

// Register handles native user registration
func (h *AuthHandlers) Register(c *gin.Context) {
	var req auth.RegisterRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	authResp, err := h.authService.RegisterNativeUser(req)
	if err != nil {
		if err == auth.ErrUserExists {
			c.JSON(http.StatusConflict, gin.H{
				"error":   "user_exists",
				"message": "User already exists with this email",
			})
			return
		}
		if err == auth.ErrUsernameExists {
			c.JSON(http.StatusConflict, gin.H{
				"error":   "username_exists",
				"message": "Username is already taken",
			})
			return
		}

		// Log the actual error for debugging
		fmt.Printf("Registration error: %v\n", err)

		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "registration_failed",
			"message": "Failed to create account: " + err.Error(),
		})
		return
	}

	// Index user in Elasticsearch for search (7.1.3)
	if h.search != nil {
		go h.indexUserForSearch(&authResp.User)
	}

	c.JSON(http.StatusCreated, gin.H{
		"message": "Account created successfully",
		"auth":    authResp,
	})
}

// Login handles native user login
func (h *AuthHandlers) Login(c *gin.Context) {
	var req auth.LoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	authResp, err := h.authService.LoginNativeUser(req)
	if err != nil {
		if err == auth.ErrUserNotFound || err == auth.ErrInvalidCredentials {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error":   "invalid_credentials",
				"message": "Invalid email or password",
			})
			return
		}

		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "login_failed",
			"message": "Login failed",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Login successful",
		"auth":    authResp,
	})
}

// GoogleOAuth initiates Google OAuth flow
func (h *AuthHandlers) GoogleOAuth(c *gin.Context) {
	log.Printf("[OAuth] GoogleOAuth: Starting Google OAuth flow")
	log.Printf("[OAuth] GoogleOAuth: Request URL: %s", c.Request.URL.String())
	log.Printf("[OAuth] GoogleOAuth: Request Headers: %v", c.Request.Header)

	// Check if this is a plugin-initiated OAuth (has session_id query param)
	sessionID := c.Query("session_id")
	if sessionID == "" {
		sessionID = uuid.New().String()
		log.Printf("[OAuth] GoogleOAuth: Generated new session_id: %s", sessionID)
	} else {
		log.Printf("[OAuth] GoogleOAuth: Using provided session_id: %s", sessionID)
	}

	state := sessionID // Use session ID as state for plugin flow

	// Store OAuth session for plugin polling
	oauthSessionMutex.Lock()
	oauthSessions[sessionID] = &OAuthSession{
		Provider:  "google",
		State:     state,
		CreatedAt: time.Now(),
	}
	oauthSessionMutex.Unlock()
	log.Printf("[OAuth] GoogleOAuth: Created OAuth session for state: %s", state)

	// Store state in cookie for traditional flow
	c.SetCookie("oauth_state", state, 600, "/", "", false, true)

	authURL := h.authService.GetGoogleOAuthURL(state)
	log.Printf("[OAuth] GoogleOAuth: Redirecting to Google OAuth URL: %s", authURL)
	c.Redirect(http.StatusTemporaryRedirect, authURL)
}

// GoogleCallback handles Google OAuth callback
func (h *AuthHandlers) GoogleCallback(c *gin.Context) {
	log.Printf("[OAuth] GoogleCallback: Received callback")
	log.Printf("[OAuth] GoogleCallback: Request URL: %s", c.Request.URL.String())
	log.Printf("[OAuth] GoogleCallback: Query params - state: %s, code length: %d, error: %s",
		c.Query("state"), len(c.Query("code")), c.Query("error"))

	state := c.Query("state")

	// Check for OAuth provider errors (user denied access, etc.)
	if oauthErr := c.Query("error"); oauthErr != "" {
		errorDesc := c.Query("error_description")
		log.Printf("[OAuth] GoogleCallback: OAuth error from Google: %s - %s", oauthErr, errorDesc)
		if errorDesc == "" {
			errorDesc = getOAuthErrorMessage(oauthErr)
		}
		// Store error in session for plugin polling
		oauthSessionMutex.Lock()
		if session, ok := oauthSessions[state]; ok {
			session.Error = errorDesc
			log.Printf("[OAuth] GoogleCallback: Stored error in session for state: %s", state)
		} else {
			log.Printf("[OAuth] GoogleCallback: WARNING - No session found for state: %s", state)
		}
		oauthSessionMutex.Unlock()

		c.Header("Content-Type", "text/html")
		c.String(http.StatusOK, getOAuthErrorHTML("Google", errorDesc))
		return
	}

	// Verify state parameter
	storedState, _ := c.Cookie("oauth_state")
	log.Printf("[OAuth] GoogleCallback: State verification - callback state: %s, cookie state: %s", state, storedState)

	if storedState != state {
		// Check if state exists in our session storage (plugin flow)
		oauthSessionMutex.RLock()
		_, sessionExists := oauthSessions[state]
		oauthSessionMutex.RUnlock()

		log.Printf("[OAuth] GoogleCallback: State mismatch - checking session storage, exists: %v", sessionExists)

		if !sessionExists {
			log.Printf("[OAuth] GoogleCallback: ERROR - Session not found for state: %s", state)
			c.Header("Content-Type", "text/html")
			c.String(http.StatusOK, getOAuthErrorHTML("Google", "OAuth session expired or invalid. Please try again."))
			return
		}
	}

	code := c.Query("code")
	if code == "" {
		log.Printf("[OAuth] GoogleCallback: ERROR - No authorization code provided")
		c.Header("Content-Type", "text/html")
		c.String(http.StatusOK, getOAuthErrorHTML("Google", "Authorization code not provided by Google"))
		return
	}

	log.Printf("[OAuth] GoogleCallback: Exchanging code for token (code length: %d)", len(code))
	authResp, err := h.authService.HandleGoogleCallback(code)
	if err != nil {
		log.Printf("[OAuth] GoogleCallback: ERROR - HandleGoogleCallback failed: %v", err)
		errorCode, errorMsg := parseOAuthError(err, "Google")
		// Store error in session for plugin polling
		oauthSessionMutex.Lock()
		if session, ok := oauthSessions[state]; ok {
			session.Error = errorMsg
			log.Printf("[OAuth] GoogleCallback: Stored error in session: %s", errorMsg)
		}
		oauthSessionMutex.Unlock()

		c.Header("Content-Type", "text/html")
		c.String(http.StatusOK, getOAuthErrorHTML("Google", errorMsg+" ("+errorCode+")"))
		return
	}

	log.Printf("[OAuth] GoogleCallback: SUCCESS - User authenticated: %s (ID: %s)", authResp.User.Username, authResp.User.ID)

	// Index user in Elasticsearch for search (7.1.3)
	if h.search != nil {
		go h.indexUserForSearch(&authResp.User)
	}

	// Store auth response in session for plugin polling
	oauthSessionMutex.Lock()
	if session, ok := oauthSessions[state]; ok {
		session.AuthResp = authResp
		log.Printf("[OAuth] GoogleCallback: Stored auth response in session for state: %s", state)
	} else {
		log.Printf("[OAuth] GoogleCallback: WARNING - No session found to store auth response for state: %s", state)
	}
	oauthSessionMutex.Unlock()

	// Clear state cookie
	c.SetCookie("oauth_state", "", -1, "/", "", false, true)

	// Return success HTML page
	log.Printf("[OAuth] GoogleCallback: Returning success HTML page")
	c.Header("Content-Type", "text/html")
	c.String(http.StatusOK, getOAuthSuccessHTML("Google", authResp.User.Username))
}

// DiscordOAuth initiates Discord OAuth flow
func (h *AuthHandlers) DiscordOAuth(c *gin.Context) {
	// Check if this is a plugin-initiated OAuth (has session_id query param)
	sessionID := c.Query("session_id")
	if sessionID == "" {
		sessionID = uuid.New().String()
	}

	state := sessionID // Use session ID as state for plugin flow

	// Store OAuth session for plugin polling
	oauthSessionMutex.Lock()
	oauthSessions[sessionID] = &OAuthSession{
		Provider:  "discord",
		State:     state,
		CreatedAt: time.Now(),
	}
	oauthSessionMutex.Unlock()

	// Store state in cookie for traditional flow
	c.SetCookie("oauth_state", state, 600, "/", "", false, true)

	authURL := h.authService.GetDiscordOAuthURL(state)
	c.Redirect(http.StatusTemporaryRedirect, authURL)
}

// DiscordCallback handles Discord OAuth callback
func (h *AuthHandlers) DiscordCallback(c *gin.Context) {
	state := c.Query("state")

	// Check for OAuth provider errors (user denied access, etc.)
	if oauthErr := c.Query("error"); oauthErr != "" {
		errorDesc := c.Query("error_description")
		if errorDesc == "" {
			errorDesc = getOAuthErrorMessage(oauthErr)
		}
		// Store error in session for plugin polling
		oauthSessionMutex.Lock()
		if session, ok := oauthSessions[state]; ok {
			session.Error = errorDesc
		}
		oauthSessionMutex.Unlock()

		c.Header("Content-Type", "text/html")
		c.String(http.StatusOK, getOAuthErrorHTML("Discord", errorDesc))
		return
	}

	// Verify state parameter
	storedState, _ := c.Cookie("oauth_state")
	if storedState != state {
		// Check if state exists in our session storage (plugin flow)
		oauthSessionMutex.RLock()
		_, sessionExists := oauthSessions[state]
		oauthSessionMutex.RUnlock()

		if !sessionExists {
			c.Header("Content-Type", "text/html")
			c.String(http.StatusOK, getOAuthErrorHTML("Discord", "OAuth session expired or invalid. Please try again."))
			return
		}
	}

	code := c.Query("code")
	if code == "" {
		c.Header("Content-Type", "text/html")
		c.String(http.StatusOK, getOAuthErrorHTML("Discord", "Authorization code not provided by Discord"))
		return
	}

	authResp, err := h.authService.HandleDiscordCallback(code)
	if err != nil {
		errorCode, errorMsg := parseOAuthError(err, "Discord")
		// Store error in session for plugin polling
		oauthSessionMutex.Lock()
		if session, ok := oauthSessions[state]; ok {
			session.Error = errorMsg
		}
		oauthSessionMutex.Unlock()

		c.Header("Content-Type", "text/html")
		c.String(http.StatusOK, getOAuthErrorHTML("Discord", errorMsg+" ("+errorCode+")"))
		return
	}

	// Index user in Elasticsearch for search (7.1.3)
	if h.search != nil {
		go h.indexUserForSearch(&authResp.User)
	}

	// Store auth response in session for plugin polling
	oauthSessionMutex.Lock()
	if session, ok := oauthSessions[state]; ok {
		session.AuthResp = authResp
	}
	oauthSessionMutex.Unlock()

	// Clear state cookie
	c.SetCookie("oauth_state", "", -1, "/", "", false, true)

	// Return success HTML page
	c.Header("Content-Type", "text/html")
	c.String(http.StatusOK, getOAuthSuccessHTML("Discord", authResp.User.Username))
}

// getOAuthErrorMessage returns a user-friendly message for OAuth error codes
func getOAuthErrorMessage(errorCode string) string {
	switch errorCode {
	case "access_denied":
		return "You declined to authorize the application. No account was created."
	case "invalid_request":
		return "The authorization request was invalid. Please try again."
	case "unauthorized_client":
		return "This application is not authorized. Please contact support."
	case "unsupported_response_type":
		return "Authorization server configuration error. Please contact support."
	case "invalid_scope":
		return "The requested permissions are not valid. Please contact support."
	case "server_error":
		return "The authorization server encountered an error. Please try again later."
	case "temporarily_unavailable":
		return "The authorization server is temporarily unavailable. Please try again later."
	default:
		return "Authentication was cancelled or failed. Please try again."
	}
}

// parseOAuthError extracts meaningful error information from OAuth errors
func parseOAuthError(err error, provider string) (string, string) {
	errStr := err.Error()

	// Check for common error patterns
	if strings.Contains(errStr, "failed to exchange code") {
		return "token_exchange_failed", provider + " authorization code is invalid or expired. Please try signing in again."
	}
	if strings.Contains(errStr, "failed to get user info") {
		return "user_info_failed", "Could not retrieve your " + provider + " profile. Please try again."
	}
	if strings.Contains(errStr, "database error") {
		return "database_error", "A database error occurred. Please try again later."
	}
	if strings.Contains(errStr, "email") && strings.Contains(errStr, "required") {
		return "email_required", "Your " + provider + " account does not have an email address. Please use a different sign-in method."
	}

	// Default error
	return "oauth_failed", provider + " authentication failed. Please try again."
}

// Me returns current user information
func (h *AuthHandlers) Me(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error":   "unauthorized",
			"message": "User not authenticated",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"user": user,
	})
}

// GetStreamToken generates a getstream.io Chat token for the authenticated user
// Returns token, API key, and user ID for direct plugin-to-getstream.io communication
func (h *AuthHandlers) GetStreamToken(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error":   "unauthorized",
			"message": "User not authenticated",
		})
		return
	}

	// Extract user ID from user object
	userObj, ok := user.(*models.User)
	if !ok {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "internal_error",
			"message": "Invalid user object",
		})
		return
	}

	// Ensure user exists in getstream.io Chat (create if needed)
	if h.streamClient != nil {
		err := h.streamClient.CreateUser(userObj.ID, userObj.Username)
		if err != nil {
			log.Printf("Warning: Failed to ensure user exists in getstream.io Chat: %v", err)
			// Continue anyway - user might already exist
		}
	}

	if h.streamClient == nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "internal_error",
			"message": "Stream client not configured",
		})
		return
	}

	// Generate token with 24 hour expiration
	expiration := time.Now().Add(24 * time.Hour)
	token, err := h.streamClient.CreateToken(userObj.ID, expiration)
	if err != nil {
		log.Printf("Failed to create getstream.io token: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "token_generation_failed",
			"message": "Failed to generate authentication token",
		})
		return
	}

	// Get API key from environment
	// Note: API key is safe to expose to clients (it's a public identifier)
	// Only the API secret (used server-side for token generation) must be protected
	apiKey := os.Getenv("STREAM_API_KEY")
	if apiKey == "" {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "configuration_error",
			"message": "Stream API key not configured",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"token":   token,
		"api_key": apiKey,
		"user_id": userObj.ID,
	})
}

// indexUserForSearch indexes a user in Elasticsearch for search (7.1.3)
func (h *AuthHandlers) indexUserForSearch(user *models.User) {
	if h.search == nil {
		return
	}

	ctx := context.Background()
	doc := map[string]interface{}{
		"id":             user.ID,
		"username":       user.Username,
		"display_name":   user.DisplayName,
		"bio":            user.Bio,
		"genre":          user.Genre,
		"follower_count": user.FollowerCount,
		"created_at":     user.CreatedAt.Format(time.RFC3339),
	}

	// Add completion suggest field for autocomplete (7.1.10)
	if user.Username != "" {
		doc["username_suggest"] = map[string]interface{}{
			"input": []string{user.Username},
		}
	}

	if err := h.search.IndexUser(ctx, user.ID, doc); err != nil {
		// Log but don't fail - search indexing is non-critical
		log.Printf("Warning: Failed to index user %s in Elasticsearch: %v", user.ID, err)
	}
}

// AuthMiddleware validates JWT tokens
func (h *AuthHandlers) AuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error":   "missing_token",
				"message": "Authorization header required",
			})
			c.Abort()
			return
		}

		// Extract token from "Bearer <token>"
		parts := strings.Split(authHeader, " ")
		if len(parts) != 2 || parts[0] != "Bearer" {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error":   "invalid_token_format",
				"message": "Authorization header must be 'Bearer <token>'",
			})
			c.Abort()
			return
		}

		token := parts[1]
		user, err := h.authService.ValidateToken(token)
		if err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error":   "invalid_token",
				"message": "Token validation failed",
			})
			c.Abort()
			return
		}

		// Store user in context for handlers
		c.Set("user", user)
		c.Set("user_id", user.ID)
		c.Next()
	}
}

// UploadProfilePicture handles profile picture uploads
func (h *AuthHandlers) UploadProfilePicture(c *gin.Context) {
	// Get user ID from context (set by authentication middleware)
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error":   "unauthorized",
			"message": "User not authenticated",
		})
		return
	}

	// Parse multipart form
	err := c.Request.ParseMultipartForm(10 << 20) // 10 MB max
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_file",
			"message": "Failed to parse upload form",
		})
		return
	}

	// Get the file from form
	file, header, err := c.Request.FormFile("profile_picture")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "missing_file",
			"message": "No profile picture file provided",
		})
		return
	}
	defer file.Close()

	// Upload to S3
	uploadResult, err := h.s3Uploader.UploadProfilePicture(c.Request.Context(), file, header, userID.(string))
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "upload_failed",
			"message": err.Error(),
		})
		return
	}

	// Update user's profile_picture_url in database
	var user models.User
	if err := database.DB.First(&user, "id = ?", userID.(string)).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "user_not_found",
			"message": "Could not find user to update",
		})
		return
	}

	if err := database.DB.Model(&user).Update("profile_picture_url", uploadResult.URL).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "database_update_failed",
			"message": "Profile picture uploaded but failed to save to profile",
			"url":     uploadResult.URL, // Still return URL so client can retry
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":             "Profile picture uploaded successfully",
		"url":                 uploadResult.URL,
		"key":                 uploadResult.Key,
		"profile_picture_url": uploadResult.URL,
	})
}

// OAuthPoll allows the plugin to poll for OAuth completion
// GET /api/v1/auth/oauth/poll?session_id=...
func (h *AuthHandlers) OAuthPoll(c *gin.Context) {
	sessionID := c.Query("session_id")
	log.Printf("[OAuth] OAuthPoll: Poll request for session_id: %s", sessionID)

	if sessionID == "" {
		log.Printf("[OAuth] OAuthPoll: ERROR - Missing session_id")
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "missing_session_id",
			"message": "session_id query parameter is required",
		})
		return
	}

	oauthSessionMutex.RLock()
	session, exists := oauthSessions[sessionID]
	oauthSessionMutex.RUnlock()

	if !exists {
		log.Printf("[OAuth] OAuthPoll: Session not found for session_id: %s", sessionID)
		c.JSON(http.StatusNotFound, gin.H{
			"status":  "not_found",
			"message": "OAuth session not found or expired",
		})
		return
	}

	log.Printf("[OAuth] OAuthPoll: Session found - provider: %s, hasAuthResp: %v, hasError: %v, age: %v",
		session.Provider, session.AuthResp != nil, session.Error != "", time.Since(session.CreatedAt))

	// Check if session is expired (10 minutes)
	if time.Since(session.CreatedAt) > 10*time.Minute {
		log.Printf("[OAuth] OAuthPoll: Session expired for session_id: %s", sessionID)
		// Clean up expired session
		oauthSessionMutex.Lock()
		delete(oauthSessions, sessionID)
		oauthSessionMutex.Unlock()

		c.JSON(http.StatusGone, gin.H{
			"status":  "expired",
			"message": "OAuth session has expired. Please try again.",
		})
		return
	}

	// Check if auth completed with error
	if session.Error != "" {
		log.Printf("[OAuth] OAuthPoll: Returning error for session_id: %s - %s", sessionID, session.Error)
		// Clean up session after reporting error
		oauthSessionMutex.Lock()
		delete(oauthSessions, sessionID)
		oauthSessionMutex.Unlock()

		c.JSON(http.StatusOK, gin.H{
			"status":  "error",
			"message": session.Error,
		})
		return
	}

	// Check if auth completed successfully
	if session.AuthResp != nil {
		log.Printf("[OAuth] OAuthPoll: SUCCESS - Returning auth for session_id: %s, user: %s", sessionID, session.AuthResp.User.Username)
		// Clean up session after returning token
		oauthSessionMutex.Lock()
		delete(oauthSessions, sessionID)
		oauthSessionMutex.Unlock()

		c.JSON(http.StatusOK, gin.H{
			"status":   "complete",
			"message":  fmt.Sprintf("%s authentication successful", session.Provider),
			"auth":     session.AuthResp,
			"provider": session.Provider,
		})
		return
	}

	// Auth still pending
	log.Printf("[OAuth] OAuthPoll: Still pending for session_id: %s", sessionID)
	c.JSON(http.StatusOK, gin.H{
		"status":   "pending",
		"message":  "Waiting for authentication...",
		"provider": session.Provider,
	})
}

// getOAuthSuccessHTML returns an HTML page for successful OAuth
func getOAuthSuccessHTML(provider, username string) string {
	return fmt.Sprintf(`<!DOCTYPE html>
<html>
<head>
    <title>Sidechain - Sign In Successful</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%%, #16213e 100%%);
            color: white;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            margin: 0;
        }
        .container {
            text-align: center;
            padding: 40px;
            background: rgba(255,255,255,0.1);
            border-radius: 16px;
            backdrop-filter: blur(10px);
            max-width: 400px;
        }
        .success-icon {
            font-size: 64px;
            margin-bottom: 20px;
        }
        h1 { margin-bottom: 10px; }
        p { color: #a0a0a0; margin-bottom: 20px; }
        .username {
            color: #00d4ff;
            font-weight: bold;
        }
        .note {
            font-size: 14px;
            color: #808080;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="success-icon">✓</div>
        <h1>Welcome to Sidechain!</h1>
        <p>Successfully signed in with %s as <span class="username">%s</span></p>
        <p class="note">You can close this window and return to the plugin.</p>
    </div>
</body>
</html>`, provider, username)
}

// getOAuthErrorHTML returns an HTML page for OAuth errors
func getOAuthErrorHTML(provider, errorMsg string) string {
	return fmt.Sprintf(`<!DOCTYPE html>
<html>
<head>
    <title>Sidechain - Sign In Failed</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%%, #16213e 100%%);
            color: white;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            margin: 0;
        }
        .container {
            text-align: center;
            padding: 40px;
            background: rgba(255,255,255,0.1);
            border-radius: 16px;
            backdrop-filter: blur(10px);
            max-width: 400px;
        }
        .error-icon {
            font-size: 64px;
            margin-bottom: 20px;
        }
        h1 { margin-bottom: 10px; color: #ff6b6b; }
        p { color: #a0a0a0; margin-bottom: 20px; }
        .note {
            font-size: 14px;
            color: #808080;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="error-icon">✗</div>
        <h1>Sign In Failed</h1>
        <p>%s sign in failed:</p>
        <p>%s</p>
        <p class="note">You can close this window and try again in the plugin.</p>
    </div>
</body>
</html>`, provider, errorMsg)
}

// ProxyProfilePicture proxies profile pictures from S3 to work around JUCE SSL issues on Linux
// GET /api/v1/users/:id/profile-picture
func (h *AuthHandlers) ProxyProfilePicture(c *gin.Context) {
	userID := c.Param("id")
	if userID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "user ID required"})
		return
	}

	// Get user from database
	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user not found"})
		return
	}

	if user.ProfilePictureURL == "" {
		c.JSON(http.StatusNotFound, gin.H{"error": "no profile picture"})
		return
	}

	// Fetch the image from S3
	resp, err := http.Get(user.ProfilePictureURL)
	if err != nil {
		log.Printf("Failed to fetch profile picture: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to fetch image"})
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		c.JSON(http.StatusNotFound, gin.H{"error": "image not found on S3"})
		return
	}

	// Set appropriate headers
	contentType := resp.Header.Get("Content-Type")
	if contentType == "" {
		contentType = "image/jpeg"
	}
	c.Header("Content-Type", contentType)
	c.Header("Cache-Control", "max-age=3600") // Cache for 1 hour

	// Stream the response
	io.Copy(c.Writer, resp.Body)
}
