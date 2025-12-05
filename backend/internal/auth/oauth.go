package auth

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"golang.org/x/oauth2"
	"gorm.io/gorm"
)

// Temporary: prevent unused import errors
var _ = http.StatusOK
var _ oauth2.Config

// OAuthUserInfo represents user info from OAuth providers
type OAuthUserInfo struct {
	ID           string     `json:"id"`
	Email        string     `json:"email"`
	Name         string     `json:"name"`
	AvatarURL    string     `json:"avatar_url"`
	AccessToken  string     `json:"-"` // OAuth access token for API calls
	RefreshToken string     `json:"-"` // OAuth refresh token for token renewal
	TokenExpiry  *time.Time `json:"-"` // When the access token expires
}

// GoogleUserInfo represents Google OAuth user response
type GoogleUserInfo struct {
	Sub           string `json:"sub"`
	Email         string `json:"email"`
	EmailVerified bool   `json:"email_verified"`
	Name          string `json:"name"`
	Picture       string `json:"picture"`
	GivenName     string `json:"given_name"`
	FamilyName    string `json:"family_name"`
}

// DiscordUserInfo represents Discord OAuth user response
type DiscordUserInfo struct {
	ID            string `json:"id"`
	Username      string `json:"username"`
	Discriminator string `json:"discriminator"`
	Email         string `json:"email"`
	Verified      bool   `json:"verified"`
	Avatar        string `json:"avatar"`
}

// HandleGoogleCallback processes Google OAuth callback
func (s *Service) HandleGoogleCallback(code string) (*AuthResponse, error) {
	log.Printf("[OAuth/Service] HandleGoogleCallback: Starting token exchange")
	userInfo, err := s.getGoogleUserInfo(code)
	if err != nil {
		log.Printf("[OAuth/Service] HandleGoogleCallback: Failed to get user info: %v", err)
		return nil, fmt.Errorf("failed to get Google user info: %w", err)
	}
	log.Printf("[OAuth/Service] HandleGoogleCallback: Got user info - email: %s, name: %s", userInfo.Email, userInfo.Name)

	return s.findOrCreateUserFromOAuth("google", userInfo)
}

// HandleDiscordCallback processes Discord OAuth callback
func (s *Service) HandleDiscordCallback(code string) (*AuthResponse, error) {
	userInfo, err := s.getDiscordUserInfo(code)
	if err != nil {
		return nil, fmt.Errorf("failed to get Discord user info: %w", err)
	}

	return s.findOrCreateUserFromOAuth("discord", userInfo)
}

// findOrCreateUserFromOAuth implements email-based account unification
func (s *Service) findOrCreateUserFromOAuth(provider string, userInfo *OAuthUserInfo) (*AuthResponse, error) {
	log.Printf("[OAuth/Service] findOrCreateUserFromOAuth: Looking up OAuth account - provider: %s, providerUserID: %s", provider, userInfo.ID)

	// First, check if this OAuth account is already linked
	var existingOAuth models.OAuthProvider
	err := database.DB.Where("provider = ? AND provider_user_id = ?", provider, userInfo.ID).
		Preload("User").First(&existingOAuth).Error

	if err == nil {
		log.Printf("[OAuth/Service] findOrCreateUserFromOAuth: Found existing OAuth link for user: %s", existingOAuth.User.Username)
		// OAuth account already linked - update tokens and return existing user
		s.updateOAuthTokens(&existingOAuth, userInfo)
		return s.generateAuthResponse(&existingOAuth.User)
	} else if !errors.Is(err, gorm.ErrRecordNotFound) {
		log.Printf("[OAuth/Service] findOrCreateUserFromOAuth: Database error checking OAuth: %v", err)
		return nil, fmt.Errorf("database error checking OAuth: %w", err)
	}

	log.Printf("[OAuth/Service] findOrCreateUserFromOAuth: No existing OAuth link, checking by email: %s", userInfo.Email)

	// Check if user exists by email (account unification)
	existingUser, err := s.FindUserByEmail(userInfo.Email)
	if err == nil {
		log.Printf("[OAuth/Service] findOrCreateUserFromOAuth: Found existing user by email: %s, linking OAuth", existingUser.Username)
		// User exists with this email - link OAuth to existing account
		return s.linkOAuthToExistingUser(existingUser, provider, userInfo)
	} else if !errors.Is(err, ErrUserNotFound) {
		log.Printf("[OAuth/Service] findOrCreateUserFromOAuth: Database error finding user: %v", err)
		return nil, fmt.Errorf("database error finding user: %w", err)
	}

	log.Printf("[OAuth/Service] findOrCreateUserFromOAuth: No existing user, creating new account")
	// No existing user - create new account with OAuth
	return s.createUserWithOAuth(provider, userInfo)
}

// updateOAuthTokens updates stored OAuth tokens when user re-authenticates
func (s *Service) updateOAuthTokens(oauthProvider *models.OAuthProvider, userInfo *OAuthUserInfo) {
	// Only update if we have new tokens
	if userInfo.AccessToken != "" {
		oauthProvider.AccessToken = &userInfo.AccessToken
	}
	if userInfo.RefreshToken != "" {
		oauthProvider.RefreshToken = &userInfo.RefreshToken
	}
	if userInfo.TokenExpiry != nil {
		oauthProvider.TokenExpiry = userInfo.TokenExpiry
	}

	// Update avatar if changed
	if userInfo.AvatarURL != "" && userInfo.AvatarURL != oauthProvider.AvatarURL {
		oauthProvider.AvatarURL = userInfo.AvatarURL
	}

	// Save updates (fire and forget - don't fail login if token update fails)
	database.DB.Save(oauthProvider)
}

// linkOAuthToExistingUser links OAuth provider to existing user account
func (s *Service) linkOAuthToExistingUser(user *models.User, provider string, userInfo *OAuthUserInfo) (*AuthResponse, error) {
	// Store tokens if provided
	var accessToken, refreshToken *string
	if userInfo.AccessToken != "" {
		accessToken = &userInfo.AccessToken
	}
	if userInfo.RefreshToken != "" {
		refreshToken = &userInfo.RefreshToken
	}

	oauthProvider := models.OAuthProvider{
		ID:             uuid.New().String(),
		UserID:         user.ID,
		Provider:       provider,
		ProviderUserID: userInfo.ID,
		Email:          userInfo.Email,
		Name:           userInfo.Name,
		AvatarURL:      userInfo.AvatarURL,
		AccessToken:    accessToken,
		RefreshToken:   refreshToken,
		TokenExpiry:    userInfo.TokenExpiry,
	}

	err := database.DB.Create(&oauthProvider).Error
	if err != nil {
		return nil, fmt.Errorf("failed to link OAuth provider: %w", err)
	}

	// Update user avatar if they don't have one
	if user.AvatarURL == "" && userInfo.AvatarURL != "" {
		user.AvatarURL = userInfo.AvatarURL
		database.DB.Save(user)
	}

	return s.generateAuthResponse(user)
}

// createUserWithOAuth creates new user account from OAuth info
func (s *Service) createUserWithOAuth(provider string, userInfo *OAuthUserInfo) (*AuthResponse, error) {
	// Generate unique username from OAuth name
	username := generateUsernameFromName(userInfo.Name)

	// Ensure username is unique
	username, err := s.ensureUniqueUsername(username)
	if err != nil {
		return nil, fmt.Errorf("failed to generate unique username: %w", err)
	}

	// Create user
	user := models.User{
		ID:            uuid.New().String(),
		Email:         userInfo.Email,
		Username:      username,
		DisplayName:   userInfo.Name,
		AvatarURL:     userInfo.AvatarURL,
		EmailVerified: true, // OAuth emails are pre-verified
		StreamUserID:  uuid.New().String(),
	}

	// Set OAuth provider ID
	if provider == "google" {
		user.GoogleID = &userInfo.ID
	} else if provider == "discord" {
		user.DiscordID = &userInfo.ID
	}

	// Store tokens if provided
	var accessToken, refreshToken *string
	if userInfo.AccessToken != "" {
		accessToken = &userInfo.AccessToken
	}
	if userInfo.RefreshToken != "" {
		refreshToken = &userInfo.RefreshToken
	}

	// Create user in transaction
	err = database.DB.Transaction(func(tx *gorm.DB) error {
		// Create user
		if err := tx.Create(&user).Error; err != nil {
			return err
		}

		// Create OAuth provider link with tokens
		oauthProvider := models.OAuthProvider{
			ID:             uuid.New().String(),
			UserID:         user.ID,
			Provider:       provider,
			ProviderUserID: userInfo.ID,
			Email:          userInfo.Email,
			Name:           userInfo.Name,
			AvatarURL:      userInfo.AvatarURL,
			AccessToken:    accessToken,
			RefreshToken:   refreshToken,
			TokenExpiry:    userInfo.TokenExpiry,
		}

		return tx.Create(&oauthProvider).Error
	})

	if err != nil {
		return nil, fmt.Errorf("failed to create user with OAuth: %w", err)
	}

	// Create getstream.io user for social features (feeds + chat)
	if s.streamClient != nil {
		if err := s.streamClient.CreateUser(user.StreamUserID, user.Username); err != nil {
			// Log but don't fail registration - getstream.io user can be created later
			fmt.Printf("Warning: failed to create getstream.io user: %v\n", err)
		}
	}

	return s.generateAuthResponse(&user)
}

// getGoogleUserInfo fetches user info from Google OAuth
func (s *Service) getGoogleUserInfo(code string) (*OAuthUserInfo, error) {
	log.Printf("[OAuth/Service] getGoogleUserInfo: Exchanging authorization code for token")
	token, err := s.googleConfig.Exchange(context.Background(), code)
	if err != nil {
		log.Printf("[OAuth/Service] getGoogleUserInfo: Token exchange failed: %v", err)
		return nil, fmt.Errorf("failed to exchange code: %w", err)
	}
	log.Printf("[OAuth/Service] getGoogleUserInfo: Token exchange successful, fetching user info")

	client := s.googleConfig.Client(context.Background(), token)
	resp, err := client.Get("https://www.googleapis.com/oauth2/v2/userinfo")
	if err != nil {
		log.Printf("[OAuth/Service] getGoogleUserInfo: Failed to fetch user info: %v", err)
		return nil, fmt.Errorf("failed to get user info: %w", err)
	}
	defer resp.Body.Close()

	log.Printf("[OAuth/Service] getGoogleUserInfo: User info response status: %d", resp.StatusCode)

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Printf("[OAuth/Service] getGoogleUserInfo: Failed to read response body: %v", err)
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	var googleUser GoogleUserInfo
	err = json.Unmarshal(body, &googleUser)
	if err != nil {
		log.Printf("[OAuth/Service] getGoogleUserInfo: Failed to parse JSON: %v", err)
		return nil, fmt.Errorf("failed to parse user info: %w", err)
	}

	log.Printf("[OAuth/Service] getGoogleUserInfo: Parsed user - sub: %s, email: %s, name: %s", googleUser.Sub, googleUser.Email, googleUser.Name)

	// Extract token expiry time
	var tokenExpiry *time.Time
	if !token.Expiry.IsZero() {
		tokenExpiry = &token.Expiry
	}

	return &OAuthUserInfo{
		ID:           googleUser.Sub,
		Email:        googleUser.Email,
		Name:         googleUser.Name,
		AvatarURL:    googleUser.Picture,
		AccessToken:  token.AccessToken,
		RefreshToken: token.RefreshToken,
		TokenExpiry:  tokenExpiry,
	}, nil
}

// getDiscordUserInfo fetches user info from Discord OAuth
func (s *Service) getDiscordUserInfo(code string) (*OAuthUserInfo, error) {
	token, err := s.discordConfig.Exchange(context.Background(), code)
	if err != nil {
		return nil, fmt.Errorf("failed to exchange code: %w", err)
	}

	client := s.discordConfig.Client(context.Background(), token)
	resp, err := client.Get("https://discord.com/api/users/@me")
	if err != nil {
		return nil, fmt.Errorf("failed to get user info: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	var discordUser DiscordUserInfo
	err = json.Unmarshal(body, &discordUser)
	if err != nil {
		return nil, fmt.Errorf("failed to parse user info: %w", err)
	}

	avatarURL := ""
	if discordUser.Avatar != "" {
		avatarURL = fmt.Sprintf("https://cdn.discordapp.com/avatars/%s/%s.png", discordUser.ID, discordUser.Avatar)
	}

	// Extract token expiry time
	var tokenExpiry *time.Time
	if !token.Expiry.IsZero() {
		tokenExpiry = &token.Expiry
	}

	return &OAuthUserInfo{
		ID:           discordUser.ID,
		Email:        discordUser.Email,
		Name:         discordUser.Username,
		AvatarURL:    avatarURL,
		AccessToken:  token.AccessToken,
		RefreshToken: token.RefreshToken,
		TokenExpiry:  tokenExpiry,
	}, nil
}

// ensureUniqueUsername generates a unique username
func (s *Service) ensureUniqueUsername(baseUsername string) (string, error) {
	username := baseUsername
	counter := 1

	for {
		var existingUser models.User
		err := database.DB.Where("LOWER(username) = LOWER(?)", username).First(&existingUser).Error

		if errors.Is(err, gorm.ErrRecordNotFound) {
			// Username is available
			return username, nil
		} else if err != nil {
			return "", fmt.Errorf("database error: %w", err)
		}

		// Username taken, try with counter
		username = fmt.Sprintf("%s%d", baseUsername, counter)
		counter++

		if counter > 999 {
			return "", errors.New("unable to generate unique username")
		}
	}
}

// generateUsernameFromName creates a username from display name
func generateUsernameFromName(name string) string {
	// Clean the name to create a valid username
	username := strings.ToLower(strings.ReplaceAll(name, " ", ""))

	// Remove non-alphanumeric characters
	cleaned := ""
	for _, char := range username {
		if (char >= 'a' && char <= 'z') || (char >= '0' && char <= '9') {
			cleaned += string(char)
		}
	}

	if cleaned == "" {
		cleaned = "producer"
	}

	if len(cleaned) > 20 {
		cleaned = cleaned[:20]
	}

	return cleaned
}
