package middleware

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"go.uber.org/zap"
)

// AdminImpersonationMiddleware checks if an admin is trying to impersonate another user
// If X-Impersonate-User header is provided, validates that:
// 1. The authenticated user is an admin
// 2. The impersonated user exists
// 3. Sets the impersonated user context
func AdminImpersonationMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		impersonateEmail := c.GetHeader("X-Impersonate-User")

		// If no impersonation header, continue normally
		if impersonateEmail == "" {
			c.Next()
			return
		}

		// Get authenticated user from context (set by auth middleware)
		authenticatedUserID, exists := c.Get("user_id")
		if !exists {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
			c.Abort()
			return
		}

		// Fetch authenticated user to check if they're admin
		var authenticatedUser models.User
		if err := database.DB.Where("id = ?", authenticatedUserID).First(&authenticatedUser).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch user"})
			c.Abort()
			return
		}

		// Check if authenticated user is admin
		if !authenticatedUser.IsAdmin {
			c.JSON(http.StatusForbidden, gin.H{"error": "Only admin users can impersonate other users"})
			c.Abort()
			return
		}

		// Fetch the impersonated user
		var impersonatedUser models.User
		if err := database.DB.Where("email = ?", impersonateEmail).First(&impersonatedUser).Error; err != nil {
			c.JSON(http.StatusNotFound, gin.H{"error": "Impersonated user not found"})
			c.Abort()
			return
		}

		// Replace user context with impersonated user
		c.Set("user_id", impersonatedUser.ID)
		c.Set("email", impersonatedUser.Email)
		c.Set("username", impersonatedUser.Username)
		c.Set("stream_user_id", impersonatedUser.StreamUserID)
		c.Set("is_admin", impersonatedUser.IsAdmin)

		// Log impersonation for audit trail
		logger.Log.Info("Admin impersonation initiated",
			zap.String("admin_id", authenticatedUserID.(string)),
			zap.String("admin_email", authenticatedUser.Email),
			zap.String("impersonated_user_id", impersonatedUser.ID),
			zap.String("impersonated_user_email", impersonatedUser.Email),
			zap.String("request_method", c.Request.Method),
			zap.String("request_path", c.Request.URL.Path),
		)

		c.Next()
	}
}
