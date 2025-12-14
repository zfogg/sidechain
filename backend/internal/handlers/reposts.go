package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/util"
	"gorm.io/gorm"
)

// CreateRepost reposts a post to the current user's feed (like a retweet)
// POST /api/v1/posts/:id/repost
func (h *Handlers) CreateRepost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Parse optional quote text
	var req struct {
		Quote string `json:"quote"`
	}
	c.ShouldBindJSON(&req)

	// Check if post exists and get post data
	var post models.AudioPost
	if err := database.DB.Preload("User").First(&post, "id = ?", postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Post not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to find post"})
		return
	}

	// Cannot repost your own post
	if post.UserID == userID {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Cannot repost your own post"})
		return
	}

	// Check if already reposted
	var existingRepost models.Repost
	err := database.DB.Where("user_id = ? AND original_post_id = ?", userID, postID).First(&existingRepost).Error
	if err == nil {
		// Already reposted
		c.JSON(http.StatusConflict, gin.H{
			"error":      "Post already reposted",
			"repost_id":  existingRepost.ID,
			"reposted":   true,
			"created_at": existingRepost.CreatedAt,
		})
		return
	}

	// Get the reposting user's info for stream activity
	var user models.User
	if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to find user"})
		return
	}

	// Create repost record
	repost := models.Repost{
		UserID:         userID,
		OriginalPostID: postID,
		Quote:          req.Quote,
	}

	if err := database.DB.Create(&repost).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to create repost"})
		return
	}

	// Increment repost count on the original post
	database.DB.Model(&post).UpdateColumn("repost_count", gorm.Expr("repost_count + 1"))

	// Create Stream.io activity for the repost
	activity := &stream.Activity{
		Actor:     "user:" + userID,
		Verb:      "reposted",
		Object:    "loop:" + postID,
		ForeignID: "repost:" + repost.ID,
		Time:      time.Now().UTC().Format(time.RFC3339),
		// Include original post data for the feed
		AudioURL:     post.AudioURL,
		BPM:          post.BPM,
		Key:          post.Key,
		DAW:          post.DAW,
		DurationBars: post.DurationBars,
		Genre:        post.Genre,
		Extra: map[string]interface{}{
			"repost_id":         repost.ID,
			"is_repost":         true,
			"original_post_id":  postID,
			"original_user_id":  post.UserID,
			"original_username": post.User.Username,
			"original_avatar":   post.User.GetAvatarURL(),
			"original_filename": post.Filename, // Display filename for the original post
			"original_created":  post.CreatedAt,
			"quote":             req.Quote,
		},
	}

	// Add activity to stream feed
	if err := h.stream.CreateLoopActivity(userID, activity); err != nil {
		// Log error but don't fail - the repost is saved in DB
		// Feed sync can happen later
		c.JSON(http.StatusCreated, gin.H{
			"message":    "Repost created (feed sync pending)",
			"repost_id":  repost.ID,
			"reposted":   true,
			"created_at": repost.CreatedAt,
			"feed_error": err.Error(),
		})
		return
	}

	// Update stream activity ID if available
	if activity.ID != "" {
		repost.StreamActivityID = activity.ID
		database.DB.Model(&repost).UpdateColumn("stream_activity_id", activity.ID)
	}

	// Send notification to original post owner via GetStream.io
	if h.stream != nil && post.UserID != userID {
		// Get target user's StreamUserID for notification
		var targetUser models.User
		if err := database.DB.First(&targetUser, "id = ?", post.UserID).Error; err == nil && targetUser.StreamUserID != "" {
			h.stream.NotifyRepost(userID, targetUser.StreamUserID, postID)
		}
	}

	c.JSON(http.StatusCreated, gin.H{
		"message":    "Post reposted successfully",
		"repost_id":  repost.ID,
		"reposted":   true,
		"created_at": repost.CreatedAt,
	})
}

// UndoRepost removes a repost
// DELETE /api/v1/posts/:id/repost
func (h *Handlers) UndoRepost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Find the repost
	var repost models.Repost
	err := database.DB.Where("user_id = ? AND original_post_id = ?", userID, postID).First(&repost).Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "Repost not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to find repost"})
		return
	}

	// Delete the repost (soft delete)
	if err := database.DB.Delete(&repost).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to undo repost"})
		return
	}

	// Decrement repost count on the original post
	database.DB.Model(&models.AudioPost{}).Where("id = ?", postID).UpdateColumn("repost_count", gorm.Expr("GREATEST(repost_count - 1, 0)"))

	// Remove activity from stream feed if we have the activity ID
	if repost.StreamActivityID != "" && h.stream != nil {
		// Note: Stream.io activity removal would go here if implemented
		// h.stream.DeleteLoopActivity(userID, repost.StreamActivityID)
	}

	c.JSON(http.StatusOK, gin.H{
		"message":  "Repost removed successfully",
		"reposted": false,
	})
}

// GetReposts returns users who reposted a specific post
// GET /api/v1/posts/:id/reposts
func (h *Handlers) GetReposts(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}
	_ = userID // userID available for future use (e.g., checking if current user reposted)

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	// Parse pagination
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	if limit > 50 {
		limit = 50
	}

	// Get reposts with user info
	var reposts []models.Repost
	err := database.DB.
		Preload("User").
		Where("original_post_id = ?", postID).
		Order("created_at DESC").
		Limit(limit).
		Offset(offset).
		Find(&reposts).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get reposts"})
		return
	}

	// Get total count
	var totalCount int64
	database.DB.Model(&models.Repost{}).Where("original_post_id = ?", postID).Count(&totalCount)

	// Transform to response format
	repostList := make([]gin.H, len(reposts))
	for i, r := range reposts {
		repostList[i] = gin.H{
			"id": r.ID,
			"user": gin.H{
				"id":           r.User.ID,
				"username":     r.User.Username,
				"display_name": r.User.DisplayName,
				"avatar_url":   r.User.GetAvatarURL(),
			},
			"quote":      r.Quote,
			"created_at": r.CreatedAt,
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"reposts":     repostList,
		"total_count": totalCount,
		"limit":       limit,
		"offset":      offset,
		"has_more":    offset+len(reposts) < int(totalCount),
	})
}

// IsPostReposted checks if the current user has reposted a specific post
// GET /api/v1/posts/:id/reposted
func (h *Handlers) IsPostReposted(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("id")
	if postID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Post ID is required"})
		return
	}

	var repost models.Repost
	err := database.DB.Where("user_id = ? AND original_post_id = ?", userID, postID).First(&repost).Error

	c.JSON(http.StatusOK, gin.H{
		"reposted":   err == nil,
		"repost_id":  repost.ID,
		"created_at": repost.CreatedAt,
	})
}

// GetUserReposts returns reposts made by a specific user
// GET /api/v1/users/:id/reposts
func (h *Handlers) GetUserReposts(c *gin.Context) {
	_, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	targetUserID := c.Param("id")
	if targetUserID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "User ID is required"})
		return
	}

	// Parse pagination
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	if limit > 50 {
		limit = 50
	}

	// Get reposts with original post info
	var reposts []models.Repost
	err := database.DB.
		Preload("OriginalPost").
		Preload("OriginalPost.User").
		Where("user_id = ?", targetUserID).
		Order("created_at DESC").
		Limit(limit).
		Offset(offset).
		Find(&reposts).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to get reposts"})
		return
	}

	// Get total count
	var totalCount int64
	database.DB.Model(&models.Repost{}).Where("user_id = ?", targetUserID).Count(&totalCount)

	// Transform to response format
	repostList := make([]gin.H, len(reposts))
	for i, r := range reposts {
		repostList[i] = gin.H{
			"id":    r.ID,
			"quote": r.Quote,
			"post": gin.H{
				"id":         r.OriginalPost.ID,
				"user_id":    r.OriginalPost.UserID,
				"user":       formatUserResponse(&r.OriginalPost.User),
				"audio_url":  r.OriginalPost.AudioURL,
				"filename":   r.OriginalPost.Filename, // Display filename (required)
				"duration":   r.OriginalPost.Duration,
				"bpm":        r.OriginalPost.BPM,
				"key":        r.OriginalPost.Key,
				"genre":      r.OriginalPost.Genre,
				"like_count": r.OriginalPost.LikeCount,
				"play_count": r.OriginalPost.PlayCount,
				"save_count": r.OriginalPost.SaveCount,
				"created_at": r.OriginalPost.CreatedAt,
			},
			"reposted_at": r.CreatedAt,
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"reposts":     repostList,
		"total_count": totalCount,
		"limit":       limit,
		"offset":      offset,
		"has_more":    offset+len(reposts) < int(totalCount),
	})
}
