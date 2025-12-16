package config

import (
	"os"
	"path/filepath"
	"runtime"

	"github.com/spf13/viper"
)

var configDir string
var configFilePath string
var credentialsPath string

// getConfigDir returns platform-specific config directory
func getConfigDir() (string, error) {
	if runtime.GOOS == "windows" {
		// Windows: %LOCALAPPDATA%\sidechain\cli
		appData := os.Getenv("LOCALAPPDATA")
		if appData == "" {
			appData = os.Getenv("APPDATA")
		}
		if appData == "" {
			home, err := os.UserHomeDir()
			if err != nil {
				return "", err
			}
			appData = home
		}
		return filepath.Join(appData, "sidechain", "cli"), nil
	}

	// Unix-like (macOS, Linux): ~/.config/sidechain/cli
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(home, ".config", "sidechain", "cli"), nil
}

// getSystemConfigPaths returns platform-specific system config paths
func getSystemConfigPaths() []string {
	if runtime.GOOS == "windows" {
		// Windows: %ProgramFiles%\Sidechain\cli\config.toml
		return []string{filepath.Join(os.Getenv("ProgramFiles"), "Sidechain", "cli", "config.toml")}
	}

	// Unix-like systems: /etc/sidechain/cli/config.toml and /usr/local/etc/sidechain/cli/config.toml
	return []string{
		"/etc/sidechain/cli/config.toml",
		"/usr/local/etc/sidechain/cli/config.toml",
	}
}

// Init initializes the configuration
func Init(configPath string) error {
	// Determine config directory
	var err error
	if configPath != "" {
		configDir = filepath.Dir(configPath)
		configFilePath = configPath
	} else {
		configDir, err = getConfigDir()
		if err != nil {
			return err
		}
		configFilePath = filepath.Join(configDir, "config.toml")
	}

	// Create config directory if it doesn't exist
	if err := os.MkdirAll(configDir, 0700); err != nil {
		return err
	}

	credentialsPath = filepath.Join(configDir, "credentials")

	// Setup Viper
	viper.SetConfigType("toml")

	// Set development defaults
	setDefaults()

	// Load system config first (if exists) - serves as foundation
	for _, sysConfigPath := range getSystemConfigPaths() {
		if _, err := os.Stat(sysConfigPath); err == nil {
			viper.SetConfigFile(sysConfigPath)
			_ = viper.ReadInConfig()
			break // Use first system config found
		}
	}

	// Load user config second (overrides system config)
	viper.SetConfigFile(configFilePath)
	_ = viper.ReadInConfig()

	return nil
}

func setDefaults() {
	// Development defaults
	viper.SetDefault("api.base_url", "http://localhost:8787")
	viper.SetDefault("api.timeout", 30)
	viper.SetDefault("output.format", "text")

	// Set download directory default
	home, err := os.UserHomeDir()
	if err != nil {
		home = "."
	}
	viper.SetDefault("output.download_dir", filepath.Join(home, "Music"))

	viper.SetDefault("log.level", "info")
	viper.SetDefault("log.file", filepath.Join(configDir, "sidechain-cli.log"))
}

// expandPath expands ~ to home directory
func expandPath(path string) string {
	if len(path) > 0 && path[0] == '~' {
		home, err := os.UserHomeDir()
		if err == nil {
			return filepath.Join(home, path[1:])
		}
	}
	return path
}

// GetString returns a string configuration value
func GetString(key string) string {
	value := viper.GetString(key)
	// Expand tilde in path-like configuration keys
	if key == "output.download_dir" || key == "log.file" {
		return expandPath(value)
	}
	return value
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
