package auth

import (
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// MockCall records a method call for assertion
type MockCall struct {
	Method string
	Args   []interface{}
}

// MockAuthService is a mock implementation of AuthServiceInterface for testing.
type MockAuthService struct {
	mu sync.Mutex

	// Call tracking
	Calls []MockCall

	// Configurable function overrides
	RegisterNativeUserFunc    func(req RegisterRequest) (*AuthResponse, error)
	LoginNativeUserFunc       func(req LoginRequest) (*AuthResponse, error)
	FindUserByEmailFunc       func(email string) (*models.User, error)
	ValidateTokenFunc         func(tokenString string) (*models.User, error)
	GetGoogleOAuthURLFunc     func(state string) string
	GetDiscordOAuthURLFunc    func(state string) string
	RequestPasswordResetFunc  func(email string) (*models.PasswordReset, error)
	ResetPasswordFunc         func(token, newPassword string) error

	// Default error to return
	DefaultError error

	// Pre-configured users for testing
	Users map[string]*models.User // keyed by email
}

// NewMockAuthService creates a new mock auth service with sensible defaults
func NewMockAuthService() *MockAuthService {
	return &MockAuthService{
		Calls: make([]MockCall, 0),
		Users: make(map[string]*models.User),
	}
}

// recordCall records a method call for later assertion
func (m *MockAuthService) recordCall(method string, args ...interface{}) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Calls = append(m.Calls, MockCall{Method: method, Args: args})
}

// GetCalls returns all recorded calls (thread-safe)
func (m *MockAuthService) GetCalls() []MockCall {
	m.mu.Lock()
	defer m.mu.Unlock()
	result := make([]MockCall, len(m.Calls))
	copy(result, m.Calls)
	return result
}

// GetCallsForMethod returns calls for a specific method
func (m *MockAuthService) GetCallsForMethod(method string) []MockCall {
	m.mu.Lock()
	defer m.mu.Unlock()
	var result []MockCall
	for _, call := range m.Calls {
		if call.Method == method {
			result = append(result, call)
		}
	}
	return result
}

// Reset clears all recorded calls
func (m *MockAuthService) Reset() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Calls = make([]MockCall, 0)
}

// AssertCalled checks if a method was called at least once
func (m *MockAuthService) AssertCalled(method string) bool {
	return len(m.GetCallsForMethod(method)) > 0
}

// AddUser adds a test user to the mock service
func (m *MockAuthService) AddUser(user *models.User) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.Users[user.Email] = user
}

// ============================================================================
// AuthServiceInterface implementation
// ============================================================================

func (m *MockAuthService) RegisterNativeUser(req RegisterRequest) (*AuthResponse, error) {
	m.recordCall("RegisterNativeUser", req)
	if m.RegisterNativeUserFunc != nil {
		return m.RegisterNativeUserFunc(req)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}

	// Check if user already exists
	if _, exists := m.Users[req.Email]; exists {
		return nil, ErrUserExists
	}

	// Create mock user
	user := &models.User{
		ID:          uuid.New().String(),
		Email:       req.Email,
		Username:    req.Username,
		DisplayName: req.DisplayName,
	}
	m.AddUser(user)

	return &AuthResponse{
		Token:     "mock_token_" + user.ID,
		User:      *user,
		ExpiresAt: time.Now().Add(24 * time.Hour),
	}, nil
}

func (m *MockAuthService) LoginNativeUser(req LoginRequest) (*AuthResponse, error) {
	m.recordCall("LoginNativeUser", req)
	if m.LoginNativeUserFunc != nil {
		return m.LoginNativeUserFunc(req)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}

	// Check if user exists
	user, exists := m.Users[req.Email]
	if !exists {
		return nil, ErrInvalidCredentials
	}

	return &AuthResponse{
		Token:     "mock_token_" + user.ID,
		User:      *user,
		ExpiresAt: time.Now().Add(24 * time.Hour),
	}, nil
}

func (m *MockAuthService) FindUserByEmail(email string) (*models.User, error) {
	m.recordCall("FindUserByEmail", email)
	if m.FindUserByEmailFunc != nil {
		return m.FindUserByEmailFunc(email)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}

	user, exists := m.Users[email]
	if !exists {
		return nil, ErrUserNotFound
	}
	return user, nil
}

func (m *MockAuthService) ValidateToken(tokenString string) (*models.User, error) {
	m.recordCall("ValidateToken", tokenString)
	if m.ValidateTokenFunc != nil {
		return m.ValidateTokenFunc(tokenString)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}

	// Default: return nil user (token invalid)
	return nil, ErrUserNotFound
}

func (m *MockAuthService) GetGoogleOAuthURL(state string) string {
	m.recordCall("GetGoogleOAuthURL", state)
	if m.GetGoogleOAuthURLFunc != nil {
		return m.GetGoogleOAuthURLFunc(state)
	}
	return "https://accounts.google.com/o/oauth2/v2/auth?state=" + state
}

func (m *MockAuthService) GetDiscordOAuthURL(state string) string {
	m.recordCall("GetDiscordOAuthURL", state)
	if m.GetDiscordOAuthURLFunc != nil {
		return m.GetDiscordOAuthURLFunc(state)
	}
	return "https://discord.com/api/oauth2/authorize?state=" + state
}

func (m *MockAuthService) RequestPasswordReset(email string) (*models.PasswordReset, error) {
	m.recordCall("RequestPasswordReset", email)
	if m.RequestPasswordResetFunc != nil {
		return m.RequestPasswordResetFunc(email)
	}
	if m.DefaultError != nil {
		return nil, m.DefaultError
	}

	return &models.PasswordReset{
		ID:        uuid.New().String(),
		UserID:    "mock_user_id",
		Token:     uuid.New().String(),
		ExpiresAt: time.Now().Add(time.Hour),
	}, nil
}

func (m *MockAuthService) ResetPassword(token, newPassword string) error {
	m.recordCall("ResetPassword", token, newPassword)
	if m.ResetPasswordFunc != nil {
		return m.ResetPasswordFunc(token, newPassword)
	}
	return m.DefaultError
}

// Ensure MockAuthService implements AuthServiceInterface
var _ AuthServiceInterface = (*MockAuthService)(nil)
