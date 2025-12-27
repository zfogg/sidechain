package handlers

import (
	"context"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
	"go.uber.org/zap"
)

// TrackPostClick records a user's click/view on a post
// POST /api/v1/posts/:id/click
func (h *Handlers) TrackPostClick(c *gin.Context) {
	postID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		// Allow anonymous tracking if user not authenticated
		userID = ""
	}

	var req struct {
		Source    string `json:"source,omitempty"`     // "feed", "search", "profile", "web", "plugin"
		SessionID string `json:"session_id,omitempty"` // Optional session identifier
	}

	// Parse optional request body
	if err := c.ShouldBindJSON(&req); err == nil {
		// Binding succeeded, use the values
	}

	// Verify the post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	// Create click record
	click := models.PostClick{
		PostID:    postID,
		UserID:    userID,
		Timestamp: time.Now().UTC(),
		Source:    req.Source,
		SessionID: req.SessionID,
	}

	if err := database.DB.Create(&click).Error; err != nil {
		logger.Warn("Error saving click record", zap.Error(err))
		// Don't fail the request - just log the error
	}

	// Optionally sync to Gorse as feedback if user is authenticated
	if userID != "" && h.gorse != nil {
		go func() {
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			defer cancel()
			if err := h.gorse.SyncFeedbackWithContext(ctx, userID, postID, "click"); err != nil {
				logger.Warn("Failed to sync click to Gorse", zap.Error(err))
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "click_recorded",
		"post_id": postID,
	})
}

// GetPostMetrics returns aggregate metrics for a post
// GET /api/v1/posts/:id/metrics
func (h *Handlers) GetPostMetrics(c *gin.Context) {
	postID := c.Param("id")

	// Verify the post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	// Count clicks for this post
	var clickCount int64
	if err := database.DB.Model(&models.PostClick{}).
		Where("post_id = ?", postID).
		Count(&clickCount).Error; err != nil {
		logger.Warn("Error counting clicks", zap.Error(err))
		clickCount = 0
	}

	c.JSON(http.StatusOK, gin.H{
		"post_id":       postID,
		"like_count":    post.LikeCount,
		"comment_count": post.CommentCount,
		"click_count":   int(clickCount),
		"timestamp":     post.UpdatedAt.UnixMilli(),
	})
}

// GetSearchMetrics returns search metrics
// GET /api/v1/metrics/search
func (h *Handlers) GetSearchMetrics(c *gin.Context) {
	stats := metrics.GetManager().GetSearchStats()

	c.JSON(http.StatusOK, gin.H{
		"data":      stats,
		"timestamp": stats["timestamp"],
	})
}

// GetAllMetrics returns all application metrics
// GET /api/v1/metrics
func (h *Handlers) GetAllMetrics(c *gin.Context) {
	allMetrics := metrics.GetManager().GetAllMetrics()

	c.JSON(http.StatusOK, gin.H{
		"metrics":   allMetrics,
		"timestamp": allMetrics["search"].(map[string]interface{})["timestamp"],
	})
}

// ResetMetrics resets all metrics (admin only)
// POST /api/v1/metrics/reset
func (h *Handlers) ResetMetrics(c *gin.Context) {
	// BUG FIX: Added admin check - this endpoint was accessible to any authenticated user
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	// Check if user is admin
	var user models.User
	if err := database.DB.Where("id = ?", userID).First(&user).Error; err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user_not_found"})
		return
	}

	if !user.IsAdmin {
		c.JSON(http.StatusForbidden, gin.H{"error": "admin_access_required"})
		return
	}

	metrics.GetManager().ResetAll()

	c.JSON(http.StatusOK, gin.H{
		"message": "Metrics reset successfully",
	})
}
