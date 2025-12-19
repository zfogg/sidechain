package middleware

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// RequireAdmin middleware ensures the request is authenticated and the user is an admin.
// It checks for a valid JWT token (which must be set by an earlier auth middleware)
// and verifies the user has admin privileges.
func RequireAdmin() gin.HandlerFunc {
	return func(c *gin.Context) {
		// Get user_id from context (set by auth middleware)
		userIDInterface, exists := c.Get("user_id")
		if !exists {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
			c.Abort()
			return
		}

		userID, ok := userIDInterface.(string)
		if !ok || userID == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "invalid_user_context"})
			c.Abort()
			return
		}

		// Fetch user from database to check admin status
		var user models.User
		if err := database.DB.Where("id = ?", userID).First(&user).Error; err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "user_not_found"})
			c.Abort()
			return
		}

		// Check if user is admin
		if !user.IsAdmin {
			c.JSON(http.StatusForbidden, gin.H{"error": "admin_access_required"})
			c.Abort()
			return
		}

		c.Next()
	}
}
