package auth

import "github.com/zfogg/sidechain/backend/internal/models"

// AuthServiceInterface defines the contract for authentication operations.
// This enables mocking for unit tests without requiring a real database.
type AuthServiceInterface interface {
	// Registration and Login
	RegisterNativeUser(req RegisterRequest) (*AuthResponse, error)
	LoginNativeUser(req LoginRequest) (*AuthResponse, error)

	// User lookup
	FindUserByEmail(email string) (*models.User, error)

	// Token operations
	ValidateToken(tokenString string) (*models.User, error)

	// OAuth URLs
	GetGoogleOAuthURL(state string) string
	GetDiscordOAuthURL(state string) string

	// Password reset
	RequestPasswordReset(email string) (*models.PasswordReset, error)
	ResetPassword(token, newPassword string) error
}

// Ensure Service implements AuthServiceInterface
var _ AuthServiceInterface = (*Service)(nil)
