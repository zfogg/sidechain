package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
	"gorm.io/gorm"
)

// ArchivePost archives a post (hides from feeds without deleting)
// POST /api/v1/posts/:id/archive
func (h *Handlers) ArchivePost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Find the post and verify ownership
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to find post"})
		return
	}

	// Check ownership - only post owner can archive
	if post.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "You can only archive your own posts"})
		return
	}

	// Check if already archived
	if post.IsArchived {
		c.JSON(http.StatusOK, gin.H{
			"message":     "Post already archived",
			"is_archived": true,
		})
		return
	}

	// Archive the post
	if err := database.DB.Model(&post).Update("is_archived", true).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to archive post"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":     "Post archived successfully",
		"is_archived": true,
	})
}

// UnarchivePost unarchives a post (makes it visible in feeds again)
// POST /api/v1/posts/:id/unarchive
func (h *Handlers) UnarchivePost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Find the post and verify ownership
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to find post"})
		return
	}

	// Check ownership - only post owner can unarchive
	if post.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "You can only unarchive your own posts"})
		return
	}

	// Check if already not archived
	if !post.IsArchived {
		c.JSON(http.StatusOK, gin.H{
			"message":     "Post is not archived",
			"is_archived": false,
		})
		return
	}

	// Unarchive the post
	if err := database.DB.Model(&post).Update("is_archived", false).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to unarchive post"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":     "Post unarchived successfully",
		"is_archived": false,
	})
}

// GetArchivedPosts returns the current user's archived posts
// GET /api/v1/users/me/archived
func (h *Handlers) GetArchivedPosts(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	// Parse pagination
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	if limit > 50 {
		limit = 50
	}

	// Get archived posts with user details
	var posts []models.AudioPost
	err := database.DB.
		Preload("User").
		Where("user_id = ? AND is_archived = ?", userID, true).
		Order("created_at DESC").
		Limit(limit).
		Offset(offset).
		Find(&posts).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get archived posts"})
		return
	}

	// Get total count
	var totalCount int64
	database.DB.Model(&models.AudioPost{}).
		Where("user_id = ? AND is_archived = ?", userID, true).
		Count(&totalCount)

	// Transform to response format
	postsResponse := make([]gin.H, len(posts))
	for i, post := range posts {
		postsResponse[i] = gin.H{
			"id":           post.ID,
			"user_id":      post.UserID,
			"user":         formatUserResponse(&post.User),
			"audio_url":    post.AudioURL,
			"duration":     post.Duration,
			"bpm":          post.BPM,
			"key":          post.Key,
			"genre":        post.Genre,
			"like_count":   post.LikeCount,
			"play_count":   post.PlayCount,
			"save_count":   post.SaveCount,
			"repost_count": post.RepostCount,
			"created_at":   post.CreatedAt,
			"is_archived":  true,
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"posts":       postsResponse,
		"total_count": totalCount,
		"limit":       limit,
		"offset":      offset,
		"has_more":    offset+len(posts) < int(totalCount),
	})
}

// IsPostArchived checks if the current user's post is archived
// GET /api/v1/posts/:id/archived
func (h *Handlers) IsPostArchived(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	var post models.AudioPost
	err := database.DB.Where("id = ? AND user_id = ?", postID, userID).First(&post).Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to find post"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"is_archived": post.IsArchived,
	})
}
