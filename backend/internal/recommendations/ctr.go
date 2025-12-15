package recommendations

import (
	"fmt"
	"log"
	"time"

	"gorm.io/gorm"
)

// CTRMetric represents click-through rate for a recommendation source
type CTRMetric struct {
	Source      string
	Impressions int64
	Clicks      int64
	CTR         float64 // Click-through rate (clicks/impressions * 100)
	Date        time.Time
}

// CalculateCTR calculates click-through rates for each recommendation source
// Task 8.3
func CalculateCTR(db *gorm.DB, since time.Time) ([]CTRMetric, error) {
	var metrics []CTRMetric

	// Sources to calculate CTR for
	sources := []string{"for-you", "popular", "latest", "discovery", "similar"}

	for _, source := range sources {
		var impressionCount int64
		var clickCount int64

		// Count total impressions for this source since the given time
		err := db.Table("recommendation_impressions").
			Where("source = ? AND created_at >= ? AND deleted_at IS NULL", source, since).
			Count(&impressionCount).Error
		if err != nil {
			return nil, fmt.Errorf("failed to count impressions for %s: %w", source, err)
		}

		// Count total clicks for this source since the given time
		err = db.Table("recommendation_clicks").
			Where("source = ? AND created_at >= ? AND deleted_at IS NULL", source, since).
			Count(&clickCount).Error
		if err != nil {
			return nil, fmt.Errorf("failed to count clicks for %s: %w", source, err)
		}

		// Calculate CTR
		ctr := 0.0
		if impressionCount > 0 {
			ctr = (float64(clickCount) / float64(impressionCount)) * 100
		}

		metrics = append(metrics, CTRMetric{
			Source:      source,
			Impressions: impressionCount,
			Clicks:      clickCount,
			CTR:         ctr,
			Date:        time.Now(),
		})
	}

	return metrics, nil
}

// LogCTRMetrics calculates and logs CTR metrics for the past 24 hours
// Task 8.3
func LogCTRMetrics(db *gorm.DB) error {
	since := time.Now().Add(-24 * time.Hour)
	metrics, err := CalculateCTR(db, since)
	if err != nil {
		return err
	}

	log.Println("📊 CTR Metrics (Last 24 Hours):")
	log.Println("─────────────────────────────────────────────────")
	for _, m := range metrics {
		if m.Impressions > 0 {
			log.Printf("  %s: %.2f%% (%d clicks / %d impressions)",
				m.Source, m.CTR, m.Clicks, m.Impressions)
		} else {
			log.Printf("  %s: No impressions", m.Source)
		}
	}
	log.Println("─────────────────────────────────────────────────")

	return nil
}

// GetCTRBySource returns CTR metrics for a specific source over a time period
// Task 8.3
func GetCTRBySource(db *gorm.DB, source string, since time.Time) (*CTRMetric, error) {
	metrics, err := CalculateCTR(db, since)
	if err != nil {
		return nil, err
	}

	for _, m := range metrics {
		if m.Source == source {
			return &m, nil
		}
	}

	return &CTRMetric{
		Source:      source,
		Impressions: 0,
		Clicks:      0,
		CTR:         0.0,
		Date:        time.Now(),
	}, nil
}
