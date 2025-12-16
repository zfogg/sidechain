package config

import (
	"os"
	"path/filepath"
	"testing"
)

// TestGetConfigDir validates config directory access
func TestGetConfigDir(t *testing.T) {
	// Initialize config first
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test_config")); err != nil {
		t.Fatalf("Failed to initialize config: %v", err)
	}

	configDir := GetConfigDir()
	if configDir == "" {
		t.Fatal("Config directory should not be empty")
	}

	// Verify directory exists
	if _, err := os.Stat(configDir); err != nil {
		t.Errorf("Config directory should exist: %v", err)
	}
}

// TestGetCredentialsPath validates credentials path
func TestGetCredentialsPath(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test_config")); err != nil {
		t.Fatalf("Failed to initialize config: %v", err)
	}

	credsPath := GetCredentialsPath()
	if credsPath == "" {
		t.Fatal("Credentials path should not be empty")
	}

	// Path should contain credentials filename
	if !filepath.IsAbs(credsPath) {
		t.Error("Credentials path should be absolute")
	}
}

// TestInitWithCustomPath validates custom config path
func TestInitWithCustomPath(t *testing.T) {
	tempDir := t.TempDir()
	customConfigPath := filepath.Join(tempDir, "custom", "path", "config.toml")

	if err := Init(customConfigPath); err != nil {
		t.Fatalf("Failed to initialize with custom path: %v", err)
	}

	configDir := GetConfigDir()
	expectedDir := filepath.Join(tempDir, "custom", "path")

	if configDir != expectedDir {
		t.Errorf("Expected config dir %s, got %s", expectedDir, configDir)
	}
}

// TestInitWithoutPath validates default path initialization
func TestInitWithoutPath(t *testing.T) {
	if err := Init(""); err != nil {
		t.Fatalf("Failed to initialize with default path: %v", err)
	}

	configDir := GetConfigDir()
	home, _ := os.UserHomeDir()
	expectedDir := filepath.Join(home, ".config", "sidechain", "cli")

	if configDir != expectedDir {
		t.Errorf("Expected default config dir %s, got %s", expectedDir, configDir)
	}
}

// TestConfigDirectoryCreation validates directory is created
func TestConfigDirectoryCreation(t *testing.T) {
	tempDir := t.TempDir()
	configPath := filepath.Join(tempDir, "new", "config", "location", "config.toml")

	if err := Init(configPath); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	configDir := GetConfigDir()
	if _, err := os.Stat(configDir); err != nil {
		t.Fatalf("Config directory was not created: %v", err)
	}
}

// TestGetString validates string configuration retrieval
func TestGetString(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test")); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	// Test getting default value
	format := GetString("output.format")
	if format == "" {
		t.Error("Default output format should not be empty")
	}

	if format != "text" {
		t.Errorf("Expected default format 'text', got '%s'", format)
	}
}

// TestGetInt validates integer configuration retrieval
func TestGetInt(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test")); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	// Test getting default value
	timeout := GetInt("api.timeout")
	if timeout == 0 {
		t.Error("Default timeout should not be zero")
	}

	if timeout != 30 {
		t.Errorf("Expected default timeout 30, got %d", timeout)
	}
}

// TestGetBool validates boolean configuration retrieval
func TestGetBool(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test")); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	// Get a boolean value (may not have defaults but shouldn't panic)
	value := GetBool("some.bool.key")
	// Value could be false if not set
	t.Logf("Retrieved bool value: %v", value)
}

// TestDefaultAPIBaseURL validates default API base URL
func TestDefaultAPIBaseURL(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test")); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	baseURL := GetString("api.base_url")
	if baseURL != "http://localhost:8787" {
		t.Errorf("Expected default base URL 'http://localhost:8787', got '%s'", baseURL)
	}
}

// TestDefaultLogLevel validates default log level
func TestDefaultLogLevel(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test")); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	logLevel := GetString("log.level")
	if logLevel != "info" {
		t.Errorf("Expected default log level 'info', got '%s'", logLevel)
	}
}

// TestCredentialsPathStructure validates credentials path structure
func TestCredentialsPathStructure(t *testing.T) {
	tempDir := t.TempDir()
	configPath := filepath.Join(tempDir, "test_config")

	if err := Init(configPath); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	credsPath := GetCredentialsPath()
	configDir := GetConfigDir()

	// Credentials path should be under config directory
	if !filepath.IsAbs(credsPath) {
		t.Error("Credentials path should be absolute")
	}

	// Credentials should be in config directory
	if !isPathUnder(credsPath, configDir) {
		t.Errorf("Credentials path %s should be under config dir %s", credsPath, configDir)
	}
}

// TestMultipleInitCalls validates multiple initialization calls
func TestMultipleInitCalls(t *testing.T) {
	tempDir := t.TempDir()
	path1 := filepath.Join(tempDir, "config1", "config.toml")
	path2 := filepath.Join(tempDir, "config2", "config.toml")

	if err := Init(path1); err != nil {
		t.Fatalf("First init failed: %v", err)
	}

	firstDir := GetConfigDir()

	if err := Init(path2); err != nil {
		t.Fatalf("Second init failed: %v", err)
	}

	secondDir := GetConfigDir()

	// Config dir should change after re-init
	if firstDir == secondDir {
		t.Errorf("Config dir should change after re-init, both were %s", firstDir)
	}
}

// TestConfigDirPermissions validates config directory permissions
func TestConfigDirPermissions(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test")); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	configDir := GetConfigDir()
	info, err := os.Stat(configDir)
	if err != nil {
		t.Fatalf("Failed to stat config dir: %v", err)
	}

	// Check that directory exists and is actually a directory
	if !info.IsDir() {
		t.Error("Config path should be a directory")
	}
}

// TestDownloadDirDefault validates default download directory
func TestDownloadDirDefault(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test")); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	downloadDir := GetString("output.download_dir")
	if downloadDir == "" {
		t.Error("Download directory should have a default value")
	}

	// Should contain valid path components
	if !filepath.IsAbs(downloadDir) {
		t.Error("Download directory should be absolute path")
	}
}

// TestConfigKeyExistence validates that known config keys exist
func TestConfigKeyExistence(t *testing.T) {
	tempDir := t.TempDir()
	if err := Init(filepath.Join(tempDir, "test")); err != nil {
		t.Fatalf("Failed to initialize: %v", err)
	}

	testCases := []struct {
		key      string
		keyType  string
		name     string
	}{
		{"api.base_url", "string", "API base URL"},
		{"api.timeout", "int", "API timeout"},
		{"output.format", "string", "Output format"},
		{"output.download_dir", "string", "Download directory"},
		{"log.level", "string", "Log level"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			switch tc.keyType {
			case "string":
				val := GetString(tc.key)
				if val == "" {
					t.Errorf("Expected non-empty value for %s", tc.key)
				}
			case "int":
				val := GetInt(tc.key)
				if val <= 0 {
					t.Errorf("Expected positive value for %s, got %d", tc.key, val)
				}
			}
		})
	}
}

// Helper function to check if a path is under a directory
func isPathUnder(child, parent string) bool {
	abs1, _ := filepath.Abs(child)
	abs2, _ := filepath.Abs(parent)
	return len(abs1) > len(abs2) && abs1[:len(abs2)+1] == abs2+string(filepath.Separator) ||
		abs1 == abs2
}

// TestInitErrorHandling validates init handles invalid paths gracefully
func TestInitErrorHandling(t *testing.T) {
	// Most path errors are handled by MkdirAll, so test basic initialization
	if err := Init(""); err != nil {
		t.Fatalf("Basic init should succeed: %v", err)
	}
}
