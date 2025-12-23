package recommendations

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
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
func LogCTRMetrics(db *gorm.DB) error {
	since := time.Now().Add(-24 * time.Hour)
	metrics, err := CalculateCTR(db, since)
	if err != nil {
		return err
	}

	logger.Log.Info("CTR Metrics (Last 24 Hours)")
	logger.Log.Info("-----------------------------------------------")
	for _, m := range metrics {
		if m.Impressions > 0 {
			logger.Log.Info("CTR metric",
				zap.String("source", m.Source),
				zap.Float64("ctr", m.CTR),
				zap.Int64("clicks", m.Clicks),
				zap.Int64("impressions", m.Impressions))
		} else {
			logger.Log.Info("CTR metric",
				zap.String("source", m.Source),
				zap.String("status", "No impressions"))
		}
	}
	logger.Log.Info("-----------------------------------------------")

	return nil
}

// GetCTRBySource returns CTR metrics for a specific source over a time period
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
