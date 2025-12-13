package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

const MaxPinnedPosts = 3

// PinPost pins a post to the user's profile
// POST /api/v1/posts/:id/pin
func (h *Handlers) PinPost(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Get the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch post"})
		return
	}

	// Verify ownership
	if post.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "You can only pin your own posts"})
		return
	}

	// Check if already pinned
	if post.IsPinned {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post is already pinned"})
		return
	}

	// Count current pinned posts
	var pinnedCount int64
	if err := database.DB.Model(&models.AudioPost{}).
		Where("user_id = ? AND is_pinned = ?", userID, true).
		Count(&pinnedCount).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to count pinned posts"})
		return
	}

	if pinnedCount >= MaxPinnedPosts {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "Maximum pinned posts reached",
			"message": "You can only pin up to 3 posts. Unpin a post first.",
			"max":     MaxPinnedPosts,
		})
		return
	}

	// Pin the post with the next available order
	post.IsPinned = true
	post.PinOrder = int(pinnedCount) + 1

	if err := database.DB.Save(&post).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to pin post"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":   "Post pinned successfully",
		"is_pinned": true,
		"pin_order": post.PinOrder,
	})
}

// UnpinPost unpins a post from the user's profile
// DELETE /api/v1/posts/:id/pin
func (h *Handlers) UnpinPost(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Get the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch post"})
		return
	}

	// Verify ownership
	if post.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "You can only unpin your own posts"})
		return
	}

	// Check if pinned
	if !post.IsPinned {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post is not pinned"})
		return
	}

	oldOrder := post.PinOrder

	// Unpin the post
	post.IsPinned = false
	post.PinOrder = 0

	if err := database.DB.Save(&post).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to unpin post"})
		return
	}

	// Reorder remaining pinned posts to fill the gap
	if err := database.DB.Model(&models.AudioPost{}).
		Where("user_id = ? AND is_pinned = ? AND pin_order > ?", userID, true, oldOrder).
		UpdateColumn("pin_order", gorm.Expr("pin_order - 1")).Error; err != nil {
		// Log but don't fail - the unpin was successful
		// The order will be slightly off but functional
	}

	c.JSON(http.StatusOK, gin.H{
		"message":   "Post unpinned successfully",
		"is_pinned": false,
	})
}

// UpdatePinOrder updates the order of pinned posts
// PUT /api/v1/posts/:id/pin-order
func (h *Handlers) UpdatePinOrder(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	var request struct {
		Order int `json:"order" binding:"required,min=1,max=3"`
	}
	if err := c.ShouldBindJSON(&request); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid order. Must be 1, 2, or 3"})
		return
	}

	// Get the post
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch post"})
		return
	}

	// Verify ownership
	if post.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "You can only reorder your own posts"})
		return
	}

	// Check if pinned
	if !post.IsPinned {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post is not pinned"})
		return
	}

	oldOrder := post.PinOrder
	newOrder := request.Order

	if oldOrder == newOrder {
		c.JSON(http.StatusOK, gin.H{
			"message":   "Pin order unchanged",
			"pin_order": newOrder,
		})
		return
	}

	// Start transaction for reordering
	tx := database.DB.Begin()

	// Shift other posts to make room
	if newOrder < oldOrder {
		// Moving up: shift posts between new and old position down
		if err := tx.Model(&models.AudioPost{}).
			Where("user_id = ? AND is_pinned = ? AND pin_order >= ? AND pin_order < ?",
				userID, true, newOrder, oldOrder).
			UpdateColumn("pin_order", gorm.Expr("pin_order + 1")).Error; err != nil {
			tx.Rollback()
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to reorder posts"})
			return
		}
	} else {
		// Moving down: shift posts between old and new position up
		if err := tx.Model(&models.AudioPost{}).
			Where("user_id = ? AND is_pinned = ? AND pin_order > ? AND pin_order <= ?",
				userID, true, oldOrder, newOrder).
			UpdateColumn("pin_order", gorm.Expr("pin_order - 1")).Error; err != nil {
			tx.Rollback()
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to reorder posts"})
			return
		}
	}

	// Update the target post's order
	post.PinOrder = newOrder
	if err := tx.Save(&post).Error; err != nil {
		tx.Rollback()
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update pin order"})
		return
	}

	tx.Commit()

	c.JSON(http.StatusOK, gin.H{
		"message":   "Pin order updated",
		"pin_order": newOrder,
	})
}

// GetPinnedPosts returns the user's pinned posts
// GET /api/v1/users/:id/pinned
func (h *Handlers) GetPinnedPosts(c *gin.Context) {
	userID := c.Param("id")
	if userID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "User ID is required"})
		return
	}

	// Get pinned posts ordered by pin_order
	var posts []models.AudioPost
	if err := database.DB.Preload("User").
		Where("user_id = ? AND is_pinned = ? AND is_archived = ?", userID, true, false).
		Order("pin_order ASC").
		Find(&posts).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch pinned posts"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"pinned_posts": posts,
		"count":        len(posts),
	})
}

// IsPostPinned checks if a specific post is pinned
// GET /api/v1/posts/:id/pinned
func (h *Handlers) IsPostPinned(c *gin.Context) {
	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	var post models.AudioPost
	if err := database.DB.Select("id", "is_pinned", "pin_order").
		First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch post"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"is_pinned": post.IsPinned,
		"pin_order": post.PinOrder,
	})
}
