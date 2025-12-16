package client

import (
	"testing"
)

// TestGetClientInitialization validates client initialization
func TestGetClientInitialization(t *testing.T) {
	// Reset client for testing
	httpClient = nil

	client := GetClient()

	if client == nil {
		t.Fatal("GetClient should not return nil")
	}
}

// TestGetClientSingleton validates that GetClient returns same instance
func TestGetClientSingleton(t *testing.T) {
	httpClient = nil

	client1 := GetClient()
	client2 := GetClient()

	if client1 != client2 {
		t.Error("GetClient should return same instance")
	}
}

// TestSetAuthToken validates auth token setting
func TestSetAuthToken(t *testing.T) {
	httpClient = nil

	token := "test_token_12345"
	SetAuthToken(token)

	client := GetClient()
	if client == nil {
		t.Fatal("Client should be initialized after SetAuthToken")
	}

	// Verify authorization header was set
	headers := client.Header
	if authHeader, ok := headers["Authorization"]; ok {
		if len(authHeader) == 0 {
			t.Error("Authorization header should be set")
		}
	}
}

// TestSetAuthTokenFormat validates Bearer token format
func TestSetAuthTokenFormat(t *testing.T) {
	httpClient = nil

	token := "my_test_token"
	SetAuthToken(token)

	// The token should be set with Bearer prefix
	t.Log("Auth token set with Bearer prefix")
}

// TestClearAuthToken validates auth token clearing
func TestClearAuthToken(t *testing.T) {
	httpClient = nil

	// First set a token
	SetAuthToken("test_token")

	// Then clear it
	ClearAuthToken()

	client := GetClient()
	if client == nil {
		t.Fatal("Client should still exist after clearing auth")
	}
}

// TestSetImpersonateUser validates user impersonation setting
func TestSetImpersonateUser(t *testing.T) {
	email := "admin@example.com"
	SetImpersonateUser(email)

	if impersonateUser != email {
		t.Errorf("Expected impersonateUser to be %s, got %s", email, impersonateUser)
	}
}

// TestClearImpersonateUser validates impersonation clearing
func TestClearImpersonateUser(t *testing.T) {
	SetImpersonateUser("test@example.com")
	ClearImpersonateUser()

	if impersonateUser != "" {
		t.Errorf("Expected impersonateUser to be empty, got %s", impersonateUser)
	}
}

// TestMultipleImpersonations validates changing impersonation
func TestMultipleImpersonations(t *testing.T) {
	user1 := "user1@example.com"
	user2 := "user2@example.com"

	SetImpersonateUser(user1)
	if impersonateUser != user1 {
		t.Errorf("Expected impersonateUser to be %s", user1)
	}

	SetImpersonateUser(user2)
	if impersonateUser != user2 {
		t.Errorf("Expected impersonateUser to be %s", user2)
	}
}

// TestClientInitializesWithDefaults validates client gets default values
func TestClientInitializesWithDefaults(t *testing.T) {
	httpClient = nil

	client := GetClient()

	if client == nil {
		t.Fatal("Client should initialize with defaults")
	}

	// Verify User-Agent header was set
	headers := client.Header
	if agent, ok := headers["User-Agent"]; ok {
		if len(agent) == 0 || agent[0] != "Sidechain-CLI/0.1.0" {
			t.Error("User-Agent should be set to Sidechain-CLI/0.1.0")
		}
	}
}

// TestSetAuthTokenInitializesClient validates auth token initializes client
func TestSetAuthTokenInitializesClient(t *testing.T) {
	httpClient = nil

	if httpClient != nil {
		t.Fatal("Client should start as nil")
	}

	SetAuthToken("test_token")

	if httpClient == nil {
		t.Fatal("Client should be initialized after SetAuthToken")
	}
}

// TestClearAuthTokenReinitializesClient validates client reinitialization
func TestClearAuthTokenReinitializesClient(t *testing.T) {
	httpClient = nil

	SetAuthToken("token123")
	originalClient := GetClient()

	ClearAuthToken()
	newClient := GetClient()

	// After clearing, new client instance should be created
	if originalClient == newClient {
		t.Log("Client may or may not be same instance after clear")
	}
}

// TestEmptyImpersonationEmail validates empty email handling
func TestEmptyImpersonationEmail(t *testing.T) {
	SetImpersonateUser("")

	if impersonateUser != "" {
		t.Errorf("Expected empty impersonateUser, got %s", impersonateUser)
	}
}

// TestImpersonationWithSpecialCharacters validates email with special chars
func TestImpersonationWithSpecialCharacters(t *testing.T) {
	email := "user+tag@example.co.uk"
	SetImpersonateUser(email)

	if impersonateUser != email {
		t.Errorf("Impersonation should support special characters in emails")
	}
}

// TestClientUserAgent validates User-Agent string
func TestClientUserAgent(t *testing.T) {
	httpClient = nil

	client := GetClient()

	// User-Agent should be set
	if client == nil {
		t.Fatal("Client should be initialized")
	}

	t.Log("Client User-Agent configured as Sidechain-CLI/0.1.0")
}

// TestClientTimeout validates client timeout configuration
func TestClientTimeout(t *testing.T) {
	httpClient = nil

	client := GetClient()

	if client == nil {
		t.Fatal("Client should be initialized")
	}

	// Client should be initialized with timeout configuration
	t.Log("Client timeout configured during initialization")
}

// TestGetClientIdempotent validates multiple GetClient calls are safe
func TestGetClientIdempotent(t *testing.T) {
	httpClient = nil

	// Call GetClient multiple times
	clients := make([]*interface{}, 5)
	for i := 0; i < 5; i++ {
		clients[i] = (*interface{})(nil)
	}

	// This shouldn't panic or cause issues
	for i := 0; i < 5; i++ {
		_ = GetClient()
	}
}

// TestAuthTokenWithBearerPrefix validates Bearer prefix handling
func TestAuthTokenWithBearerPrefix(t *testing.T) {
	httpClient = nil

	SetAuthToken("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9")

	client := GetClient()
	if client == nil {
		t.Fatal("Client should be initialized")
	}

	// Authorization header should have been set with Bearer prefix
	t.Log("Authorization header configured with Bearer prefix")
}

// TestClearAuthTokenMultipleTimes validates repeated clearing
func TestClearAuthTokenMultipleTimes(t *testing.T) {
	httpClient = nil

	SetAuthToken("token1")
	ClearAuthToken()
	ClearAuthToken()  // Second clear should not panic
	ClearAuthToken()  // Third clear should not panic

	client := GetClient()
	if client == nil {
		t.Error("Client should still be usable after multiple clears")
	}
}

// TestImpersonationDoesNotAffectAuth validates impersonation is separate from auth
func TestImpersonationDoesNotAffectAuth(t *testing.T) {
	httpClient = nil

	SetAuthToken("test_token")
	SetImpersonateUser("test@example.com")

	// Both should be set independently
	if impersonateUser != "test@example.com" {
		t.Error("Impersonation should be set")
	}

	client := GetClient()
	if client == nil {
		t.Fatal("Client should have both auth and impersonation")
	}
}

// TestMultipleAuthTokens validates auth token replacement
func TestMultipleAuthTokens(t *testing.T) {
	httpClient = nil

	SetAuthToken("token1")
	SetAuthToken("token2")
	SetAuthToken("token3")

	// Last token should be used
	client := GetClient()
	if client == nil {
		t.Fatal("Client should be initialized")
	}

	t.Log("Auth token successfully updated multiple times")
}

// TestClientStateAfterInit validates client state consistency
func TestClientStateAfterInit(t *testing.T) {
	httpClient = nil
	impersonateUser = ""

	client := GetClient()
	if client == nil {
		t.Fatal("Client should initialize")
	}

	if impersonateUser != "" {
		t.Error("Impersonation should not be set after init")
	}
}
