package handlers

import (
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
)

// enrichUsersWithFollowState adds is_following field to user results
// Use database IDs (NOT Stream IDs) to match FollowUser behavior
func (h *Handlers) enrichUsersWithFollowState(currentUserID string, users []models.User) []gin.H {
	userResults := make([]gin.H, 0, len(users))

	for _, user := range users {
		// Check if current user is following this user
		// Use database IDs - no caching for fresh data
		isFollowing := false
		if currentUserID != "" && user.ID != "" && user.ID != currentUserID {
			isFollowing, _ = h.stream.CheckIsFollowing(currentUserID, user.ID)
		}

		userResults = append(userResults, gin.H{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.GetAvatarURL(),
			"bio":            user.Bio,
			"follower_count": user.FollowerCount,
			"genre":          user.Genre,
			"is_following":   isFollowing,
		})
	}

	return userResults
}

// SearchUsers searches for users by username or display name
// GET /api/search/users?q=query&limit=20&offset=0
func (h *Handlers) SearchUsers(c *gin.Context) {
	query := c.Query("q")
	if query == "" {
		util.RespondBadRequest(c, "search_query_required")
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Sanitize and prepare search pattern
	searchPattern := "%" + query + "%"

	var users []models.User
	result := database.DB.Where(
		"username ILIKE ? OR display_name ILIKE ?",
		searchPattern, searchPattern,
	).Order("follower_count DESC").Limit(limit).Offset(offset).Find(&users)

	if result.Error != nil {
		util.RespondInternalError(c, "search_failed", result.Error.Error())
		return
	}

	// Get current user's stream ID for follow state enrichment
	currentUserID := ""
	if currentUser, ok := util.GetUserFromContext(c); ok {
		currentUserID = currentUser.ID
	}

	// Enrich with is_following state
	userResults := h.enrichUsersWithFollowState(currentUserID, users)

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"meta": gin.H{
			"query":  query,
			"limit":  limit,
			"offset": offset,
			"count":  len(userResults),
		},
	})
}

// GetTrendingUsers returns users trending based on recent activity
// GET /api/discover/trending?limit=20&period=week
func (h *Handlers) GetTrendingUsers(c *gin.Context) {
	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	period := c.DefaultQuery("period", "week")

	// Determine time window based on period
	var since time.Time
	switch period {
	case "day":
		since = time.Now().AddDate(0, 0, -1)
	case "week":
		since = time.Now().AddDate(0, 0, -7)
	case "month":
		since = time.Now().AddDate(0, -1, 0)
	default:
		since = time.Now().AddDate(0, 0, -7)
	}

	// Find users with most activity (posts) in the time period
	// Also factor in follower growth and engagement
	// Use distinct alias 'recent_post_count' to avoid conflict with users.post_count column
	var users []models.User
	result := database.DB.
		Select("users.*, COUNT(audio_posts.id) as recent_post_count").
		Joins("LEFT JOIN audio_posts ON audio_posts.user_id = users.id AND audio_posts.created_at > ?", since).
		Group("users.id").
		Order("recent_post_count DESC, users.follower_count DESC").
		Limit(limit).
		Find(&users)

	if result.Error != nil {
		util.RespondInternalError(c, "failed_to_get_trending_users", result.Error.Error())
		return
	}

	// Get current user's stream ID for follow state enrichment
	currentUserID := ""
	if currentUser, ok := util.GetUserFromContext(c); ok {
		currentUserID = currentUser.ID
	}

	// Enrich with is_following state and add additional fields
	// Use database IDs (NOT Stream IDs) to match FollowUser behavior
	userResults := make([]gin.H, 0, len(users))

	for _, user := range users {
		// Query follow state using database IDs - no caching for fresh data
		isFollowing := false
		if currentUserID != "" && user.ID != "" && user.ID != currentUserID {
			isFollowing, _ = h.stream.CheckIsFollowing(currentUserID, user.ID)
		}

		userResults = append(userResults, gin.H{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.GetAvatarURL(),
			"bio":            user.Bio,
			"follower_count": user.FollowerCount,
			"post_count":     user.PostCount,
			"genre":          user.Genre,
			"daw_preference": user.DAWPreference,
			"is_following":   isFollowing,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users":  userResults,
		"period": period,
		"meta": gin.H{
			"limit": limit,
			"count": len(userResults),
		},
	})
}

// GetFeaturedProducers returns curated/featured producers
// GET /api/discover/featured?limit=10
func (h *Handlers) GetFeaturedProducers(c *gin.Context) {
	limit := util.ParseInt(c.DefaultQuery("limit", "10"), 10)

	// Featured producers: high follower count + active recently + has posts
	var users []models.User
	oneWeekAgo := time.Now().AddDate(0, 0, -7)

	result := database.DB.
		Where("follower_count > ? AND last_active_at > ? AND post_count > ?", 10, oneWeekAgo, 0).
		Order("follower_count DESC").
		Limit(limit).
		Find(&users)

	if result.Error != nil {
		util.RespondInternalError(c, "failed_to_get_featured", result.Error.Error())
		return
	}

	// If not enough users meet criteria, fall back to most followed
	if len(users) < limit {
		database.DB.
			Where("post_count > ?", 0).
			Order("follower_count DESC").
			Limit(limit).
			Find(&users)
	}

	// Get current user's stream ID for follow state enrichment
	currentUserID := ""
	if currentUser, ok := util.GetUserFromContext(c); ok {
		currentUserID = currentUser.ID
	}

	// Enrich with is_following state and add additional fields
	// Use database IDs (NOT Stream IDs) to match FollowUser behavior
	userResults := make([]gin.H, 0, len(users))

	for _, user := range users {
		// Query follow state using database IDs - no caching for fresh data
		isFollowing := false
		if currentUserID != "" && user.ID != "" && user.ID != currentUserID {
			isFollowing, _ = h.stream.CheckIsFollowing(currentUserID, user.ID)
		}

		userResults = append(userResults, gin.H{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.GetAvatarURL(),
			"bio":            user.Bio,
			"follower_count": user.FollowerCount,
			"post_count":     user.PostCount,
			"genre":          user.Genre,
			"daw_preference": user.DAWPreference,
			"featured":       true,
			"is_following":   isFollowing,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"meta": gin.H{
			"limit": limit,
			"count": len(userResults),
		},
	})
}

// GetUsersByGenre returns users who produce in a specific genre
// GET /api/discover/genre/:genre?limit=20&offset=0
func (h *Handlers) GetUsersByGenre(c *gin.Context) {
	genre := c.Param("genre")
	if genre == "" {
		util.RespondBadRequest(c, "genre_required")
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Find users with this genre in their genre array
	var users []models.User
	result := database.DB.
		Where("? = ANY(genre)", genre).
		Order("follower_count DESC").
		Limit(limit).
		Offset(offset).
		Find(&users)

	if result.Error != nil {
		util.RespondInternalError(c, "failed_to_get_users_by_genre", result.Error.Error())
		return
	}

	// Get current user's stream ID for follow state enrichment
	currentUserID := ""
	if currentUser, ok := util.GetUserFromContext(c); ok {
		currentUserID = currentUser.ID
	}

	// Enrich with is_following state and add additional fields
	// Use database IDs (NOT Stream IDs) to match FollowUser behavior
	userResults := make([]gin.H, 0, len(users))

	for _, user := range users {
		// Query follow state using database IDs - no caching for fresh data
		isFollowing := false
		if currentUserID != "" && user.ID != "" && user.ID != currentUserID {
			isFollowing, _ = h.stream.CheckIsFollowing(currentUserID, user.ID)
		}

		userResults = append(userResults, gin.H{
			"id":             user.ID,
			"username":       user.Username,
			"display_name":   user.DisplayName,
			"avatar_url":     user.GetAvatarURL(),
			"bio":            user.Bio,
			"follower_count": user.FollowerCount,
			"genre":          user.Genre,
			"daw_preference": user.DAWPreference,
			"is_following":   isFollowing,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"genre": genre,
		"meta": gin.H{
			"limit":  limit,
			"offset": offset,
			"count":  len(userResults),
		},
	})
}

// GetSuggestedUsers returns personalized user suggestions based on who you follow
// GET /api/discover/suggested?limit=10
func (h *Handlers) GetSuggestedUsers(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "10"), 10)

	// Get who the current user follows
	// Use database ID (NOT Stream ID) to match FollowUser behavior
	following, err := h.stream.GetFollowing(currentUser.ID, 100, 0)
	if err != nil {
		util.RespondInternalError(c, "failed_to_get_following", err.Error())
		return
	}

	// Extract followed user IDs
	followedIDs := make([]string, 0, len(following))
	for _, f := range following {
		followedIDs = append(followedIDs, f.UserID)
	}

	// Find users in similar genres who the user doesn't already follow
	var suggestedUsers []models.User

	if len(currentUser.Genre) > 0 {
		// Users with overlapping genres, not already followed, not self
		query := database.DB.
			Where("id != ? AND id NOT IN ?", currentUser.ID, append(followedIDs, currentUser.ID))

		// Build genre overlap condition
		for _, g := range currentUser.Genre {
			query = query.Or("? = ANY(genre)", g)
		}

		query.Order("follower_count DESC").Limit(limit).Find(&suggestedUsers)
	}

	// If not enough suggestions, add popular users
	if len(suggestedUsers) < limit {
		remaining := limit - len(suggestedUsers)
		var popularUsers []models.User

		excludeIDs := append(followedIDs, currentUser.ID)
		for _, u := range suggestedUsers {
			excludeIDs = append(excludeIDs, u.ID)
		}

		database.DB.
			Where("id NOT IN ?", excludeIDs).
			Order("follower_count DESC").
			Limit(remaining).
			Find(&popularUsers)

		suggestedUsers = append(suggestedUsers, popularUsers...)
	}

	userResults := make([]gin.H, 0, len(suggestedUsers))

	for _, u := range suggestedUsers {
		// Calculate shared genres
		sharedGenres := []string{}
		for _, g1 := range currentUser.Genre {
			for _, g2 := range u.Genre {
				if g1 == g2 {
					sharedGenres = append(sharedGenres, g1)
				}
			}
		}

		// Check follow state using database IDs - no caching for fresh data
		isFollowing := false
		if u.ID != "" && u.ID != currentUser.ID {
			isFollowing, _ = h.stream.CheckIsFollowing(currentUser.ID, u.ID)
		}

		userResults = append(userResults, gin.H{
			"id":             u.ID,
			"username":       u.Username,
			"display_name":   u.DisplayName,
			"avatar_url":     u.GetAvatarURL(),
			"bio":            u.Bio,
			"follower_count": u.FollowerCount,
			"genre":          u.Genre,
			"shared_genres":  sharedGenres,
			"daw_preference": u.DAWPreference,
			"is_following":   isFollowing,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"meta": gin.H{
			"limit": limit,
			"count": len(userResults),
		},
	})
}

// GetSimilarUsers returns users with similar BPM and key preferences
// GET /api/users/:id/similar?limit=10
func (h *Handlers) GetSimilarUsers(c *gin.Context) {
	targetUserID := c.Param("id")
	limit := util.ParseInt(c.DefaultQuery("limit", "10"), 10)

	// Get target user
	var targetUser models.User
	if err := database.DB.First(&targetUser, "id = ?", targetUserID).Error; err != nil {
		if util.HandleDBError(c, err, "user") {
			return
		}
	}

	// Get target user's posts to analyze their BPM/key preferences
	var targetPosts []models.AudioPost
	database.DB.Where("user_id = ?", targetUserID).Limit(20).Find(&targetPosts)

	if len(targetPosts) == 0 {
		// If no posts, fall back to genre-based similarity
		c.JSON(http.StatusOK, gin.H{
			"users":  []gin.H{},
			"reason": "no_posts_to_analyze",
			"meta": gin.H{
				"limit": limit,
				"count": 0,
			},
		})
		return
	}

	// Calculate average BPM and most common keys
	var totalBPM int
	keyCount := make(map[string]int)
	for _, post := range targetPosts {
		totalBPM += post.BPM
		if post.Key != "" {
			keyCount[post.Key]++
		}
	}
	avgBPM := totalBPM / len(targetPosts)

	// Find the most common key
	var commonKey string
	maxKeyCount := 0
	for key, count := range keyCount {
		if count > maxKeyCount {
			commonKey = key
			maxKeyCount = count
		}
	}

	// Find users with similar posts (within 20 BPM range and/or same key)
	var similarUserIDs []string
	bpmRange := 20

	subQuery := database.DB.Model(&models.AudioPost{}).
		Select("DISTINCT user_id").
		Where("user_id != ?", targetUserID).
		Where("(bpm BETWEEN ? AND ?) OR key = ?", avgBPM-bpmRange, avgBPM+bpmRange, commonKey)

	subQuery.Pluck("user_id", &similarUserIDs)

	if len(similarUserIDs) == 0 {
		c.JSON(http.StatusOK, gin.H{
			"users":        []gin.H{},
			"analyzed_bpm": avgBPM,
			"analyzed_key": commonKey,
			"reason":       "no_similar_users_found",
			"meta": gin.H{
				"limit": limit,
				"count": 0,
			},
		})
		return
	}

	// Get user details
	var similarUsers []models.User
	database.DB.
		Where("id IN ?", similarUserIDs).
		Order("follower_count DESC").
		Limit(limit).
		Find(&similarUsers)

	// Get current user's stream ID for follow state enrichment
	currentUserID := ""
	if currentUser, ok := util.GetUserFromContext(c); ok {
		currentUserID = currentUser.ID
	}

	// Enrich with is_following state
	// Use database IDs (NOT Stream IDs) to match FollowUser behavior
	userResults := make([]gin.H, 0, len(similarUsers))

	for _, u := range similarUsers {
		// Query follow state using database IDs - no caching for fresh data
		isFollowing := false
		if currentUserID != "" && u.ID != "" && u.ID != currentUserID {
			isFollowing, _ = h.stream.CheckIsFollowing(currentUserID, u.ID)
		}

		userResults = append(userResults, gin.H{
			"id":             u.ID,
			"username":       u.Username,
			"display_name":   u.DisplayName,
			"avatar_url":     u.GetAvatarURL(),
			"bio":            u.Bio,
			"follower_count": u.FollowerCount,
			"genre":          u.Genre,
			"daw_preference": u.DAWPreference,
			"is_following":   isFollowing,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"users":        userResults,
		"analyzed_bpm": avgBPM,
		"analyzed_key": commonKey,
		"meta": gin.H{
			"limit": limit,
			"count": len(userResults),
		},
	})
}

// GetAvailableGenres returns all genres with user counts
// GET /api/discover/genres
func (h *Handlers) GetAvailableGenres(c *gin.Context) {
	// Get unique genres and their counts using unnest for PostgreSQL array
	type GenreCount struct {
		Genre string `json:"genre"`
		Count int    `json:"count"`
	}

	var genreCounts []GenreCount
	result := database.DB.Raw(`
		SELECT unnest(genre) as genre, COUNT(*) as count
		FROM users
		WHERE genre IS NOT NULL AND array_length(genre, 1) > 0
		GROUP BY unnest(genre)
		ORDER BY count DESC
	`).Scan(&genreCounts)

	if result.Error != nil {
		util.RespondInternalError(c, "failed_to_get_genres", result.Error.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"genres": genreCounts,
		"meta": gin.H{
			"count": len(genreCounts),
		},
	})
}
