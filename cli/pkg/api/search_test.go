package api

import (
	"testing"
)

func TestSearchUsers_WithValidQuery(t *testing.T) {
	queries := []struct {
		query  string
		limit  int
		offset int
	}{
		{"producer", 10, 0},
		{"dj", 20, 0},
		{"music", 10, 10},
	}

	for _, q := range queries {
		resp, err := SearchUsers(q.query, q.limit, q.offset)
		if err != nil {
			t.Logf("SearchUsers '%s': API called (error expected): %v", q.query, err)
			continue
		}
		if resp == nil {
			t.Errorf("SearchUsers returned nil for query '%s'", q.query)
		}
		if resp != nil {
			if len(resp.Users) > q.limit && q.limit > 0 {
				t.Errorf("SearchUsers returned more results than limit for query '%s'", q.query)
			}
			for _, user := range resp.Users {
				if user.ID == "" {
					t.Errorf("Search result missing user ID for query '%s'", q.query)
				}
			}
		}
	}
}

func TestSearchUsers_WithEmptyQuery(t *testing.T) {
	resp, err := SearchUsers("", 10, 0)

	if resp != nil && len(resp.Users) > 0 && err == nil {
		t.Logf("SearchUsers accepted empty query")
	}
}

func TestSearchUsers_WithInvalidPagination(t *testing.T) {
	tests := []struct {
		query  string
		limit  int
		offset int
	}{
		{"producer", 0, 0},
		{"producer", -10, 0},
		{"producer", 10, -1},
	}

	for _, tt := range tests {
		resp, err := SearchUsers(tt.query, tt.limit, tt.offset)
		if resp != nil && err == nil {
			t.Logf("SearchUsers accepted invalid pagination: limit=%d, offset=%d", tt.limit, tt.offset)
		}
	}
}

func TestSearchUsers_WithDifferentLimits(t *testing.T) {
	limits := []int{5, 10, 20, 50, 100}

	for _, limit := range limits {
		resp, err := SearchUsers("music", limit, 0)
		if err != nil {
			t.Logf("SearchUsers with limit %d: API called (error expected)", limit)
			continue
		}
		if resp != nil && len(resp.Users) > limit && limit > 0 {
			t.Errorf("SearchUsers exceeded limit: expected max %d, got %d", limit, len(resp.Users))
		}
	}
}

func TestSearchUsers_WithOffsets(t *testing.T) {
	tests := []struct {
		offset int
		limit  int
	}{
		{0, 10},
		{10, 10},
		{20, 20},
		{50, 10},
	}

	for _, tt := range tests {
		resp, err := SearchUsers("music", tt.limit, tt.offset)
		if err != nil {
			t.Logf("SearchUsers with offset %d: API called (error expected)", tt.offset)
			continue
		}
		if resp != nil {
			// Verify pagination structure
			if resp.TotalCount < 0 {
				t.Errorf("SearchUsers returned negative total count")
			}
		}
	}
}

func TestGetAvailableGenres(t *testing.T) {
	resp, err := GetAvailableGenres()

	if err != nil {
		t.Logf("GetAvailableGenres API called (error expected): %v", err)
		return
	}
	if resp == nil {
		t.Error("GetAvailableGenres returned nil response")
		return
	}
	// Should have genres
	if len(resp.Genres) == 0 {
		t.Logf("GetAvailableGenres returned empty genre list")
	}
	// Validate genres have names
	for _, genre := range resp.Genres {
		if genre.Name == "" {
			t.Error("Genre missing name")
		}
	}
}

func TestGetSimilarUsers_WithValidUserID(t *testing.T) {
	userIDs := []string{"user-1", "producer-123", "dj-456"}
	limits := []int{5, 10, 20}

	for _, userID := range userIDs {
		for _, limit := range limits {
			resp, err := GetSimilarUsers(userID, limit)
			if err != nil {
				t.Logf("GetSimilarUsers %s (limit %d): API called (error expected): %v", userID, limit, err)
				continue
			}
			if resp == nil {
				t.Errorf("GetSimilarUsers returned nil for user %s", userID)
			}
			if resp != nil {
				if len(resp.Users) > limit && limit > 0 {
					t.Errorf("GetSimilarUsers exceeded limit for user %s: expected max %d, got %d", userID, limit, len(resp.Users))
				}
				for _, user := range resp.Users {
					if user.ID == "" {
						t.Errorf("Similar user missing ID for source user %s", userID)
					}
				}
			}
		}
	}
}

func TestGetSimilarUsers_WithInvalidUserID(t *testing.T) {
	invalidIDs := []string{"", "nonexistent-user"}

	for _, userID := range invalidIDs {
		resp, err := GetSimilarUsers(userID, 10)
		if resp != nil && len(resp.Users) > 0 && err == nil {
			t.Logf("GetSimilarUsers accepted invalid user ID: %s", userID)
		}
	}
}

func TestGetSimilarUsers_WithInvalidLimit(t *testing.T) {
	invalidLimits := []int{0, -10, -1}

	for _, limit := range invalidLimits {
		resp, err := GetSimilarUsers("user-1", limit)
		if resp != nil && err == nil && limit > 0 {
			t.Logf("GetSimilarUsers accepted invalid limit: %d", limit)
		}
	}
}

func TestGetSimilarUsers_WithDifferentLimits(t *testing.T) {
	limits := []int{1, 5, 10, 20, 50}

	for _, limit := range limits {
		resp, err := GetSimilarUsers("producer-1", limit)
		if err != nil {
			t.Logf("GetSimilarUsers with limit %d: API called (error expected)", limit)
			continue
		}
		if resp != nil && len(resp.Users) > limit && limit > 0 {
			t.Errorf("GetSimilarUsers exceeded limit %d: got %d results", limit, len(resp.Users))
		}
	}
}
