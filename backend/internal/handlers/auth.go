package handlers

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/auth"
	"github.com/zfogg/sidechain/backend/internal/storage"
)

// AuthHandlers contains authentication-related HTTP handlers
type AuthHandlers struct {
	authService *auth.Service
	s3Uploader  *storage.S3Uploader
}

// NewAuthHandlers creates new auth handlers
func NewAuthHandlers(authService *auth.Service, s3Uploader *storage.S3Uploader) *AuthHandlers {
	return &AuthHandlers{
		authService: authService,
		s3Uploader:  s3Uploader,
	}
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

		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "registration_failed",
			"message": "Failed to create account",
		})
		return
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
	state := uuid.New().String()

	// Store state in session (simplified - in production use proper session management)
	c.SetCookie("oauth_state", state, 600, "/", "", false, true)

	authURL := h.authService.GetGoogleOAuthURL(state)
	c.Redirect(http.StatusTemporaryRedirect, authURL)
}

// GoogleCallback handles Google OAuth callback
func (h *AuthHandlers) GoogleCallback(c *gin.Context) {
	// Check for OAuth provider errors (user denied access, etc.)
	if oauthErr := c.Query("error"); oauthErr != "" {
		errorDesc := c.Query("error_description")
		if errorDesc == "" {
			errorDesc = getOAuthErrorMessage(oauthErr)
		}
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "oauth_denied",
			"message": errorDesc,
			"code":    oauthErr,
		})
		return
	}

	// Verify state parameter
	storedState, err := c.Cookie("oauth_state")
	if err != nil || storedState != c.Query("state") {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_state",
			"message": "OAuth session expired or invalid. Please try again.",
		})
		return
	}

	code := c.Query("code")
	if code == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "missing_code",
			"message": "Authorization code not provided by Google",
		})
		return
	}

	authResp, err := h.authService.HandleGoogleCallback(code)
	if err != nil {
		// Provide more specific error messages based on error type
		errorCode, errorMsg := parseOAuthError(err, "Google")
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   errorCode,
			"message": errorMsg,
		})
		return
	}

	// Clear state cookie
	c.SetCookie("oauth_state", "", -1, "/", "", false, true)

	c.JSON(http.StatusOK, gin.H{
		"message": "Google authentication successful",
		"auth":    authResp,
	})
}

// DiscordOAuth initiates Discord OAuth flow
func (h *AuthHandlers) DiscordOAuth(c *gin.Context) {
	state := uuid.New().String()

	c.SetCookie("oauth_state", state, 600, "/", "", false, true)

	authURL := h.authService.GetDiscordOAuthURL(state)
	c.Redirect(http.StatusTemporaryRedirect, authURL)
}

// DiscordCallback handles Discord OAuth callback
func (h *AuthHandlers) DiscordCallback(c *gin.Context) {
	// Check for OAuth provider errors (user denied access, etc.)
	if oauthErr := c.Query("error"); oauthErr != "" {
		errorDesc := c.Query("error_description")
		if errorDesc == "" {
			errorDesc = getOAuthErrorMessage(oauthErr)
		}
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "oauth_denied",
			"message": errorDesc,
			"code":    oauthErr,
		})
		return
	}

	// Verify state parameter
	storedState, err := c.Cookie("oauth_state")
	if err != nil || storedState != c.Query("state") {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_state",
			"message": "OAuth session expired or invalid. Please try again.",
		})
		return
	}

	code := c.Query("code")
	if code == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "missing_code",
			"message": "Authorization code not provided by Discord",
		})
		return
	}

	authResp, err := h.authService.HandleDiscordCallback(code)
	if err != nil {
		// Provide more specific error messages based on error type
		errorCode, errorMsg := parseOAuthError(err, "Discord")
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   errorCode,
			"message": errorMsg,
		})
		return
	}

	// Clear state cookie
	c.SetCookie("oauth_state", "", -1, "/", "", false, true)

	c.JSON(http.StatusOK, gin.H{
		"message": "Discord authentication successful",
		"auth":    authResp,
	})
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

	// TODO: Update user's avatar_url in database
	// For now, just return the upload result
	c.JSON(http.StatusOK, gin.H{
		"message": "Profile picture uploaded successfully",
		"url":     uploadResult.URL,
		"key":     uploadResult.Key,
	})
}
