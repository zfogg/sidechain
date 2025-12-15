package handlers

import (
	"fmt"
	"net/http"
	"os"
	"strconv"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/util"
)

// GetForYouFeed returns personalized recommendations for the current user
// GET /api/v1/recommendations/for-you
// Query params: ?genre=electronic&min_bpm=120&max_bpm=140
// Task 6.2, 6.3
func (h *Handlers) GetForYouFeed(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))
	genre := c.Query("genre")
	minBPM, _ := strconv.Atoi(c.Query("min_bpm"))
	maxBPM, _ := strconv.Atoi(c.Query("max_bpm"))

	if limit > 100 {
		limit = 100
	}

	// Create Gorse recommendation service (using REST client)
	gorseURL := os.Getenv("GORSE_URL")
	if gorseURL == "" {
		gorseURL = "http://localhost:8087"
	}
	gorseAPIKey := os.Getenv("GORSE_API_KEY")
	if gorseAPIKey == "" {
		gorseAPIKey = "sidechain_gorse_api_key"
	}
	recService := recommendations.NewGorseRESTClient(gorseURL, gorseAPIKey, database.DB)

	// Get recommendations with optional filters
	var scores []recommendations.PostScore
	var err error

	if genre != "" {
		// Filter by genre (Task 6.1, 6.2)
		scores, err = recService.GetForYouFeedByGenre(userID, genre, limit, offset)
	} else if minBPM > 0 || maxBPM > 0 {
		// Filter by BPM range (Task 6.3)
		scores, err = recService.GetForYouFeedByBPMRange(userID, minBPM, maxBPM, limit, offset)
	} else {
		// No filters, get general recommendations
		scores, err = recService.GetForYouFeed(userID, limit, offset)
	}

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_recommendations",
			"message": err.Error(),
		})
		return
	}

	// Convert to activities format (similar to feed endpoints)
	activities := make([]map[string]interface{}, 0, len(scores))
	for _, score := range scores {
		activity := map[string]interface{}{
			"id":                    score.Post.ID,
			"audio_url":             score.Post.AudioURL,
			"bpm":                   score.Post.BPM,
			"key":                   score.Post.Key,
			"daw":                   score.Post.DAW,
			"duration":              score.Post.Duration,
			"genre":                 score.Post.Genre,
			"created_at":            score.Post.CreatedAt,
			"like_count":            score.Post.LikeCount,
			"play_count":            score.Post.PlayCount,
			"recommendation_reason": score.Reason,
		}
		activities = append(activities, activity)
	}

	meta := gin.H{
		"limit":  limit,
		"offset": offset,
		"count":  len(activities),
	}
	if genre != "" {
		meta["filter_genre"] = genre
	}
	if minBPM > 0 {
		meta["filter_min_bpm"] = minBPM
	}
	if maxBPM > 0 {
		meta["filter_max_bpm"] = maxBPM
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta":       meta,
	})
}

// GetSimilarPosts returns posts similar to a given post
// GET /api/v1/recommendations/similar-posts/:post_id
// Query params: ?genre=electronic (Task 6.4)
func (h *Handlers) GetSimilarPosts(c *gin.Context) {
	postID := c.Param("post_id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "10"))
	genre := c.Query("genre")

	if limit > 50 {
		limit = 50
	}

	// Create Gorse recommendation service (using REST client)
	gorseURL := os.Getenv("GORSE_URL")
	if gorseURL == "" {
		gorseURL = "http://localhost:8087"
	}
	gorseAPIKey := os.Getenv("GORSE_API_KEY")
	if gorseAPIKey == "" {
		gorseAPIKey = "sidechain_gorse_api_key"
	}
	recService := recommendations.NewGorseRESTClient(gorseURL, gorseAPIKey, database.DB)

	// Get similar posts with optional genre filter (Task 6.4)
	var posts []models.AudioPost
	var err error

	if genre != "" {
		posts, err = recService.GetSimilarPostsByGenre(postID, genre, limit)
	} else {
		posts, err = recService.GetSimilarPosts(postID, limit)
	}

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_similar_posts",
			"message": err.Error(),
		})
		return
	}

	// Convert to activities format
	activities := make([]map[string]interface{}, 0, len(posts))
	for _, post := range posts {
		activity := map[string]interface{}{
			"id":         post.ID,
			"audio_url":  post.AudioURL,
			"bpm":        post.BPM,
			"key":        post.Key,
			"daw":        post.DAW,
			"duration":   post.Duration,
			"genre":      post.Genre,
			"created_at": post.CreatedAt,
			"like_count": post.LikeCount,
			"play_count": post.PlayCount,
		}
		activities = append(activities, activity)
	}

	meta := gin.H{
		"limit": limit,
		"count": len(activities),
	}
	if genre != "" {
		meta["filter_genre"] = genre
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta":       meta,
	})
}

// GetUsersToFollow returns recommended users to follow based on collaborative filtering
// GET /api/v1/users/recommended
func (h *Handlers) GetUsersToFollow(c *gin.Context) {
	currentUserID := c.GetString("user_id")
	if currentUserID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	if limit > 50 {
		limit = 50
	}

	// Use the injected Gorse client if available
	var recService *recommendations.GorseRESTClient
	if h.gorse != nil {
		recService = h.gorse
	} else {
		// Fallback to creating a new client
		gorseURL := os.Getenv("GORSE_URL")
		if gorseURL == "" {
			gorseURL = "http://localhost:8087"
		}
		gorseAPIKey := os.Getenv("GORSE_API_KEY")
		if gorseAPIKey == "" {
			gorseAPIKey = "sidechain_gorse_api_key"
		}
		recService = recommendations.NewGorseRESTClient(gorseURL, gorseAPIKey, database.DB)
	}

	// Get recommended users to follow
	userScores, err := recService.GetUsersToFollow(currentUserID, limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_recommendations",
			"message": err.Error(),
		})
		return
	}

	// Convert to user format with recommendation reasons
	userList := make([]map[string]interface{}, 0, len(userScores))
	for _, us := range userScores {
		userData := map[string]interface{}{
			"id":             us.User.ID,
			"username":       us.User.Username,
			"display_name":   us.User.DisplayName,
			"avatar_url":     us.User.GetAvatarURL(),
			"bio":            us.User.Bio,
			"genre":          us.User.Genre,
			"daw_preference": us.User.DAWPreference,
			"follower_count": us.User.FollowerCount,
			"reason":         us.Reason,
		}
		userList = append(userList, userData)
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userList,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(userList),
		},
	})
}

// GetRecommendedUsers returns users with similar taste (recommendations)
// GET /api/v1/recommendations/similar-users/:user_id
func (h *Handlers) GetRecommendedUsers(c *gin.Context) {
	currentUserID := c.GetString("user_id")

	if currentUserID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "10"))

	if limit > 50 {
		limit = 50
	}

	// Create Gorse recommendation service (using REST client)
	gorseURL := os.Getenv("GORSE_URL")
	if gorseURL == "" {
		gorseURL = "http://localhost:8087"
	}
	gorseAPIKey := os.Getenv("GORSE_API_KEY")
	if gorseAPIKey == "" {
		gorseAPIKey = "sidechain_gorse_api_key"
	}
	recService := recommendations.NewGorseRESTClient(gorseURL, gorseAPIKey, database.DB)

	// Get similar users (using current user's preferences to find similar users)
	users, err := recService.GetSimilarUsers(currentUserID, limit)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_similar_users",
			"message": err.Error(),
		})
		return
	}

	// Convert to user format
	userList := make([]map[string]interface{}, 0, len(users))
	for _, user := range users {
		userData := map[string]interface{}{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.GetAvatarURL(),
			"bio":            user.Bio,
			"genre":          user.Genre,
			"daw_preference": user.DAWPreference,
		}
		userList = append(userList, userData)
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userList,
		"meta": gin.H{
			"limit": limit,
			"count": len(userList),
		},
	})
}

// NotInterestedInPost marks a post as "not interested" (negative feedback)
// POST /api/v1/recommendations/dislike/:post_id
// Task 5.2
func (h *Handlers) NotInterestedInPost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("post_id")
	if postID == "" {
		util.RespondBadRequest(c, "post_id_required")
		return
	}

	// Send negative feedback to Gorse (Task 5.2)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFeedback(userID, postID, "dislike"); err != nil {
				fmt.Printf("Warning: Failed to sync dislike to Gorse: %v\n", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "not_interested_recorded",
		"post_id": postID,
	})
}

// SkipPost records when a user skips past a post (negative signal)
// POST /api/v1/recommendations/skip/:post_id
// Task 5.3
func (h *Handlers) SkipPost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("post_id")
	if postID == "" {
		util.RespondBadRequest(c, "post_id_required")
		return
	}

	// Send skip feedback to Gorse (Task 5.3)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFeedback(userID, postID, "skip"); err != nil {
				fmt.Printf("Warning: Failed to sync skip to Gorse: %v\n", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "skip_recorded",
		"post_id": postID,
	})
}

// HidePost hides a post from the user's feed (strongest negative signal)
// POST /api/v1/recommendations/hide/:post_id
// Task 5.4
func (h *Handlers) HidePost(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	postID := c.Param("post_id")
	if postID == "" {
		util.RespondBadRequest(c, "post_id_required")
		return
	}

	// Send hide feedback to Gorse (Task 5.4)
	if h.gorse != nil {
		go func() {
			if err := h.gorse.SyncFeedback(userID, postID, "hide"); err != nil {
				fmt.Printf("Warning: Failed to sync hide to Gorse: %v\n", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "post_hidden",
		"post_id": postID,
		"message": "This post will no longer appear in your recommendations",
	})
}

// GetPopular returns globally popular posts based on engagement metrics
// GET /api/v1/recommendations/popular
// Query params: ?limit=20&offset=0
// Task 7.3
func (h *Handlers) GetPopular(c *gin.Context) {
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	if limit > 100 {
		limit = 100
	}

	// Use the injected Gorse client if available
	var recService *recommendations.GorseRESTClient
	if h.gorse != nil {
		recService = h.gorse
	} else {
		// Fallback to creating a new client
		gorseURL := os.Getenv("GORSE_URL")
		if gorseURL == "" {
			gorseURL = "http://localhost:8087"
		}
		gorseAPIKey := os.Getenv("GORSE_API_KEY")
		if gorseAPIKey == "" {
			gorseAPIKey = "sidechain_gorse_api_key"
		}
		recService = recommendations.NewGorseRESTClient(gorseURL, gorseAPIKey, database.DB)
	}

	// Get popular posts
	scores, err := recService.GetPopular(limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_popular_posts",
			"message": err.Error(),
		})
		return
	}

	// Convert to activities format
	activities := make([]map[string]interface{}, 0, len(scores))
	for _, score := range scores {
		activity := map[string]interface{}{
			"id":                    score.Post.ID,
			"audio_url":             score.Post.AudioURL,
			"bpm":                   score.Post.BPM,
			"key":                   score.Post.Key,
			"daw":                   score.Post.DAW,
			"duration":              score.Post.Duration,
			"genre":                 score.Post.Genre,
			"created_at":            score.Post.CreatedAt,
			"like_count":            score.Post.LikeCount,
			"play_count":            score.Post.PlayCount,
			"recommendation_reason": score.Reason,
			"score":                 score.Score,
		}
		activities = append(activities, activity)
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
			"source": "popular",
		},
	})
}

// GetLatest returns recently added posts
// GET /api/v1/recommendations/latest
// Query params: ?limit=20&offset=0
// Task 7.3
func (h *Handlers) GetLatest(c *gin.Context) {
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

	if limit > 100 {
		limit = 100
	}

	// Use the injected Gorse client if available
	var recService *recommendations.GorseRESTClient
	if h.gorse != nil {
		recService = h.gorse
	} else {
		// Fallback to creating a new client
		gorseURL := os.Getenv("GORSE_URL")
		if gorseURL == "" {
			gorseURL = "http://localhost:8087"
		}
		gorseAPIKey := os.Getenv("GORSE_API_KEY")
		if gorseAPIKey == "" {
			gorseAPIKey = "sidechain_gorse_api_key"
		}
		recService = recommendations.NewGorseRESTClient(gorseURL, gorseAPIKey, database.DB)
	}

	// Get latest posts
	scores, err := recService.GetLatest(limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_latest_posts",
			"message": err.Error(),
		})
		return
	}

	// Convert to activities format
	activities := make([]map[string]interface{}, 0, len(scores))
	for _, score := range scores {
		activity := map[string]interface{}{
			"id":                    score.Post.ID,
			"audio_url":             score.Post.AudioURL,
			"bpm":                   score.Post.BPM,
			"key":                   score.Post.Key,
			"daw":                   score.Post.DAW,
			"duration":              score.Post.Duration,
			"genre":                 score.Post.Genre,
			"created_at":            score.Post.CreatedAt,
			"like_count":            score.Post.LikeCount,
			"play_count":            score.Post.PlayCount,
			"recommendation_reason": score.Reason,
			"score":                 score.Score,
		}
		activities = append(activities, activity)
	}

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
			"source": "latest",
		},
	})
}
