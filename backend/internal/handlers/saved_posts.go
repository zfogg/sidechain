package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
	"gorm.io/gorm"
)

// SavePost saves/bookmarks a post for the current user
// POST /api/v1/posts/:id/save
func (h *Handlers) SavePost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Check if post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to find post"})
		return
	}

	// Check if already saved
	var existingSave models.SavedPost
	err := database.DB.Where("user_id = ? AND post_id = ?", userID, postID).First(&existingSave).Error
	if err == nil {
		// Already saved
		c.JSON(http.StatusOK, gin.H{
			"message": "Post already saved",
			"saved":   true,
		})
		return
	}

	// Create saved post record
	savedPost := models.SavedPost{
		UserID: userID,
		PostID: postID,
	}

	if err := database.DB.Create(&savedPost).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save post"})
		return
	}

	if err := database.DB.Model(&post).UpdateColumn("save_count", gorm.Expr("save_count + 1")).Error; err != nil {
		logger.WarnWithFields("Failed to increment save count for post "+postID, err)
	}

	c.JSON(http.StatusOK, gin.H{
		"message":  "Post saved successfully",
		"saved":    true,
		"saved_at": savedPost.CreatedAt,
	})
}

// UnsavePost removes a saved/bookmarked post for the current user
// DELETE /api/v1/posts/:id/save
func (h *Handlers) UnsavePost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Find and delete the saved post record
	result := database.DB.Where("user_id = ? AND post_id = ?", userID, postID).Delete(&models.SavedPost{})
	if result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to unsave post"})
		return
	}

	if result.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "Post was not saved"})
		return
	}

	// Decrement save count on the post
	if err := database.DB.Model(&models.AudioPost{}).Where("id = ?", postID).UpdateColumn("save_count", gorm.Expr("GREATEST(save_count - 1, 0)")).Error; err != nil {
		logger.WarnWithFields("Failed to decrement save count for post "+postID, err)
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Post unsaved successfully",
		"saved":   false,
	})
}

// GetSavedPosts returns the current user's saved posts
// GET /api/v1/users/me/saved
func (h *Handlers) GetSavedPosts(c *gin.Context) {
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

	// Get saved posts with post details
	var savedPosts []models.SavedPost
	err := database.DB.
		Preload("Post").
		Preload("Post.User").
		Where("user_id = ?", userID).
		Order("created_at DESC").
		Limit(limit).
		Offset(offset).
		Find(&savedPosts).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get saved posts"})
		return
	}

	// Get total count
	var totalCount int64
	if err := database.DB.Model(&models.SavedPost{}).Where("user_id = ?", userID).Count(&totalCount).Error; err != nil {
		logger.WarnWithFields("Failed to count saved posts for user "+userID, err)
		totalCount = int64(len(savedPosts))
	}

	// Transform to response format
	posts := make([]gin.H, len(savedPosts))
	for i, sp := range savedPosts {
		posts[i] = gin.H{
			"id":         sp.Post.ID,
			"user_id":    sp.Post.UserID,
			"user":       formatUserResponse(&sp.Post.User),
			"audio_url":  sp.Post.AudioURL,
			"duration":   sp.Post.Duration,
			"bpm":        sp.Post.BPM,
			"key":        sp.Post.Key,
			"genre":      sp.Post.Genre,
			"like_count": sp.Post.LikeCount,
			"play_count": sp.Post.PlayCount,
			"save_count": sp.Post.SaveCount,
			"created_at": sp.Post.CreatedAt,
			"saved_at":   sp.CreatedAt,
			"is_saved":   true,
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"posts":       posts,
		"total_count": totalCount,
		"limit":       limit,
		"offset":      offset,
		"has_more":    offset+len(posts) < int(totalCount),
	})
}

// IsPostSaved checks if the current user has saved a specific post
// GET /api/v1/posts/:id/saved
func (h *Handlers) IsPostSaved(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	var savedPost models.SavedPost
	err := database.DB.Where("user_id = ? AND post_id = ?", userID, postID).First(&savedPost).Error

	c.JSON(http.StatusOK, gin.H{
		"saved":    err == nil,
		"saved_at": savedPost.CreatedAt,
	})
}

// Helper to format user for response
func formatUserResponse(user *models.User) gin.H {
	if user == nil {
		return nil
	}
	return gin.H{
		"id":           user.ID,
		"username":     user.Username,
		"display_name": user.DisplayName,
		"avatar_url":   user.GetAvatarURL(),
	}
}
