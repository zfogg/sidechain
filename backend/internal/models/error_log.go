package models

import (
	"database/sql/driver"
	"encoding/json"
	"time"
)

// ErrorLog represents an error reported by the plugin
type ErrorLog struct {
	ID          string    `gorm:"primaryKey" json:"id"`
	UserID      string    `gorm:"index:,type:btree" json:"user_id"`
	Source      string    `json:"source"`      // Network, Audio, UI
	Severity    string    `gorm:"index:,type:btree" json:"severity"` // Info, Warning, Error, Critical
	Message     string    `gorm:"type:text" json:"message"`
	Context     Context   `gorm:"type:jsonb" json:"context"`
	Occurrences int       `json:"occurrences"`
	FirstSeen   time.Time `json:"first_seen"`
	LastSeen    time.Time `json:"last_seen"`
	IsResolved  bool      `json:"is_resolved"`
	CreatedAt   time.Time `json:"created_at"`
	UpdatedAt   time.Time `json:"updated_at"`
}

// Context is a map of string to interface for error context
type Context map[string]interface{}

// Value implements the driver.Valuer interface for JSONB
func (c Context) Value() (driver.Value, error) {
	return json.Marshal(c)
}

// Scan implements the sql.Scanner interface for JSONB
func (c *Context) Scan(value interface{}) error {
	bytes, ok := value.([]byte)
	if !ok {
		return nil
	}
	return json.Unmarshal(bytes, &c)
}

// TableName specifies the table name for ErrorLog
func (ErrorLog) TableName() string {
	return "error_logs"
}

// ErrorBatch represents a batch of errors from the plugin
type ErrorBatch struct {
	Errors []ErrorLogPayload `json:"errors"`
}

// ErrorLogPayload is the payload format sent by the plugin
type ErrorLogPayload struct {
	Source      string                 `json:"source"`
	Severity    string                 `json:"severity"`
	Message     string                 `json:"message"`
	Context     map[string]interface{} `json:"context"`
	Timestamp   int64                  `json:"timestamp"`
	Occurrences int                    `json:"occurrences"`
}

// ErrorStats represents error statistics
type ErrorStats struct {
	TotalErrors          int64            `json:"total_errors"`
	ErrorsBySeverity     map[string]int64 `json:"errors_by_severity"`
	ErrorsBySource       map[string]int64 `json:"errors_by_source"`
	TopErrors            []TopErrorItem   `json:"top_errors"`
	ErrorTrendHourly     []TrendItem      `json:"error_trend_hourly"`
	AverageErrorsPerHour float64          `json:"average_errors_per_hour"`
}

// TopErrorItem represents a frequently occurring error
type TopErrorItem struct {
	Message     string `json:"message"`
	Count       int64  `json:"count"`
	Severity    string `json:"severity"`
	LastSeen    time.Time `json:"last_seen"`
}

// TrendItem represents an error count at a specific time
type TrendItem struct {
	Hour      time.Time `json:"hour"`
	ErrorCount int64    `json:"error_count"`
}
