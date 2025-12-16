package errors

import (
	"errors"
	"strings"
	"testing"
)

// TestNewCLIError creates and validates a CLI error
func TestNewCLIError(t *testing.T) {
	cause := errors.New("underlying error")
	err := NewCLIError(ErrorTypeValidation, "Test error", cause)

	if err == nil {
		t.Fatal("NewCLIError returned nil")
	}

	if err.Type != ErrorTypeValidation {
		t.Errorf("Expected type %s, got %s", ErrorTypeValidation, err.Type)
	}

	if err.Message != "Test error" {
		t.Errorf("Expected message 'Test error', got '%s'", err.Message)
	}

	if err.Cause != cause {
		t.Error("Cause not set correctly")
	}
}

// TestWithSuggestion adds suggestion to error
func TestWithSuggestion(t *testing.T) {
	err := NewCLIError(ErrorTypeValidation, "Test", nil)
	suggestion := "Try something else"

	result := err.WithSuggestion(suggestion)

	if !result.HasSuggestion() {
		t.Error("HasSuggestion returned false")
	}

	if result.Suggestion != suggestion {
		t.Errorf("Expected suggestion '%s', got '%s'", suggestion, result.Suggestion)
	}
}

// TestNetworkError creates network error
func TestNetworkError(t *testing.T) {
	err := NetworkError("Connection failed")

	if err.Type != ErrorTypeNetwork {
		t.Errorf("Expected type %s, got %s", ErrorTypeNetwork, err.Type)
	}

	if !err.HasSuggestion() {
		t.Error("Expected suggestion for network error")
	}

	if !strings.Contains(err.Suggestion, "internet") {
		t.Error("Expected helpful suggestion about internet connection")
	}
}

// TestTimeoutError creates timeout error
func TestTimeoutError(t *testing.T) {
	err := TimeoutError()

	if err.Type != ErrorTypeTimeout {
		t.Errorf("Expected type %s, got %s", ErrorTypeTimeout, err.Type)
	}

	if !err.HasSuggestion() {
		t.Error("Expected suggestion for timeout error")
	}
}

// TestAuthError creates auth error
func TestAuthError(t *testing.T) {
	err := AuthError("Invalid credentials")

	if err.Type != ErrorTypeAuth {
		t.Errorf("Expected type %s, got %s", ErrorTypeAuth, err.Type)
	}

	if !strings.Contains(err.Message, "Invalid") {
		t.Error("Expected auth message")
	}

	if !strings.Contains(err.Suggestion, "login") {
		t.Error("Expected login suggestion")
	}
}

// TestSessionExpiredError creates session expired error
func TestSessionExpiredError(t *testing.T) {
	err := SessionExpiredError()

	if err.Type != ErrorTypeSessionExpired {
		t.Errorf("Expected type %s, got %s", ErrorTypeSessionExpired, err.Type)
	}

	if !strings.Contains(err.Suggestion, "auth login") {
		t.Error("Expected login suggestion for expired session")
	}
}

// TestUnauthorizedError creates unauthorized error
func TestUnauthorizedError(t *testing.T) {
	err := UnauthorizedError()

	if err.Type != ErrorTypeUnauthorized {
		t.Errorf("Expected type %s, got %s", ErrorTypeUnauthorized, err.Type)
	}
}

// TestForbiddenError creates forbidden error
func TestForbiddenError(t *testing.T) {
	err := ForbiddenError()

	if err.Type != ErrorTypeForbidden {
		t.Errorf("Expected type %s, got %s", ErrorTypeForbidden, err.Type)
	}
}

// TestValidationError creates validation error
func TestValidationError(t *testing.T) {
	err := ValidationError("email", "invalid format")

	if err.Type != ErrorTypeValidation {
		t.Errorf("Expected type %s, got %s", ErrorTypeValidation, err.Type)
	}

	if !strings.Contains(err.Message, "email") {
		t.Error("Expected field name in message")
	}

	if !strings.Contains(err.Message, "invalid format") {
		t.Error("Expected reason in message")
	}
}

// TestFileNotFoundError creates file not found error
func TestFileNotFoundError(t *testing.T) {
	path := "/path/to/file.mp3"
	err := FileNotFoundError(path)

	if err.Type != ErrorTypeFileNotFound {
		t.Errorf("Expected type %s, got %s", ErrorTypeFileNotFound, err.Type)
	}

	if !strings.Contains(err.Message, path) {
		t.Error("Expected path in message")
	}

	if !err.HasSuggestion() {
		t.Error("Expected suggestion for file not found")
	}
}

// TestAudioFormatError creates audio format error
func TestAudioFormatError(t *testing.T) {
	err := AudioFormatError("xyz")

	if err.Type != ErrorTypeAudioFormat {
		t.Errorf("Expected type %s, got %s", ErrorTypeAudioFormat, err.Type)
	}

	if !strings.Contains(err.Message, "xyz") {
		t.Error("Expected format in message")
	}

	if !strings.Contains(err.Suggestion, "mp3") {
		t.Error("Expected supported formats in suggestion")
	}
}

// TestAudioSizeError creates audio size error
func TestAudioSizeError(t *testing.T) {
	err := AudioSizeError(150.5, 100)

	if err.Type != ErrorTypeAudioSize {
		t.Errorf("Expected type %s, got %s", ErrorTypeAudioSize, err.Type)
	}

	if !strings.Contains(err.Message, "150.5") {
		t.Error("Expected actual size in message")
	}

	if !strings.Contains(err.Message, "100") {
		t.Error("Expected max size in message")
	}
}

// TestRateLimitError creates rate limit error
func TestRateLimitError(t *testing.T) {
	retryAfter := 60
	err := RateLimitError(retryAfter)

	if err.Type != ErrorTypeRateLimit {
		t.Errorf("Expected type %s, got %s", ErrorTypeRateLimit, err.Type)
	}

	if err.RetryAfter != retryAfter {
		t.Errorf("Expected RetryAfter %d, got %d", retryAfter, err.RetryAfter)
	}

	if !strings.Contains(err.Suggestion, "60") {
		t.Error("Expected retry time in suggestion")
	}
}

// TestNotFoundError creates not found error
func TestNotFoundError(t *testing.T) {
	err := NotFoundError("User", "john_doe")

	if err.Type != ErrorTypeNotFound {
		t.Errorf("Expected type %s, got %s", ErrorTypeNotFound, err.Type)
	}

	if !strings.Contains(err.Message, "User") {
		t.Error("Expected resource type in message")
	}

	if !strings.Contains(err.Message, "john_doe") {
		t.Error("Expected identifier in message")
	}
}

// TestConflictError creates conflict error
func TestConflictError(t *testing.T) {
	err := ConflictError("Username already exists")

	if err.Type != ErrorTypeConflict {
		t.Errorf("Expected type %s, got %s", ErrorTypeConflict, err.Type)
	}

	if !err.HasSuggestion() {
		t.Error("Expected suggestion for conflict error")
	}
}

// TestCategorizeError categorizes standard errors
func TestCategorizeError(t *testing.T) {
	testCases := []struct {
		input    error
		expected ErrorType
		name     string
	}{
		{errors.New("connection refused"), ErrorTypeNetwork, "connection refused"},
		{errors.New("timeout"), ErrorTypeTimeout, "timeout"},
		{errors.New("context deadline exceeded"), ErrorTypeTimeout, "context deadline"},
		{errors.New("401 unauthorized"), ErrorTypeAuth, "401 error"},
		{errors.New("403 forbidden"), ErrorTypeForbidden, "403 error"},
		{errors.New("404 not found"), ErrorTypeNotFound, "404 error"},
		{errors.New("429 rate limit"), ErrorTypeRateLimit, "429 error"},
		{errors.New("500 server error"), ErrorTypeServer, "500 error"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			err := CategorizeError(tc.input)

			if err.Type != tc.expected {
				t.Errorf("Expected type %s, got %s", tc.expected, err.Type)
			}
		})
	}
}

// TestFormatError formats error for display
func TestFormatError(t *testing.T) {
	err := AuthError("Invalid credentials")
	formatted := FormatError(err)

	if !strings.Contains(formatted, "Error") {
		t.Error("Expected 'Error' in formatted message")
	}

	if !strings.Contains(formatted, "auth") {
		t.Error("Expected error type in formatted message")
	}

	if !strings.Contains(formatted, "Suggestion") {
		t.Error("Expected suggestion in formatted message")
	}
}

// TestFormatError_WithoutSuggestion formats error without suggestion
func TestFormatError_NoSuggestion(t *testing.T) {
	err := NewCLIError(ErrorTypeUnknown, "Some error", nil)
	formatted := FormatError(err)

	if !strings.Contains(formatted, "Error") {
		t.Error("Expected 'Error' in formatted message")
	}

	if !strings.Contains(formatted, "Some error") {
		t.Error("Expected error message in formatted output")
	}
}

// TestFormatError_Nil handles nil error
func TestFormatError_Nil(t *testing.T) {
	formatted := FormatError(nil)

	if formatted != "" {
		t.Errorf("Expected empty string for nil error, got '%s'", formatted)
	}
}

// TestUnwrap returns underlying error
func TestUnwrap(t *testing.T) {
	cause := errors.New("underlying error")
	err := NewCLIError(ErrorTypeValidation, "Test", cause)

	if err.Unwrap() != cause {
		t.Error("Unwrap did not return the correct underlying error")
	}
}

// TestErrorImplementsError verifies CLIError implements error interface
func TestErrorImplementsError(t *testing.T) {
	err := NewCLIError(ErrorTypeValidation, "Test", nil)

	// This will compile only if CLIError implements error
	var _ error = err

	if err.Error() != "Test" {
		t.Errorf("Error() returned '%s', expected 'Test'", err.Error())
	}
}
