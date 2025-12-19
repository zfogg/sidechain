package config

import (
	"fmt"
	"os"

	"golang.org/x/oauth2"
	"golang.org/x/oauth2/google"
)

// OAuthConfig holds OAuth provider configurations
type OAuthConfig struct {
	GoogleConfig *oauth2.Config
	DiscordConfig *oauth2.Config
}

// LoadOAuthConfig loads OAuth configuration from environment variables
// REQUIRED environment variables:
// - OAUTH_REDIRECT_URL: Base URL for OAuth callbacks (e.g., https://api.example.com)
// - GOOGLE_CLIENT_ID: Google OAuth client ID
// - GOOGLE_CLIENT_SECRET: Google OAuth client secret
// - DISCORD_CLIENT_ID: Discord OAuth client ID
// - DISCORD_CLIENT_SECRET: Discord OAuth client secret
func LoadOAuthConfig() (*OAuthConfig, error) {
	// These must be set - fail fast if missing
	redirectURL := os.Getenv("OAUTH_REDIRECT_URL")
	if redirectURL == "" {
		return nil, fmt.Errorf("OAUTH_REDIRECT_URL environment variable not set - this is REQUIRED for OAuth to work")
	}

	googleClientID := os.Getenv("GOOGLE_CLIENT_ID")
	if googleClientID == "" {
		return nil, fmt.Errorf("GOOGLE_CLIENT_ID environment variable not set")
	}

	googleClientSecret := os.Getenv("GOOGLE_CLIENT_SECRET")
	if googleClientSecret == "" {
		return nil, fmt.Errorf("GOOGLE_CLIENT_SECRET environment variable not set")
	}

	discordClientID := os.Getenv("DISCORD_CLIENT_ID")
	if discordClientID == "" {
		return nil, fmt.Errorf("DISCORD_CLIENT_ID environment variable not set")
	}

	discordClientSecret := os.Getenv("DISCORD_CLIENT_SECRET")
	if discordClientSecret == "" {
		return nil, fmt.Errorf("DISCORD_CLIENT_SECRET environment variable not set")
	}

	// Build callback URLs
	googleCallbackURL := redirectURL + "/api/v1/auth/google/callback"
	discordCallbackURL := redirectURL + "/api/v1/auth/discord/callback"

	return &OAuthConfig{
		GoogleConfig: &oauth2.Config{
			ClientID:     googleClientID,
			ClientSecret: googleClientSecret,
			RedirectURL:  googleCallbackURL,
			Scopes:       []string{"openid", "profile", "email"},
			Endpoint:     google.Endpoint,
		},
		DiscordConfig: &oauth2.Config{
			ClientID:     discordClientID,
			ClientSecret: discordClientSecret,
			RedirectURL:  discordCallbackURL,
			Scopes:       []string{"identify", "email"},
			Endpoint: oauth2.Endpoint{
				AuthURL:  "https://discord.com/api/oauth2/authorize",
				TokenURL: "https://discord.com/api/oauth2/token",
			},
		},
	}, nil
}
