package errors

import (
	"errors"
	"fmt"
	"strings"
)

// ErrorType categorizes different error types
type ErrorType string

const (
	// Network errors
	ErrorTypeNetwork      ErrorType = "network"
	ErrorTypeTimeout      ErrorType = "timeout"
	ErrorTypeConnection   ErrorType = "connection"
	ErrorTypeHTTP         ErrorType = "http"

	// Authentication errors
	ErrorTypeAuth         ErrorType = "auth"
	ErrorTypeUnauthorized ErrorType = "unauthorized"
	ErrorTypeForbidden    ErrorType = "forbidden"
	ErrorTypeSessionExpired ErrorType = "session_expired"

	// Validation errors
	ErrorTypeValidation   ErrorType = "validation"
	ErrorTypeFileNotFound ErrorType = "file_not_found"
	ErrorTypeInvalidFormat ErrorType = "invalid_format"

	// Server errors
	ErrorTypeServer       ErrorType = "server"
	ErrorTypeNotFound     ErrorType = "not_found"
	ErrorTypeConflict     ErrorType = "conflict"
	ErrorTypeRateLimit    ErrorType = "rate_limit"

	// Audio errors
	ErrorTypeAudio        ErrorType = "audio"
	ErrorTypeAudioFormat  ErrorType = "audio_format"
	ErrorTypeAudioSize    ErrorType = "audio_size"

	// Unknown errors
	ErrorTypeUnknown      ErrorType = "unknown"
)

// CLIError represents a structured error with context
type CLIError struct {
	Type        ErrorType
	Message     string
	Cause       error
	Suggestion  string
	StatusCode  int
	RetryAfter  int
}

// Error implements the error interface
func (e *CLIError) Error() string {
	return e.Message
}

// WithSuggestion adds a helpful suggestion to the error
func (e *CLIError) WithSuggestion(suggestion string) *CLIError {
	e.Suggestion = suggestion
	return e
}

// HasSuggestion returns true if the error has a suggestion
func (e *CLIError) HasSuggestion() bool {
	return e.Suggestion != ""
}

// Unwrap returns the underlying error
func (e *CLIError) Unwrap() error {
	return e.Cause
}

// NewCLIError creates a new CLI error
func NewCLIError(errorType ErrorType, message string, cause error) *CLIError {
	return &CLIError{
		Type:    errorType,
		Message: message,
		Cause:   cause,
	}
}

// NetworkError creates a network error
func NetworkError(message string) *CLIError {
	err := NewCLIError(ErrorTypeNetwork, message, nil)
	err.Suggestion = "Check your internet connection and try again."
	return err
}

// TimeoutError creates a timeout error
func TimeoutError() *CLIError {
	err := NewCLIError(ErrorTypeTimeout, "Request timed out", nil)
	err.Suggestion = "The server is taking too long to respond. Try again in a moment."
	return err
}

// AuthError creates an authentication error
func AuthError(message string) *CLIError {
	err := NewCLIError(ErrorTypeAuth, message, nil)
	err.Suggestion = "Try logging in again with 'sidechain-cli auth login'"
	return err
}

// SessionExpiredError creates a session expired error
func SessionExpiredError() *CLIError {
	err := NewCLIError(ErrorTypeSessionExpired, "Your session has expired", nil)
	err.Suggestion = "Run 'sidechain-cli auth login' to refresh your session."
	return err
}

// UnauthorizedError creates an unauthorized error
func UnauthorizedError() *CLIError {
	err := NewCLIError(ErrorTypeUnauthorized, "You don't have permission to perform this action", nil)
	err.Suggestion = "Make sure you're logged in with an account that has the required permissions."
	return err
}

// ForbiddenError creates a forbidden error
func ForbiddenError() *CLIError {
	err := NewCLIError(ErrorTypeForbidden, "Access denied", nil)
	err.Suggestion = "Contact an administrator if you believe this is an error."
	return err
}

// ValidationError creates a validation error
func ValidationError(field, reason string) *CLIError {
	message := fmt.Sprintf("Validation error: %s - %s", field, reason)
	return NewCLIError(ErrorTypeValidation, message, nil)
}

// FileNotFoundError creates a file not found error
func FileNotFoundError(path string) *CLIError {
	err := NewCLIError(ErrorTypeFileNotFound, fmt.Sprintf("File not found: %s", path), nil)
	err.Suggestion = "Check the file path and try again."
	return err
}

// AudioFormatError creates an audio format error
func AudioFormatError(format string) *CLIError {
	err := NewCLIError(ErrorTypeAudioFormat,
		fmt.Sprintf("Unsupported audio format: .%s", format),
		nil)
	err.Suggestion = "Supported formats: mp3, wav, flac, ogg, aac, m4a, opus. Convert your file and try again."
	return err
}

// AudioSizeError creates an audio size error
func AudioSizeError(sizeMB float64, maxMB int) *CLIError {
	err := NewCLIError(ErrorTypeAudioSize,
		fmt.Sprintf("File too large: %.1f MB (max: %d MB)", sizeMB, maxMB),
		nil)
	err.Suggestion = fmt.Sprintf("Compress your audio file to under %d MB using ffmpeg or Audacity.", maxMB)
	return err
}

// ServerError creates a server error
func ServerError() *CLIError {
	err := NewCLIError(ErrorTypeServer, "Server error", nil)
	err.Suggestion = "The server encountered an error. Try again in a few moments."
	return err
}

// NotFoundError creates a not found error
func NotFoundError(resourceType, identifier string) *CLIError {
	err := NewCLIError(ErrorTypeNotFound,
		fmt.Sprintf("%s not found: %s", resourceType, identifier),
		nil)
	return err
}

// RateLimitError creates a rate limit error
func RateLimitError(retryAfter int) *CLIError {
	err := NewCLIError(ErrorTypeRateLimit,
		"Rate limit exceeded. Too many requests.",
		nil)
	err.RetryAfter = retryAfter
	err.Suggestion = fmt.Sprintf("Please wait %d seconds before trying again.", retryAfter)
	return err
}

// ConflictError creates a conflict error
func ConflictError(message string) *CLIError {
	err := NewCLIError(ErrorTypeConflict, message, nil)
	err.Suggestion = "This resource already exists. Try a different name or identifier."
	return err
}

// CategorizeError converts a standard error into a CLIError
func CategorizeError(err error) *CLIError {
	if err == nil {
		return nil
	}

	// Check if it's already a CLIError
	var cliErr *CLIError
	if errors.As(err, &cliErr) {
		return cliErr
	}

	// Categorize based on error message
	errMsg := err.Error()

	switch {
	case strings.Contains(errMsg, "connection refused"):
		return NetworkError("Could not connect to server. Make sure it's running.")
	case strings.Contains(errMsg, "timeout"):
		return TimeoutError()
	case strings.Contains(errMsg, "context deadline exceeded"):
		return TimeoutError()
	case strings.Contains(errMsg, "401") || strings.Contains(errMsg, "unauthorized"):
		return AuthError("Invalid credentials")
	case strings.Contains(errMsg, "403") || strings.Contains(errMsg, "forbidden"):
		return ForbiddenError()
	case strings.Contains(errMsg, "404") || strings.Contains(errMsg, "not found"):
		return NotFoundError("Resource", "unknown")
	case strings.Contains(errMsg, "429") || strings.Contains(errMsg, "rate limit"):
		return RateLimitError(60)
	case strings.Contains(errMsg, "500") || strings.Contains(errMsg, "server error"):
		return ServerError()
	default:
		return NewCLIError(ErrorTypeUnknown, errMsg, err)
	}
}

// FormatError returns a user-friendly error message
func FormatError(err error) string {
	if err == nil {
		return ""
	}

	cliErr := CategorizeError(err)
	var sb strings.Builder

	// Format the main error message
	sb.WriteString("❌ Error")
	if cliErr.Type != ErrorTypeUnknown {
		sb.WriteString(" (")
		sb.WriteString(string(cliErr.Type))
		sb.WriteString(")")
	}
	sb.WriteString(": ")
	sb.WriteString(cliErr.Message)
	sb.WriteString("\n")

	// Add suggestion if available
	if cliErr.HasSuggestion() {
		sb.WriteString("\n💡 Suggestion: ")
		sb.WriteString(cliErr.Suggestion)
		sb.WriteString("\n")
	}

	// Add retry info for rate limiting
	if cliErr.Type == ErrorTypeRateLimit && cliErr.RetryAfter > 0 {
		sb.WriteString("\n⏱️  Retry in: ")
		sb.WriteString(fmt.Sprintf("%d seconds\n", cliErr.RetryAfter))
	}

	return sb.String()
}
