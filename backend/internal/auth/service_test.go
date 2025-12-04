package auth

import (
	"fmt"
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// AuthServiceTestSuite contains auth service tests
type AuthServiceTestSuite struct {
	suite.Suite
	db          *gorm.DB
	authService *Service
}

// SetupSuite initializes test database and auth service
func (suite *AuthServiceTestSuite) SetupSuite() {
	// Build test DSN from environment or use defaults
	host := getEnvOrDefault("POSTGRES_HOST", "localhost")
	port := getEnvOrDefault("POSTGRES_PORT", "5432")
	user := getEnvOrDefault("POSTGRES_USER", "postgres")
	password := getEnvOrDefault("POSTGRES_PASSWORD", "")
	dbname := getEnvOrDefault("POSTGRES_DB", "sidechain_test")

	testDSN := fmt.Sprintf("host=%s port=%s user=%s dbname=%s sslmode=disable", host, port, user, dbname)
	if password != "" {
		testDSN = fmt.Sprintf("host=%s port=%s user=%s password=%s dbname=%s sslmode=disable", host, port, user, password, dbname)
	}

	db, err := gorm.Open(postgres.Open(testDSN), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent), // Quiet during tests
	})
	if err != nil {
		suite.T().Skipf("Skipping auth tests: database not available (%v)", err)
		return
	}

	// Set global DB for database package
	database.DB = db

	// Create test tables
	err = db.AutoMigrate(
		&models.User{},
		&models.OAuthProvider{},
		&models.PasswordReset{},
	)
	require.NoError(suite.T(), err)

	suite.db = db

	// Initialize auth service with test secrets (nil stream client for tests)
	suite.authService = NewService(
		[]byte("test_jwt_secret_key"),
		nil, // Stream client not needed for unit tests
		"test_google_client_id",
		"test_google_client_secret",
		"test_discord_client_id",
		"test_discord_client_secret",
	)
}

// TearDownSuite cleans up after tests
func (suite *AuthServiceTestSuite) TearDownSuite() {
	// Clean up test database
	suite.db.Exec("DROP TABLE IF EXISTS users, oauth_providers, password_resets CASCADE")

	sqlDB, _ := suite.db.DB()
	sqlDB.Close()
}

// SetupTest cleans database before each test
func (suite *AuthServiceTestSuite) SetupTest() {
	// Clean tables
	suite.db.Exec("DELETE FROM oauth_providers")
	suite.db.Exec("DELETE FROM users")
	suite.db.Exec("DELETE FROM password_resets")
}

// TestRegisterNativeUser tests user registration
func (suite *AuthServiceTestSuite) TestRegisterNativeUser() {
	t := suite.T()

	// Test successful registration
	req := RegisterRequest{
		Email:       "test@producer.com",
		Username:    "testbeat",
		Password:    "password123",
		DisplayName: "Test Producer",
	}

	authResp, err := suite.authService.RegisterNativeUser(req)
	require.NoError(t, err)
	require.NotNil(t, authResp)

	// Verify user was created
	assert.NotEmpty(t, authResp.Token)
	assert.Equal(t, req.Email, authResp.User.Email)
	assert.Equal(t, req.Username, authResp.User.Username)
	assert.Equal(t, req.DisplayName, authResp.User.DisplayName)
	assert.NotNil(t, authResp.User.PasswordHash)
	assert.NotEmpty(t, authResp.User.StreamUserID)

	// Test duplicate email registration
	_, err = suite.authService.RegisterNativeUser(req)
	assert.Error(t, err)
	assert.Equal(t, ErrUserExists, err)

	// Test duplicate username
	req2 := RegisterRequest{
		Email:       "different@producer.com",
		Username:    "testbeat", // Same username
		Password:    "password456",
		DisplayName: "Different Producer",
	}

	_, err = suite.authService.RegisterNativeUser(req2)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "username already taken")
}

// TestLoginNativeUser tests user login
func (suite *AuthServiceTestSuite) TestLoginNativeUser() {
	t := suite.T()

	// Create test user first
	registerReq := RegisterRequest{
		Email:       "login@test.com",
		Username:    "logintest",
		Password:    "testpass123",
		DisplayName: "Login Test",
	}

	_, err := suite.authService.RegisterNativeUser(registerReq)
	require.NoError(t, err)

	// Test successful login
	loginReq := LoginRequest{
		Email:    "login@test.com",
		Password: "testpass123",
	}

	authResp, err := suite.authService.LoginNativeUser(loginReq)
	require.NoError(t, err)
	require.NotNil(t, authResp)

	assert.NotEmpty(t, authResp.Token)
	assert.Equal(t, loginReq.Email, authResp.User.Email)
	assert.NotNil(t, authResp.User.LastActiveAt)

	// Test invalid email
	loginReq.Email = "nonexistent@test.com"
	_, err = suite.authService.LoginNativeUser(loginReq)
	assert.Error(t, err)
	assert.Equal(t, ErrUserNotFound, err)

	// Test invalid password
	loginReq.Email = "login@test.com"
	loginReq.Password = "wrongpassword"
	_, err = suite.authService.LoginNativeUser(loginReq)
	assert.Error(t, err)
	assert.Equal(t, ErrInvalidCredentials, err)

	// Test case-insensitive email
	loginReq.Email = "LOGIN@TEST.COM"
	loginReq.Password = "testpass123"
	_, err = suite.authService.LoginNativeUser(loginReq)
	assert.NoError(t, err)
}

// TestJWTTokenValidation tests JWT token generation and validation
func (suite *AuthServiceTestSuite) TestJWTTokenValidation() {
	t := suite.T()

	// Create test user with proper UUID
	user := models.User{
		Email:        "jwt@test.com",
		Username:     "jwttest",
		DisplayName:  "JWT Test",
		StreamUserID: "550e8400-e29b-41d4-a716-446655440000", // Valid UUID
	}

	// Save user to database first
	err := suite.db.Create(&user).Error
	require.NoError(t, err)

	// Generate token
	authResp, err := suite.authService.generateAuthResponse(&user)
	require.NoError(t, err)

	// Validate token
	validatedUser, err := suite.authService.ValidateToken(authResp.Token)
	require.NoError(t, err)

	assert.Equal(t, user.ID, validatedUser.ID)
	assert.Equal(t, user.Email, validatedUser.Email)
	assert.Equal(t, user.Username, validatedUser.Username)

	// Test invalid token
	_, err = suite.authService.ValidateToken("invalid.jwt.token")
	assert.Error(t, err)

	// Test expired token (would need to mock time for proper test)
	// For now, test with wrong signing key
	wrongService := NewService([]byte("wrong_secret"), nil, "", "", "", "")
	_, err = wrongService.ValidateToken(authResp.Token)
	assert.Error(t, err)
}

// TestAccountUnification tests email-based account linking
func (suite *AuthServiceTestSuite) TestAccountUnification() {
	t := suite.T()

	email := "unify@test.com"

	// Create native account first
	registerReq := RegisterRequest{
		Email:       email,
		Username:    "unifytest",
		Password:    "password123",
		DisplayName: "Unify Test",
	}

	authResp1, err := suite.authService.RegisterNativeUser(registerReq)
	require.NoError(t, err)

	// Simulate OAuth user info with same email
	oauthInfo := &OAuthUserInfo{
		ID:        "google_123456",
		Email:     email, // Same email!
		Name:      "Unify Test Google",
		AvatarURL: "https://example.com/avatar.jpg",
	}

	// This should link OAuth to existing account, not create new user
	authResp2, err := suite.authService.findOrCreateUserFromOAuth("google", oauthInfo)
	require.NoError(t, err)

	// Should be the same user
	assert.Equal(t, authResp1.User.ID, authResp2.User.ID)
	assert.Equal(t, authResp1.User.Email, authResp2.User.Email)

	// Verify OAuth provider was linked
	var oauthProvider models.OAuthProvider
	err = suite.db.Where("user_id = ? AND provider = ?", authResp1.User.ID, "google").First(&oauthProvider).Error
	require.NoError(t, err)
	assert.Equal(t, "google_123456", oauthProvider.ProviderUserID)

	// Verify user can now login with password (original account still works)
	loginReq := LoginRequest{
		Email:    email,
		Password: "password123",
	}
	_, err = suite.authService.LoginNativeUser(loginReq)
	assert.NoError(t, err)
}

// TestReverseAccountUnification tests OAuth first, then native account
func (suite *AuthServiceTestSuite) TestReverseAccountUnification() {
	t := suite.T()

	email := "reverse@test.com"

	// Create OAuth account first
	oauthInfo := &OAuthUserInfo{
		ID:        "discord_789012",
		Email:     email,
		Name:      "Reverse Test",
		AvatarURL: "https://discord.com/avatar.jpg",
	}

	authResp1, err := suite.authService.findOrCreateUserFromOAuth("discord", oauthInfo)
	require.NoError(t, err)

	// User should have no password initially
	assert.Nil(t, authResp1.User.PasswordHash)

	// Try to register with same email - should add password to OAuth account
	registerReq := RegisterRequest{
		Email:       email,
		Username:    "reversetest",
		Password:    "newpassword123",
		DisplayName: "Reverse Test Updated",
	}

	authResp2, err := suite.authService.RegisterNativeUser(registerReq)
	require.NoError(t, err)

	// Should be the same user with password added
	assert.Equal(t, authResp1.User.ID, authResp2.User.ID)
	assert.NotNil(t, authResp2.User.PasswordHash)

	// User can now login with password
	loginReq := LoginRequest{
		Email:    email,
		Password: "newpassword123",
	}
	_, err = suite.authService.LoginNativeUser(loginReq)
	assert.NoError(t, err)
}

// TestUsernameGeneration tests username generation from OAuth names
func (suite *AuthServiceTestSuite) TestUsernameGeneration() {
	t := suite.T()

	testCases := []struct {
		name     string
		expected string
	}{
		{"John Doe", "johndoe"},
		{"UPPERCASE NAME", "uppercasename"},
		{"Special@Characters!", "specialcharacters"},
		{"", "producer"},
		{"VeryLongNameThatExceedsTwentyCharacters", "verylongnamethatexce"}, // Truncated to 20
	}

	for _, tc := range testCases {
		result := generateUsernameFromName(tc.name)
		assert.Equal(t, tc.expected, result, "Failed for input: %s", tc.name)
	}
}

// TestConcurrentRegistration tests concurrent user registration
func (suite *AuthServiceTestSuite) TestConcurrentRegistration() {
	t := suite.T()

	const numGoroutines = 10
	results := make(chan error, numGoroutines)

	// Attempt to register multiple users concurrently
	for i := 0; i < numGoroutines; i++ {
		go func(index int) {
			req := RegisterRequest{
				Email:       fmt.Sprintf("concurrent%d@test.com", index),
				Username:    fmt.Sprintf("concurrent%d", index),
				Password:    "password123",
				DisplayName: fmt.Sprintf("Concurrent User %d", index),
			}

			_, err := suite.authService.RegisterNativeUser(req)
			results <- err
		}(i)
	}

	// Check all registrations succeeded
	for i := 0; i < numGoroutines; i++ {
		err := <-results
		assert.NoError(t, err, "Concurrent registration %d failed", i)
	}

	// Verify all users were created
	var userCount int64
	suite.db.Model(&models.User{}).Where("email LIKE 'concurrent%@test.com'").Count(&userCount)
	assert.Equal(t, int64(numGoroutines), userCount)
}

// Run the test suite
func TestAuthServiceSuite(t *testing.T) {
	// Skip if no test database available
	if os.Getenv("SKIP_DB_TESTS") == "true" {
		t.Skip("Skipping database tests")
	}

	suite.Run(t, new(AuthServiceTestSuite))
}

// getEnvOrDefault returns environment variable or default value
func getEnvOrDefault(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}
