package handlers

import (
	"fmt"
	"log"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/container"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

// ErrorTrackingHandler handles error tracking endpoints.
// Uses dependency injection via container for service dependencies.
type ErrorTrackingHandler struct {
	container *container.Container
}

// NewErrorTrackingHandler creates a new error tracking handler.
// All dependencies are accessed through the container.
func NewErrorTrackingHandler(c *container.Container) *ErrorTrackingHandler {
	return &ErrorTrackingHandler{
		container: c,
	}
}

// RecordErrors saves a batch of errors from the plugin
// POST /api/v1/errors/batch
func (h *ErrorTrackingHandler) RecordErrors(c *gin.Context) {
	var req models.ErrorBatch
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error": "not_authenticated",
		})
		return
	}

	if len(req.Errors) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{
			"error": "no_errors_provided",
		})
		return
	}

	// Process each error
	recordedCount := 0
	for _, errPayload := range req.Errors {
		// Validate required fields
		if errPayload.Message == "" {
			log.Printf("Skipping error with empty message")
			continue
		}

		if errPayload.Severity == "" {
			errPayload.Severity = "Error"
		}

		if errPayload.Source == "" {
			errPayload.Source = "Unknown"
		}

		// Check if similar error exists in last 1 hour
		existing := h.findSimilarError(userID.(string), errPayload.Message, errPayload.Source)

		if existing != nil {
			// Update existing error
			existing.Occurrences += errPayload.Occurrences
			existing.LastSeen = time.Now()
			if err := database.DB.Save(existing).Error; err != nil {
				log.Printf("Failed to update error log: %v", err)
				continue
			}
			recordedCount++
		} else {
			// Create new error log
			context := models.Context{}
			if errPayload.Context != nil {
				context = errPayload.Context
			}

			errorLog := &models.ErrorLog{
				ID:          uuid.New().String(),
				UserID:      userID.(string),
				Source:      errPayload.Source,
				Severity:    errPayload.Severity,
				Message:     errPayload.Message,
				Context:     context,
				Occurrences: errPayload.Occurrences,
				FirstSeen:   time.Now(),
				LastSeen:    time.Now(),
				IsResolved:  false,
				CreatedAt:   time.Now(),
				UpdatedAt:   time.Now(),
			}

			if err := database.DB.Create(errorLog).Error; err != nil {
				log.Printf("Failed to create error log: %v", err)
				continue
			}
			recordedCount++
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"status":         "recorded",
		"recorded_count": recordedCount,
		"total_count":    len(req.Errors),
	})

	log.Printf("Recorded %d/%d errors for user %s", recordedCount, len(req.Errors), userID)
}

// GetErrorStats returns error statistics for the authenticated user
// GET /api/v1/errors/stats
func (h *ErrorTrackingHandler) GetErrorStats(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error": "not_authenticated",
		})
		return
	}

	hours := 24 // Default to last 24 hours
	if h, ok := c.GetQuery("hours"); ok {
		fmt.Sscanf(h, "%d", &hours)
		if hours < 1 {
			hours = 1
		}
		if hours > 168 { // Max 1 week
			hours = 168
		}
	}

	sinceTime := time.Now().Add(-time.Duration(hours) * time.Hour)

	stats := &models.ErrorStats{
		ErrorsBySeverity: make(map[string]int64),
		ErrorsBySource:   make(map[string]int64),
		TopErrors:        make([]models.TopErrorItem, 0),
		ErrorTrendHourly: make([]models.TrendItem, 0),
	}

	// Total error count
	if err := database.DB.
		Model(&models.ErrorLog{}).
		Where("user_id = ? AND created_at >= ?", userID, sinceTime).
		Count(&stats.TotalErrors).Error; err != nil {
		log.Printf("Failed to count total errors: %v", err)
	}

	// Errors by severity
	var severityStats []struct {
		Severity string
		Count    int64
	}
	if err := database.DB.
		Model(&models.ErrorLog{}).
		Select("severity, COUNT(*) as count").
		Where("user_id = ? AND created_at >= ?", userID, sinceTime).
		Group("severity").
		Scan(&severityStats).Error; err == nil {
		for _, s := range severityStats {
			stats.ErrorsBySeverity[s.Severity] = s.Count
		}
	}

	// Errors by source
	var sourceStats []struct {
		Source string
		Count  int64
	}
	if err := database.DB.
		Model(&models.ErrorLog{}).
		Select("source, COUNT(*) as count").
		Where("user_id = ? AND created_at >= ?", userID, sinceTime).
		Group("source").
		Scan(&sourceStats).Error; err == nil {
		for _, s := range sourceStats {
			stats.ErrorsBySource[s.Source] = s.Count
		}
	}

	// Top errors (most frequent)
	var topErrors []models.ErrorLog
	if err := database.DB.
		Where("user_id = ? AND created_at >= ?", userID, sinceTime).
		Order("occurrences DESC").
		Limit(10).
		Find(&topErrors).Error; err == nil {
		for _, err := range topErrors {
			stats.TopErrors = append(stats.TopErrors, models.TopErrorItem{
				Message:  err.Message,
				Count:    int64(err.Occurrences),
				Severity: err.Severity,
				LastSeen: err.LastSeen,
			})
		}
	}

	// Error trend (hourly)
	if err := h.getErrorTrend(userID.(string), hours, stats); err != nil {
		log.Printf("Failed to get error trend: %v", err)
	}

	// Calculate average
	if hours > 0 {
		stats.AverageErrorsPerHour = float64(stats.TotalErrors) / float64(hours)
	}

	c.JSON(http.StatusOK, stats)
}

// ResolveError marks an error as resolved
// PUT /api/v1/errors/:error_id/resolve
func (h *ErrorTrackingHandler) ResolveError(c *gin.Context) {
	errorID := c.Param("error_id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error": "not_authenticated",
		})
		return
	}

	// Update error
	result := database.DB.
		Model(&models.ErrorLog{}).
		Where("id = ? AND user_id = ?", errorID, userID).
		Update("is_resolved", true)

	if result.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "database_error",
			"message": result.Error.Error(),
		})
		return
	}

	if result.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{
			"error": "error_not_found",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status": "resolved",
	})
}

// GetErrorDetails returns details for a specific error
// GET /api/v1/errors/:error_id
func (h *ErrorTrackingHandler) GetErrorDetails(c *gin.Context) {
	errorID := c.Param("error_id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error": "not_authenticated",
		})
		return
	}

	var errorLog models.ErrorLog
	if err := database.DB.
		Where("id = ? AND user_id = ?", errorID, userID).
		First(&errorLog).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{
				"error": "error_not_found",
			})
		} else {
			c.JSON(http.StatusInternalServerError, gin.H{
				"error": "database_error",
			})
		}
		return
	}

	c.JSON(http.StatusOK, errorLog)
}

// findSimilarError finds a similar error in the last hour
func (h *ErrorTrackingHandler) findSimilarError(userID, message, source string) *models.ErrorLog {
	sinceTime := time.Now().Add(-1 * time.Hour)
	var errorLog models.ErrorLog
	err := database.DB.
		Where("user_id = ? AND message = ? AND source = ? AND created_at >= ?",
			userID, message, source, sinceTime).
		Order("last_seen DESC").
		First(&errorLog).Error

	if err == gorm.ErrRecordNotFound {
		return nil
	}
	if err != nil {
		log.Printf("Error finding similar error: %v", err)
		return nil
	}
	return &errorLog
}

// getErrorTrend retrieves hourly error count trend
func (h *ErrorTrackingHandler) getErrorTrend(userID string, hours int, stats *models.ErrorStats) error {
	type HourResult struct {
		Hour       time.Time
		ErrorCount int64
	}

	query := database.DB.
		Model(&models.ErrorLog{}).
		Select("DATE_TRUNC('hour', created_at) as hour, COUNT(*) as error_count").
		Where("user_id = ? AND created_at >= ?", userID, time.Now().Add(-time.Duration(hours)*time.Hour)).
		Group("DATE_TRUNC('hour', created_at)").
		Order("hour DESC")

	var results []HourResult
	if err := query.Scan(&results).Error; err != nil {
		return err
	}

	// Reverse to get ascending order (oldest first)
	for i := len(results) - 1; i >= 0; i-- {
		stats.ErrorTrendHourly = append(stats.ErrorTrendHourly, models.TrendItem{
			Hour:       results[i].Hour,
			ErrorCount: results[i].ErrorCount,
		})
	}

	return nil
}
