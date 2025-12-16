package formatter

import (
	"encoding/json"
	"fmt"
	"text/tabwriter"

	json2 "github.com/json-iterator/go"
	"github.com/fatih/color"
	"github.com/zfogg/sidechain/cli/pkg/config"
)

var (
	// Colors for output
	Bold    = color.New(color.Bold)
	Success = color.New(color.FgGreen)
	Error   = color.New(color.FgRed)
	Info    = color.New(color.FgCyan)
	Warning = color.New(color.FgYellow)
)

// PrintSuccess prints a success message
func PrintSuccess(format string, args ...interface{}) {
	Success.Printf(format+"\n", args...)
}

// PrintError prints an error message
func PrintError(format string, args ...interface{}) {
	Error.Printf("Error: "+format+"\n", args...)
}

// PrintInfo prints an info message
func PrintInfo(format string, args ...interface{}) {
	Info.Printf(format+"\n", args...)
}

// PrintWarning prints a warning message
func PrintWarning(format string, args ...interface{}) {
	Warning.Printf("Warning: "+format+"\n", args...)
}

// PrintTable prints data as a table
func PrintTable(headers []string, rows [][]string) {
	w := tabwriter.NewWriter(color.Output, 0, 0, 2, ' ', 0)

	// Print headers
	for i, h := range headers {
		Bold.Fprint(w, h)
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

// PrintJSON prints data as JSON
func PrintJSON(data interface{}) error {
	outputFmt := config.GetString("output.format")
	if outputFmt != "json" {
		outputFmt = "text"
	}

	if outputFmt == "json" {
		encoder := json.NewEncoder(color.Output)
		encoder.SetIndent("", "  ")
		return encoder.Encode(data)
	}

	return nil
}

// FormatAsJSON converts data to JSON string
func FormatAsJSON(data interface{}) (string, error) {
	marshaler := json2.ConfigDefault
	jsonData, err := marshaler.Marshal(data)
	if err != nil {
		return "", err
	}
	return string(jsonData), nil
}

// FormatAsPrettyJSON converts data to pretty JSON string
func FormatAsPrettyJSON(data interface{}) (string, error) {
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

// PrintObject prints an object based on output format
func PrintObject(data interface{}, name string) error {
	outputFmt := config.GetString("output.format")

	if outputFmt == "json" {
		jsonStr, err := FormatAsPrettyJSON(data)
		if err != nil {
			return err
		}
		fmt.Println(jsonStr)
		return nil
	}

	// Default text format
	jsonStr, err := FormatAsPrettyJSON(data)
	if err != nil {
		return err
	}
	fmt.Println(jsonStr)
	return nil
}

// PrintKeyValue prints key-value pairs
func PrintKeyValue(data map[string]interface{}) {
	outputFmt := config.GetString("output.format")

	if outputFmt == "json" {
		jsonStr, _ := FormatAsPrettyJSON(data)
		fmt.Println(jsonStr)
		return
	}

	for key, value := range data {
		Bold.Print(key + ": ")
		fmt.Printf("%v\n", value)
	}
}
