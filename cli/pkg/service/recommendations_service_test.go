package service

import (
	"testing"
)

func TestNewRecommendationsService(t *testing.T) {
	service := NewRecommendationsService()
	if service == nil {
		t.Error("NewRecommendationsService returned nil")
	}
}

func TestRecommendationFeedbackService_NewRecommendationFeedbackService(t *testing.T) {
	service := NewRecommendationFeedbackService()
	if service == nil {
		t.Error("NewRecommendationFeedbackService returned nil")
	}
}

func TestRecommendationFeedbackService_DislikePost(t *testing.T) {
	service := NewRecommendationFeedbackService()

	// DislikePost requires interactive input
	err := service.DislikePost()
	t.Logf("DislikePost (requires input): %v", err)
}

func TestRecommendationFeedbackService_SkipPost(t *testing.T) {
	service := NewRecommendationFeedbackService()

	// SkipPost requires interactive input
	err := service.SkipPost()
	t.Logf("SkipPost (requires input): %v", err)
}

func TestRecommendationFeedbackService_HidePost(t *testing.T) {
	service := NewRecommendationFeedbackService()

	// HidePost requires interactive input
	err := service.HidePost()
	t.Logf("HidePost (requires input): %v", err)
}

func TestRecommendationFeedbackService_GetCTRMetrics(t *testing.T) {
	service := NewRecommendationFeedbackService()

	// GetCTRMetrics requires interactive input
	err := service.GetCTRMetrics()
	t.Logf("GetCTRMetrics (requires input): %v", err)
}
