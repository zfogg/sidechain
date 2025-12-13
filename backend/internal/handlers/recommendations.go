package handlers

import (
	"net/http"
	"os"
	"strconv"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
)

// GetForYouFeed returns personalized recommendations for the current user
// GET /api/v1/recommendations/for-you
func (h *Handlers) GetForYouFeed(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}

	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "20"))
	offset, _ := strconv.Atoi(c.DefaultQuery("offset", "0"))

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

	// Get recommendations
	scores, err := recService.GetForYouFeed(userID, limit, offset)
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

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
		},
	})
}

// GetSimilarPosts returns posts similar to a given post
// GET /api/v1/recommendations/similar-posts/:post_id
func (h *Handlers) GetSimilarPosts(c *gin.Context) {
	postID := c.Param("post_id")
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

	// Get similar posts
	posts, err := recService.GetSimilarPosts(postID, limit)
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

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit": limit,
			"count": len(activities),
		},
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
