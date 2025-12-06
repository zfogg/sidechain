package handlers

import (
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

// CreateComment creates a new comment on a post
// POST /api/v1/posts/:id/comments
func (h *Handlers) CreateComment(c *gin.Context) {
	postID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var req struct {
		Content  string  `json:"content" binding:"required,min=1,max=2000"`
		ParentID *string `json:"parent_id,omitempty"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	// Verify the post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	// If replying to a comment, verify the parent exists and belongs to the same post
	if req.ParentID != nil && *req.ParentID != "" {
		var parentComment models.Comment
		if err := database.DB.First(&parentComment, "id = ? AND post_id = ?", *req.ParentID, postID).Error; err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "parent_comment_not_found"})
			return
		}
		// Only allow 1 level of nesting - if parent has a parent, use the parent's parent
		if parentComment.ParentID != nil {
			req.ParentID = parentComment.ParentID
		}
	}

	// Create the comment
	comment := models.Comment{
		PostID:   postID,
		UserID:   userID.(string),
		Content:  req.Content,
		ParentID: req.ParentID,
	}

	if err := database.DB.Create(&comment).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_create_comment", "message": err.Error()})
		return
	}

	// Increment post comment count
	database.DB.Model(&post).UpdateColumn("comment_count", gorm.Expr("comment_count + 1"))

	// Load the user for response
	database.DB.Preload("User").First(&comment, "id = ?", comment.ID)

	// Extract mentions and create notifications
	mentions := extractMentions(req.Content)
	go h.processMentions(comment.ID, mentions, userID.(string), postID)

	// Notify post owner (if not commenting on own post)
	if post.UserID != userID.(string) {
		go h.notifyCommentOnPost(comment, post)
	}

	c.JSON(http.StatusCreated, gin.H{
		"comment": comment,
	})
}

// GetComments retrieves comments for a post with pagination
// GET /api/v1/posts/:id/comments
func (h *Handlers) GetComments(c *gin.Context) {
	postID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))
	parentID := c.Query("parent_id") // Optional: get replies to specific comment

	if limit > 100 {
		limit = 100
	}

	// Verify the post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "post_not_found"})
		return
	}

	var comments []models.Comment
	query := database.DB.
		Preload("User").
		Where("post_id = ? AND is_deleted = false", postID).
		Order("created_at DESC").
		Limit(limit).
		Offset(offset)

	if parentID != "" {
		// Get replies to a specific comment
		query = query.Where("parent_id = ?", parentID)
	} else {
		// Get top-level comments only (no parent)
		query = query.Where("parent_id IS NULL")
	}

	if err := query.Find(&comments).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_comments", "message": err.Error()})
		return
	}

	// Get total count for pagination
	var total int64
	countQuery := database.DB.Model(&models.Comment{}).Where("post_id = ? AND is_deleted = false", postID)
	if parentID != "" {
		countQuery = countQuery.Where("parent_id = ?", parentID)
	} else {
		countQuery = countQuery.Where("parent_id IS NULL")
	}
	countQuery.Count(&total)

	// For top-level comments, also get reply counts
	if parentID == "" {
		for i := range comments {
			var replyCount int64
			database.DB.Model(&models.Comment{}).
				Where("parent_id = ? AND is_deleted = false", comments[i].ID).
				Count(&replyCount)
			// We can't add to struct, but we'll include in response below
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"comments": comments,
		"meta": gin.H{
			"total":  total,
			"limit":  limit,
			"offset": offset,
		},
	})
}

// GetCommentReplies retrieves replies to a specific comment
// GET /api/v1/comments/:id/replies
func (h *Handlers) GetCommentReplies(c *gin.Context) {
	commentID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	if limit > 100 {
		limit = 100
	}

	// Verify the comment exists
	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	var replies []models.Comment
	err := database.DB.
		Preload("User").
		Where("parent_id = ? AND is_deleted = false", commentID).
		Order("created_at ASC").
		Limit(limit).
		Offset(offset).
		Find(&replies).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_replies", "message": err.Error()})
		return
	}

	var total int64
	database.DB.Model(&models.Comment{}).Where("parent_id = ? AND is_deleted = false", commentID).Count(&total)

	c.JSON(http.StatusOK, gin.H{
		"replies": replies,
		"meta": gin.H{
			"total":  total,
			"limit":  limit,
			"offset": offset,
		},
	})
}

// UpdateComment updates a comment (only within 5 minutes of creation)
// PUT /api/v1/comments/:id
func (h *Handlers) UpdateComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var req struct {
		Content string `json:"content" binding:"required,min=1,max=2000"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	// Check ownership
	if comment.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "not_comment_owner"})
		return
	}

	// Check if comment was deleted
	if comment.IsDeleted {
		c.JSON(http.StatusBadRequest, gin.H{"error": "comment_deleted"})
		return
	}

	// Check 5-minute edit window
	editWindow := 5 * time.Minute
	if time.Since(comment.CreatedAt) > editWindow {
		c.JSON(http.StatusForbidden, gin.H{
			"error":   "edit_window_expired",
			"message": "Comments can only be edited within 5 minutes of creation",
		})
		return
	}

	// Update the comment
	now := time.Now()
	comment.Content = req.Content
	comment.IsEdited = true
	comment.EditedAt = &now

	if err := database.DB.Save(&comment).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_update_comment", "message": err.Error()})
		return
	}

	// Reload with user
	database.DB.Preload("User").First(&comment, "id = ?", comment.ID)

	c.JSON(http.StatusOK, gin.H{
		"comment": comment,
	})
}

// DeleteComment soft-deletes a comment
// DELETE /api/v1/comments/:id
func (h *Handlers) DeleteComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	// Check ownership
	if comment.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "not_comment_owner"})
		return
	}

	// Soft delete - mark as deleted but keep for threading
	comment.IsDeleted = true
	comment.Content = "[Comment deleted]"

	if err := database.DB.Save(&comment).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_delete_comment", "message": err.Error()})
		return
	}

	// Decrement post comment count
	database.DB.Model(&models.AudioPost{}).Where("id = ?", comment.PostID).UpdateColumn("comment_count", gorm.Expr("GREATEST(comment_count - 1, 0)"))

	c.JSON(http.StatusOK, gin.H{
		"message": "comment_deleted",
	})
}

// LikeComment adds a like to a comment
// POST /api/v1/comments/:id/like
func (h *Handlers) LikeComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	if comment.IsDeleted {
		c.JSON(http.StatusBadRequest, gin.H{"error": "cannot_like_deleted_comment"})
		return
	}

	// Add reaction via Stream.io if comment has a stream activity ID
	if h.stream != nil && comment.StreamActivityID != "" {
		err := h.stream.AddReactionWithEmoji("like", userID.(string), comment.StreamActivityID, "")
		if err != nil {
			// Log but don't fail - update local count anyway
			fmt.Printf("Failed to add Stream.io reaction: %v\n", err)
		}
	}

	// Increment like count
	database.DB.Model(&comment).UpdateColumn("like_count", gorm.Expr("like_count + 1"))

	c.JSON(http.StatusOK, gin.H{
		"message":    "comment_liked",
		"like_count": comment.LikeCount + 1,
	})
}

// UnlikeComment removes a like from a comment
// DELETE /api/v1/comments/:id/like
func (h *Handlers) UnlikeComment(c *gin.Context) {
	commentID := c.Param("id")
	_, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	// Decrement like count (don't go below 0)
	database.DB.Model(&comment).UpdateColumn("like_count", gorm.Expr("GREATEST(like_count - 1, 0)"))

	newCount := comment.LikeCount - 1
	if newCount < 0 {
		newCount = 0
	}

	c.JSON(http.StatusOK, gin.H{
		"message":    "comment_unliked",
		"like_count": newCount,
	})
}

// processMentions creates mention records and sends notifications
func (h *Handlers) processMentions(commentID string, usernames []string, authorID string, postID string) {
	if len(usernames) == 0 {
		return
	}

	for _, username := range usernames {
		var user models.User
		if err := database.DB.Where("LOWER(username) = ?", username).First(&user).Error; err != nil {
			continue // User doesn't exist, skip
		}

		// Don't notify yourself
		if user.ID == authorID {
			continue
		}

		// Create mention record
		mention := models.CommentMention{
			CommentID:       commentID,
			MentionedUserID: user.ID,
		}
		database.DB.Create(&mention)

		// Send notification via Stream.io
		if h.stream != nil {
			h.stream.NotifyMention(authorID, user.ID, postID, commentID)
		}

		// Mark notification as sent
		mention.NotificationSent = true
		database.DB.Save(&mention)
	}
}

// notifyCommentOnPost sends a notification to the post owner when someone comments
func (h *Handlers) notifyCommentOnPost(comment models.Comment, post models.AudioPost) {
	if h.stream == nil {
		return
	}

	// Get commenter info for the notification
	var commenter models.User
	database.DB.First(&commenter, "id = ?", comment.UserID)

	// NotifyComment(actorUserID, targetUserID, loopID, commentText)
	// Pass a preview of the comment content for the notification
	h.stream.NotifyComment(comment.UserID, post.UserID, post.ID, comment.Content)
}
