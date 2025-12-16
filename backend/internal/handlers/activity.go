package handlers

import (
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// GetFollowingActivityTimeline returns active stories from followed users
// GET /api/v1/activity/following
func (h *Handlers) GetFollowingActivityTimeline(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	// Get pagination params with defaults
	limit := 20
	if l := c.Query("limit"); l != "" {
		if parsed, err := strconv.Atoi(l); err == nil {
			limit = parsed
		}
	}

	offset := 0
	if o := c.Query("offset"); o != "" {
		if parsed, err := strconv.Atoi(o); err == nil {
			offset = parsed
		}
	}

	// Validate pagination params
	if limit <= 0 || limit > 100 {
		limit = 20
	}
	if offset < 0 {
		offset = 0
	}

	// Get list of followed user IDs from getstream.io
	var followedUserIDs []string
	if h.stream != nil {
		following, err := h.stream.GetFollowing(currentUser.ID, 100, 0)
		if err == nil {
			for _, f := range following {
				followedUserIDs = append(followedUserIDs, f.UserID)
			}
		}
	}

	// Include current user's own stories
	followedUserIDs = append(followedUserIDs, currentUser.ID)

	// Query active stories (not expired) from followed users, sorted by newest first
	var stories []models.Story
	query := database.DB.Where("user_id IN ? AND expires_at > ?", followedUserIDs, time.Now().UTC()).
		Preload("User").
		Order("created_at DESC").
		Limit(limit).
		Offset(offset)

	if err := query.Find(&stories).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch activity timeline",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"stories": stories,
		"count":   len(stories),
		"limit":   limit,
		"offset":  offset,
	})
}

// GetGlobalActivityTimeline returns active stories from all users
// GET /api/v1/activity/global
func (h *Handlers) GetGlobalActivityTimeline(c *gin.Context) {
	// Get pagination params with defaults
	limit := 20
	if l := c.Query("limit"); l != "" {
		if parsed, err := strconv.Atoi(l); err == nil {
			limit = parsed
		}
	}

	offset := 0
	if o := c.Query("offset"); o != "" {
		if parsed, err := strconv.Atoi(o); err == nil {
			offset = parsed
		}
	}

	// Validate pagination params
	if limit <= 0 || limit > 100 {
		limit = 20
	}
	if offset < 0 {
		offset = 0
	}

	// Query all active stories (not expired), sorted by newest first
	var stories []models.Story
	query := database.DB.Where("expires_at > ?", time.Now().UTC()).
		Preload("User").
		Order("created_at DESC").
		Limit(limit).
		Offset(offset)

	if err := query.Find(&stories).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch global activity timeline",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"stories": stories,
		"count":   len(stories),
		"limit":   limit,
		"offset":  offset,
	})
}
