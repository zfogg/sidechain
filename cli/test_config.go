package main

import (
	"fmt"
	"github.com/zfogg/sidechain/cli/pkg/config"
)

func main() {
	// Initialize config with default path
	if err := config.Init(""); err != nil {
		fmt.Printf("Error: %v\n", err)
		return
	}

	// Test reading configuration values
	baseURL := config.GetString("api.base_url")
	timeout := config.GetInt("api.timeout")
	format := config.GetString("output.format")
	downloadDir := config.GetString("output.download_dir")
	logLevel := config.GetString("log.level")

	fmt.Printf("Configuration loaded successfully:\n")
	fmt.Printf("  API Base URL:   %s\n", baseURL)
	fmt.Printf("  API Timeout:    %d seconds\n", timeout)
	fmt.Printf("  Output Format:  %s\n", format)
	fmt.Printf("  Download Dir:   %s\n", downloadDir)
	fmt.Printf("  Log Level:      %s\n", logLevel)
	fmt.Printf("  Config Dir:     %s\n", config.GetConfigDir())
}
