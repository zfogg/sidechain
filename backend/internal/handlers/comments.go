package handlers

import (
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
	"github.com/zfogg/sidechain/backend/internal/websocket"
	"gorm.io/gorm"
)

// CreateComment creates a new comment on a post
// POST /api/v1/posts/:id/comments
func (h *Handlers) CreateComment(c *gin.Context) {
	postID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		Content  string  `json:"content" binding:"required,min=1,max=2000"`
		ParentID *string `json:"parent_id,omitempty"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, err.Error())
		return
	}

	// Verify the post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		util.RespondNotFound(c, "post")
		return
	}

	// Check comment audience restrictions
	// Post owner can always comment on their own posts
	if post.UserID != userID {
		switch post.CommentAudience {
		case models.CommentAudienceOff:
			util.RespondForbidden(c, "Comments are disabled for this post")
			return
		case models.CommentAudienceFollowers:
			// Check if the commenter follows the post owner
			if h.stream != nil {
				isFollowing, err := h.stream.CheckIsFollowing(userID, post.UserID)
				if err != nil || !isFollowing {
					util.RespondForbidden(c, "Only followers can comment on this post")
					return
				}
			}
		}
		// CommentAudienceEveryone allows all users to comment
	}

	// If replying to a comment, verify the parent exists and belongs to the same post
	if req.ParentID != nil && *req.ParentID != "" {
		var parentComment models.Comment
		if err := database.DB.First(&parentComment, "id = ? AND post_id = ?", *req.ParentID, postID).Error; err != nil {
			util.RespondValidationError(c, "parent_id", "Parent comment not found")
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
		UserID:   userID,
		Content:  req.Content,
		ParentID: req.ParentID,
	}

	if err := database.DB.Create(&comment).Error; err != nil {
		util.RespondInternalError(c, "Failed to create comment")
		return
	}

	if err := database.DB.Model(&post).UpdateColumn("comment_count", gorm.Expr("comment_count + 1")).Error; err != nil {
		logger.WarnWithFields("Failed to increment comment count for post "+postID, err)
	}

	// Sync post engagement metrics to Elasticsearch
	if h.search != nil {
		go func() {
			var updatedPost models.AudioPost
			if err := database.DB.Select("like_count", "play_count", "comment_count").First(&updatedPost, "id = ?", postID).Error; err == nil {
				if err := h.search.UpdatePostEngagement(c.Request.Context(), postID, updatedPost.LikeCount, updatedPost.PlayCount, updatedPost.CommentCount); err != nil {
					logger.WarnWithFields("Failed to update post engagement in search index", err)
				}
			}
		}()
	}

	// Load the user for response
	if err := database.DB.Preload("User").First(&comment, "id = ?", comment.ID).Error; err != nil {
		logger.WarnWithFields("Failed to load comment with user for post "+postID, err)
	}

	// Extract mentions and create notifications
	mentions := util.ExtractMentions(req.Content)
	if err := h.processMentions(comment.ID, mentions, userID, postID); err != nil {
		logger.ErrorWithFields("Failed to process mentions", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to send mention notifications"})
		return
	}

	// Notify post owner (if not commenting on own post)
	if post.UserID != userID {
		if err := h.notifyCommentOnPost(comment, post); err != nil {
			logger.ErrorWithFields("Failed to notify post owner of comment", err)
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to send notification"})
			return
		}

		// Send WebSocket notification to post owner and broadcast metrics
		if h.wsHandler != nil {
			go func() {
				var commenter models.User
				if err := database.DB.First(&commenter, "id = ?", userID).Error; err == nil {
					avatarURL := commenter.ProfilePictureURL
					if avatarURL == "" {
						avatarURL = commenter.OAuthProfilePictureURL
					}
					payload := &websocket.CommentPayload{
						CommentID:   comment.ID,
						PostID:      postID,
						UserID:      userID,
						Username:    commenter.Username,
						DisplayName: commenter.DisplayName,
						AvatarURL:   avatarURL,
						Body:        req.Content,
						CreatedAt:   comment.CreatedAt.UnixMilli(),
					}
					h.wsHandler.NotifyNewComment(post.UserID, payload)

					// Broadcast updated comment count to all viewers
					h.wsHandler.BroadcastCommentCountUpdate(postID, post.CommentCount+1)
				}
			}()
		}
	}

	c.JSON(http.StatusCreated, gin.H{
		"comment": comment,
	})
}

// GetComments retrieves comments for a post with pagination
// GET /api/v1/posts/:id/comments
func (h *Handlers) GetComments(c *gin.Context) {
	postID := c.Param("id")
	limit, err := strconv.Atoi(c.DefaultQuery("limit", "20"))
	if err != nil || limit <= 0 {
		limit = 20
	}
	offset, err := strconv.Atoi(c.DefaultQuery("offset", "0"))
	if err != nil || offset < 0 {
		offset = 0
	}
	parentID := c.Query("parent_id") // Optional: get replies to specific comment

	if limit > 100 {
		limit = 100
	}

	// Verify the post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", postID).Error; err != nil {
		util.RespondNotFound(c, "post")
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
		util.RespondInternalError(c, "Failed to get comments")
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
	if err := countQuery.Count(&total).Error; err != nil {
		logger.WarnWithFields("Failed to count comments for post "+postID, err)
	}

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
	limit, err := strconv.Atoi(c.DefaultQuery("limit", "20"))
	if err != nil || limit <= 0 {
		limit = 20
	}
	offset, err := strconv.Atoi(c.DefaultQuery("offset", "0"))
	if err != nil || offset < 0 {
		offset = 0
	}

	if limit > 100 {
		limit = 100
	}

	// Verify the comment exists
	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		util.RespondNotFound(c, "comment")
		return
	}

	var replies []models.Comment
	err = database.DB.
		Preload("User").
		Where("parent_id = ? AND is_deleted = false", commentID).
		Order("created_at ASC").
		Limit(limit).
		Offset(offset).
		Find(&replies).Error

	if err != nil {
		util.RespondInternalError(c, "Failed to get replies")
		return
	}

	var total int64
	if err := database.DB.Model(&models.Comment{}).Where("parent_id = ? AND is_deleted = false", commentID).Count(&total).Error; err != nil {
		logger.WarnWithFields("Failed to count replies for comment "+commentID, err)
	}

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
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		Content string `json:"content" binding:"required,min=1,max=2000"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, err.Error())
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		util.RespondNotFound(c, "comment")
		return
	}

	// Check ownership
	if comment.UserID != userID {
		util.RespondForbidden(c, "You do not own this comment")
		return
	}

	// Check if comment was deleted
	if comment.IsDeleted {
		util.RespondValidationError(c, "comment", "Comment has been deleted")
		return
	}

	// Check 5-minute edit window
	editWindow := 5 * time.Minute
	if time.Since(comment.CreatedAt) > editWindow {
		util.RespondForbidden(c, "Comments can only be edited within 5 minutes of creation")
		return
	}

	// Update the comment
	now := time.Now()
	comment.Content = req.Content
	comment.IsEdited = true
	comment.EditedAt = &now

	if err := database.DB.Save(&comment).Error; err != nil {
		util.RespondInternalError(c, "Failed to update comment")
		return
	}

	// Reload with user
	if err := database.DB.Preload("User").First(&comment, "id = ?", comment.ID).Error; err != nil {
		logger.WarnWithFields("Failed to reload comment with user for ID "+comment.ID, err)
	}

	c.JSON(http.StatusOK, gin.H{
		"comment": comment,
	})
}

// DeleteComment soft-deletes a comment
// DELETE /api/v1/comments/:id
func (h *Handlers) DeleteComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		util.RespondNotFound(c, "comment")
		return
	}

	// Check ownership
	if comment.UserID != userID {
		util.RespondForbidden(c, "You do not own this comment")
		return
	}

	// Soft delete - mark as deleted but keep for threading
	comment.IsDeleted = true
	comment.Content = "[Comment deleted]"

	if err := database.DB.Save(&comment).Error; err != nil {
		util.RespondInternalError(c, "Failed to delete comment")
		return
	}

	// Decrement post comment count
	if err := database.DB.Model(&models.AudioPost{}).Where("id = ?", comment.PostID).UpdateColumn("comment_count", gorm.Expr("GREATEST(comment_count - 1, 0)")).Error; err != nil {
		logger.WarnWithFields("Failed to decrement comment count for post "+comment.PostID, err)
	}

	// Sync post engagement metrics to Elasticsearch
	if h.search != nil {
		go func() {
			var post models.AudioPost
			if err := database.DB.Select("like_count", "play_count", "comment_count").First(&post, "id = ?", comment.PostID).Error; err == nil {
				if err := h.search.UpdatePostEngagement(c.Request.Context(), post.ID, post.LikeCount, post.PlayCount, post.CommentCount); err != nil {
					logger.WarnWithFields("Failed to update post engagement in search index", err)
				}
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "comment_deleted",
	})
}

// LikeComment adds a like to a comment
// POST /api/v1/comments/:id/like
func (h *Handlers) LikeComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		util.RespondNotFound(c, "comment")
		return
	}

	if comment.IsDeleted {
		util.RespondValidationError(c, "comment", "Cannot like a deleted comment")
		return
	}

	// Add reaction via Stream.io if comment has a stream activity ID
	if h.stream != nil && comment.StreamActivityID != "" {
		err := h.stream.AddReactionWithEmoji("like", userID, comment.StreamActivityID, "")
		if err != nil {
			// Log but don't fail - update local count anyway
			logger.WarnWithFields("Failed to add Stream.io reaction", err)
		}
	}

	if err := database.DB.Model(&comment).UpdateColumn("like_count", gorm.Expr("like_count + 1")).Error; err != nil {
		logger.WarnWithFields("Failed to increment comment like count for comment "+commentID, err)
	}

	// Send WebSocket notification to comment owner
	if h.wsHandler != nil {
		go func() {
			var liker models.User
			var post models.AudioPost
			if err := database.DB.First(&liker, "id = ?", userID).Error; err == nil {
				if err := database.DB.First(&post, "id = ?", comment.PostID).Error; err == nil {
					payload := &websocket.CommentLikePayload{
						CommentID: commentID,
						PostID:    comment.PostID,
						UserID:    userID,
						Username:  liker.Username,
						LikeCount: comment.LikeCount + 1,
					}
					h.wsHandler.NotifyCommentLike(comment.UserID, payload)
				}
			}
		}()
	}

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

	if err := database.DB.Model(&comment).UpdateColumn("like_count", gorm.Expr("GREATEST(like_count - 1, 0)")).Error; err != nil {
		logger.WarnWithFields("Failed to decrement comment like count for comment "+commentID, err)
	}

	newCount := comment.LikeCount - 1
	if newCount < 0 {
		newCount = 0
	}

	// Send WebSocket notification to comment owner
	userID, ok := util.GetUserIDFromContext(c)
	if ok && h.wsHandler != nil {
		go func() {
			var unliker models.User
			if err := database.DB.First(&unliker, "id = ?", userID).Error; err == nil {
				payload := &websocket.CommentLikePayload{
					CommentID: commentID,
					PostID:    comment.PostID,
					UserID:    userID,
					Username:  unliker.Username,
					LikeCount: newCount,
				}
				h.wsHandler.NotifyCommentUnlike(comment.UserID, payload)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"message":    "comment_unliked",
		"like_count": newCount,
	})
}

// processMentions creates mention records and sends notifications
func (h *Handlers) processMentions(commentID string, usernames []string, authorID string, postID string) error {
	if len(usernames) == 0 {
		return nil
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
		if err := database.DB.Create(&mention).Error; err != nil {
			logger.WarnWithFields("Failed to create mention record", err)
			continue
		}

		// Send notification via Stream.io
		if h.stream != nil {
			if err := h.stream.NotifyMention(authorID, user.ID, postID, commentID); err != nil {
				return fmt.Errorf("failed to send mention notification: %w", err)
			}
		}

		// Mark notification as sent
		mention.NotificationSent = true
		if err := database.DB.Save(&mention).Error; err != nil {
			logger.WarnWithFields("Failed to update mention notification status", err)
		}
	}

	return nil
}

// notifyCommentOnPost sends a notification to the post owner when someone comments
func (h *Handlers) notifyCommentOnPost(comment models.Comment, post models.AudioPost) error {
	if h.stream == nil {
		return nil
	}

	// Get commenter info for the notification
	var commenter models.User
	if err := database.DB.First(&commenter, "id = ?", comment.UserID).Error; err != nil {
		return fmt.Errorf("failed to fetch commenter for notification: %w", err)
	}

	// NotifyComment(actorUserID, targetUserID, loopID, commentText)
	// Pass a preview of the comment content for the notification
	if err := h.stream.NotifyComment(comment.UserID, post.UserID, post.ID, comment.Content); err != nil {
		return fmt.Errorf("failed to send comment notification: %w", err)
	}

	return nil
}

// ReportComment reports a comment for moderation
// POST /api/v1/comments/:id/report
func (h *Handlers) ReportComment(c *gin.Context) {
	commentID := c.Param("id")
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		Reason      string `json:"reason" binding:"required"`
		Description string `json:"description,omitempty"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "message": err.Error()})
		return
	}

	// Validate reason
	validReasons := []string{"spam", "harassment", "inappropriate", "copyright", "violence", "other"}
	valid := false
	for _, r := range validReasons {
		if req.Reason == r {
			valid = true
			break
		}
	}
	if !valid {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_reason", "message": "Reason must be one of: spam, harassment, inappropriate, copyright, violence, other"})
		return
	}

	// Find the comment
	var comment models.Comment
	if err := database.DB.First(&comment, "id = ?", commentID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "comment_not_found"})
		return
	}

	// Create report
	report := models.Report{
		ReporterID:   userID,
		TargetType:   models.ReportTargetComment,
		TargetID:     commentID,
		TargetUserID: &comment.UserID,
		Reason:       models.ReportReason(req.Reason),
		Description:  req.Description,
		Status:       models.ReportStatusPending,
	}

	if err := database.DB.Create(&report).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_create_report", "message": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message":   "report_created",
		"report_id": report.ID,
	})
}

// ============================================================================
// COMMENTS - PROFESSIONAL ENHANCEMENTS
// ============================================================================

// NOTE: Common enhancements (caching, analytics, rate limiting, moderation,
// search, webhooks, export, performance, anti-abuse, notifications,
// accessibility, localization) are documented in common_todos.go.
// See that file for shared TODO items.

// TODO: PROFESSIONAL-5.1 - Implement comment threading improvements
// - Support unlimited nesting levels (currently only 1 level)
// - Visual thread indentation in responses
// - Collapse/expand comment threads
// - Thread depth indicators
// - "Show more replies" functionality

// TODO: PROFESSIONAL-5.2 - Add comment reactions
// - React to comments with emojis (already partially implemented via LikeComment)
// - Multiple reaction types per comment
// - Reaction counts per emoji type
// - Reaction aggregation and display
// - Real-time reaction updates

// TODO: PROFESSIONAL-5.3 - Implement comment moderation tools
// - Comment content filtering (profanity detection)
// - Auto-hide comments with low scores (if voting implemented)
// - Manual moderation queue for flagged comments
// - Bulk moderation actions
// - Comment approval workflow (for sensitive posts)

// TODO: PROFESSIONAL-5.4 - Add comment editing enhancements
// - Show edit history (what changed, when)
// - Edit timestamp indicator ("edited 5 minutes ago")
// - Undo edit functionality
// - Edit reason/notes (optional)
// - Prevent edit abuse (limit edit frequency)

// TODO: PROFESSIONAL-5.5 - Implement comment sorting options
// - Sort by newest (default)
// - Sort by oldest
// - Sort by most liked/reactions
// - Sort by most replies
// - Sort by controversial (mix of likes/dislikes)
// - User preference for default sort

// TODO: PROFESSIONAL-5.6 - Add comment-specific search functionality
// - Search comments by content (full-text search)
// - Search comments by author
// - Search comments within a specific post
// - See common_todos.go for general search infrastructure

// TODO: PROFESSIONAL-5.7 - Implement comment-specific notifications
// - Notify on replies to your comments
// - Notify on reactions to your comments
// - Batch notifications (e.g., "5 people replied to your comment")
// - See common_todos.go for general notification infrastructure

// TODO: PROFESSIONAL-5.8 - Add comment-specific analytics
// - Track comment engagement (replies, reactions)
// - Track comment velocity (comments per hour after post)
// - Track top commenters per post
// - See common_todos.go for general analytics infrastructure

// TODO: PROFESSIONAL-5.9 - Implement comment voting system
// - Upvote/downvote comments (beyond just likes)
// - Comment score calculation
// - Sort by score
// - Hide low-score comments
// - Voting reputation system

// TODO: PROFESSIONAL-5.10 - Add comment mentions and tags
// - @mention users in comments (already partially implemented)
// - Tag users in comment replies
// - Notification when mentioned
// - User mention autocomplete
// - Mention validation (user exists, accessible)

// TODO: PROFESSIONAL-5.11 - Implement comment formatting
// - Support markdown formatting
// - Support code blocks (for sharing code snippets)
// - Support links (with preview)
// - Support emoji in comments
// - Rich text formatting options

// TODO: PROFESSIONAL-5.12 - Add comment attachments
// - Attach audio snippets in comments
// - Attach images in comments
// - Attach MIDI files in comments
// - Attachment size limits
// - Attachment moderation

// TODO: PROFESSIONAL-5.13 - Implement comment pinning
// - Pin comments to top of thread (post owner)
// - Pinned comment indicator
// - Multiple pinned comments support
// - Pin/unpin functionality
// - Pinned comment analytics

// TODO: PROFESSIONAL-5.14 - Add comment-specific reporting improvements
// - Multiple report reasons (already implemented)
// - Report comment with context
// - Report comment author
// - See common_todos.go for general moderation infrastructure

// TODO: PROFESSIONAL-5.15 - Implement comment-specific rate limiting
// - Limit comments per minute per user
// - Limit comments per post per user
// - See common_todos.go for general rate limiting infrastructure

// TODO: PROFESSIONAL-5.16 - Add comment-specific caching
// - Cache comment counts per post
// - Cache recent comments for popular posts
// - See common_todos.go for general caching infrastructure

// TODO: PROFESSIONAL-5.17 - Implement comment archiving
// - Archive old comments (older than X months)
// - Archive low-engagement comments
// - Access archived comments
// - Archive analytics
// - Auto-archive policy

// TODO: PROFESSIONAL-5.18 - Add comment moderation queue
// - Queue comments awaiting moderation
// - Admin/moderation dashboard
// - Bulk approve/reject actions
// - Moderation history logs
// - Moderation statistics

// TODO: PROFESSIONAL-5.19 - Implement comment shadow banning
// - Hide comments from public view (author still sees)
// - Shadow ban repeat offenders
// - Gradual shadow ban (temporary)
// - Shadow ban appeals process
// - Transparency reporting

// TODO: PROFESSIONAL-5.20 - Add comment-specific export functionality
// - Export comments as JSON/CSV
// - Export comment threads
// - Export user's comment history
// - See common_todos.go for general export/backup infrastructure

// TODO: PROFESSIONAL-5.21 - Implement comment-specific webhooks
// - Webhook for new comments
// - Webhook for comment updates
// - Webhook for comment reactions
// - See common_todos.go for general webhook infrastructure

// TODO: PROFESSIONAL-5.22 - Add comment privacy controls
// - Private comments (only visible to post owner)
// - Hide comments from specific users
// - Comment visibility settings
// - Block user's comments
// - Comment privacy levels

// TODO: PROFESSIONAL-5.23 - Implement comment collaboration features
// - Multiple users edit comment (if enabled)
// - Comment co-authors
// - Comment versioning
// - Comment collaboration history
// - Conflict resolution

// TODO: PROFESSIONAL-5.24 - Add comment performance optimization
// - Lazy load comment threads
// - Paginate large comment threads efficiently
// - Optimize database queries (N+1 problem prevention)
// - Reduce latency for comment creation
// - Async comment processing

// TODO: PROFESSIONAL-5.25 - Implement comment-specific localization
// - Auto-translate comments (user preference)
// - Multi-language comment support
// - See common_todos.go for general localization infrastructure

// TODO: PROFESSIONAL-5.26 - Add comment accessibility features
// - Comment reading time estimates
// - See common_todos.go for general accessibility infrastructure

// TODO: PROFESSIONAL-5.27 - Implement comment-specific anti-abuse measures
// - Detect bot comments
// - Verify comments from real users
// - Pattern detection for spam
// - See common_todos.go for general anti-abuse infrastructure

// TODO: PROFESSIONAL-5.28 - Add comment insights for creators
// - Most engaged commenters
// - Comment sentiment analysis
// - Comment quality metrics
// - Creator response rate tracking
// - Comment engagement trends

// TODO: PROFESSIONAL-5.29 - Implement comment content warnings
// - Content warning labels
// - Hide sensitive comments by default
// - User preference to show/hide warnings
// - Age-appropriate filtering
// - Sensitivity labels

// TODO: PROFESSIONAL-5.30 - Add comment quality scoring
// - Score comments based on engagement
// - Score comments based on author reputation
// - Score comments based on content quality
// - Use scores for sorting and ranking
// - Quality-based comment recommendations
