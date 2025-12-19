package handlers

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/search"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"github.com/zfogg/sidechain/backend/internal/util"
	"github.com/zfogg/sidechain/backend/internal/websocket"
	"gorm.io/gorm"
)

// FollowUser follows another user
func (h *Handlers) FollowUser(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		TargetUserID string `json:"target_user_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	if err := h.stream.FollowUser(userID, req.TargetUserID); err != nil {
		util.RespondInternalError(c, "follow_failed", "Failed to follow user")
		return
	}

	// Real-time Gorse feedback sync (Task 1.3)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFollowEvent(userID, req.TargetUserID); err != nil {
				fmt.Printf("Warning: Failed to sync follow to Gorse: %v\n", err)
			}
		}()
	}

	// Send real-time WebSocket notification to the target user
	if h.wsHandler != nil {
		// Fetch follower and followee info for the notification
		var follower, followee models.User
		database.DB.First(&follower, "id = ?", userID)
		database.DB.First(&followee, "id = ?", req.TargetUserID)

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

		// Get effective avatar URL
		followerAvatar := follower.ProfilePictureURL
		if followerAvatar == "" {
			followerAvatar = follower.OAuthProfilePictureURL
		}

		h.wsHandler.NotifyFollow(req.TargetUserID, &websocket.FollowPayload{
			FollowerID:     userID,
			FollowerName:   followerName,
			FollowerAvatar: followerAvatar,
			FolloweeID:     req.TargetUserID,
			FolloweeName:   followeeName,
			FollowerCount:  followerCount,
		})

		// Invalidate the follower's timeline - they can now see followed user's posts (Phase 2.2)
		h.wsHandler.BroadcastFeedInvalidation("timeline", "follow")
	}

	c.JSON(http.StatusOK, gin.H{
		"status":         "following",
		"target_user":    req.TargetUserID,
		"following_user": userID,
	})
}

// UnfollowUser unfollows a user
func (h *Handlers) UnfollowUser(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		TargetUserID string `json:"target_user_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	if err := h.stream.UnfollowUser(userID, req.TargetUserID); err != nil {
		util.RespondInternalError(c, "unfollow_failed", "Failed to unfollow user")
		return
	}

	// Real-time Gorse feedback removal (Task 1.3)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.RemoveFollowEvent(userID, req.TargetUserID); err != nil {
				fmt.Printf("Warning: Failed to remove follow from Gorse: %v\n", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "unfollowed",
		"target_user": req.TargetUserID,
	})
}

// LikePost likes a post with optional emoji
func (h *Handlers) LikePost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		ActivityID string `json:"activity_id" binding:"required"`
		Emoji      string `json:"emoji,omitempty"` // Optional emoji like ❤️, 🔥, 🎵
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Use emoji reaction if provided, otherwise default like
	if req.Emoji != "" {
		if err := h.stream.AddReactionWithEmoji("like", userID, req.ActivityID, req.Emoji); err != nil {
			util.RespondInternalError(c, "reaction_failed", "Failed to add emoji reaction")
			return
		}
	} else {
		if err := h.stream.AddReaction("like", userID, req.ActivityID); err != nil {
			util.RespondInternalError(c, "like_failed", "Failed to like post")
			return
		}
	}

	// Real-time Gorse feedback sync (Task 1.1)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFeedback(userID, req.ActivityID, "like"); err != nil {
				fmt.Printf("Warning: Failed to sync like to Gorse: %v\n", err)
			}
		}()
	}

	// Phase 0.6: Re-index post in Elasticsearch with updated engagement metrics
	if h.search != nil {
		go func() {
			var post models.AudioPost
			if err := database.DB.Where("stream_activity_id = ?", req.ActivityID).First(&post).Error; err == nil {
				var user models.User
				if err := database.DB.First(&user, "id = ?", post.UserID).Error; err == nil {
					postDoc := search.AudioPostToSearchDoc(post, user.Username)
					if err := h.search.IndexPost(c.Request.Context(), post.ID, postDoc); err != nil {
						fmt.Printf("Warning: Failed to re-index post %s after like in Elasticsearch: %v\n", post.ID, err)
					}
				}
			}
		}()
	}

	// Send WebSocket notification to post owner and broadcast to all viewers
	if h.wsHandler != nil {
		go func() {
			var post models.AudioPost
			var user models.User
			if err := database.DB.Where("stream_activity_id = ?", req.ActivityID).First(&post).Error; err == nil {
				if err := database.DB.First(&user, "id = ?", userID).Error; err == nil {
					payload := &websocket.LikePayload{
						PostID:    post.ID,
						UserID:    userID,
						Username:  user.Username,
						LikeCount: post.LikeCount,
						Emoji:     req.Emoji,
					}
					h.wsHandler.NotifyLike(post.UserID, payload)

					// Broadcast updated like count to all viewers
					h.wsHandler.BroadcastLikeCountUpdate(post.ID, post.LikeCount)
				}
			}
		}()
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
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		ActivityID string `json:"activity_id" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Remove the like reaction using activity ID and user ID
	if err := h.stream.RemoveReactionByActivityAndUser(req.ActivityID, userID, "like"); err != nil {
		util.RespondInternalError(c, "unlike_failed", "Failed to unlike post: "+err.Error())
		return
	}

	// Phase 0.6: Re-index post in Elasticsearch with updated engagement metrics
	if h.search != nil {
		go func() {
			var post models.AudioPost
			if err := database.DB.Where("stream_activity_id = ?", req.ActivityID).First(&post).Error; err == nil {
				var user models.User
				if err := database.DB.First(&user, "id = ?", post.UserID).Error; err == nil {
					postDoc := search.AudioPostToSearchDoc(post, user.Username)
					if err := h.search.IndexPost(c.Request.Context(), post.ID, postDoc); err != nil {
						fmt.Printf("Warning: Failed to re-index post %s after unlike in Elasticsearch: %v\n", post.ID, err)
					}
				}
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "unliked",
		"activity_id": req.ActivityID,
		"user_id":     userID,
	})
}

// GetUserProfile gets a user's profile by ID or username (public endpoint)
// GET /api/users/:id/profile (where :id can be user ID, stream ID, or username)
func (h *Handlers) GetUserProfile(c *gin.Context) {
	targetParam := c.Param("id")
	currentUserID := c.GetString("user_id") // May be empty if not authenticated

	// Fetch user from database - accept ID, stream_user_id, or username
	// Cast id to text to avoid UUID type comparison errors with usernames
	var user models.User
	if err := database.DB.Where("CAST(id AS TEXT) = ? OR stream_user_id = ? OR username = ?", targetParam, targetParam, targetParam).First(&user).Error; err != nil {
		if util.HandleDBError(c, err, "user") {
			return
		}
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
	// Use database IDs (NOT Stream IDs) to match FollowUser behavior
	var isFollowing bool
	var isFollowedBy bool
	if currentUserID != "" && currentUserID != user.ID && h.stream != nil {
		isFollowing, _ = h.stream.CheckIsFollowing(currentUserID, user.ID)
		isFollowedBy, _ = h.stream.CheckIsFollowing(user.ID, currentUserID)
	}

	// Check for pending follow request if account is private
	var followRequestStatus string
	var followRequestID string
	if user.IsPrivate && currentUserID != "" && currentUserID != user.ID && !isFollowing {
		var request models.FollowRequest
		if err := database.DB.Where("requester_id = ? AND target_id = ?", currentUserID, user.ID).
			First(&request).Error; err == nil {
			followRequestStatus = string(request.Status)
			followRequestID = request.ID
		}
	}

	// Check if current user has muted this profile
	var isMuted bool
	if currentUserID != "" && currentUserID != user.ID {
		var mutedUser models.MutedUser
		if err := database.DB.Where("user_id = ? AND muted_user_id = ?", currentUserID, user.ID).
			First(&mutedUser).Error; err == nil {
			isMuted = true
		}
	}

	// Count posts (exclude archived)
	var postCount int64
	database.DB.Model(&models.AudioPost{}).Where("user_id = ? AND is_public = true AND is_archived = false", user.ID).Count(&postCount)

	// Fetch story highlights (7.5.6.2)
	var highlights []models.StoryHighlight
	database.DB.Where("user_id = ?", user.ID).Order("sort_order ASC, created_at DESC").Find(&highlights)

	// avatar_url returns the effective avatar (prefer uploaded, fallback to OAuth)
	avatarURL := user.ProfilePictureURL
	if avatarURL == "" {
		avatarURL = user.OAuthProfilePictureURL
	}

	// Build response with base fields
	response := gin.H{
		"id":                         user.ID,
		"username":                   user.Username,
		"display_name":               user.DisplayName,
		"bio":                        user.Bio,
		"location":                   user.Location,
		"avatar_url":                 avatarURL, // Effective avatar for display
		"profile_picture_url":        user.ProfilePictureURL,
		"oauth_profile_picture_url":  user.OAuthProfilePictureURL,
		"daw_preference":             user.DAWPreference,
		"genre":                      user.Genre,
		"social_links":               user.SocialLinks,
		"follower_count":             followerCount,
		"following_count":            followingCount,
		"post_count":                 postCount,
		"is_following":               isFollowing,
		"is_followed_by":             isFollowedBy,
		"is_own_profile":             currentUserID == user.ID,
		"is_private":                 user.IsPrivate,
		"is_muted":                   isMuted,
		"follow_request_status":      followRequestStatus,
		"follow_request_id":          followRequestID,
		"highlights":                 highlights,
		"created_at":                 user.CreatedAt,
	}

	// Add activity status info only if the user allows it
	// Viewing your own profile always shows activity status
	if currentUserID == user.ID {
		response["is_online"] = user.IsOnline
		response["last_active_at"] = user.LastActiveAt
		response["show_activity_status"] = user.ShowActivityStatus
		response["show_last_active"] = user.ShowLastActive
	} else {
		// For other users, respect their privacy settings
		if user.ShowActivityStatus {
			response["is_online"] = user.IsOnline
		}
		if user.ShowLastActive {
			response["last_active_at"] = user.LastActiveAt
		}
	}

	c.JSON(http.StatusOK, response)
}

// GetMyProfile gets the authenticated user's own profile
// GET /api/profile
func (h *Handlers) GetMyProfile(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

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

	// Count posts (exclude archived)
	var postCount int64
	database.DB.Model(&models.AudioPost{}).Where("user_id = ? AND is_archived = false", currentUser.ID).Count(&postCount)

	// Fetch story highlights (7.5.6.2)
	var highlights []models.StoryHighlight
	database.DB.Where("user_id = ?", currentUser.ID).Order("sort_order ASC, created_at DESC").Find(&highlights)

	// avatar_url returns the effective avatar (prefer uploaded, fallback to OAuth)
	avatarURL := currentUser.ProfilePictureURL
	if avatarURL == "" {
		avatarURL = currentUser.OAuthProfilePictureURL
	}

	c.JSON(http.StatusOK, gin.H{
		"id":                         currentUser.ID,
		"email":                      currentUser.Email,
		"username":                   currentUser.Username,
		"display_name":               currentUser.DisplayName,
		"bio":                        currentUser.Bio,
		"location":                   currentUser.Location,
		"avatar_url":                 avatarURL, // Effective avatar for display
		"profile_picture_url":        currentUser.ProfilePictureURL,
		"oauth_profile_picture_url":  currentUser.OAuthProfilePictureURL,
		"daw_preference":             currentUser.DAWPreference,
		"genre":                      currentUser.Genre,
		"social_links":               currentUser.SocialLinks,
		"follower_count":             followerCount,
		"following_count":            followingCount,
		"post_count":                 postCount,
		"email_verified":             currentUser.EmailVerified,
		"is_private":                 currentUser.IsPrivate,
		"highlights":                 highlights,
		"created_at":                 currentUser.CreatedAt,
	})
}

// UpdateMyProfile updates the authenticated user's profile
// PUT /api/profile
func (h *Handlers) UpdateMyProfile(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var req struct {
		Username          *string             `json:"username"`
		DisplayName       *string             `json:"display_name"`
		Bio               *string             `json:"bio"`
		Location          *string             `json:"location"`
		DAWPreference     *string             `json:"daw_preference"`
		Genre             []string            `json:"genre"`
		SocialLinks       *models.SocialLinks `json:"social_links"`
		ProfilePictureURL *string             `json:"profile_picture_url"`
		IsPrivate         *bool               `json:"is_private"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Build update map for non-nil fields
	updates := make(map[string]interface{})

	// Validate and handle username update
	if req.Username != nil {
		newUsername := strings.TrimSpace(*req.Username)
		if newUsername == "" {
			util.RespondBadRequest(c, "invalid_username", "Username cannot be empty")
			return
		}

		// Check if new username is already taken (only if different from current username)
		if newUsername != currentUser.Username {
			var existingUser models.User
			if err := database.DB.Where("LOWER(username) = LOWER(?)", newUsername).First(&existingUser).Error; err == nil {
				// Username already exists
				util.RespondBadRequest(c, "username_taken", "This username is already taken")
				return
			} else if err != gorm.ErrRecordNotFound {
				// Database error
				util.RespondInternalError(c, "username_check_failed", err.Error())
				return
			}
		}

		updates["username"] = newUsername
	}

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
	if req.IsPrivate != nil {
		updates["is_private"] = *req.IsPrivate
	}

	if len(updates) == 0 {
		util.RespondBadRequest(c, "no_fields_to_update")
		return
	}

	// Update in database
	if err := database.DB.Model(currentUser).Updates(updates).Error; err != nil {
		util.RespondInternalError(c, "update_failed", err.Error())
		return
	}

	// Reload user
	database.DB.First(currentUser, "id = ?", currentUser.ID)

	// Re-sync user to Gorse when profile changes (Task 2.3 & 2.4)
	// This updates recommendation preferences and user-as-item for follow recommendations
	if h.gorse != nil {
		go func() {
			userID := currentUser.ID
			// Sync user (for recommendation preferences like genre, DAW)
			if err := h.gorse.SyncUser(userID); err != nil {
				fmt.Printf("Warning: Failed to sync user %s to Gorse: %v\n", userID, err)
			}
			// Sync user-as-item (for follow recommendations - privacy, follower count, etc.)
			if err := h.gorse.SyncUserAsItem(userID); err != nil {
				fmt.Printf("Warning: Failed to sync user-as-item %s to Gorse: %v\n", userID, err)
			}
		}()
	}

	if h.stream != nil {
		go func() {
			customData := make(map[string]interface{})
			customData["display_name"] = currentUser.DisplayName
			customData["bio"] = currentUser.Bio
			customData["profile_picture_url"] = currentUser.ProfilePictureURL
			customData["genre"] = currentUser.Genre
			customData["daw_preference"] = currentUser.DAWPreference
			customData["is_private"] = currentUser.IsPrivate

			if err := h.stream.UpdateUserProfile(currentUser.StreamUserID, currentUser.Username, customData); err != nil {
				logger.WarnWithFields("Failed to sync user profile to Stream.io", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "profile_updated",
		"user": gin.H{
			"id":                  currentUser.ID,
			"username":            currentUser.Username,
			"display_name":        currentUser.DisplayName,
			"bio":                 currentUser.Bio,
			"location":            currentUser.Location,
			"daw_preference":      currentUser.DAWPreference,
			"genre":               currentUser.Genre,
			"social_links":        currentUser.SocialLinks,
			"profile_picture_url": currentUser.ProfilePictureURL,
			"is_private":          currentUser.IsPrivate,
		},
		"updated_at": time.Now().UTC(),
	})
}

// ChangeUsername allows users to change their username with uniqueness validation
// PUT /api/v1/users/username
func (h *Handlers) ChangeUsername(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var req struct {
		Username string `json:"username" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "username_required")
		return
	}

	newUsername := strings.TrimSpace(req.Username)

	// Validate username format
	if len(newUsername) < 3 {
		util.RespondBadRequest(c, "username_too_short", "Username must be at least 3 characters")
		return
	}

	if len(newUsername) > 30 {
		util.RespondBadRequest(c, "username_too_long", "Username must be at most 30 characters")
		return
	}

	// Only allow alphanumeric, underscores, and hyphens
	validUsername := regexp.MustCompile(`^[a-zA-Z0-9_-]+$`)
	if !validUsername.MatchString(newUsername) {
		util.RespondBadRequest(c, "username_invalid_chars", "Username can only contain letters, numbers, underscores, and hyphens")
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
		util.RespondError(c, http.StatusConflict, "username_taken", "This username is already taken")
		return
	}
	if err != gorm.ErrRecordNotFound {
		// Database error
		util.RespondInternalError(c, "database_error")
		return
	}

	// Update username
	oldUsername := currentUser.Username
	if err := database.DB.Model(currentUser).Update("username", newUsername).Error; err != nil {
		util.RespondInternalError(c, "update_failed", err.Error())
		return
	}

	if h.stream != nil {
		go func() {
			customData := make(map[string]interface{})
			customData["display_name"] = currentUser.DisplayName
			customData["bio"] = currentUser.Bio
			customData["profile_picture_url"] = currentUser.ProfilePictureURL
			if err := h.stream.UpdateUserProfile(currentUser.StreamUserID, newUsername, customData); err != nil {
				logger.WarnWithFields("Failed to sync username change to Stream.io", err)
			}
		}()
	}

	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncUser(currentUser.ID); err != nil {
				logger.WarnWithFields("Failed to sync username change to Gorse", err)
			}
		}()
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
	targetParam := c.Param("id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	// Validate user exists - accept ID, stream_user_id, or username
	// Cast id to text to avoid UUID type comparison errors with usernames
	var user models.User
	if err := database.DB.Where("CAST(id AS TEXT) = ? OR stream_user_id = ? OR username = ?", targetParam, targetParam, targetParam).First(&user).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Get followers from Stream.io
	followers, err := h.stream.GetFollowers(user.StreamUserID, limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_followers", err.Error())
		return
	}

	// Enrich with user data from database
	followerUsers := make([]gin.H, 0, len(followers))
	for _, f := range followers {
		var followerUser models.User
		if err := database.DB.First(&followerUser, "id = ? OR stream_user_id = ?", f.UserID, f.UserID).Error; err == nil {
			avatarURL := followerUser.ProfilePictureURL
			if avatarURL == "" {
				avatarURL = followerUser.OAuthProfilePictureURL
			}
			followerUsers = append(followerUsers, gin.H{
				"id":           followerUser.ID,
				"username":     followerUser.Username,
				"display_name": followerUser.DisplayName,
				"avatar_url":   avatarURL,
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
	targetParam := c.Param("id")
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Validate user exists - accept ID, stream_user_id, or username
	// Cast id to text to avoid UUID type comparison errors with usernames
	var user models.User
	if err := database.DB.Where("CAST(id AS TEXT) = ? OR stream_user_id = ? OR username = ?", targetParam, targetParam, targetParam).First(&user).Error; err != nil {
		if util.HandleDBError(c, err, "user") {
			return
		}
	}

	// Get following from Stream.io
	following, err := h.stream.GetFollowing(user.StreamUserID, limit, offset)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_following", err.Error())
		return
	}

	// Enrich with user data from database
	followingUsers := make([]gin.H, 0, len(following))
	for _, f := range following {
		var followedUser models.User
		if err := database.DB.First(&followedUser, "id = ? OR stream_user_id = ?", f.UserID, f.UserID).Error; err == nil {
			avatarURL := followedUser.ProfilePictureURL
			if avatarURL == "" {
				avatarURL = followedUser.OAuthProfilePictureURL
			}
			followingUsers = append(followingUsers, gin.H{
				"id":           followedUser.ID,
				"username":     followedUser.Username,
				"display_name": followedUser.DisplayName,
				"avatar_url":   avatarURL,
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
	targetParam := c.Param("id")
	currentUserID := c.GetString("user_id") // May be empty if not authenticated
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Validate user exists - accept ID, stream_user_id, or username
	// Cast id to text to avoid UUID type comparison errors with usernames
	var user models.User
	if err := database.DB.Where("CAST(id AS TEXT) = ? OR stream_user_id = ? OR username = ?", targetParam, targetParam, targetParam).First(&user).Error; err != nil {
		if util.HandleDBError(c, err, "user") {
			return
		}
	}

	// Check if user is private and if viewer has permission
	if user.IsPrivate && currentUserID != user.ID {
		// Not viewing own profile - check if following
		canView := false
		if currentUserID != "" && h.stream != nil {
			// Check if current user follows this private account
			isFollowing, _ := h.stream.CheckIsFollowing(currentUserID, user.StreamUserID)
			canView = isFollowing
		}

		if !canView {
			// Get effective avatar URL
			avatarURL := user.ProfilePictureURL
			if avatarURL == "" {
				avatarURL = user.OAuthProfilePictureURL
			}

			// Return empty posts with privacy message
			c.JSON(http.StatusOK, gin.H{
				"posts": []gin.H{},
				"user": gin.H{
					"id":           user.ID,
					"username":     user.Username,
					"display_name": user.DisplayName,
					"avatar_url":   avatarURL,
					"is_private":   true,
				},
				"meta": gin.H{
					"limit":      limit,
					"offset":     offset,
					"count":      0,
					"is_private": true,
					"message":    "This account is private. Follow to see their posts.",
				},
			})
			return
		}
	}

	// Check if current user is following this profile owner
	// All posts on a user profile are by the same user, so we check once
	isFollowingUser := false
	if currentUserID != "" && currentUserID != user.ID && h.stream != nil {
		isFollowingUser, _ = h.stream.CheckIsFollowing(currentUserID, user.StreamUserID)
		log.Printf("GetUserPosts: currentUserID=%s, targetUserID=%s, isFollowing=%v", currentUserID, user.ID, isFollowingUser)
	} else {
		log.Printf("GetUserPosts: Skipping follow check - currentUserID=%s, targetUserID=%s, ownProfile=%v", currentUserID, user.ID, currentUserID == user.ID)
	}

	// Get enriched activities from Stream.io
	// Note: Posts are created using user.ID, so we must query with user.ID (not StreamUserID)
	activities, err := h.stream.GetEnrichedUserFeed(user.ID, limit, offset)
	if err != nil {
		// Log the error but try database fallback
		log.Printf("Stream.io GetEnrichedUserFeed error (will try DB fallback): %v", err)
	}

	// If Stream.io returns empty or errored, fall back to database
	if len(activities) == 0 {
		log.Printf("GetUserPosts: Using database fallback (Stream.io returned %d activities, err=%v)", len(activities), err)
		var audioPosts []models.AudioPost
		if err := database.DB.
			Where("user_id = ? AND is_public = ? AND is_archived = ?", user.ID, true, false).
			Order("is_pinned DESC, pin_order ASC, created_at DESC").
			Limit(limit).
			Offset(offset).
			Find(&audioPosts).Error; err != nil {
			util.RespondInternalError(c, "failed_to_get_posts", err.Error())
			return
		}

		// Get effective avatar URL
		userAvatarURL := user.ProfilePictureURL
		if userAvatarURL == "" {
			userAvatarURL = user.OAuthProfilePictureURL
		}

		// Convert AudioPost to activity-like format for the client
		dbPosts := make([]gin.H, len(audioPosts))
		for i, post := range audioPosts {
			dbPosts[i] = gin.H{
				"id":               post.ID,
				"actor":            "user:" + post.UserID,
				"verb":             "post",
				"object":           "audio:" + post.ID,
				"time":             post.CreatedAt.Format(time.RFC3339),
				// User info (required by frontend)
				"user_id":          post.UserID,
				"username":         user.Username,
				"display_name":     user.DisplayName,
				"profile_picture_url": userAvatarURL,
				// Audio metadata
				"audio_url":        post.AudioURL,
				"waveform_url":     post.WaveformURL,
				"filename":         post.Filename,
				"duration_seconds": post.Duration,
				"duration_bars":    post.DurationBars,
				"bpm":              post.BPM,
				"key":              post.Key,
				"daw":              post.DAW,
				"genre":            post.Genre,
				// Engagement metrics
				"like_count":       post.LikeCount,
				"play_count":       post.PlayCount,
				"comment_count":    post.CommentCount,
				"has_midi":         post.MIDIPatternID != nil,
				"midi_pattern_id":  post.MIDIPatternID,
				"is_remix":         post.RemixOfPostID != nil || post.RemixOfStoryID != nil,
				"remix_of_post_id": post.RemixOfPostID,
				"remix_of_story_id": post.RemixOfStoryID,
				"remix_type":       post.RemixType,
				"remix_chain_depth": post.RemixChainDepth,
				"remix_count":      post.RemixCount,
				"status":           post.ProcessingStatus,
				"is_pinned":        post.IsPinned,
				"pin_order":        post.PinOrder,
				"is_following":     isFollowingUser,
				"is_own_post":      currentUserID == post.UserID,
				"actor_data": gin.H{
					"id":         user.ID,
					"username":   user.Username,
					"avatar_url": userAvatarURL,
				},
			}
		}

		log.Printf("GetUserPosts: Returning %d posts from database with is_following=%v", len(dbPosts), isFollowingUser)
		c.JSON(http.StatusOK, gin.H{
			"posts": dbPosts,
			"user": gin.H{
				"id":           user.ID,
				"username":     user.Username,
				"display_name": user.DisplayName,
				"avatar_url":   userAvatarURL,
			},
			"meta": gin.H{
				"limit":  limit,
				"offset": offset,
				"count":  len(dbPosts),
				"source": "database",
			},
		})
		return
	}

	// Get effective avatar URL for stream response
	streamAvatarURL := user.ProfilePictureURL
	if streamAvatarURL == "" {
		streamAvatarURL = user.OAuthProfilePictureURL
	}

	// Enrich activities with is_following state and user avatar
	// All posts from Stream.io are by the same user (profile owner), so apply to all
	log.Printf("GetUserPosts: Enriching %d activities from Stream.io with is_following=%v", len(activities), isFollowingUser)
	enrichedActivities := make([]gin.H, len(activities))
	for i, activity := range activities {
		// Convert EnrichedActivity to map using JSON marshal/unmarshal
		// This preserves all fields from the enriched activity
		var activityMap map[string]interface{}
		activityBytes, _ := json.Marshal(activity)
		json.Unmarshal(activityBytes, &activityMap)

		// Add is_following to the activity
		activityMap["is_following"] = isFollowingUser
		activityMap["is_own_post"] = currentUserID == user.ID

		// Add user avatar URL (required for PostCard in frontend)
		// Frontend looks for both user_avatar_url and profile_picture_url
		activityMap["user_avatar_url"] = streamAvatarURL
		activityMap["profile_picture_url"] = user.ProfilePictureURL

		enrichedActivities[i] = activityMap
	}

	log.Printf("GetUserPosts: Returning %d posts with is_following=%v", len(enrichedActivities), isFollowingUser)

	c.JSON(http.StatusOK, gin.H{
		"posts": enrichedActivities,
		"user": gin.H{
			"id":           user.ID,
			"username":     user.Username,
			"display_name": user.DisplayName,
			"avatar_url":   streamAvatarURL,
		},
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(enrichedActivities),
			"source": "stream",
		},
	})
}

// FollowUserByID follows a user by ID (path param version)
// POST /api/users/:id/follow
// If target user is private, creates a follow request instead
func (h *Handlers) FollowUserByID(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}
	targetParam := c.Param("id")

	// Validate target user exists - accept ID, stream_user_id, or username
	// Cast id to text to avoid UUID type comparison errors with usernames
	var targetUser models.User
	if err := database.DB.Where("CAST(id AS TEXT) = ? OR stream_user_id = ? OR username = ?", targetParam, targetParam, targetParam).First(&targetUser).Error; err != nil {
		if util.HandleDBError(c, err, "user") {
			return
		}
	}

	// Can't follow yourself
	if currentUser.ID == targetUser.ID {
		util.RespondBadRequest(c, "cannot_follow_self")
		return
	}

	// Check if target account is private
	if targetUser.IsPrivate {
		// Check if there's already a pending request
		var existingRequest models.FollowRequest
		err := database.DB.Where("requester_id = ? AND target_id = ? AND status = ?",
			currentUser.ID, targetUser.ID, models.FollowRequestStatusPending).
			First(&existingRequest).Error

		if err == nil {
			// Already have a pending request
			c.JSON(http.StatusOK, gin.H{
				"status":      "requested",
				"target_user": targetUser.ID,
				"username":    targetUser.Username,
				"request_id":  existingRequest.ID,
				"message":     "Follow request already pending",
			})
			return
		}

		if err != gorm.ErrRecordNotFound {
			util.RespondInternalError(c, "check_failed", "Failed to check existing request")
			return
		}

		// Check if they previously rejected and we're trying again - allow it
		// First delete any old rejected request
		database.DB.Where("requester_id = ? AND target_id = ? AND status = ?",
			currentUser.ID, targetUser.ID, models.FollowRequestStatusRejected).
			Delete(&models.FollowRequest{})

		// Create a new follow request
		followRequest := models.FollowRequest{
			RequesterID: currentUser.ID,
			TargetID:    targetUser.ID,
			Status:      models.FollowRequestStatusPending,
		}

		if err := database.DB.Create(&followRequest).Error; err != nil {
			util.RespondInternalError(c, "request_failed", "Failed to create follow request")
			return
		}

		// TODO: Send notification to target user about new follow request

		c.JSON(http.StatusOK, gin.H{
			"status":      "requested",
			"target_user": targetUser.ID,
			"username":    targetUser.Username,
			"request_id":  followRequest.ID,
			"message":     "Follow request sent",
		})
		return
	}

	// Public account - follow directly via Stream.io
	// Use database IDs (not Stream IDs) to match CheckIsFollowing behavior
	if err := h.stream.FollowUser(currentUser.ID, targetUser.ID); err != nil {
		util.RespondInternalError(c, "follow_failed", err.Error())
		return
	}

	// Sync follow event to Gorse for recommendations (async, don't block response)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFollowEvent(currentUser.ID, targetUser.ID); err != nil {
				log.Printf("Warning: failed to sync follow to Gorse: %v", err)
			}
		}()
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
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}
	targetParam := c.Param("id")

	// Validate target user exists - accept ID, stream_user_id, or username
	// Cast id to text to avoid UUID type comparison errors with usernames
	var targetUser models.User
	if err := database.DB.Where("CAST(id AS TEXT) = ? OR stream_user_id = ? OR username = ?", targetParam, targetParam, targetParam).First(&targetUser).Error; err != nil {
		if util.HandleDBError(c, err, "user") {
			return
		}
	}

	// Unfollow via Stream.io
	// Use database IDs (not Stream IDs) to match CheckIsFollowing behavior
	if err := h.stream.UnfollowUser(currentUser.ID, targetUser.ID); err != nil {
		util.RespondInternalError(c, "unfollow_failed", err.Error())
		return
	}

	// Remove follow event from Gorse for recommendations (async, don't block response)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.RemoveFollowEvent(currentUser.ID, targetUser.ID); err != nil {
				log.Printf("Warning: failed to remove follow from Gorse: %v", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":      "unfollowed",
		"target_user": targetUser.ID,
	})
}

