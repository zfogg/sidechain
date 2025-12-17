package handlers

import (
	"fmt"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
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
		fmt.Printf("Error saving click record: %v\n", err)
		// Don't fail the request - just log the error
	}

	// Optionally sync to Gorse as feedback if user is authenticated
	if userID != "" && h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFeedback(userID, postID, "click"); err != nil {
				fmt.Printf("Warning: Failed to sync click to Gorse: %v\n", err)
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
		fmt.Printf("Error counting clicks: %v\n", err)
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

// GetSearchMetrics returns search metrics (Phase 7.1)
// GET /api/v1/metrics/search
func (h *Handlers) GetSearchMetrics(c *gin.Context) {
	stats := metrics.GetManager().GetSearchStats()

	c.JSON(http.StatusOK, gin.H{
		"data":      stats,
		"timestamp": stats["timestamp"],
	})
}

// GetAllMetrics returns all application metrics (Phase 7.1)
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
	// TODO: Add admin check here
	metrics.GetManager().ResetAll()

	c.JSON(http.StatusOK, gin.H{
		"message": "Metrics reset successfully",
	})
}
