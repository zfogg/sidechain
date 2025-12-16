package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// ErrorLogPayload is the format for sending error data
type ErrorLogPayload struct {
	Source     string                 `json:"source"`      // Network, Audio, UI, Unknown
	Severity   string                 `json:"severity"`    // Info, Warning, Error, Critical
	Message    string                 `json:"message"`
	Context    map[string]interface{} `json:"context,omitempty"`
	Timestamp  string                 `json:"timestamp,omitempty"`
	Occurrences int                   `json:"occurrences,omitempty"`
}

// ErrorBatch is a batch of errors to report
type ErrorBatch struct {
	Errors []ErrorLogPayload `json:"errors"`
}

// ErrorLog represents a stored error
type ErrorLog struct {
	ID          string                 `json:"id"`
	UserID      string                 `json:"user_id"`
	Source      string                 `json:"source"`
	Severity    string                 `json:"severity"`
	Message     string                 `json:"message"`
	Context     map[string]interface{} `json:"context"`
	Occurrences int                    `json:"occurrences"`
	FirstSeen   string                 `json:"first_seen"`
	LastSeen    string                 `json:"last_seen"`
	IsResolved  bool                   `json:"is_resolved"`
	CreatedAt   string                 `json:"created_at"`
	UpdatedAt   string                 `json:"updated_at"`
}

// TopErrorItem represents a frequently occurring error
type TopErrorItem struct {
	Message   string `json:"message"`
	Count     int    `json:"count"`
	Severity  string `json:"severity"`
	LastSeen  string `json:"last_seen"`
}

// TrendItem represents hourly error count
type TrendItem struct {
	Hour       int `json:"hour"`
	ErrorCount int `json:"error_count"`
}

// ErrorStats contains aggregated error statistics
type ErrorStats struct {
	TotalErrors         int                          `json:"total_errors"`
	ErrorsBySeverity    map[string]int               `json:"errors_by_severity"`
	ErrorsBySource      map[string]int               `json:"errors_by_source"`
	TopErrors           []TopErrorItem               `json:"top_errors"`
	ErrorTrendHourly    []TrendItem                  `json:"error_trend_hourly"`
	AverageErrorsPerHour float64                     `json:"average_errors_per_hour"`
}

// RecordErrors reports a batch of errors
func RecordErrors(errors []ErrorLogPayload) (int, error) {
	logger.Debug("Recording errors batch", "count", len(errors))

	batch := ErrorBatch{Errors: errors}

	var response struct {
		Count int `json:"count"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(batch).
		SetResult(&response).
		Post("/api/v1/errors/batch")

	if err != nil {
		return 0, err
	}

	if !resp.IsSuccess() {
		return 0, fmt.Errorf("failed to record errors: %s", resp.Status())
	}

	return response.Count, nil
}

// GetErrorStats retrieves error statistics
func GetErrorStats(hours int) (*ErrorStats, error) {
	logger.Debug("Fetching error stats", "hours", hours)

	var response ErrorStats

	resp, err := client.GetClient().
		R().
		SetQueryParam("hours", fmt.Sprintf("%d", hours)).
		SetResult(&response).
		Get("/api/v1/errors/stats")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch error stats: %s", resp.Status())
	}

	return &response, nil
}

// GetErrorDetails retrieves details for a specific error
func GetErrorDetails(errorID string) (*ErrorLog, error) {
	logger.Debug("Fetching error details", "error_id", errorID)

	var response struct {
		Error ErrorLog `json:"error"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/errors/%s", errorID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch error details: %s", resp.Status())
	}

	return &response.Error, nil
}

// ResolveError marks an error as resolved
func ResolveError(errorID string) (*ErrorLog, error) {
	logger.Debug("Resolving error", "error_id", errorID)

	var response struct {
		Error ErrorLog `json:"error"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Put(fmt.Sprintf("/api/v1/errors/%s/resolve", errorID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to resolve error: %s", resp.Status())
	}

	return &response.Error, nil
}
