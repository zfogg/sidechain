package util

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// GetUserFromContext extracts the authenticated user from the Gin context.
// Returns the user and true if found, or nil and false if not authenticated.
// If the user is not authenticated, it automatically responds with 401 Unauthorized.
func GetUserFromContext(c *gin.Context) (*models.User, bool) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return nil, false
	}
	userPtr, ok := user.(*models.User)
	if !ok {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "invalid user data in context"})
		return nil, false
	}
	return userPtr, true
}

// GetUserIDFromContext extracts the user ID from the Gin context.
// Returns the user ID and true if found, or empty string and false if not authenticated.
// If the user is not authenticated, it automatically responds with 401 Unauthorized.
func GetUserIDFromContext(c *gin.Context) (string, bool) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return "", false
	}
	userIDStr, ok := userID.(string)
	if !ok {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "invalid user ID in context"})
		return "", false
	}
	return userIDStr, true
}
