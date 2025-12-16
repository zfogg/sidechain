package api

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// GetFeed retrieves a feed by type with pagination
func GetFeed(feedType string, page, pageSize int) (*FeedResponse, error) {
	logger.Debug("Fetching feed", "type", feedType, "page", page)

	var response FeedResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/feed/%s", feedType))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch feed: %s", resp.Status())
	}

	return &response, nil
}

// SearchPosts searches for posts with a query
func SearchPosts(query string, page, pageSize int) (*PostListResponse, error) {
	logger.Debug("Searching posts", "query", query, "page", page)

	var response PostListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"q":         query,
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/posts/search")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to search posts: %s", resp.Status())
	}

	return &response, nil
}

// Sound represents a sound/sample in the system
type Sound struct {
	ID          string    `json:"id"`
	Name        string    `json:"name"`
	Description string    `json:"description"`
	UserID      string    `json:"user_id"`
	BPM         int       `json:"bpm,omitempty"`
	Key         string    `json:"key,omitempty"`
	Genre       []string  `json:"genre,omitempty"`
	Duration    int       `json:"duration"`
	PlayCount   int       `json:"play_count"`
	DownloadCount int     `json:"download_count"`
	CreatedAt   time.Time `json:"created_at"`
	UpdatedAt   time.Time `json:"updated_at"`
}

type SoundSearchResponse struct {
	Sounds     []Sound `json:"sounds"`
	TotalCount int     `json:"total_count"`
	Page       int     `json:"page"`
	PageSize   int     `json:"page_size"`
}

// SearchSounds searches for sounds
func SearchSounds(query string, page, pageSize int) (*SoundSearchResponse, error) {
	logger.Debug("Searching sounds", "query", query)

	var response SoundSearchResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"q":         query,
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/sounds/search")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to search sounds: %s", resp.Status())
	}

	return &response, nil
}

// GetTrendingSounds retrieves trending sounds
func GetTrendingSounds(page, pageSize int) (*SoundSearchResponse, error) {
	logger.Debug("Fetching trending sounds")

	var response SoundSearchResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/sounds/trending")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch trending sounds: %s", resp.Status())
	}

	return &response, nil
}

// GetSoundInfo retrieves information about a specific sound
func GetSoundInfo(soundID string) (*Sound, error) {
	logger.Debug("Fetching sound info", "sound_id", soundID)

	var response struct {
		Sound Sound `json:"sound"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/sounds/%s", soundID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch sound info: %s", resp.Status())
	}

	return &response.Sound, nil
}

// GetSoundPosts retrieves posts that use a specific sound
func GetSoundPosts(soundID string, page, pageSize int) (*PostListResponse, error) {
	logger.Debug("Fetching posts using sound", "sound_id", soundID)

	var response PostListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/sounds/%s/posts", soundID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch sound posts: %s", resp.Status())
	}

	return &response, nil
}

// GetTrendingUsers retrieves trending producers
func GetTrendingUsers(page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Fetching trending users")

	var response UserListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/users/trending")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch trending users: %s", resp.Status())
	}

	return &response, nil
}

// GetFeaturedUsers retrieves featured producers
func GetFeaturedUsers(page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Fetching featured users")

	var response UserListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/users/featured")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch featured users: %s", resp.Status())
	}

	return &response, nil
}

// GetSuggestedUsers retrieves suggested users to follow
func GetSuggestedUsers(page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Fetching suggested users")

	var response UserListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/users/suggested")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch suggested users: %s", resp.Status())
	}

	return &response, nil
}

// UserListResponse represents a paginated list of users
type UserListResponse struct {
	Users      []User `json:"users"`
	TotalCount int    `json:"total_count"`
	Page       int    `json:"page"`
	PageSize   int    `json:"page_size"`
}

// GetUsersByGenre retrieves users by genre
func GetUsersByGenre(genre string, page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Fetching users by genre", "genre", genre)

	var response UserListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"genre":     genre,
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/users/by-genre")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch users by genre: %s", resp.Status())
	}

	return &response, nil
}

// GetRecommendations retrieves recommended users similar to a given user
func GetRecommendations(username string, page, pageSize int) (*UserListResponse, error) {
	logger.Debug("Fetching recommendations", "username", username)

	var response UserListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/discover/recommendations/%s", username))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch recommendations: %s", resp.Status())
	}

	return &response, nil
}

// GetEnrichedTimeline retrieves timeline with reaction counts and enrichment
func GetEnrichedTimeline(page, pageSize int) (*FeedResponse, error) {
	logger.Debug("Fetching enriched timeline", "page", page)

	var response FeedResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/feed/timeline/enriched")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch enriched timeline: %s", resp.Status())
	}

	return &response, nil
}

// GetLatestFeed retrieves recent posts in chronological order
func GetLatestFeed(page, pageSize int) (*FeedResponse, error) {
	logger.Debug("Fetching latest feed", "page", page)

	var response FeedResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/recommendations/latest")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch latest feed: %s", resp.Status())
	}

	return &response, nil
}

// GetForYouFeedWithFilters retrieves personalized feed with optional genre/BPM filtering
func GetForYouFeedWithFilters(page, pageSize int, genre string, minBPM, maxBPM int) (*FeedResponse, error) {
	logger.Debug("Fetching for-you feed with filters", "page", page, "genre", genre, "min_bpm", minBPM, "max_bpm", maxBPM)

	var response FeedResponse

	queryParams := map[string]string{
		"page":      fmt.Sprintf("%d", page),
		"page_size": fmt.Sprintf("%d", pageSize),
	}

	if genre != "" {
		queryParams["genre"] = genre
	}
	if minBPM > 0 {
		queryParams["min_bpm"] = fmt.Sprintf("%d", minBPM)
	}
	if maxBPM > 0 {
		queryParams["max_bpm"] = fmt.Sprintf("%d", maxBPM)
	}

	resp, err := client.GetClient().
		R().
		SetQueryParams(queryParams).
		SetResult(&response).
		Get("/api/v1/recommendations/for-you")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch for-you feed: %s", resp.Status())
	}

	return &response, nil
}
