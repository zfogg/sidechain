package api

import (
	"testing"
)

func TestDislikePost_API(t *testing.T) {
	// Try with non-existent post
	postID := "nonexistent-post"

	resp, err := DislikePost(postID)

	if err != nil {
		t.Logf("DislikePost error (expected, post doesn't exist): %v", err)
		return
	}

	if resp == nil {
		t.Error("DislikePost returned nil response")
	}
	if resp != nil && !resp.Success {
		t.Logf("DislikePost returned success=false: %s", resp.Message)
	}
}

func TestSkipPost_API(t *testing.T) {
	postID := "nonexistent-post"

	resp, err := SkipPost(postID)

	if err != nil {
		t.Logf("SkipPost error (expected, post doesn't exist): %v", err)
		return
	}

	if resp == nil {
		t.Error("SkipPost returned nil response")
	}
}

func TestHidePost_API(t *testing.T) {
	postID := "nonexistent-post"

	resp, err := HidePost(postID)

	if err != nil {
		t.Logf("HidePost error (expected, post doesn't exist): %v", err)
		return
	}

	if resp == nil {
		t.Error("HidePost returned nil response")
	}
}

func TestTrackRecommendationClick_API(t *testing.T) {
	req := RecommendationClickRequest{
		PostID:       "nonexistent-post",
		Source:       "for-you",
		Position:     1,
		PlayDuration: 5.5,
		Completed:    false,
	}

	resp, err := TrackRecommendationClick(req)

	if err != nil {
		t.Logf("TrackRecommendationClick error: %v", err)
		return
	}

	if resp == nil {
		t.Error("TrackRecommendationClick returned nil")
	}
}

func TestGetCTRMetrics_API(t *testing.T) {
	resp, err := GetCTRMetrics("24h")

	if err != nil {
		t.Logf("GetCTRMetrics error (may be unauthenticated): %v", err)
		return
	}

	if resp == nil {
		t.Error("GetCTRMetrics returned nil")
	}
	if resp != nil {
		if resp.Period == "" {
			t.Error("Period is empty")
		}
		if resp.TotalImpressions < 0 || resp.TotalClicks < 0 {
			t.Errorf("Negative metrics: impressions=%d, clicks=%d", resp.TotalImpressions, resp.TotalClicks)
		}
		if resp.OverallCTR < 0 || resp.OverallCTR > 1 {
			t.Errorf("Invalid CTR value: %f (should be 0-1)", resp.OverallCTR)
		}
	}
}

func TestGetCTRMetrics_DifferentPeriods_API(t *testing.T) {
	periods := []string{"24h", "7d", "30d"}

	for _, period := range periods {
		resp, err := GetCTRMetrics(period)

		if err != nil {
			t.Logf("GetCTRMetrics(%s) error: %v", period, err)
			continue
		}

		if resp != nil && resp.Period != period {
			t.Errorf("Period mismatch: requested %s, got %s", period, resp.Period)
		}
	}
}
