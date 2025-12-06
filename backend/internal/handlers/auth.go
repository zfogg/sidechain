package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

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
func (h *Handlers) AuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		token := c.GetHeader("Authorization")
		if token == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "no token provided"})
			c.Abort()
			return
		}

		// Simple validation - in production use proper JWT parsing
		if len(token) < 20 {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid token"})
			c.Abort()
			return
		}

		// Extract user ID from token (simplified)
		userID := "user_123" // In production, extract from JWT
		c.Set("user_id", userID)
		c.Next()
	}
}
