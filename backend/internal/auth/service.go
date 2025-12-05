package auth

import (
	"errors"
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"golang.org/x/crypto/bcrypt"
	"golang.org/x/oauth2"
	"golang.org/x/oauth2/google"
	"gorm.io/gorm"
)

var (
	ErrUserNotFound       = errors.New("user not found")
	ErrUserExists         = errors.New("user already exists")
	ErrUsernameExists     = errors.New("username already taken")
	ErrInvalidCredentials = errors.New("invalid credentials")
	ErrEmailNotVerified   = errors.New("email not verified")
)

// Service handles all authentication operations
type Service struct {
	jwtSecret     []byte
	streamClient  *stream.Client
	googleConfig  *oauth2.Config
	discordConfig *oauth2.Config
}

// NewService creates a new authentication service
func NewService(jwtSecret []byte, streamClient *stream.Client, googleClientID, googleClientSecret, discordClientID, discordClientSecret string) *Service {
	return &Service{
		jwtSecret:    jwtSecret,
		streamClient: streamClient,
		googleConfig: &oauth2.Config{
			ClientID:     googleClientID,
			ClientSecret: googleClientSecret,
			RedirectURL:  "http://localhost:8787/api/v1/auth/google/callback",
			Scopes:       []string{"openid", "profile", "email"},
			Endpoint:     google.Endpoint,
		},
		discordConfig: &oauth2.Config{
			ClientID:     discordClientID,
			ClientSecret: discordClientSecret,
			RedirectURL:  "http://localhost:8787/api/v1/auth/discord/callback",
			Scopes:       []string{"identify", "email"},
			Endpoint: oauth2.Endpoint{
				AuthURL:  "https://discord.com/api/oauth2/authorize",
				TokenURL: "https://discord.com/api/oauth2/token",
			},
		},
	}
}

// AuthResponse represents authentication response
type AuthResponse struct {
	Token     string      `json:"token"`
	User      models.User `json:"user"`
	ExpiresAt time.Time   `json:"expires_at"`
}

// RegisterRequest represents native registration request
type RegisterRequest struct {
	Email       string `json:"email" binding:"required,email"`
	Username    string `json:"username" binding:"required,min=3,max=30"`
	Password    string `json:"password" binding:"required,min=8"`
	DisplayName string `json:"display_name" binding:"required,min=1,max=50"`
}

// LoginRequest represents native login request
type LoginRequest struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required"`
}

// RegisterNativeUser creates a new user with email/password
func (s *Service) RegisterNativeUser(req RegisterRequest) (*AuthResponse, error) {
	// Check if user exists by email (case-insensitive)
	var existingUser models.User
	err := database.DB.Where("LOWER(email) = LOWER(?)", req.Email).First(&existingUser).Error

	if err == nil {
		// User exists - check if they can add a password
		if existingUser.PasswordHash == nil {
			// OAuth-only user trying to add password - allow upgrade
			return s.addPasswordToOAuthUser(&existingUser, req.Password)
		}
		return nil, ErrUserExists
	} else if !errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, fmt.Errorf("database error: %w", err)
	}

	// Check if username is taken
	var usernameCheck models.User
	err = database.DB.Where("LOWER(username) = LOWER(?)", req.Username).First(&usernameCheck).Error
	if err == nil {
		return nil, ErrUsernameExists
	} else if !errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, fmt.Errorf("database error: %w", err)
	}

	// Hash password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		return nil, fmt.Errorf("failed to hash password: %w", err)
	}

	// Create new user
	hashedPasswordStr := string(hashedPassword)
	user := models.User{
		ID:           uuid.New().String(),
		Email:        req.Email,
		Username:     req.Username,
		DisplayName:  req.DisplayName,
		PasswordHash: &hashedPasswordStr,
		StreamUserID: uuid.New().String(),
	}

	err = database.DB.Create(&user).Error
	if err != nil {
		return nil, fmt.Errorf("failed to create user: %w", err)
	}

	// Create Stream.io user for social features (feeds + chat)
	if s.streamClient != nil {
		if err := s.streamClient.CreateUser(user.StreamUserID, user.Username); err != nil {
			// Log but don't fail registration - Stream.io user can be created later
			fmt.Printf("Warning: failed to create Stream.io user: %v\n", err)
		}
	}

	return s.generateAuthResponse(&user)
}

// LoginNativeUser authenticates with email/password
func (s *Service) LoginNativeUser(req LoginRequest) (*AuthResponse, error) {
	var user models.User
	err := database.DB.Where("LOWER(email) = LOWER(?)", req.Email).First(&user).Error

	if errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, ErrUserNotFound
	} else if err != nil {
		return nil, fmt.Errorf("database error: %w", err)
	}

	// Check if user has a password (might be OAuth-only)
	if user.PasswordHash == nil {
		return nil, errors.New("account exists but no password set - try OAuth login")
	}

	// Verify password
	err = bcrypt.CompareHashAndPassword([]byte(*user.PasswordHash), []byte(req.Password))
	if err != nil {
		return nil, ErrInvalidCredentials
	}

	// Update last active
	now := time.Now()
	user.LastActiveAt = &now
	database.DB.Save(&user)

	return s.generateAuthResponse(&user)
}

// FindUserByEmail finds user by email (case-insensitive)
func (s *Service) FindUserByEmail(email string) (*models.User, error) {
	var user models.User
	err := database.DB.Where("LOWER(email) = LOWER(?)", email).First(&user).Error

	if errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, ErrUserNotFound
	} else if err != nil {
		return nil, fmt.Errorf("database error: %w", err)
	}

	return &user, nil
}

// addPasswordToOAuthUser adds password to OAuth-only account
func (s *Service) addPasswordToOAuthUser(user *models.User, password string) (*AuthResponse, error) {
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	if err != nil {
		return nil, fmt.Errorf("failed to hash password: %w", err)
	}

	hashedPasswordStr := string(hashedPassword)
	user.PasswordHash = &hashedPasswordStr

	err = database.DB.Save(user).Error
	if err != nil {
		return nil, fmt.Errorf("failed to update user: %w", err)
	}

	return s.generateAuthResponse(user)
}

// generateAuthResponse creates JWT token and auth response
func (s *Service) generateAuthResponse(user *models.User) (*AuthResponse, error) {
	expiresAt := time.Now().Add(24 * time.Hour) // 24 hour tokens

	claims := jwt.MapClaims{
		"user_id":        user.ID,
		"email":          user.Email,
		"username":       user.Username,
		"stream_user_id": user.StreamUserID,
		"exp":            expiresAt.Unix(),
		"iat":            time.Now().Unix(),
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenString, err := token.SignedString(s.jwtSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to sign token: %w", err)
	}

	return &AuthResponse{
		Token:     tokenString,
		User:      *user,
		ExpiresAt: expiresAt,
	}, nil
}

// ValidateToken validates a JWT token and returns user info
func (s *Service) ValidateToken(tokenString string) (*models.User, error) {
	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return s.jwtSecret, nil
	})

	if err != nil {
		return nil, fmt.Errorf("invalid token: %w", err)
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok || !token.Valid {
		return nil, errors.New("invalid token claims")
	}

	userID, ok := claims["user_id"].(string)
	if !ok {
		return nil, errors.New("invalid user_id in token")
	}

	// Fetch fresh user data
	var user models.User
	err = database.DB.Where("id = ?", userID).First(&user).Error
	if err != nil {
		return nil, fmt.Errorf("user not found: %w", err)
	}

	return &user, nil
}

// GetGoogleOAuthURL returns Google OAuth authorization URL
func (s *Service) GetGoogleOAuthURL(state string) string {
	return s.googleConfig.AuthCodeURL(state, oauth2.AccessTypeOffline)
}

// GetDiscordOAuthURL returns Discord OAuth authorization URL
func (s *Service) GetDiscordOAuthURL(state string) string {
	return s.discordConfig.AuthCodeURL(state)
}
