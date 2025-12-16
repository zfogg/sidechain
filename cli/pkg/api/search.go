package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// SearchUsersResponse represents user search results
type SearchUsersResponse struct {
	Users      []User `json:"users"`
	TotalCount int    `json:"total_count"`
	Limit      int    `json:"limit"`
	Offset     int    `json:"offset"`
}

// DiscoveryUser represents a user in discovery/trending results
type DiscoveryUser struct {
	ID            string `json:"id"`
	Username      string `json:"username"`
	DisplayName   string `json:"display_name"`
	Bio           string `json:"bio"`
	ProfilePic    string `json:"profile_picture"`
	FollowerCount int    `json:"follower_count"`
	PostCount     int    `json:"post_count"`
	IsFollowing   bool   `json:"is_following,omitempty"`
}

// DiscoveryUsersResponse represents a list of discovery users
type DiscoveryUsersResponse struct {
	Users      []DiscoveryUser `json:"users"`
	TotalCount int             `json:"total_count"`
}

// GenreInfo represents genre metadata
type GenreInfo struct {
	Name      string `json:"name"`
	UserCount int    `json:"user_count"`
}

// GenresResponse lists available genres
type GenresResponse struct {
	Genres []GenreInfo `json:"genres"`
	Count  int         `json:"count"`
}

// SearchUsers searches for users by username or display name
func SearchUsers(query string, limit, offset int) (*SearchUsersResponse, error) {
	logger.Debug("Searching users", "query", query, "limit", limit, "offset", offset)

	var response SearchUsersResponse

	resp, err := client.GetClient().
		R().
		SetQueryParam("q", query).
		SetQueryParam("limit", fmt.Sprintf("%d", limit)).
		SetQueryParam("offset", fmt.Sprintf("%d", offset)).
		SetResult(&response).
		Get("/api/v1/search/users")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to search users: %s", resp.Status())
	}

	return &response, nil
}


// GetAvailableGenres retrieves all available genres
func GetAvailableGenres() (*GenresResponse, error) {
	logger.Debug("Fetching available genres")

	var response GenresResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get("/api/v1/discover/genres")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch genres: %s", resp.Status())
	}

	return &response, nil
}


// GetSimilarUsers retrieves users with similar music taste
func GetSimilarUsers(userID string, limit int) (*DiscoveryUsersResponse, error) {
	logger.Debug("Fetching similar users", "user_id", userID, "limit", limit)

	var response DiscoveryUsersResponse

	resp, err := client.GetClient().
		R().
		SetQueryParam("limit", fmt.Sprintf("%d", limit)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/users/%s/similar", userID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch similar users: %s", resp.Status())
	}

	return &response, nil
}
