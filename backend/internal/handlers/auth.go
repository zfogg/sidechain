package handlers

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/auth"
)

// AuthHandlers contains authentication-related HTTP handlers
type AuthHandlers struct {
	authService *auth.Service
}

// NewAuthHandlers creates new auth handlers
func NewAuthHandlers(authService *auth.Service) *AuthHandlers {
	return &AuthHandlers{
		authService: authService,
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
	// Verify state parameter
	storedState, err := c.Cookie("oauth_state")
	if err != nil || storedState != c.Query("state") {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_state",
			"message": "Invalid OAuth state",
		})
		return
	}

	code := c.Query("code")
	if code == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "missing_code",
			"message": "Authorization code not provided",
		})
		return
	}

	authResp, err := h.authService.HandleGoogleCallback(code)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "oauth_failed",
			"message": "Google authentication failed",
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
	// Verify state parameter
	storedState, err := c.Cookie("oauth_state")
	if err != nil || storedState != c.Query("state") {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_state",
			"message": "Invalid OAuth state",
		})
		return
	}

	code := c.Query("code")
	if code == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "missing_code",
			"message": "Authorization code not provided",
		})
		return
	}

	authResp, err := h.authService.HandleDiscordCallback(code)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "oauth_failed",
			"message": "Discord authentication failed",
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