package service

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// ErrorTrackingService provides operations for error tracking
type ErrorTrackingService struct{}

// NewErrorTrackingService creates a new error tracking service
func NewErrorTrackingService() *ErrorTrackingService {
	return &ErrorTrackingService{}
}

// ReportError reports a single error interactively
func (ets *ErrorTrackingService) ReportError() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Error message: ")
	message, _ := reader.ReadString('\n')
	message = strings.TrimSpace(message)

	if message == "" {
		return fmt.Errorf("error message cannot be empty")
	}

	fmt.Print("Severity (Info/Warning/Error/Critical) [Error]: ")
	severity, _ := reader.ReadString('\n')
	severity = strings.TrimSpace(severity)
	if severity == "" {
		severity = "Error"
	}

	fmt.Print("Source (Network/Audio/UI/Unknown) [Unknown]: ")
	source, _ := reader.ReadString('\n')
	source = strings.TrimSpace(source)
	if source == "" {
		source = "Unknown"
	}

	// Record the error
	errors := []api.ErrorLogPayload{
		{
			Message:  message,
			Severity: severity,
			Source:   source,
		},
	}

	count, err := api.RecordErrors(errors)
	if err != nil {
		return fmt.Errorf("failed to report error: %w", err)
	}

	formatter.PrintSuccess("✓ Error reported successfully!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Message":  message,
		"Severity": severity,
		"Source":   source,
		"Recorded": fmt.Sprintf("%d error(s)", count),
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// GetErrorStats retrieves and displays error statistics
func (ets *ErrorTrackingService) GetErrorStats() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Time period (hours, default 24, max 168): ")
	hourStr, _ := reader.ReadString('\n')
	hourStr = strings.TrimSpace(hourStr)

	hours := 24
	if hourStr != "" {
		if h, err := strconv.Atoi(hourStr); err == nil && h > 0 && h <= 168 {
			hours = h
		}
	}

	// Fetch stats
	stats, err := api.GetErrorStats(hours)
	if err != nil {
		return fmt.Errorf("failed to fetch error stats: %w", err)
	}

	formatter.PrintInfo("📊 Error Statistics (Last %d hours)", hours)
	fmt.Printf("\n")

	// Display summary
	keyValues := map[string]interface{}{
		"Total Errors": stats.TotalErrors,
		"Avg/Hour":     fmt.Sprintf("%.2f", stats.AverageErrorsPerHour),
	}
	formatter.PrintKeyValue(keyValues)
	fmt.Printf("\n")

	// Errors by severity
	if len(stats.ErrorsBySeverity) > 0 {
		fmt.Println("By Severity:")
		severityHeaders := []string{"Level", "Count"}
		severityRows := make([][]string, 0)
		for level, count := range stats.ErrorsBySeverity {
			severityRows = append(severityRows, []string{level, fmt.Sprintf("%d", count)})
		}
		formatter.PrintTable(severityHeaders, severityRows)
		fmt.Printf("\n")
	}

	// Errors by source
	if len(stats.ErrorsBySource) > 0 {
		fmt.Println("By Source:")
		sourceHeaders := []string{"Source", "Count"}
		sourceRows := make([][]string, 0)
		for source, count := range stats.ErrorsBySource {
			sourceRows = append(sourceRows, []string{source, fmt.Sprintf("%d", count)})
		}
		formatter.PrintTable(sourceHeaders, sourceRows)
		fmt.Printf("\n")
	}

	// Top errors
	if len(stats.TopErrors) > 0 {
		fmt.Println("Top Errors:")
		topHeaders := []string{"Message", "Count", "Severity", "Last Seen"}
		topRows := make([][]string, len(stats.TopErrors))
		for i, err := range stats.TopErrors {
			topRows[i] = []string{
				truncate(err.Message, 40),
				fmt.Sprintf("%d", err.Count),
				err.Severity,
				err.LastSeen,
			}
		}
		formatter.PrintTable(topHeaders, topRows)
	}

	return nil
}

// ViewErrorDetails displays details for a specific error
func (ets *ErrorTrackingService) ViewErrorDetails() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Error ID: ")
	errorID, _ := reader.ReadString('\n')
	errorID = strings.TrimSpace(errorID)

	if errorID == "" {
		return fmt.Errorf("error ID cannot be empty")
	}

	// Fetch error details
	errorLog, err := api.GetErrorDetails(errorID)
	if err != nil {
		return fmt.Errorf("failed to fetch error details: %w", err)
	}

	formatter.PrintInfo("🐛 Error Details")
	fmt.Printf("\n")

	keyValues := map[string]interface{}{
		"ID":          errorLog.ID,
		"Message":     errorLog.Message,
		"Severity":    errorLog.Severity,
		"Source":      errorLog.Source,
		"Occurrences": errorLog.Occurrences,
		"Status":      map[bool]string{true: "✓ Resolved", false: "⏳ Open"}[errorLog.IsResolved],
		"First Seen":  errorLog.FirstSeen,
		"Last Seen":   errorLog.LastSeen,
		"Created":     errorLog.CreatedAt,
		"Updated":     errorLog.UpdatedAt,
	}
	formatter.PrintKeyValue(keyValues)

	// Display context if available
	if len(errorLog.Context) > 0 {
		fmt.Printf("\nContext:\n")
		contextHeaders := []string{"Key", "Value"}
		contextRows := make([][]string, 0)
		for key, value := range errorLog.Context {
			contextRows = append(contextRows, []string{key, fmt.Sprintf("%v", value)})
		}
		formatter.PrintTable(contextHeaders, contextRows)
	}

	return nil
}

// ResolveError marks an error as resolved
func (ets *ErrorTrackingService) ResolveError() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Error ID to resolve: ")
	errorID, _ := reader.ReadString('\n')
	errorID = strings.TrimSpace(errorID)

	if errorID == "" {
		return fmt.Errorf("error ID cannot be empty")
	}

	// Mark error as resolved
	errorLog, err := api.ResolveError(errorID)
	if err != nil {
		return fmt.Errorf("failed to resolve error: %w", err)
	}

	formatter.PrintSuccess("✓ Error marked as resolved!")
	fmt.Printf("\n")
	keyValues := map[string]interface{}{
		"Error ID": errorLog.ID,
		"Message":  errorLog.Message,
		"Status":   "Resolved",
	}
	formatter.PrintKeyValue(keyValues)

	return nil
}

// Helper function to truncate long strings
func truncate(s string, length int) string {
	if len(s) > length {
		return s[:length-3] + "..."
	}
	return s
}
