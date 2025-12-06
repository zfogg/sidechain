package handlers

import (
	"fmt"
	"net/http"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/websocket"
	"gorm.io/gorm"
)

// FollowUser follows another user
func (h *Handlers) FollowUser(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		TargetUserID string `json:"target_user_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.stream.FollowUser(userID, req.TargetUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to follow user"})
		return
	}

	// Send real-time WebSocket notification to the target user
	if h.wsHandler != nil {
		// Fetch follower and followee info for the notification
		var follower, followee models.User
		database.DB.Select("id, username, display_name, profile_picture_url").First(&follower, "id = ?", userID)
		database.DB.Select("id, username, display_name").First(&followee, "id = ?", req.TargetUserID)

		// Get updated follower count for the target user
		followerCount := 0
		if stats, err := h.stream.GetFollowStats(req.TargetUserID); err == nil {
			followerCount = stats.FollowerCount
		}

		followerName := follower.DisplayName
		if followerName == "" {
			followerName = follower.Username
		}
		followeeName := followee.DisplayName
		if followeeName == "" {
			followeeName = followee.Username
		}

		h.wsHandler.NotifyFollow(req.TargetUserID, &websocket.FollowPayload{
			FollowerID:     userID,
			FollowerName:   followerName,
			FollowerAvatar: follower.ProfilePictureURL,
			FolloweeID:     req.TargetUserID,
			FolloweeName:   followeeName,
			FollowerCount:  followerCount,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"status":         "following",
		"target_user":    req.TargetUserID,
		"following_user": userID,
	})
}

// UnfollowUser unfollows a user
func (h *Handlers) UnfollowUser(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		TargetUserID string `json:"target_user_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	if err := h.stream.UnfollowUser(userID, req.TargetUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to unfollow user"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "unfollowed",
		"target_user": req.TargetUserID,
	})
}

// LikePost likes a post with optional emoji
func (h *Handlers) LikePost(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		ActivityID string `json:"activity_id" binding:"required"`
		Emoji      string `json:"emoji,omitempty"` // Optional emoji like ❤️, 🔥, 🎵
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Use emoji reaction if provided, otherwise default like
	if req.Emoji != "" {
		if err := h.stream.AddReactionWithEmoji("like", userID, req.ActivityID, req.Emoji); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to add emoji reaction"})
			return
		}
	} else {
		if err := h.stream.AddReaction("like", userID, req.ActivityID); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to like post"})
			return
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "liked",
		"activity_id": req.ActivityID,
		"user_id":     userID,
		"emoji":       req.Emoji,
	})
}

// UnlikePost removes a like from a post
func (h *Handlers) UnlikePost(c *gin.Context) {
	userID := c.GetString("user_id")

	var req struct {
		ActivityID string `json:"activity_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Remove the like reaction using activity ID and user ID
	if err := h.stream.RemoveReactionByActivityAndUser(req.ActivityID, userID, "like"); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to unlike post", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "unliked",
		"activity_id": req.ActivityID,
		"user_id":     userID,
	})
}

// GetUserProfile gets a user's profile by ID (public endpoint)
// GET /api/users/:id/profile
func (h *Handlers) GetUserProfile(c *gin.Context) {
	targetUserID := c.Param("id")
	currentUserID := c.GetString("user_id") // May be empty if not authenticated

	// Fetch user from database
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found", "message": "User not found"})
		return
	}

	// Fetch follow stats from Stream.io (source of truth)
	var followStats *stream.FollowStats
	var followStatsErr error
	if h.stream != nil {
		followStats, followStatsErr = h.stream.GetFollowStats(user.StreamUserID)
		if followStatsErr != nil {
			// Log but don't fail - use cached values from DB
			fmt.Printf("Warning: Failed to get follow stats from Stream.io: %v\n", followStatsErr)
		}
	}

	// Use Stream.io stats if available, otherwise fall back to cached DB values
	followerCount := user.FollowerCount
	followingCount := user.FollowingCount
	if followStats != nil {
		followerCount = followStats.FollowerCount
		followingCount = followStats.FollowingCount
		// Optionally update cache in background
		go func() {
			database.DB.Model(&user).Updates(map[string]interface{}{
				"follower_count":  followerCount,
				"following_count": followingCount,
			})
		}()
	}

	// Check if current user follows this profile
	var isFollowing bool
	var isFollowedBy bool
	if currentUserID != "" && currentUserID != user.ID && h.stream != nil {
		isFollowing, _ = h.stream.CheckIsFollowing(currentUserID, user.StreamUserID)
		isFollowedBy, _ = h.stream.CheckIsFollowing(user.StreamUserID, currentUserID)
	}

	// Count posts
	var postCount int64
	database.DB.Model(&models.AudioPost{}).Where("user_id = ? AND is_public = true", user.ID).Count(&postCount)

	// Fetch story highlights (7.5.6.2)
	var highlights []models.StoryHighlight
	database.DB.Where("user_id = ?", user.ID).Order("sort_order ASC, created_at DESC").Find(&highlights)

	c.JSON(http.StatusOK, gin.H{
		"id":                  user.ID,
		"username":            user.Username,
		"display_name":        user.DisplayName,
		"bio":                 user.Bio,
		"location":            user.Location,
		"avatar_url":          user.AvatarURL,
		"profile_picture_url": user.ProfilePictureURL,
		"daw_preference":      user.DAWPreference,
		"genre":               user.Genre,
		"social_links":        user.SocialLinks,
		"follower_count":      followerCount,
		"following_count":     followingCount,
		"post_count":          postCount,
		"is_following":        isFollowing,
		"is_followed_by":      isFollowedBy,
		"highlights":          highlights,
		"created_at":          user.CreatedAt,
	})
}

// GetMyProfile gets the authenticated user's own profile
// GET /api/profile
func (h *Handlers) GetMyProfile(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	// Fetch follow stats from Stream.io
	var followStats *stream.FollowStats
	if h.stream != nil {
		followStats, _ = h.stream.GetFollowStats(currentUser.StreamUserID)
	}

	followerCount := currentUser.FollowerCount
	followingCount := currentUser.FollowingCount
	if followStats != nil {
		followerCount = followStats.FollowerCount
		followingCount = followStats.FollowingCount
	}

	// Count posts
	var postCount int64
	database.DB.Model(&models.AudioPost{}).Where("user_id = ?", currentUser.ID).Count(&postCount)

	// Fetch story highlights (7.5.6.2)
	var highlights []models.StoryHighlight
	database.DB.Where("user_id = ?", currentUser.ID).Order("sort_order ASC, created_at DESC").Find(&highlights)

	c.JSON(http.StatusOK, gin.H{
		"id":                  currentUser.ID,
		"email":               currentUser.Email,
		"username":            currentUser.Username,
		"display_name":        currentUser.DisplayName,
		"bio":                 currentUser.Bio,
		"location":            currentUser.Location,
		"avatar_url":          currentUser.AvatarURL,
		"profile_picture_url": currentUser.ProfilePictureURL,
		"daw_preference":      currentUser.DAWPreference,
		"genre":               currentUser.Genre,
		"social_links":        currentUser.SocialLinks,
		"follower_count":      followerCount,
		"following_count":     followingCount,
		"post_count":          postCount,
		"email_verified":      currentUser.EmailVerified,
		"highlights":          highlights,
		"created_at":          currentUser.CreatedAt,
	})
}

// UpdateMyProfile updates the authenticated user's profile
// PUT /api/profile
func (h *Handlers) UpdateMyProfile(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	var req struct {
		DisplayName       *string             `json:"display_name"`
		Bio               *string             `json:"bio"`
		Location          *string             `json:"location"`
		DAWPreference     *string             `json:"daw_preference"`
		Genre             []string            `json:"genre"`
		SocialLinks       *models.SocialLinks `json:"social_links"`
		ProfilePictureURL *string             `json:"profile_picture_url"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Build update map for non-nil fields
	updates := make(map[string]interface{})
	if req.DisplayName != nil {
		updates["display_name"] = *req.DisplayName
	}
	if req.Bio != nil {
		updates["bio"] = *req.Bio
	}
	if req.Location != nil {
		updates["location"] = *req.Location
	}
	if req.DAWPreference != nil {
		updates["daw_preference"] = *req.DAWPreference
	}
	if req.Genre != nil {
		updates["genre"] = req.Genre
	}
	if req.SocialLinks != nil {
		updates["social_links"] = req.SocialLinks
	}
	if req.ProfilePictureURL != nil {
		updates["profile_picture_url"] = *req.ProfilePictureURL
	}

	if len(updates) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "no_fields_to_update"})
		return
	}

	// Update in database
	if err := database.DB.Model(currentUser).Updates(updates).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "update_failed", "message": err.Error()})
		return
	}

	// Reload user
	database.DB.First(currentUser, "id = ?", currentUser.ID)

	c.JSON(http.StatusOK, gin.H{
		"message": "profile_updated",
		"user": gin.H{
			"id":                  currentUser.ID,
			"display_name":        currentUser.DisplayName,
			"bio":                 currentUser.Bio,
			"location":            currentUser.Location,
			"daw_preference":      currentUser.DAWPreference,
			"genre":               currentUser.Genre,
			"social_links":        currentUser.SocialLinks,
			"profile_picture_url": currentUser.ProfilePictureURL,
		},
		"updated_at": time.Now().UTC(),
	})
}

// ChangeUsername allows users to change their username with uniqueness validation
// PUT /api/v1/users/username
func (h *Handlers) ChangeUsername(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)

	var req struct {
		Username string `json:"username" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "username_required"})
		return
	}

	newUsername := strings.TrimSpace(req.Username)

	// Validate username format
	if len(newUsername) < 3 {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "username_too_short",
			"message": "Username must be at least 3 characters",
		})
		return
	}

	if len(newUsername) > 30 {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "username_too_long",
			"message": "Username must be at most 30 characters",
		})
		return
	}

	// Only allow alphanumeric, underscores, and hyphens
	validUsername := regexp.MustCompile(`^[a-zA-Z0-9_-]+$`)
	if !validUsername.MatchString(newUsername) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "username_invalid_chars",
			"message": "Username can only contain letters, numbers, underscores, and hyphens",
		})
		return
	}

	// Check if username is unchanged
	if strings.EqualFold(newUsername, currentUser.Username) {
		c.JSON(http.StatusOK, gin.H{
			"message":  "username_unchanged",
			"username": currentUser.Username,
		})
		return
	}

	// Check uniqueness (case-insensitive)
	var existingUser models.User
	err := database.DB.Where("LOWER(username) = LOWER(?) AND id != ?", newUsername, currentUser.ID).First(&existingUser).Error
	if err == nil {
		// User found with this username
		c.JSON(http.StatusConflict, gin.H{
			"error":   "username_taken",
			"message": "This username is already taken",
		})
		return
	}
	if err != gorm.ErrRecordNotFound {
		// Database error
		c.JSON(http.StatusInternalServerError, gin.H{"error": "database_error"})
		return
	}

	// Update username
	oldUsername := currentUser.Username
	if err := database.DB.Model(currentUser).Update("username", newUsername).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "update_failed",
			"message": err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":      "username_changed",
		"username":     newUsername,
		"old_username": oldUsername,
		"updated_at":   time.Now().UTC(),
	})
}

// TODO: Phase 4.5.1.16 - Test GetUserFollowers / GetUserFollowing - pagination
// Note: Followers/Following lists backend endpoints exist (GET /api/users/:id/followers, GET /api/users/:id/following)
// Note: Plugin UI shows counts but clicking them opens FollowersList panel - this is already implemented

// GetUserFollowers gets the list of users who follow a user
// GET /api/users/:id/followers
func (h *Handlers) GetUserFollowers(c *gin.Context) {
	targetUserID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Validate user exists
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Get followers from Stream.io
	followers, err := h.stream.GetFollowers(user.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_followers", "message": err.Error()})
		return
	}

	// Enrich with user data from database
	followerUsers := make([]gin.H, 0, len(followers))
	for _, f := range followers {
		var followerUser models.User
		if err := database.DB.First(&followerUser, "id = ? OR stream_user_id = ?", f.UserID, f.UserID).Error; err == nil {
			followerUsers = append(followerUsers, gin.H{
				"id":           followerUser.ID,
				"username":     followerUser.Username,
				"display_name": followerUser.DisplayName,
				"avatar_url":   followerUser.AvatarURL,
				"bio":          followerUser.Bio,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"followers": followerUsers,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(followerUsers),
		},
	})
}

// GetUserFollowing gets the list of users a user follows
// GET /api/users/:id/following
func (h *Handlers) GetUserFollowing(c *gin.Context) {
	targetUserID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Validate user exists
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Get following from Stream.io
	following, err := h.stream.GetFollowing(user.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_following", "message": err.Error()})
		return
	}

	// Enrich with user data from database
	followingUsers := make([]gin.H, 0, len(following))
	for _, f := range following {
		var followedUser models.User
		if err := database.DB.First(&followedUser, "id = ? OR stream_user_id = ?", f.UserID, f.UserID).Error; err == nil {
			followingUsers = append(followingUsers, gin.H{
				"id":           followedUser.ID,
				"username":     followedUser.Username,
				"display_name": followedUser.DisplayName,
				"avatar_url":   followedUser.AvatarURL,
				"bio":          followedUser.Bio,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"following": followingUsers,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(followingUsers),
		},
	})
}

// GetUserPosts gets a user's posts with enriched data (likes, etc.)
// GET /api/users/:id/posts
func (h *Handlers) GetUserPosts(c *gin.Context) {
	targetUserID := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Validate user exists
	var user models.User
	if err := database.DB.First(&user, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Get enriched activities from Stream.io
	activities, err := h.stream.GetEnrichedUserFeed(user.StreamUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_get_posts", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"posts": activities,
		"user": gin.H{
			"id":           user.ID,
			"username":     user.Username,
			"display_name": user.DisplayName,
			"avatar_url":   user.AvatarURL,
		},
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
		},
	})
}

// FollowUserByID follows a user by ID (path param version)
// POST /api/users/:id/follow
func (h *Handlers) FollowUserByID(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)
	targetUserID := c.Param("id")

	// Validate target user exists
	var targetUser models.User
	if err := database.DB.First(&targetUser, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Can't follow yourself
	if currentUser.ID == targetUser.ID {
		c.JSON(http.StatusBadRequest, gin.H{"error": "cannot_follow_self"})
		return
	}

	// Follow via Stream.io
	if err := h.stream.FollowUser(currentUser.StreamUserID, targetUser.StreamUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "follow_failed", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "following",
		"target_user": targetUser.ID,
		"username":    targetUser.Username,
	})
}

// UnfollowUserByID unfollows a user by ID (path param version)
// DELETE /api/users/:id/follow
func (h *Handlers) UnfollowUserByID(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}
	currentUser := user.(*models.User)
	targetUserID := c.Param("id")

	// Validate target user exists
	var targetUser models.User
	if err := database.DB.First(&targetUser, "id = ? OR stream_user_id = ?", targetUserID, targetUserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Unfollow via Stream.io
	if err := h.stream.UnfollowUser(currentUser.StreamUserID, targetUser.StreamUserID); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "unfollow_failed", "message": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "unfollowed",
		"target_user": targetUser.ID,
	})
}

// Legacy placeholder handlers for backwards compatibility
// GetProfile gets user profile information (deprecated - use GetMyProfile)
func (h *Handlers) GetProfile(c *gin.Context) {
	h.GetMyProfile(c)
}

// UpdateProfile updates user profile information (deprecated - use UpdateMyProfile)
func (h *Handlers) UpdateProfile(c *gin.Context) {
	h.UpdateMyProfile(c)
}
