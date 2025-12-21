package handlers

import (
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/util"
)

// trackImpression records that a recommendation was shown to a user
func trackImpression(userID, postID, source string, position int, score float64, reason string) {
	impression := models.RecommendationImpression{
		UserID:   userID,
		PostID:   postID,
		Source:   source,
		Position: position,
		Score:    &score,
		Reason:   &reason,
	}

	// Async write - don't block the request
	go func() {
		if err := database.DB.Create(&impression).Error; err != nil {
			fmt.Printf("Warning: Failed to track impression: %v\n", err)
		}
	}()
}

// trackImpressions records multiple recommendation impressions
func trackImpressions(userID, source string, scores []recommendations.PostScore) {
	if len(scores) == 0 {
		return
	}

	impressions := make([]models.RecommendationImpression, 0, len(scores))
	for i, score := range scores {
		impressions = append(impressions, models.RecommendationImpression{
			UserID:   userID,
			PostID:   score.Post.ID,
			Source:   source,
			Position: i,
			Score:    &score.Score,
			Reason:   &score.Reason,
		})
	}

	// Async batch insert - don't block the request
	go func() {
		if err := database.DB.CreateInBatches(&impressions, 100).Error; err != nil {
			fmt.Printf("Warning: Failed to track %d impressions: %v\n", len(impressions), err)
		}
	}()
}

// GetForYouFeed returns personalized recommendations for the current user
// GET /api/v1/recommendations/for-you
// Query params: ?genre=electronic&min_bpm=120&max_bpm=140
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

	// Use the centralized Gorse client from handlers
	if h.kernel.Gorse() == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"error":   "recommendations_unavailable",
			"message": "Recommendation service is not configured",
		})
		return
	}
	recService := h.kernel.Gorse()

	// Get recommendations with optional filters
	var scores []recommendations.PostScore
	var err error
	var recommendationType string

	if genre != "" {
		// Filter by genre
		recommendationType = "for-you-by-genre"
		scores, err = recService.GetForYouFeedByGenre(userID, genre, limit, offset)
	} else if minBPM > 0 || maxBPM > 0 {
		// Filter by BPM range
		recommendationType = "for-you-by-bpm"
		scores, err = recService.GetForYouFeedByBPMRange(userID, minBPM, maxBPM, limit, offset)
	} else {
		// No filters, get general recommendations
		recommendationType = "for-you"
		scores, err = recService.GetForYouFeed(userID, limit, offset)
	}

	if err != nil {
		// Record Gorse error metric
		metrics.Get().GorseErrors.WithLabelValues("recommendation_fetch").Inc()
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_recommendations",
			"message": err.Error(),
		})
		return
	}

	// Record Gorse recommendations fetched
	metrics.Get().GorseRecommendations.WithLabelValues(recommendationType).Add(float64(len(scores)))

	// Convert to activities format with user enrichment
	activities := make([]map[string]interface{}, 0, len(scores))
	for _, score := range scores {
		// Fetch user data
		var user models.User
		database.DB.Where("id = ?", score.Post.UserID).First(&user)

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
			"user": gin.H{
				"id":         user.ID,
				"username":   user.Username,
				"avatar_url": user.ProfilePictureURL,
			},
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

	// Track impressions for CTR analysis
	trackImpressions(userID, "for-you", scores)

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta":       meta,
	})
}

// GetSimilarPosts returns posts similar to a given post
// GET /api/v1/recommendations/similar-posts/:post_id
// Query params: ?genre=electronic
func (h *Handlers) GetSimilarPosts(c *gin.Context) {
	postID := c.Param("post_id")
	limit, _ := strconv.Atoi(c.DefaultQuery("limit", "10"))
	genre := c.Query("genre")

	if limit > 50 {
		limit = 50
	}

	// Use the centralized Gorse client from handlers
	if h.kernel.Gorse() == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"error":   "recommendations_unavailable",
			"message": "Recommendation service is not configured",
		})
		return
	}
	recService := h.kernel.Gorse()

	// Get similar posts with optional genre filter
	var posts []models.AudioPost
	var err error
	var recommendationType string

	if genre != "" {
		recommendationType = "similar-posts-by-genre"
		posts, err = recService.GetSimilarPostsByGenre(postID, genre, limit)
	} else {
		recommendationType = "similar-posts"
		posts, err = recService.GetSimilarPosts(postID, limit)
	}

	if err != nil {
		// Record Gorse error metric
		metrics.Get().GorseErrors.WithLabelValues("similar_posts").Inc()
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_similar_posts",
			"message": err.Error(),
		})
		return
	}

	// Record Gorse recommendations fetched
	metrics.Get().GorseRecommendations.WithLabelValues(recommendationType).Add(float64(len(posts)))

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

	// Use the centralized Gorse client from handlers
	if h.kernel.Gorse() == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"error":   "recommendations_unavailable",
			"message": "Recommendation service is not configured",
		})
		return
	}
	recService := h.kernel.Gorse()

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

	// Use the centralized Gorse client from handlers
	if h.kernel.Gorse() == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"error":   "recommendations_unavailable",
			"message": "Recommendation service is not configured",
		})
		return
	}
	recService := h.kernel.Gorse()

	// Get similar users (using current user's preferences to find similar users)
	users, err := recService.GetSimilarUsers(currentUserID, limit)
	if err != nil {
		// Record Gorse error metric
		metrics.Get().GorseErrors.WithLabelValues("user_recommendation").Inc()
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_similar_users",
			"message": err.Error(),
		})
		return
	}

	// Record Gorse recommendations fetched
	metrics.Get().GorseRecommendations.WithLabelValues("similar-users").Add(float64(len(users)))

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

	// Send negative feedback to Gorse
	if h.kernel.Gorse() != nil {
		go func() {
			if err := h.kernel.Gorse().SyncFeedback(userID, postID, "dislike"); err != nil {
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

	// Send skip feedback to Gorse
	if h.kernel.Gorse() != nil {
		go func() {
			if err := h.kernel.Gorse().SyncFeedback(userID, postID, "skip"); err != nil {
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

	// Send hide feedback to Gorse
	if h.kernel.Gorse() != nil {
		go func() {
			if err := h.kernel.Gorse().SyncFeedback(userID, postID, "hide"); err != nil {
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
// GetPopular returns globally popular posts based on engagement metrics
// GET /api/v1/recommendations/popular
// Query params: ?limit=20&offset=0
func (h *Handlers) GetPopular(c *gin.Context) {
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

	// Use the centralized Gorse client from handlers
	if h.kernel.Gorse() == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"error":   "recommendations_unavailable",
			"message": "Recommendation service is not configured",
		})
		return
	}
	recService := h.kernel.Gorse()

	// Get popular posts
	scores, err := recService.GetPopular(limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_popular_posts",
			"message": err.Error(),
		})
		return
	}

	// Convert to activities format with user enrichment
	activities := make([]map[string]interface{}, 0, len(scores))
	for _, score := range scores {
		// Fetch user data
		var user models.User
		database.DB.Where("id = ?", score.Post.UserID).First(&user)

		activity := map[string]interface{}{
			"id":                    score.Post.ID,
			"audio_url":             score.Post.AudioURL,
			"waveform_url":          score.Post.WaveformURL,
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
			"user": gin.H{
				"id":         user.ID,
				"username":   user.Username,
				"avatar_url": user.ProfilePictureURL,
			},
		}
		activities = append(activities, activity)
	}

	// Track impressions for CTR analysis
	trackImpressions(userID, "popular", scores)

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
func (h *Handlers) GetLatest(c *gin.Context) {
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

	// Use the centralized Gorse client from handlers
	if h.kernel.Gorse() == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"error":   "recommendations_unavailable",
			"message": "Recommendation service is not configured",
		})
		return
	}
	recService := h.kernel.Gorse()

	// Get latest posts
	scores, err := recService.GetLatest(limit, offset)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_latest_posts",
			"message": err.Error(),
		})
		return
	}

	// Convert to activities format with user enrichment
	activities := make([]map[string]interface{}, 0, len(scores))
	for _, score := range scores {
		// Fetch user data
		var user models.User
		database.DB.Where("id = ?", score.Post.UserID).First(&user)

		activity := map[string]interface{}{
			"id":                    score.Post.ID,
			"audio_url":             score.Post.AudioURL,
			"waveform_url":          score.Post.WaveformURL,
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
			"user": gin.H{
				"id":         user.ID,
				"username":   user.Username,
				"avatar_url": user.ProfilePictureURL,
			},
		}
		activities = append(activities, activity)
	}

	// Track impressions for CTR analysis
	trackImpressions(userID, "latest", scores)

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

// GetDiscoveryFeed returns a blended feed of popular, latest, and personalized content
// GET /api/v1/recommendations/discovery-feed
// Query params: ?limit=20&offset=0
// Mix 30% popular, 20% latest, 50% personalized
func (h *Handlers) GetDiscoveryFeed(c *gin.Context) {
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

	// Use the centralized Gorse client from handlers
	if h.kernel.Gorse() == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"error":   "recommendations_unavailable",
			"message": "Recommendation service is not configured",
		})
		return
	}
	recService := h.kernel.Gorse()

	// Calculate how many items to fetch from each source
	// Ratio: 30% popular, 20% latest, 50% personalized
	popularCount := int(float64(limit) * 0.3)
	latestCount := int(float64(limit) * 0.2)
	personalizedCount := limit - popularCount - latestCount // Remaining goes to personalized

	// Fetch from all three sources concurrently
	type result struct {
		popular      []recommendations.PostScore
		latest       []recommendations.PostScore
		personalized []recommendations.PostScore
		err          error
	}

	resultChan := make(chan result, 1)
	go func() {
		var res result

		// Fetch popular posts
		popular, err := recService.GetPopular(popularCount, offset)
		if err != nil {
			res.err = fmt.Errorf("failed to get popular posts: %w", err)
			resultChan <- res
			return
		}
		res.popular = popular

		// Fetch latest posts
		latest, err := recService.GetLatest(latestCount, offset)
		if err != nil {
			res.err = fmt.Errorf("failed to get latest posts: %w", err)
			resultChan <- res
			return
		}
		res.latest = latest

		// Fetch personalized recommendations
		personalized, err := recService.GetForYouFeed(userID, personalizedCount, offset)
		if err != nil {
			res.err = fmt.Errorf("failed to get personalized feed: %w", err)
			resultChan <- res
			return
		}
		res.personalized = personalized

		resultChan <- res
	}()

	res := <-resultChan
	if res.err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "failed_to_get_discovery_feed",
			"message": res.err.Error(),
		})
		return
	}

	// Interleave the three sources to create a diverse feed
	// Pattern: personalized, popular, personalized, latest, personalized, popular, etc.
	blendedScores := make([]recommendations.PostScore, 0, limit)
	seenIDs := make(map[string]bool)

	popIdx, latIdx, persIdx := 0, 0, 0
	for len(blendedScores) < limit {
		added := false

		// Try to add personalized (50% of total)
		if persIdx < len(res.personalized) {
			post := res.personalized[persIdx]
			if !seenIDs[post.Post.ID] {
				blendedScores = append(blendedScores, post)
				seenIDs[post.Post.ID] = true
				added = true
			}
			persIdx++
		}

		if len(blendedScores) >= limit {
			break
		}

		// Try to add popular (30% of total)
		if popIdx < len(res.popular) {
			post := res.popular[popIdx]
			if !seenIDs[post.Post.ID] {
				blendedScores = append(blendedScores, post)
				seenIDs[post.Post.ID] = true
				added = true
			}
			popIdx++
		}

		if len(blendedScores) >= limit {
			break
		}

		// Try to add latest (20% of total)
		if latIdx < len(res.latest) {
			post := res.latest[latIdx]
			if !seenIDs[post.Post.ID] {
				blendedScores = append(blendedScores, post)
				seenIDs[post.Post.ID] = true
				added = true
			}
			latIdx++
		}

		// If we couldn't add anything from any source, break to avoid infinite loop
		if !added && popIdx >= len(res.popular) && latIdx >= len(res.latest) && persIdx >= len(res.personalized) {
			break
		}
	}

	// Convert to activities format with user enrichment
	activities := make([]map[string]interface{}, 0, len(blendedScores))
	for _, score := range blendedScores {
		// Fetch user data
		var user models.User
		database.DB.Where("id = ?", score.Post.UserID).First(&user)

		activity := map[string]interface{}{
			"id":                    score.Post.ID,
			"audio_url":             score.Post.AudioURL,
			"waveform_url":          score.Post.WaveformURL,
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
			"user": gin.H{
				"id":         user.ID,
				"username":   user.Username,
				"avatar_url": user.ProfilePictureURL,
			},
		}
		activities = append(activities, activity)
	}

	// Track impressions for CTR analysis
	trackImpressions(userID, "discovery", blendedScores)

	c.JSON(http.StatusOK, gin.H{
		"activities": activities,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(activities),
			"source": "discovery",
			"blend": gin.H{
				"popular_count":      len(res.popular),
				"latest_count":       len(res.latest),
				"personalized_count": len(res.personalized),
				"blend_ratio":        "30% popular, 20% latest, 50% personalized",
			},
		},
	})
}

// TrackRecommendationClick tracks when a user clicks/plays a recommended post
// POST /api/v1/recommendations/click
// Body: {"post_id": "uuid", "source": "for-you", "position": 0, "play_duration": 45.2, "completed": true}
func (h *Handlers) TrackRecommendationClick(c *gin.Context) {
	userID, ok := util.GetUserIDFromContext(c)
	if !ok {
		return
	}

	var req struct {
		PostID       string   `json:"post_id" binding:"required"`
		Source       string   `json:"source" binding:"required"` // "for-you", "popular", "latest", "discovery", "similar"
		Position     *int     `json:"position"`
		PlayDuration *float64 `json:"play_duration"`
		Completed    bool     `json:"completed"`
		SessionID    *string  `json:"session_id"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Create click record
	click := models.RecommendationClick{
		UserID:       userID,
		PostID:       req.PostID,
		Source:       req.Source,
		Position:     req.Position,
		PlayDuration: req.PlayDuration,
		Completed:    req.Completed,
		SessionID:    req.SessionID,
	}

	// Try to find the corresponding impression to link them
	var impression models.RecommendationImpression
	err := database.DB.Where("user_id = ? AND post_id = ? AND source = ? AND clicked = false",
		userID, req.PostID, req.Source).
		Order("created_at DESC").
		First(&impression).Error

	if err == nil {
		// Found the impression, link them and mark as clicked
		click.RecommendationImpressionID = &impression.ID

		// Update impression as clicked
		go func() {
			now := time.Now()
			database.DB.Model(&impression).Updates(map[string]interface{}{
				"clicked":    true,
				"clicked_at": now,
			})
		}()
	}

	// Save the click
	if err := database.DB.Create(&click).Error; err != nil {
		util.RespondInternalError(c, "failed_to_track_click", err.Error())
		return
	}

	// Send to Gorse as stronger signal if they completed playback
	if h.kernel.Gorse() != nil && req.Completed {
		go func() {
			if err := h.kernel.Gorse().SyncFeedback(userID, req.PostID, "like"); err != nil {
				fmt.Printf("Warning: Failed to sync completed play to Gorse: %v\n", err)
			}
		}()
	}

	c.JSON(http.StatusOK, gin.H{
		"status":  "click_tracked",
		"post_id": req.PostID,
		"source":  req.Source,
	})
}

// GetCTRMetrics returns click-through rate metrics for recommendation sources
// GET /api/v1/recommendations/metrics/ctr
// Query params: ?period=24h (24h, 7d, 30d)
func (h *Handlers) GetCTRMetrics(c *gin.Context) {
	period := c.DefaultQuery("period", "24h")

	var since time.Time
	switch period {
	case "24h":
		since = time.Now().Add(-24 * time.Hour)
	case "7d":
		since = time.Now().Add(-7 * 24 * time.Hour)
	case "30d":
		since = time.Now().Add(-30 * 24 * time.Hour)
	default:
		util.RespondBadRequest(c, "invalid_period", "Period must be one of: 24h, 7d, 30d")
		return
	}

	metrics, err := recommendations.CalculateCTR(database.DB, since)
	if err != nil {
		util.RespondInternalError(c, "failed_to_calculate_ctr", err.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"metrics": metrics,
		"period":  period,
		"since":   since,
	})
}
