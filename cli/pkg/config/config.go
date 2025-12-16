package config

import (
	"os"
	"path/filepath"

	"github.com/spf13/viper"
)

var configDir string
var credentialsPath string

// Init initializes the configuration
func Init(configPath string) error {
	// Determine config directory
	if configPath != "" {
		configDir = filepath.Dir(configPath)
	} else {
		home, err := os.UserHomeDir()
		if err != nil {
			return err
		}
		configDir = filepath.Join(home, ".sidechain")
	}

	// Create config directory if it doesn't exist
	if err := os.MkdirAll(configDir, 0700); err != nil {
		return err
	}

	credentialsPath = filepath.Join(configDir, "credentials")

	// Setup Viper
	viper.SetConfigType("yaml")
	viper.SetConfigName("config")
	viper.AddConfigPath(configDir)

	// Set defaults
	setDefaults()

	// Read config file (optional - it might not exist yet)
	_ = viper.ReadInConfig()

	return nil
}

func setDefaults() {
	viper.SetDefault("api.base_url", "http://localhost:8080")
	viper.SetDefault("api.timeout", 30)
	viper.SetDefault("output.format", "text")
	viper.SetDefault("output.download_dir", filepath.Join(os.Getenv("HOME"), "Music"))
	viper.SetDefault("log.level", "info")
	viper.SetDefault("log.file", filepath.Join(configDir, "sidechain-cli.log"))
}

// GetString returns a string configuration value
func GetString(key string) string {
	return viper.GetString(key)
}

// GetInt returns an int configuration value
func GetInt(key string) int {
	return viper.GetInt(key)
}

// GetBool returns a bool configuration value
func GetBool(key string) bool {
	return viper.GetBool(key)
}

// SetString sets a string configuration value
func SetString(key string, value string) error {
	viper.Set(key, value)
	return viper.WriteConfig()
}

// GetConfigDir returns the configuration directory path
func GetConfigDir() string {
	return configDir
}

// GetCredentialsPath returns the path to the credentials file
func GetCredentialsPath() string {
	return credentialsPath
}
