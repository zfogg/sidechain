package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// GetPopularSounds retrieves trending sounds
func GetPopularSounds(page, pageSize int) (*SoundSearchResponse, error) {
	logger.Debug("Getting popular sounds", "page", page, "page_size", pageSize)
	return GetTrendingSounds(page, pageSize)
}

// GetLatestPosts retrieves the latest posts from the feed
func GetLatestPosts(page, pageSize int) (*FeedResponse, error) {
	logger.Debug("Getting latest posts", "page", page, "page_size", pageSize)
	return GetFeed("latest", page, pageSize)
}

// GetPopularPosts retrieves popular posts
func GetPopularPosts(page, pageSize int) (*FeedResponse, error) {
	logger.Debug("Getting popular posts", "page", page, "page_size", pageSize)
	return GetFeed("trending", page, pageSize)
}

// GetForYouFeed retrieves personalized for-you feed
func GetForYouFeed(page, pageSize int) (*FeedResponse, error) {
	logger.Debug("Getting for-you feed", "page", page, "page_size", pageSize)
	return GetFeed("for-you", page, pageSize)
}

// DiscoverByGenre discovers users and posts in a specific genre
func DiscoverByGenre(genre string, page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Discovering by genre", "genre", genre, "page", page)
	return GetUsersByGenre(genre, page, pageSize)
}

// DiscoverSimilarUsers finds users similar to a given user
func DiscoverSimilarUsers(userID string, limit int) (*DiscoveryUsersResponse, error) {
	logger.Debug("Discovering similar users", "user_id", userID, "limit", limit)
	return GetSimilarUsers(userID, limit)
}

// DiscoverFeatured retrieves featured users to follow
func DiscoverFeatured(page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Discovering featured users", "page", page, "page_size", pageSize)
	return GetFeaturedUsers(page, pageSize)
}

// DiscoverSuggested retrieves suggested users to follow
func DiscoverSuggested(page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Discovering suggested users", "page", page, "page_size", pageSize)
	return GetSuggestedUsers(page, pageSize)
}

// GetPopularUsers retrieves trending/popular users
func GetPopularUsers(page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Getting popular users", "page", page, "page_size", pageSize)
	return GetTrendingUsers(page, pageSize)
}

// GetPersonalizedRecommendations retrieves personalized recommendations for current user
func GetPersonalizedRecommendations(username string, page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Getting personalized recommendations", "username", username, "page", page)
	return GetRecommendations(username, page, pageSize)
}

// DiscoveryRequest represents a discovery query
type DiscoveryRequest struct {
	Type  string // "trending", "latest", "for-you", "genre", "featured", "suggested"
	Query string // genre name or other query parameter
	Page  int
	Limit int
}

// DiscoveryResponse wraps discovery results
type DiscoveryResponse struct {
	Posts []Post            `json:"posts,omitempty"`
	Users []User            `json:"users,omitempty"`
	Sounds []Sound           `json:"sounds,omitempty"`
	Type  string            `json:"type"`
	Count int               `json:"count"`
}

// SubmitDiscoveryFeedback submits feedback for discovery results
func SubmitDiscoveryFeedback(postID string, action string) error {
	logger.Debug("Submitting discovery feedback", "post_id", postID, "action", action)

	request := map[string]string{
		"action": action, // "like", "dislike", "skip", "save"
	}

	resp, err := client.GetClient().
		R().
		SetBody(request).
		Post(fmt.Sprintf("/api/v1/posts/%s/feedback", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to submit feedback: %s", resp.Status())
	}

	return nil
}
