package output

import (
	"encoding/json"
	"fmt"
	"text/tabwriter"

	json2 "github.com/json-iterator/go"
	"github.com/fatih/color"
	"github.com/zfogg/sidechain/cli/pkg/config"
)

// OutputFormat represents the output format type
type OutputFormat string

const (
	FormatJSON  OutputFormat = "json"
	FormatTable OutputFormat = "table"
	FormatText  OutputFormat = "text"
)

// GetOutputFormat returns the configured output format
func GetOutputFormat() OutputFormat {
	format := config.GetString("output.format")
	switch format {
	case "json":
		return FormatJSON
	case "table":
		return FormatTable
	default:
		return FormatText
	}
}

// ValidateOutputFormat checks if format is valid
func ValidateOutputFormat(format string) bool {
	return format == "json" || format == "table" || format == "text"
}

// Print outputs data in the configured format with optional title
func Print(title string, data interface{}) error {
	format := GetOutputFormat()

	switch format {
	case FormatJSON:
		return printJSON(title, data)
	case FormatTable:
		// Table format doesn't make sense for generic objects, fall back to text
		return printText(title, data)
	default:
		return printText(title, data)
	}
}

// PrintList outputs a list/array in the configured format
func PrintList(title string, items interface{}, columns []string) error {
	format := GetOutputFormat()

	switch format {
	case FormatJSON:
		return printListJSON(title, items)
	case FormatTable:
		// For table format, items should be [][]string (rows)
		// columns parameter provides headers
		if rows, ok := items.([][]string); ok {
			printTable(columns, rows)
			return nil
		}
		// Fall back to JSON format if items aren't in table format
		return printListJSON(title, items)
	default:
		return printListText(title, items)
	}
}

// PrintRecord outputs a single record (map/object) in the configured format
func PrintRecord(title string, record map[string]interface{}) error {
	format := GetOutputFormat()

	switch format {
	case FormatJSON:
		return printRecordJSON(title, record)
	case FormatTable:
		// Print as two-column table (key, value)
		headers := []string{"Field", "Value"}
		rows := make([][]string, 0, len(record))
		for k, v := range record {
			rows = append(rows, []string{k, fmt.Sprintf("%v", v)})
		}
		printTable(headers, rows)
		return nil
	default:
		return printRecordText(title, record)
	}
}

// PrintSuccess prints a success message
func PrintSuccess(msg string, args ...interface{}) {
	colored := color.New(color.FgGreen)
	colored.Printf(msg+"\n", args...)
}

// PrintError prints an error message
func PrintError(msg string, args ...interface{}) {
	colored := color.New(color.FgRed)
	colored.Printf("Error: "+msg+"\n", args...)
}

// PrintInfo prints an info message
func PrintInfo(msg string, args ...interface{}) {
	colored := color.New(color.FgCyan)
	colored.Printf(msg+"\n", args...)
}

// PrintWarning prints a warning message
func PrintWarning(msg string, args ...interface{}) {
	colored := color.New(color.FgYellow)
	colored.Printf("Warning: "+msg+"\n", args...)
}

// Helper functions

func printJSON(title string, data interface{}) error {
	if title != "" {
		fmt.Printf("{\n  \"%s\": ", title)
	}
	encoder := json.NewEncoder(color.Output)
	encoder.SetIndent("", "  ")
	if title != "" {
		fmt.Println()
	}
	err := encoder.Encode(data)
	if title != "" {
		fmt.Println("}")
	}
	return err
}

func printListJSON(title string, items interface{}) error {
	if title != "" {
		fmt.Printf("{\n  \"%s\": ", title)
	}
	encoder := json.NewEncoder(color.Output)
	encoder.SetIndent("", "  ")
	if title != "" {
		fmt.Println()
	}
	err := encoder.Encode(items)
	if title != "" {
		fmt.Println("}")
	}
	return err
}

func printRecordJSON(title string, record map[string]interface{}) error {
	jsonStr, err := formatAsPrettyJSON(record)
	if err != nil {
		return err
	}
	if title != "" {
		fmt.Printf("%s:\n", title)
	}
	fmt.Println(jsonStr)
	return nil
}

func printText(title string, data interface{}) error {
	if title != "" {
		fmt.Printf("%s:\n", title)
	}
	jsonStr, err := formatAsPrettyJSON(data)
	if err != nil {
		return err
	}
	fmt.Println(jsonStr)
	return nil
}

func printListText(title string, items interface{}) error {
	if title != "" {
		fmt.Printf("%s:\n", title)
	}
	jsonStr, err := formatAsPrettyJSON(items)
	if err != nil {
		return err
	}
	fmt.Println(jsonStr)
	return nil
}

func printRecordText(title string, record map[string]interface{}) error {
	if title != "" {
		fmt.Printf("%s:\n", title)
	}
	bold := color.New(color.Bold)
	for key, value := range record {
		bold.Print(key + ": ")
		fmt.Printf("%v\n", value)
	}
	return nil
}

func printTable(headers []string, rows [][]string) {
	w := tabwriter.NewWriter(color.Output, 0, 0, 2, ' ', 0)
	bold := color.New(color.Bold)

	// Print headers
	for i, h := range headers {
		bold.Fprint(w, h)
		if i < len(headers)-1 {
			fmt.Fprint(w, "\t")
		}
	}
	fmt.Fprintln(w)

	// Print rows
	for _, row := range rows {
		for i, cell := range row {
			fmt.Fprint(w, cell)
			if i < len(row)-1 {
				fmt.Fprint(w, "\t")
			}
		}
		fmt.Fprintln(w)
	}

	w.Flush()
}

func formatAsPrettyJSON(data interface{}) (string, error) {
	marshaler := json2.ConfigDefault
	jsonData, err := marshaler.Marshal(data)
	if err != nil {
		return "", err
	}

	// Unmarshal and remarshal for pretty printing
	var obj interface{}
	if err := json.Unmarshal(jsonData, &obj); err != nil {
		return "", err
	}

	prettyJSON, err := json.MarshalIndent(obj, "", "  ")
	if err != nil {
		return "", err
	}

	return string(prettyJSON), nil
}

// FormatAsJSON converts data to JSON string (convenience function)
func FormatAsJSON(data interface{}) (string, error) {
	marshaler := json2.ConfigDefault
	jsonData, err := marshaler.Marshal(data)
	if err != nil {
		return "", err
	}
	return string(jsonData), nil
}

// FormatAsPrettyJSON converts data to pretty JSON string (convenience function)
func FormatAsPrettyJSON(data interface{}) (string, error) {
	return formatAsPrettyJSON(data)
}
