package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// RecommendationFeedbackResponse represents the response to a feedback action
type RecommendationFeedbackResponse struct {
	Message string `json:"message"`
	Success bool   `json:"success"`
	PostID  string `json:"post_id,omitempty"`
}

// RecommendationClickRequest represents a click tracking request
type RecommendationClickRequest struct {
	PostID       string `json:"post_id"`
	Source       string `json:"source"`       // for-you, popular, latest, discovery, similar
	Position     int    `json:"position"`
	PlayDuration float64 `json:"play_duration,omitempty"`
	Completed    bool   `json:"completed"`
	SessionID    string `json:"session_id,omitempty"`
}

// CTRMetricsResponse contains click-through rate metrics
type CTRMetricsResponse struct {
	Period             string               `json:"period"`
	TotalImpressions   int                  `json:"total_impressions"`
	TotalClicks        int                  `json:"total_clicks"`
	OverallCTR         float64              `json:"overall_ctr"`
	CTRBySource        map[string]float64   `json:"ctr_by_source"`
	ImpressionsBySource map[string]int      `json:"impressions_by_source"`
	ClicksBySource      map[string]int      `json:"clicks_by_source"`
	CompletionRate      float64             `json:"completion_rate"`
}

// DislikePost marks a post as not interested
func DislikePost(postID string) (*RecommendationFeedbackResponse, error) {
	logger.Debug("Disliking post", "post_id", postID)

	var response RecommendationFeedbackResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/recommendations/dislike/%s", postID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to dislike post: %s", resp.Status())
	}

	return &response, nil
}

// SkipPost records a post skip
func SkipPost(postID string) (*RecommendationFeedbackResponse, error) {
	logger.Debug("Skipping post", "post_id", postID)

	var response RecommendationFeedbackResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/recommendations/skip/%s", postID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to skip post: %s", resp.Status())
	}

	return &response, nil
}

// HidePost hides a post from recommendations
func HidePost(postID string) (*RecommendationFeedbackResponse, error) {
	logger.Debug("Hiding post", "post_id", postID)

	var response RecommendationFeedbackResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/recommendations/hide/%s", postID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to hide post: %s", resp.Status())
	}

	return &response, nil
}

// TrackRecommendationClick tracks a click/play action on a recommended post
func TrackRecommendationClick(req RecommendationClickRequest) (*RecommendationFeedbackResponse, error) {
	logger.Debug("Tracking recommendation click", "post_id", req.PostID)

	var response RecommendationFeedbackResponse

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post("/api/v1/recommendations/click")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to track click: %s", resp.Status())
	}

	return &response, nil
}

// GetCTRMetrics retrieves click-through rate metrics
func GetCTRMetrics(period string) (*CTRMetricsResponse, error) {
	logger.Debug("Fetching CTR metrics", "period", period)

	var response CTRMetricsResponse

	resp, err := client.GetClient().
		R().
		SetQueryParam("period", period).
		SetResult(&response).
		Get("/api/v1/recommendations/metrics/ctr")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch CTR metrics: %s", resp.Status())
	}

	return &response, nil
}
