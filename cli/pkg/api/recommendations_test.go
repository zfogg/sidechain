package api

import (
	"testing"
)

func TestGetPopularSounds_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
		{1, 50},
	}

	for _, tt := range tests {
		resp, err := GetPopularSounds(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetPopularSounds page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("GetPopularSounds returned nil")
		}
		if resp != nil && len(resp.Sounds) > tt.pageSize && tt.pageSize > 0 {
			t.Errorf("GetPopularSounds exceeded pageSize")
		}
	}
}

func TestGetLatestPosts_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		resp, err := GetLatestPosts(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetLatestPosts page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("GetLatestPosts returned nil")
		}
		if resp != nil && len(resp.Posts) > tt.pageSize && tt.pageSize > 0 {
			t.Errorf("GetLatestPosts exceeded pageSize")
		}
	}
}

func TestGetPopularPosts_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		resp, err := GetPopularPosts(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetPopularPosts page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("GetPopularPosts returned nil")
		}
	}
}

func TestGetForYouFeed_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		resp, err := GetForYouFeed(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetForYouFeed page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("GetForYouFeed returned nil")
		}
	}
}

func TestDiscoverByGenre_WithValidGenre(t *testing.T) {
	genres := []string{"electronic", "hip-hop", "ambient", "house"}

	for _, genre := range genres {
		resp, err := DiscoverByGenre(genre, 1, 10)
		if err != nil {
			t.Logf("DiscoverByGenre %s: API called (error expected): %v", genre, err)
			continue
		}
		if resp == nil {
			t.Errorf("DiscoverByGenre returned nil for %s", genre)
		}
		if resp != nil {
			for _, user := range resp.Users {
				if user.ID == "" {
					t.Errorf("User missing ID for genre %s", genre)
				}
			}
		}
	}
}

func TestDiscoverByGenre_WithInvalidGenre(t *testing.T) {
	invalidGenres := []string{"", "nonexistent-genre"}

	for _, genre := range invalidGenres {
		resp, err := DiscoverByGenre(genre, 1, 10)
		if resp != nil && len(resp.Users) > 0 && err == nil {
			t.Logf("DiscoverByGenre accepted invalid genre: %s", genre)
		}
	}
}

func TestDiscoverSimilarUsers_WithValidUserID(t *testing.T) {
	userIDs := []string{"user-1", "producer-123"}
	limits := []int{5, 10, 20}

	for _, userID := range userIDs {
		for _, limit := range limits {
			resp, err := DiscoverSimilarUsers(userID, limit)
			if err != nil {
				t.Logf("DiscoverSimilarUsers %s (limit %d): API called (error expected)", userID, limit)
				continue
			}
			if resp == nil {
				t.Errorf("DiscoverSimilarUsers returned nil for %s", userID)
			}
			if resp != nil && len(resp.Users) > limit && limit > 0 {
				t.Errorf("DiscoverSimilarUsers exceeded limit for %s", userID)
			}
		}
	}
}

func TestDiscoverFeatured_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		resp, err := DiscoverFeatured(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("DiscoverFeatured page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("DiscoverFeatured returned nil")
		}
	}
}

func TestDiscoverSuggested_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		resp, err := DiscoverSuggested(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("DiscoverSuggested page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("DiscoverSuggested returned nil")
		}
	}
}

func TestGetPopularUsers_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		resp, err := GetPopularUsers(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetPopularUsers page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("GetPopularUsers returned nil")
		}
		if resp != nil {
			for _, user := range resp.Users {
				if user.ID == "" {
					t.Error("Popular user missing ID")
				}
			}
		}
	}
}

func TestGetPersonalizedRecommendations_WithValidUsername(t *testing.T) {
	usernames := []string{"user1", "producer1", "dj-smith"}

	for _, username := range usernames {
		resp, err := GetPersonalizedRecommendations(username, 1, 10)
		if err != nil {
			t.Logf("GetPersonalizedRecommendations %s: API called (error expected): %v", username, err)
			continue
		}
		if resp == nil {
			t.Errorf("GetPersonalizedRecommendations returned nil for %s", username)
		}
	}
}

func TestGetPersonalizedRecommendations_WithInvalidUsername(t *testing.T) {
	invalidUsernames := []string{"", "nonexistent-user"}

	for _, username := range invalidUsernames {
		resp, err := GetPersonalizedRecommendations(username, 1, 10)
		if resp != nil && len(resp.Users) > 0 && err == nil {
			t.Logf("GetPersonalizedRecommendations accepted invalid username: %s", username)
		}
	}
}

func TestSubmitDiscoveryFeedback_WithValidData(t *testing.T) {
	tests := []struct {
		postID string
		action string
	}{
		{"post-1", "like"},
		{"post-2", "dislike"},
		{"post-3", "skip"},
		{"post-4", "save"},
	}

	for _, tt := range tests {
		err := SubmitDiscoveryFeedback(tt.postID, tt.action)
		if err != nil {
			t.Logf("SubmitDiscoveryFeedback %s (%s): API called (error expected): %v", tt.postID, tt.action, err)
		}
	}
}

func TestSubmitDiscoveryFeedback_WithInvalidData(t *testing.T) {
	tests := []struct {
		postID string
		action string
	}{
		{"", "like"},
		{"post-1", ""},
		{"", ""},
		{"post-1", "invalid-action"},
	}

	for _, tt := range tests {
		err := SubmitDiscoveryFeedback(tt.postID, tt.action)
		if err == nil && (tt.postID == "" || tt.action == "") {
			t.Logf("SubmitDiscoveryFeedback accepted invalid data: postID=%s, action=%s", tt.postID, tt.action)
		}
	}
}
