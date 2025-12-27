package timeline

import (
	"context"
	"fmt"
	"sort"
	"time"

	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/recommendations"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"go.uber.org/zap"
	"gorm.io/gorm"
)

// TimelineItem represents a single item in the user's timeline
type TimelineItem struct {
	ID        string                 `json:"id"`
	Type      string                 `json:"type"` // "post", "recommendation"
	Post      *models.AudioPost      `json:"post,omitempty"`
	User      *models.User           `json:"user,omitempty"`
	Score     float64                `json:"score"`
	Reason    string                 `json:"reason,omitempty"` // Why this was recommended
	Source    string                 `json:"source"`           // "following", "gorse", "trending", "recent"
	CreatedAt time.Time              `json:"created_at"`
	Extra     map[string]interface{} `json:"extra,omitempty"`
}

// TimelineResponse is the response from GetTimeline
type TimelineResponse struct {
	Items      []TimelineItem `json:"items"`
	Meta       TimelineMeta   `json:"meta"`
	NextCursor string         `json:"next_cursor,omitempty"`
}

// TimelineMeta contains metadata about the timeline response
type TimelineMeta struct {
	Limit          int  `json:"limit"`
	Offset         int  `json:"offset"`
	Count          int  `json:"count"`
	HasMore        bool `json:"has_more"`
	FollowingCount int  `json:"following_count"` // How many items from followed users
	GorseCount     int  `json:"gorse_count"`     // How many items from Gorse recommendations
	TrendingCount  int  `json:"trending_count"`  // How many items from trending
	RecentCount    int  `json:"recent_count"`    // How many items from recent posts
}

// Service handles timeline generation and ranking
type Service struct {
	db           *gorm.DB
	streamClient *stream.Client
	gorseClient  *recommendations.GorseRESTClient
}

// NewService creates a new timeline service
// Both streamClient and gorseClient should be initialized in main.go and passed here
func NewService(streamClient *stream.Client, gorseClient *recommendations.GorseRESTClient) *Service {
	return &Service{
		db:           database.DB,
		streamClient: streamClient,
		gorseClient:  gorseClient,
	}
}

// GetTimeline returns a unified timeline for the user
// It combines:
// 1. Posts from users they follow (via Stream.io)
// 2. Personalized recommendations from Gorse
// 3. Trending posts (if not enough content)
// 4. Recent posts (as final fallback)
func (s *Service) GetTimeline(ctx context.Context, userID string, limit, offset int) (*TimelineResponse, error) {
	// We'll fetch more than needed to allow for deduplication and ranking
	fetchLimit := limit * 3

	// Get muted user IDs for filtering
	mutedUserIDs, err := s.getMutedUserIDs(userID)
	if err != nil {
		// Log but continue - don't break timeline if mute lookup fails
		logger.Log.Warn("Failed to get muted users", zap.Error(err))
		mutedUserIDs = []string{}
	}
	mutedUserSet := make(map[string]bool, len(mutedUserIDs))
	for _, id := range mutedUserIDs {
		mutedUserSet[id] = true
	}

	// Collect items from all sources in parallel
	type sourceResult struct {
		items  []TimelineItem
		source string
		err    error
	}

	resultsChan := make(chan sourceResult, 4)

	// 1. Get posts from followed users (Stream.io timeline)
	go func() {
		items, err := s.getFollowingPosts(ctx, userID, fetchLimit)
		resultsChan <- sourceResult{items: items, source: "following", err: err}
	}()

	// 2. Get Gorse recommendations
	go func() {
		items, err := s.getGorseRecommendations(ctx, userID, fetchLimit)
		resultsChan <- sourceResult{items: items, source: "gorse", err: err}
	}()

	// 3. Get trending posts
	go func() {
		items, err := s.getTrendingPosts(ctx, fetchLimit)
		resultsChan <- sourceResult{items: items, source: "trending", err: err}
	}()

	// 4. Get recent posts (fallback)
	go func() {
		items, err := s.getRecentPosts(ctx, userID, fetchLimit)
		resultsChan <- sourceResult{items: items, source: "recent", err: err}
	}()

	// Collect all results
	allItems := make([]TimelineItem, 0, fetchLimit*4)
	var followingCount, gorseCount, trendingCount, recentCount int

	for i := 0; i < 4; i++ {
		result := <-resultsChan
		if result.err != nil {
			// Log but continue - we don't want one source failure to break the timeline
			logger.Log.Warn("Timeline source failed",
				zap.String("source", result.source),
				zap.Error(result.err))
			continue
		}

		switch result.source {
		case "following":
			followingCount = len(result.items)
			logger.Log.Debug("Timeline: following posts fetched", zap.Int("count", followingCount))
		case "gorse":
			gorseCount = len(result.items)
			logger.Log.Debug("Timeline: gorse recommendations fetched", zap.Int("count", gorseCount))
		case "trending":
			trendingCount = len(result.items)
			logger.Log.Debug("Timeline: trending posts fetched", zap.Int("count", trendingCount))
		case "recent":
			recentCount = len(result.items)
			logger.Log.Debug("Timeline: recent posts fetched", zap.Int("count", recentCount))
		}

		allItems = append(allItems, result.items...)
	}

	logger.Log.Debug("Timeline: allItems count after collection", zap.Int("count", len(allItems)))

	// Deduplicate by post ID
	seen := make(map[string]bool)
	uniqueItems := make([]TimelineItem, 0, len(allItems))
	for _, item := range allItems {
		if !seen[item.ID] {
			seen[item.ID] = true
			uniqueItems = append(uniqueItems, item)
		}
	}
	logger.Log.Debug("Timeline: uniqueItems after dedup", zap.Int("count", len(uniqueItems)))

	// Filter out posts from muted users
	filteredItems := make([]TimelineItem, 0, len(uniqueItems))
	for _, item := range uniqueItems {
		// Check if the post author is muted
		if item.Post != nil && mutedUserSet[item.Post.UserID] {
			continue // Skip posts from muted users
		}
		if item.User != nil && mutedUserSet[item.User.ID] {
			continue // Skip posts from muted users (backup check)
		}
		filteredItems = append(filteredItems, item)
	}
	logger.Log.Debug("Timeline: filteredItems after mute filtering", zap.Int("count", len(filteredItems)))

	// Rank and sort items
	rankedItems := s.rankItems(filteredItems, userID)
	logger.Log.Debug("Timeline: rankedItems after ranking",
		zap.Int("count", len(rankedItems)),
		zap.Int("limit", limit),
		zap.Int("offset", offset),
		zap.Bool("has_more", (offset+limit) < len(rankedItems)))

	// Apply pagination
	start := offset
	if start >= len(rankedItems) {
		start = len(rankedItems)
	}
	end := start + limit
	if end > len(rankedItems) {
		end = len(rankedItems)
	}

	paginatedItems := rankedItems[start:end]

	// Enrich items with user data
	s.enrichItemsWithUsers(paginatedItems)

	return &TimelineResponse{
		Items: paginatedItems,
		Meta: TimelineMeta{
			Limit:          limit,
			Offset:         offset,
			Count:          len(paginatedItems),
			HasMore:        end < len(rankedItems),
			FollowingCount: followingCount,
			GorseCount:     gorseCount,
			TrendingCount:  trendingCount,
			RecentCount:    recentCount,
		},
	}, nil
}

// getFollowingPosts gets posts from users the current user follows via Stream.io
func (s *Service) getFollowingPosts(ctx context.Context, userID string, limit int) ([]TimelineItem, error) {
	// Get user's Stream user ID
	var user models.User
	if err := s.db.First(&user, "id = ?", userID).Error; err != nil {
		return nil, fmt.Errorf("user not found: %w", err)
	}

	activities, err := s.streamClient.GetUserTimeline(user.StreamUserID, limit, 0)
	if err != nil {
		return nil, fmt.Errorf("failed to get stream timeline: %w", err)
	}

	items := make([]TimelineItem, 0, len(activities))
	for _, act := range activities {
		// Extract post ID from object (format: "loop:uuid")
		postID := extractIDFromObject(act.Object)
		if postID == "" {
			continue
		}

		// Fetch the actual post from database (exclude archived posts)
		var post models.AudioPost
		if err := s.db.Preload("User").First(&post, "id = ? AND deleted_at IS NULL AND is_archived = ?", postID, false).Error; err != nil {
			continue // Skip if post not found or archived
		}

		createdAt := time.Now()
		if act.Time != "" {
			if t, err := time.Parse(time.RFC3339, act.Time); err == nil {
				createdAt = t
			}
		}

		items = append(items, TimelineItem{
			ID:        post.ID,
			Type:      "post",
			Post:      &post,
			User:      &post.User,
			Score:     1.0, // Base score for following
			Source:    "following",
			CreatedAt: createdAt,
		})
	}

	return items, nil
}

// getGorseRecommendations gets personalized recommendations from Gorse
func (s *Service) getGorseRecommendations(ctx context.Context, userID string, limit int) ([]TimelineItem, error) {
	scores, err := s.gorseClient.GetForYouFeed(userID, limit, 0)
	if err != nil {
		return nil, fmt.Errorf("failed to get gorse recommendations: %w", err)
	}

	items := make([]TimelineItem, 0, len(scores))
	for _, score := range scores {
		post := score.Post
		items = append(items, TimelineItem{
			ID:        post.ID,
			Type:      "recommendation",
			Post:      &post,
			Score:     score.Score,
			Reason:    score.Reason,
			Source:    "gorse",
			CreatedAt: post.CreatedAt,
		})
	}

	return items, nil
}

// getTrendingPosts gets trending posts
func (s *Service) getTrendingPosts(ctx context.Context, limit int) ([]TimelineItem, error) {
	// Get trending posts based on engagement velocity
	// Posts from the last 7 days with high engagement
	sevenDaysAgo := time.Now().AddDate(0, 0, -7)

	var posts []models.AudioPost
	err := s.db.
		Preload("User").
		Where("is_public = ? AND deleted_at IS NULL AND is_archived = ? AND created_at > ?", true, false, sevenDaysAgo).
		Order("(like_count + play_count * 0.5 + comment_count * 2) / GREATEST(1, EXTRACT(EPOCH FROM (NOW() - created_at)) / 3600) DESC").
		Limit(limit).
		Find(&posts).Error

	if err != nil {
		return nil, fmt.Errorf("failed to get trending posts: %w", err)
	}

	items := make([]TimelineItem, 0, len(posts))
	for i, post := range posts {
		// Calculate a trending score based on position
		score := 0.8 - (float64(i) * 0.01) // Decreasing score by position

		items = append(items, TimelineItem{
			ID:        post.ID,
			Type:      "post",
			Post:      &post,
			User:      &post.User,
			Score:     score,
			Reason:    "trending",
			Source:    "trending",
			CreatedAt: post.CreatedAt,
		})
	}

	return items, nil
}

// getRecentPosts gets recent public posts as a fallback
func (s *Service) getRecentPosts(ctx context.Context, userID string, limit int) ([]TimelineItem, error) {
	var posts []models.AudioPost
	err := s.db.
		Preload("User").
		Where("is_public = ? AND deleted_at IS NULL AND is_archived = ? AND user_id != ?", true, false, userID).
		Order("created_at DESC").
		Limit(limit).
		Find(&posts).Error

	if err != nil {
		return nil, fmt.Errorf("failed to get recent posts: %w", err)
	}

	items := make([]TimelineItem, 0, len(posts))
	for _, post := range posts {
		items = append(items, TimelineItem{
			ID:        post.ID,
			Type:      "post",
			Post:      &post,
			User:      &post.User,
			Score:     0.5, // Lower base score for recent posts
			Source:    "recent",
			CreatedAt: post.CreatedAt,
		})
	}

	return items, nil
}

// rankItems scores and sorts timeline items
func (s *Service) rankItems(items []TimelineItem, userID string) []TimelineItem {
	// Apply ranking weights
	for i := range items {
		item := &items[i]

		// Base score from source
		baseScore := item.Score

		// Source priority weights
		switch item.Source {
		case "following":
			baseScore *= 1.5 // Highest priority for followed users
		case "gorse":
			baseScore *= 1.3 // High priority for personalized recommendations
		case "trending":
			baseScore *= 1.0 // Standard priority for trending
		case "recent":
			baseScore *= 0.7 // Lower priority for just recent
		}

		// Recency boost - newer posts get a boost
		if item.Post != nil {
			hoursSincePost := time.Since(item.CreatedAt).Hours()
			if hoursSincePost < 1 {
				baseScore *= 1.5 // Very recent
			} else if hoursSincePost < 6 {
				baseScore *= 1.3 // Pretty recent
			} else if hoursSincePost < 24 {
				baseScore *= 1.1 // Today
			} else if hoursSincePost > 168 { // More than a week old
				baseScore *= 0.8 // Decay for old content
			}

			// Engagement boost
			if item.Post.LikeCount > 50 {
				baseScore *= 1.3
			} else if item.Post.LikeCount > 20 {
				baseScore *= 1.2
			} else if item.Post.LikeCount > 5 {
				baseScore *= 1.1
			}

			// Play count boost
			if item.Post.PlayCount > 100 {
				baseScore *= 1.2
			} else if item.Post.PlayCount > 50 {
				baseScore *= 1.1
			}
		}

		item.Score = baseScore
	}

	// Sort by score (descending)
	sort.Slice(items, func(i, j int) bool {
		// If scores are very close, prefer more recent
		if abs(items[i].Score-items[j].Score) < 0.1 {
			return items[i].CreatedAt.After(items[j].CreatedAt)
		}
		return items[i].Score > items[j].Score
	})

	return items
}

// enrichItemsWithUsers loads user data for items that don't have it
func (s *Service) enrichItemsWithUsers(items []TimelineItem) {
	for i := range items {
		if items[i].Post != nil && items[i].User == nil {
			var user models.User
			if err := s.db.First(&user, "id = ?", items[i].Post.UserID).Error; err == nil {
				items[i].User = &user
			}
		}
	}
}

// extractIDFromObject extracts the ID from a Stream.io object string
// e.g., "loop:abc123" -> "abc123"
func extractIDFromObject(object string) string {
	for i := len(object) - 1; i >= 0; i-- {
		if object[i] == ':' {
			return object[i+1:]
		}
	}
	return ""
}

// abs returns the absolute value of a float64
func abs(x float64) float64 {
	if x < 0 {
		return -x
	}
	return x
}

// SyncUserToGorseWithContext syncs a user to Gorse for recommendations with context
func (s *Service) SyncUserToGorseWithContext(ctx context.Context, userID string) error {
	return s.gorseClient.SyncUserWithContext(ctx, userID)
}

// SyncPostToGorseWithContext syncs a post to Gorse for recommendations with context
func (s *Service) SyncPostToGorseWithContext(ctx context.Context, postID string) error {
	return s.gorseClient.SyncItemWithContext(ctx, postID)
}

// RecordInteractionWithContext records a user interaction with a post to Gorse with context
func (s *Service) RecordInteractionWithContext(ctx context.Context, userID, postID, interactionType string) error {
	return s.gorseClient.SyncFeedbackWithContext(ctx, userID, postID, interactionType)
}

// getMutedUserIDs returns the IDs of users that the given user has muted
func (s *Service) getMutedUserIDs(userID string) ([]string, error) {
	var mutedUserIDs []string
	err := s.db.Model(&models.MutedUser{}).
		Where("user_id = ?", userID).
		Pluck("muted_user_id", &mutedUserIDs).Error
	return mutedUserIDs, err
}
