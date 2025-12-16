package service

import (
	"testing"
)

func TestNewSearchService(t *testing.T) {
	service := NewSearchService()
	if service == nil {
		t.Error("NewSearchService returned nil")
	}
}

func TestSearchService_SearchUsers(t *testing.T) {
	service := NewSearchService()

	// SearchUsers requires interactive input
	err := service.SearchUsers()
	t.Logf("SearchUsers (requires input): %v", err)
}

func TestSearchService_GetTrendingUsers(t *testing.T) {
	service := NewSearchService()

	err := service.GetTrendingUsers()
	t.Logf("GetTrendingUsers: %v", err)
}

func TestSearchService_GetFeaturedProducers(t *testing.T) {
	service := NewSearchService()

	err := service.GetFeaturedProducers()
	t.Logf("GetFeaturedProducers: %v", err)
}

func TestSearchService_GetSuggestedUsers(t *testing.T) {
	service := NewSearchService()

	err := service.GetSuggestedUsers()
	t.Logf("GetSuggestedUsers: %v", err)
}

func TestSearchService_ListGenres(t *testing.T) {
	service := NewSearchService()

	err := service.ListGenres()
	t.Logf("ListGenres: %v", err)
}

func TestSearchService_GetUsersByGenre(t *testing.T) {
	service := NewSearchService()

	// GetUsersByGenre requires interactive input
	err := service.GetUsersByGenre()
	t.Logf("GetUsersByGenre (requires input): %v", err)
}

func TestSearchService_GetSimilarUsers(t *testing.T) {
	service := NewSearchService()

	// GetSimilarUsers requires interactive input
	err := service.GetSimilarUsers()
	t.Logf("GetSimilarUsers (requires input): %v", err)
}
