package service

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// RecommendationFeedbackService provides operations for recommendation feedback
type RecommendationFeedbackService struct{}

// NewRecommendationFeedbackService creates a new recommendation feedback service
func NewRecommendationFeedbackService() *RecommendationFeedbackService {
	return &RecommendationFeedbackService{}
}

// DislikePost marks a post as not interested
func (rfs *RecommendationFeedbackService) DislikePost() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID to dislike: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	// Send dislike feedback
	resp, err := api.DislikePost(postID)
	if err != nil {
		return fmt.Errorf("failed to dislike post: %w", err)
	}

	formatter.PrintSuccess("✓ Post marked as not interested!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Post ID": postID,
		"Message": resp.Message,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// SkipPost records a post skip
func (rfs *RecommendationFeedbackService) SkipPost() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID to skip: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	// Send skip feedback
	resp, err := api.SkipPost(postID)
	if err != nil {
		return fmt.Errorf("failed to skip post: %w", err)
	}

	formatter.PrintSuccess("✓ Post skipped!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Post ID": postID,
		"Message": resp.Message,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// HidePost hides a post from recommendations
func (rfs *RecommendationFeedbackService) HidePost() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Post ID to hide: ")
	postID, _ := reader.ReadString('\n')
	postID = strings.TrimSpace(postID)

	if postID == "" {
		return fmt.Errorf("post ID cannot be empty")
	}

	// Send hide feedback
	resp, err := api.HidePost(postID)
	if err != nil {
		return fmt.Errorf("failed to hide post: %w", err)
	}

	formatter.PrintSuccess("✓ Post hidden from recommendations!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Post ID": postID,
		"Message": resp.Message,
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// GetCTRMetrics retrieves and displays click-through rate metrics
func (rfs *RecommendationFeedbackService) GetCTRMetrics() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Time period (24h/7d/30d) [24h]: ")
	periodStr, _ := reader.ReadString('\n')
	periodStr = strings.TrimSpace(periodStr)

	if periodStr == "" {
		periodStr = "24h"
	}

	// Validate period
	validPeriods := map[string]bool{
		"24h": true,
		"7d":  true,
		"30d": true,
	}
	if !validPeriods[periodStr] {
		return fmt.Errorf("invalid period: %s (must be 24h, 7d, or 30d)", periodStr)
	}

	// Fetch metrics
	metrics, err := api.GetCTRMetrics(periodStr)
	if err != nil {
		return fmt.Errorf("failed to fetch CTR metrics: %w", err)
	}

	formatter.PrintInfo("📊 Click-Through Rate Metrics (%s)", metrics.Period)
	fmt.Printf("\n")

	// Display summary
	keyValues := map[string]interface{}{
		"Total Impressions":  metrics.TotalImpressions,
		"Total Clicks":       metrics.TotalClicks,
		"Overall CTR":        fmt.Sprintf("%.2f%%", metrics.OverallCTR*100),
		"Completion Rate":    fmt.Sprintf("%.2f%%", metrics.CompletionRate*100),
	}
	formatter.PrintKeyValue(keyValues)
	fmt.Printf("\n")

	// CTR by source
	if len(metrics.CTRBySource) > 0 {
		fmt.Println("CTR by Source:")
		sourceHeaders := []string{"Source", "CTR", "Impressions", "Clicks"}
		sourceRows := make([][]string, 0)
		for source := range metrics.CTRBySource {
			ctr := metrics.CTRBySource[source]
			impressions := metrics.ImpressionsBySource[source]
			clicks := metrics.ClicksBySource[source]
			sourceRows = append(sourceRows, []string{
				source,
				fmt.Sprintf("%.2f%%", ctr*100),
				fmt.Sprintf("%d", impressions),
				fmt.Sprintf("%d", clicks),
			})
		}
		formatter.PrintTable(sourceHeaders, sourceRows)
	}

	return nil
}
