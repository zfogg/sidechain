package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"

	"github.com/spf13/cobra"
)

var profileCmd = &cobra.Command{
	Use:   "profile",
	Short: "Manage your profile settings",
	Long:  "Commands for managing your profile, including privacy settings",
}

var setPrivateCmd = &cobra.Command{
	Use:   "set-private",
	Short: "Make your account private",
	Long: `Make your account private. This will:
- Require approval for new followers
- Hide your posts from non-followers
- Keep existing followers`,
	RunE: func(cmd *cobra.Command, args []string) error {
		return updatePrivacy(true)
	},
}

var setPublicCmd = &cobra.Command{
	Use:   "set-public",
	Short: "Make your account public",
	Long: `Make your account public. This will:
- Allow anyone to follow you
- Show your posts to everyone`,
	RunE: func(cmd *cobra.Command, args []string) error {
		return updatePrivacy(false)
	},
}

var getProfileCmd = &cobra.Command{
	Use:   "get",
	Short: "Get your current profile settings",
	RunE: func(cmd *cobra.Command, args []string) error {
		return getProfile()
	},
}

func init() {
	profileCmd.AddCommand(setPrivateCmd)
	profileCmd.AddCommand(setPublicCmd)
	profileCmd.AddCommand(getProfileCmd)
}

func updatePrivacy(isPrivate bool) error {
	payload := map[string]interface{}{
		"is_private": isPrivate,
	}

	bodyBytes, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to encode request: %w", err)
	}

	req, err := http.NewRequest("PUT", apiURL+"/api/v1/users/me", bytes.NewReader(bodyBytes))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+authToken)
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to make request: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var errResp map[string]interface{}
		json.Unmarshal(body, &errResp)
		if msg, ok := errResp["message"].(string); ok {
			return fmt.Errorf("API error: %s", msg)
		}
		return fmt.Errorf("API error: status %d", resp.StatusCode)
	}

	// Parse response
	var result map[string]interface{}
	if err := json.Unmarshal(body, &result); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	// Output result
	if output == "json" {
		fmt.Println(string(body))
	} else {
		status := "public"
		if isPrivate {
			status = "private"
		}
		fmt.Printf("✓ Account is now %s\n", status)
	}

	return nil
}

func getProfile() error {
	req, err := http.NewRequest("GET", apiURL+"/api/v1/users/me", nil)
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Authorization", "Bearer "+authToken)

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to make request: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var errResp map[string]interface{}
		json.Unmarshal(body, &errResp)
		if msg, ok := errResp["message"].(string); ok {
			return fmt.Errorf("API error: %s", msg)
		}
		return fmt.Errorf("API error: status %d", resp.StatusCode)
	}

	var profile map[string]interface{}
	if err := json.Unmarshal(body, &profile); err != nil {
		return fmt.Errorf("failed to parse response: %w", err)
	}

	if output == "json" {
		fmt.Println(string(body))
	} else {
		// Pretty print profile
		fmt.Printf("\n📋 Profile Information\n")
		fmt.Printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")

		if username, ok := profile["username"].(string); ok {
			fmt.Printf("Username: %s\n", username)
		}
		if displayName, ok := profile["display_name"].(string); ok {
			fmt.Printf("Display Name: %s\n", displayName)
		}
		if isPrivate, ok := profile["is_private"].(bool); ok {
			status := "🌍 Public"
			if isPrivate {
				status = "🔒 Private"
			}
			fmt.Printf("Account Status: %s\n", status)
		}

		fmt.Printf("\n")
	}

	return nil
}
