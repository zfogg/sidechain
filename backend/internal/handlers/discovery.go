package handlers

import (
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/lib/pq"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/metrics"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/search"
	"github.com/zfogg/sidechain/backend/internal/util"
)

// ============================================================================
// SEARCH & DISCOVERY - ELASTICSEARCH INTEGRATION
// ============================================================================

// Phase 1: Search Endpoints with Elasticsearch Backend

// Architecture:
// 1. Primary Search (Elasticsearch):
// - SearchUsers - Full-text user search with ranking
// - SearchPosts - Post search with genre/BPM/key filters
// - SearchStories - Story search by creator
// - AdvancedSearch - Unified cross-entity search

// 2. Autocomplete (Elasticsearch + PostgreSQL):
// - AutocompleteUsers - Username suggestions with completion suggester
// - AutocompleteGenres - Genre suggestions from post metadata

// 3. Error Recovery :
// Each search endpoint implements graceful degradation:
// - Try Elasticsearch first (if available)
// - If ES unavailable or returns error, fall back to PostgreSQL
// - Return "fallback": true in response metadata for client awareness
// - Ensures system remains functional even if Elasticsearch is down

// 4. Analytics :
// - trackSearchQuery logs all searches to audit trail
// - Records: user, entity type, query, result count, filters
// - Non-blocking: analytics in background goroutine
// - Delegates to search.Client for Elasticsearch indexing

// Response Format:
// All search endpoints return consistent structure:
// {
// "users/posts/stories": [...],
// "meta": {
// "query": "search term",
// "limit": 20,
// "offset": 0,
// "count": 5,
// "fallback": false
// }
// }

// trackSearchQuery logs search analytics
// Note: In production, this would be indexed to an analytics service
func (h *Handlers) trackSearchQuery(c *gin.Context, entityType string, query string, resultCount int, filters map[string]interface{}) {
	// Non-blocking analytics tracking
	go func() {
		userID := c.GetString("user_id")
		if h.search != nil {
			// Delegate to search client for potential Elasticsearch analytics indexing
			h.search.TrackSearchQuery(c.Request.Context(), query, resultCount, filters)
		}

		// Log for debugging
		fmt.Printf("🔍 Search Analytics: user=%s, type=%s, query=%s, results=%d, filters=%v\n",
			userID, entityType, query, resultCount, filters)
	}()
}

// enrichUsersWithFollowState adds is_following field to user results
// Use database IDs (NOT Stream IDs) to match FollowUser behavior
func (h *Handlers) enrichUsersWithFollowState(currentUserID string, users []models.User) []gin.H {
	userResults := make([]gin.H, 0, len(users))

	for _, user := range users {
		// Check if current user is following this user
		// Use database IDs - no caching for fresh data
		isFollowing := false
		if currentUserID != "" && user.ID != "" && user.ID != currentUserID {
			var err error
			isFollowing, err = h.stream.CheckIsFollowing(currentUserID, user.ID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
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
	startTime := time.Now()
	query := c.Query("q")
	if query == "" {
		util.RespondBadRequest(c, "search_query_required")
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Get current user for follow state enrichment (optional - doesn't require auth)
	currentUserID := ""
	if userVal, exists := c.Get("user"); exists {
		if currentUser, ok := userVal.(*models.User); ok {
			currentUserID = currentUser.ID
		}
	}

	var users []models.User
	usingFallback := false

	// Try Elasticsearch first
	if h.search != nil {
		searchResult, err := h.search.SearchUsers(c.Request.Context(), query, limit, offset)
		if err == nil && searchResult != nil {
			// Convert search results back to User models for enrichment
			users = make([]models.User, len(searchResult.Users))
			for i, userID := range searchResult.Users {
				var user models.User
				if err := database.DB.First(&user, "id = ?", userID).Error; err == nil {
					users[i] = user
				}
			}
		} else {
			// Log warning and fall back to PostgreSQL
			if err != nil {
				fmt.Printf("Warning: Elasticsearch search failed, falling back to PostgreSQL: %v\n", err)
			}
			usingFallback = true
		}
	} else {
		usingFallback = true
	}

	// PostgreSQL fallback
	if usingFallback {
		searchPattern := "%" + query + "%"
		result := database.DB.Where(
			"username ILIKE ? OR display_name ILIKE ?",
			searchPattern, searchPattern,
		).Order("follower_count DESC").Limit(limit).Offset(offset).Find(&users)

		if result.Error != nil {
			util.RespondInternalError(c, "search_failed", result.Error.Error())
			return
		}
	}

	// Filter private users from results
	filteredUsers := make([]models.User, 0, len(users))
	for _, user := range users {
		// Always show the current user if searching for themselves
		if currentUserID != "" && user.ID == currentUserID {
			filteredUsers = append(filteredUsers, user)
			continue
		}

		// If user is not private, always show
		if !user.IsPrivate {
			filteredUsers = append(filteredUsers, user)
			continue
		}

		// User is private - only show if current user follows them
		if currentUserID != "" {
			isFollowing, err := h.stream.CheckIsFollowing(currentUserID, user.ID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
			if isFollowing {
				filteredUsers = append(filteredUsers, user)
			}
		}
		// Skip private users for unauthenticated users
	}
	users = filteredUsers

	// Enrich with is_following state
	userResults := h.enrichUsersWithFollowState(currentUserID, users)

	// Track search analytics
	h.trackSearchQuery(c, "users", query, len(userResults), map[string]interface{}{
		"limit":    limit,
		"offset":   offset,
		"fallback": usingFallback,
	})

	// Record metrics
	metrics.GetManager().Search.RecordQuery(metrics.QueryMetric{
		Type:        "users",
		Query:       query,
		ResultCount: len(userResults),
		Duration:    time.Since(startTime),
		CacheHit:    false,
		Error:       false,
		Timestamp:   time.Now(),
	})

	c.JSON(http.StatusOK, gin.H{
		"users": userResults,
		"meta": gin.H{
			"query":    query,
			"limit":    limit,
			"offset":   offset,
			"count":    len(userResults),
			"fallback": usingFallback,
		},
	})
}

// SearchPosts searches for audio posts with Elasticsearch
// GET /api/search/posts?q=query&genre=electronic&bpm_min=90&bpm_max=120&key=C&limit=20&offset=0
func (h *Handlers) SearchPosts(c *gin.Context) {
	startTime := time.Now()
	query := c.Query("q")
	if query == "" {
		util.RespondBadRequest(c, "missing_query", "Search query is required")
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Parse optional filters
	genres := c.QueryArray("genre")
	bpmMin := c.Query("bpm_min")
	bpmMax := c.Query("bpm_max")
	key := c.Query("key")

	var bpmMinInt *int
	var bpmMaxInt *int
	if bpmMin != "" {
		if val, err := strconv.Atoi(bpmMin); err == nil {
			bpmMinInt = &val
		}
	}
	if bpmMax != "" {
		if val, err := strconv.Atoi(bpmMax); err == nil {
			bpmMaxInt = &val
		}
	}

	var posts []models.AudioPost
	usingFallback := false

	// Try Elasticsearch first
	if h.search != nil {
		searchParams := search.SearchPostsParams{
			Query:  query,
			Genre:  genres,
			BPMMin: bpmMinInt,
			BPMMax: bpmMaxInt,
			Key:    key,
			Limit:  limit,
			Offset: offset,
		}

		searchResult, err := h.search.SearchPosts(c.Request.Context(), searchParams)
		if err == nil && searchResult != nil {
			// Convert search results back to AudioPost models with user data
			posts = make([]models.AudioPost, 0, len(searchResult.Posts))
			for _, searchPost := range searchResult.Posts {
				var post models.AudioPost
				if err := database.DB.Preload("User").First(&post, "id = ?", searchPost.ID).Error; err == nil {
					posts = append(posts, post)
				}
			}
		} else {
			// Log warning and fall back to PostgreSQL
			if err != nil {
				fmt.Printf("Warning: Elasticsearch search failed, falling back to PostgreSQL: %v\n", err)
			}
			usingFallback = true
		}
	} else {
		usingFallback = true
	}

	// PostgreSQL fallback
	if usingFallback {
		query_pattern := "%" + query + "%"
		db := database.DB.Preload("User")

		// Text search on various fields
		db = db.Where(
			"filename ILIKE ? OR original_filename ILIKE ?",
			query_pattern, query_pattern,
		)

		// Filter by genre if provided
		if len(genres) > 0 {
			db = db.Where("genre && ?", pq.Array(genres))
		}

		// Filter by BPM range if provided
		if bpmMinInt != nil || bpmMaxInt != nil {
			if bpmMinInt != nil {
				db = db.Where("bpm >= ?", *bpmMinInt)
			}
			if bpmMaxInt != nil {
				db = db.Where("bpm <= ?", *bpmMaxInt)
			}
		}

		// Filter by key if provided
		if key != "" {
			db = db.Where("key = ?", key)
		}

		result := db.Order("like_count DESC, created_at DESC").Limit(limit).Offset(offset).Find(&posts)
		if result.Error != nil {
			util.RespondInternalError(c, "search_failed", result.Error.Error())
			return
		}
	}

	// Filter private content from results
	currentUserID := c.GetString("user_id")
	filteredPosts := make([]models.AudioPost, 0, len(posts))
	for _, post := range posts {
		// Allow if post is public
		if !post.IsPublic {
			// Post is private - only allow if current user is owner
			if currentUserID != "" && post.UserID == currentUserID {
				filteredPosts = append(filteredPosts, post)
			}
			// Skip private posts for unauthenticated users
			continue
		}

		// Post is public - now check if creator is private
		if post.User.ID != "" && post.User.IsPrivate {
			// Creator is private - allow if current user is owner or follows them
			if currentUserID != "" {
				if post.UserID == currentUserID {
					// Current user is the creator
					filteredPosts = append(filteredPosts, post)
				} else {
					// Check if current user follows this private user
					isFollowing, err := h.stream.CheckIsFollowing(currentUserID, post.UserID)

					if err != nil {

						logger.WarnWithFields("Failed to check follow status", err)

						isFollowing = false

					}
					if isFollowing {
						filteredPosts = append(filteredPosts, post)
					}
				}
			}
			// Skip private creator posts for unauthenticated users
			continue
		}

		// Post is public and creator is not private - include it
		filteredPosts = append(filteredPosts, post)
	}
	posts = filteredPosts

	// Format response
	type PostResponse struct {
		ID            string                 `json:"id"`
		UserID        string                 `json:"user_id"`
		User          map[string]interface{} `json:"user"`
		Filename      string                 `json:"filename"`
		AudioURL      string                 `json:"audio_url"`
		WaveformURL   string                 `json:"waveform_url,omitempty"`
		Genre         []string               `json:"genre"`
		BPM           int                    `json:"bpm"`
		Key           string                 `json:"key"`
		DAW           string                 `json:"daw"`
		LikeCount     int                    `json:"like_count"`
		PlayCount     int                    `json:"play_count"`
		CommentCount  int                    `json:"comment_count"`
		CreatedAt     time.Time              `json:"created_at"`
	}

	postResponses := make([]PostResponse, 0, len(posts))
	for _, post := range posts {
		resp := PostResponse{
			ID:           post.ID,
			UserID:       post.UserID,
			Filename:     post.Filename,
			AudioURL:     post.AudioURL,
			WaveformURL:  post.WaveformURL,
			Genre:        post.Genre,
			BPM:          post.BPM,
			Key:          post.Key,
			DAW:          post.DAW,
			LikeCount:    post.LikeCount,
			PlayCount:    post.PlayCount,
			CommentCount: post.CommentCount,
			CreatedAt:    post.CreatedAt,
		}

		// Add user data
		if post.User.ID != "" {
			resp.User = map[string]interface{}{
				"id":                  post.User.ID,
				"username":            post.User.Username,
				"display_name":        post.User.DisplayName,
				"profile_picture_url": post.User.ProfilePictureURL,
				"follower_count":      post.User.FollowerCount,
			}
		}

		postResponses = append(postResponses, resp)
	}

	// Track search analytics
	filters := map[string]interface{}{
		"limit":    limit,
		"offset":   offset,
		"fallback": usingFallback,
	}
	if len(genres) > 0 {
		filters["genres"] = genres
	}
	if bpmMinInt != nil || bpmMaxInt != nil {
		bpmRange := make(map[string]interface{})
		if bpmMinInt != nil {
			bpmRange["min"] = *bpmMinInt
		}
		if bpmMaxInt != nil {
			bpmRange["max"] = *bpmMaxInt
		}
		filters["bpm"] = bpmRange
	}
	if key != "" {
		filters["key"] = key
	}
	h.trackSearchQuery(c, "posts", query, len(postResponses), filters)

	// Record metrics
	metrics.GetManager().Search.RecordQuery(metrics.QueryMetric{
		Type:        "posts",
		Query:       query,
		ResultCount: len(postResponses),
		Duration:    time.Since(startTime),
		CacheHit:    false, // Will be tracked by cache layer
		Error:       false,
		Timestamp:   time.Now(),
	})

	c.JSON(http.StatusOK, gin.H{
		"posts": postResponses,
		"meta": gin.H{
			"query":    query,
			"limit":    limit,
			"offset":   offset,
			"count":    len(postResponses),
			"fallback": usingFallback,
		},
	})
}

// SearchStories searches for stories
// GET /api/search/stories?q=query&limit=20&offset=0
func (h *Handlers) SearchStories(c *gin.Context) {
	query := c.Query("q")
	if query == "" {
		util.RespondBadRequest(c, "missing_query", "Search query is required")
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "20"), 20)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	var stories []models.Story
	usingFallback := false

	// Try Elasticsearch first (search by username since stories don't have direct text fields)
	if h.search != nil {
		// Note: Current search.SearchStories would need implementation
		// For now, use fallback approach - stories are typically found via user search
		usingFallback = true
	} else {
		usingFallback = true
	}

	// PostgreSQL search (for now, search by story creator's username)
	if usingFallback {
		query_pattern := "%" + query + "%"
		result := database.DB.
			Preload("User").
			Where("stories.expired_at IS NULL"). // Only active stories
			Joins("JOIN users ON users.id = stories.user_id").
			Where("users.username ILIKE ?", query_pattern).
			Order("stories.created_at DESC").
			Limit(limit).
			Offset(offset).
			Find(&stories)

		if result.Error != nil {
			util.RespondInternalError(c, "search_failed", result.Error.Error())
			return
		}
	}

	// Filter private creator stories from results
	currentUserID := c.GetString("user_id")
	filteredStories := make([]models.Story, 0, len(stories))
	for _, story := range stories {
		// Always show stories from current user
		if currentUserID != "" && story.UserID == currentUserID {
			filteredStories = append(filteredStories, story)
			continue
		}

		// If story creator is not private, always show
		if story.User.ID != "" && !story.User.IsPrivate {
			filteredStories = append(filteredStories, story)
			continue
		}

		// Story creator is private - only show if current user follows them
		if story.User.ID != "" && story.User.IsPrivate {
			if currentUserID != "" {
				isFollowing, err := h.stream.CheckIsFollowing(currentUserID, story.UserID)

				if err != nil {

					logger.WarnWithFields("Failed to check follow status", err)

					isFollowing = false

				}
				if isFollowing {
					filteredStories = append(filteredStories, story)
				}
			}
			// Skip private creator stories for unauthenticated users
			continue
		}

		// Show story by default
		filteredStories = append(filteredStories, story)
	}
	stories = filteredStories

	// Format response
	type StoryResponse struct {
		ID        string                 `json:"id"`
		UserID    string                 `json:"user_id"`
		User      map[string]interface{} `json:"user"`
		AudioURL  string                 `json:"audio_url"`
		CreatedAt time.Time              `json:"created_at"`
		ExpiresAt time.Time              `json:"expires_at"`
	}

	storyResponses := make([]StoryResponse, 0, len(stories))
	for _, story := range stories {
		resp := StoryResponse{
			ID:        story.ID,
			UserID:    story.UserID,
			AudioURL:  story.AudioURL,
			CreatedAt: story.CreatedAt,
			ExpiresAt: story.ExpiresAt,
		}

		// Add user data
		if story.User.ID != "" {
			resp.User = map[string]interface{}{
				"id":                  story.User.ID,
				"username":            story.User.Username,
				"display_name":        story.User.DisplayName,
				"profile_picture_url": story.User.ProfilePictureURL,
				"follower_count":      story.User.FollowerCount,
			}
		}

		storyResponses = append(storyResponses, resp)
	}

	// Track search analytics
	h.trackSearchQuery(c, "stories", query, len(storyResponses), map[string]interface{}{
		"limit":    limit,
		"offset":   offset,
		"fallback": usingFallback,
	})

	c.JSON(http.StatusOK, gin.H{
		"stories": storyResponses,
		"meta": gin.H{
			"query":    query,
			"limit":    limit,
			"offset":   offset,
			"count":    len(storyResponses),
			"fallback": usingFallback,
		},
	})
}

// AutocompleteUsers provides username autocomplete suggestions
// GET /api/search/autocomplete/users?q=prefix&limit=10
func (h *Handlers) AutocompleteUsers(c *gin.Context) {
	query := c.Query("q")
	if query == "" {
		util.RespondBadRequest(c, "missing_query", "Search prefix is required")
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "10"), 10)
	if limit > 50 {
		limit = 50 // Cap at 50 suggestions
	}

	var suggestions []string

	// Try Elasticsearch completion suggester first
	if h.search != nil {
		var err error
		suggestions, err = h.search.SuggestUsers(c.Request.Context(), query, limit)
		if err != nil {
			logger.WarnWithFields("SuggestUsernames: Elasticsearch suggestion failed (using DB fallback): %v", err)
		}
	}

	// PostgreSQL fallback if no ES results or ES unavailable
	if len(suggestions) == 0 {
		var usernames []string
		result := database.DB.
			Model(&models.User{}).
			Where("username ILIKE ?", query+"%").
			Order("follower_count DESC").
			Limit(limit).
			Pluck("username", &usernames)

		if result.Error == nil {
			suggestions = usernames
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"suggestions": suggestions,
		"count":       len(suggestions),
	})
}

// AutocompleteGenres provides genre autocomplete suggestions
// GET /api/search/autocomplete/genres?q=prefix&limit=10
func (h *Handlers) AutocompleteGenres(c *gin.Context) {
	query := c.Query("q")
	if query == "" {
		util.RespondBadRequest(c, "missing_query", "Search prefix is required")
		return
	}

	limit := util.ParseInt(c.DefaultQuery("limit", "10"), 10)
	if limit > 50 {
		limit = 50 // Cap at 50 suggestions
	}

	var genres []string

	// Query all distinct genres from posts and filter by prefix
	// Note: PostgreSQL array types require special handling with unnest
	var allGenres []string
	result := database.DB.
		Model(&models.AudioPost{}).
		Distinct("unnest(genre)").
		Order("count(*) DESC").
		Limit(500). // Get enough to filter
		Pluck("unnest(genre)", &allGenres)

	if result.Error == nil {
		// Filter genres by prefix (case-insensitive)
		query_lower := strings.ToLower(query)
		for _, genre := range allGenres {
			if strings.HasPrefix(strings.ToLower(genre), query_lower) {
				genres = append(genres, genre)
				if len(genres) >= limit {
					break
				}
			}
		}
	}

	// If no results, return empty slice
	if genres == nil {
		genres = []string{}
	}

	c.JSON(http.StatusOK, gin.H{
		"suggestions": genres,
		"count":       len(genres),
	})
}

// AdvancedSearch performs unified search across users, posts, and stories
// GET /api/search/advanced?q=query&type=all|users|posts|stories&limit=10&offset=0
func (h *Handlers) AdvancedSearch(c *gin.Context) {
	query := c.Query("q")
	if query == "" {
		util.RespondBadRequest(c, "missing_query", "Search query is required")
		return
	}

	searchType := c.DefaultQuery("type", "all") // all, users, posts, stories
	limit := util.ParseInt(c.DefaultQuery("limit", "10"), 10)
	offset := util.ParseInt(c.DefaultQuery("offset", "0"), 0)

	// Cap results for performance
	if limit > 50 {
		limit = 50
	}

	response := gin.H{
		"query":  query,
		"type":   searchType,
		"limit":  limit,
		"offset": offset,
		"results": gin.H{},
	}

	// Search users (if requested)
	if searchType == "all" || searchType == "users" {
		var users []models.User
		database.DB.
			Where("username ILIKE ? OR display_name ILIKE ?", "%"+query+"%", "%"+query+"%").
			Order("follower_count DESC").
			Limit(limit).
			Offset(offset).
			Find(&users)

		userResults := h.enrichUsersWithFollowState(c.GetString("user_id"), users)
		response["results"].(gin.H)["users"] = userResults
	}

	// Search posts (if requested)
	if searchType == "all" || searchType == "posts" {
		var posts []models.AudioPost
		database.DB.
			Preload("User").
			Where("filename ILIKE ? OR original_filename ILIKE ?", "%"+query+"%", "%"+query+"%").
			Order("like_count DESC, created_at DESC").
			Limit(limit).
			Offset(offset).
			Find(&posts)

		type PostSummary struct {
			ID           string `json:"id"`
			UserID       string `json:"user_id"`
			Username     string `json:"username"`
			Filename     string `json:"filename"`
			Genre        []string `json:"genre"`
			BPM          int    `json:"bpm"`
			Key          string `json:"key"`
			LikeCount    int    `json:"like_count"`
			PlayCount    int    `json:"play_count"`
			CommentCount int    `json:"comment_count"`
		}

		postSummaries := make([]PostSummary, 0, len(posts))
		for _, post := range posts {
			postSummaries = append(postSummaries, PostSummary{
				ID:           post.ID,
				UserID:       post.UserID,
				Username:     post.User.Username,
				Filename:     post.Filename,
				Genre:        post.Genre,
				BPM:          post.BPM,
				Key:          post.Key,
				LikeCount:    post.LikeCount,
				PlayCount:    post.PlayCount,
				CommentCount: post.CommentCount,
			})
		}
		response["results"].(gin.H)["posts"] = postSummaries
	}

	// Search stories (if requested)
	if searchType == "all" || searchType == "stories" {
		var stories []models.Story
		database.DB.
			Preload("User").
			Where("stories.expired_at IS NULL").
			Joins("JOIN users ON users.id = stories.user_id").
			Where("users.username ILIKE ?", "%"+query+"%").
			Order("stories.created_at DESC").
			Limit(limit).
			Offset(offset).
			Find(&stories)

		type StorySummary struct {
			ID        string `json:"id"`
			UserID    string `json:"user_id"`
			Username  string `json:"username"`
			CreatedAt time.Time `json:"created_at"`
			ExpiresAt time.Time `json:"expires_at"`
		}

		storySummaries := make([]StorySummary, 0, len(stories))
		for _, story := range stories {
			storySummaries = append(storySummaries, StorySummary{
				ID:        story.ID,
				UserID:    story.UserID,
				Username:  story.User.Username,
				CreatedAt: story.CreatedAt,
				ExpiresAt: story.ExpiresAt,
			})
		}
		response["results"].(gin.H)["stories"] = storySummaries
	}

	// Track search analytics
	totalResults := 0
	if users, ok := response["results"].(gin.H)["users"]; ok {
		if u, ok := users.([]gin.H); ok {
			totalResults += len(u)
		}
	}
	if posts, ok := response["results"].(gin.H)["posts"]; ok {
		if p, ok := posts.([]interface{}); ok {
			totalResults += len(p)
		}
	}
	if stories, ok := response["results"].(gin.H)["stories"]; ok {
		if s, ok := stories.([]interface{}); ok {
			totalResults += len(s)
		}
	}
	h.trackSearchQuery(c, "advanced", query, totalResults, map[string]interface{}{
		"search_type": searchType,
		"limit":       limit,
		"offset":      offset,
	})

	c.JSON(http.StatusOK, response)
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
			var err error
			isFollowing, err = h.stream.CheckIsFollowing(currentUserID, user.ID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
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
			var err error
			isFollowing, err = h.stream.CheckIsFollowing(currentUserID, user.ID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
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
			var err error
			isFollowing, err = h.stream.CheckIsFollowing(currentUserID, user.ID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
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
			var err error
			isFollowing, err = h.stream.CheckIsFollowing(currentUser.ID, u.ID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
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
			var err error
			isFollowing, err = h.stream.CheckIsFollowing(currentUserID, u.ID)

			if err != nil {

				logger.WarnWithFields("Failed to check follow status", err)

				isFollowing = false

			}
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

// GetAvailableKeys returns unique musical keys from audio posts
func (h *Handlers) GetAvailableKeys(c *gin.Context) {
	type KeyCount struct {
		Key   string `json:"key"`
		Count int    `json:"count"`
	}

	var keyCounts []KeyCount
	result := database.DB.Raw(`
		SELECT key, COUNT(*) as count
		FROM audio_posts
		WHERE key IS NOT NULL AND key != '' AND deleted_at IS NULL AND is_public = true
		GROUP BY key
		ORDER BY count DESC
	`).Scan(&keyCounts)

	if result.Error != nil {
		util.RespondInternalError(c, "failed_to_get_keys", result.Error.Error())
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"keys": keyCounts,
		"meta": gin.H{
			"count": len(keyCounts),
		},
	})
}

