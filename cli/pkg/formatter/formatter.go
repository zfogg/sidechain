package formatter

import (
	"github.com/fatih/color"
	"github.com/zfogg/sidechain/cli/pkg/output"
)

var (
	// Colors for output (kept for backward compatibility)
	Bold    = color.New(color.Bold)
	Success = color.New(color.FgGreen)
	Error   = color.New(color.FgRed)
	Info    = color.New(color.FgCyan)
	Warning = color.New(color.FgYellow)
)

// PrintSuccess prints a success message
func PrintSuccess(format string, args ...interface{}) {
	output.PrintSuccess(format, args...)
}

// PrintError prints an error message
func PrintError(format string, args ...interface{}) {
	output.PrintError(format, args...)
}

// PrintInfo prints an info message
func PrintInfo(format string, args ...interface{}) {
	output.PrintInfo(format, args...)
}

// PrintWarning prints a warning message
func PrintWarning(format string, args ...interface{}) {
	output.PrintWarning(format, args...)
}

// PrintTable prints data as a table using the centralized output service
func PrintTable(headers []string, rows [][]string) {
	output.PrintList("", rows, headers)
}

// PrintJSON prints data as JSON using the centralized output service
func PrintJSON(data interface{}) error {
	return output.Print("", data)
}

// FormatAsJSON converts data to JSON string
func FormatAsJSON(data interface{}) (string, error) {
	return output.FormatAsJSON(data)
}

// FormatAsPrettyJSON converts data to pretty JSON string
func FormatAsPrettyJSON(data interface{}) (string, error) {
	return output.FormatAsPrettyJSON(data)
}

// PrintObject prints an object based on output format
func PrintObject(data interface{}, name string) error {
	return output.Print(name, data)
}

// PrintKeyValue prints key-value pairs using the centralized output service
func PrintKeyValue(data map[string]interface{}) {
	output.PrintRecord("", data)
}
