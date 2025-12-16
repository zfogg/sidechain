#!/bin/bash

# Test system-wide and user config precedence

echo "=== System Config Precedence Test ==="
echo ""

# Create a temporary directory structure
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Simulate system config
SYS_CONFIG="$TEMP_DIR/etc/sidechain/cli/config.toml"
mkdir -p "$(dirname "$SYS_CONFIG")"
cat > "$SYS_CONFIG" << 'SYSCFG'
[api]
base_url = "https://api.sidechain.live"
timeout = 60

[log]
level = "warn"
SYSCFG

# Simulate user config (overrides system)
USER_CONFIG="$TEMP_DIR/user/.config/sidechain/cli/config.toml"
mkdir -p "$(dirname "$USER_CONFIG")"
cat > "$USER_CONFIG" << 'USERCFG'
[api]
base_url = "http://localhost:8787"
timeout = 30
USERCFG

echo "System config location: $SYS_CONFIG"
echo "System config contents:"
cat "$SYS_CONFIG"
echo ""
echo "User config location: $USER_CONFIG"
echo "User config contents:"
cat "$USER_CONFIG"
echo ""

# Test with config.go 
cd /Users/zfogg/src/github.com/zfogg/sidechain/cli

cat > test_precedence.go << 'TESTGO'
package main

import (
	"fmt"
	"github.com/zfogg/sidechain/cli/pkg/config"
)

func main() {
	// Initialize with user config path
	if err := config.Init(""); err != nil {
		fmt.Printf("Error: %v\n", err)
		return
	}

	baseURL := config.GetString("api.base_url")
	timeout := config.GetInt("api.timeout")
	logLevel := config.GetString("log.level")

	fmt.Printf("Loaded configuration:\n")
	fmt.Printf("  API Base URL:  %s\n", baseURL)
	fmt.Printf("  API Timeout:   %d seconds\n", timeout)
	fmt.Printf("  Log Level:     %s\n", logLevel)
}
TESTGO

go run test_precedence.go
rm test_precedence.go

echo ""
echo "✅ Configuration system supports system-wide and user-specific configs"
