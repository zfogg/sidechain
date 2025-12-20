package errors

import (
	"encoding/json"
	"fmt"
	"net/http"
)

// APIError represents a standardized API error response
type APIError struct {
	Code    ErrorCode  `json:"code"`
	Message string     `json:"message"`
	Field   string     `json:"field,omitempty"`
	Details string     `json:"details,omitempty"`
	Status  int        `json:"-"`
}

// Error implements the error interface
func (e *APIError) Error() string {
	if e.Field != "" {
		return fmt.Sprintf("%s: %s (field: %s)", e.Code, e.Message, e.Field)
	}
	return fmt.Sprintf("%s: %s", e.Code, e.Message)
}

// MarshalJSON customizes JSON encoding
func (e *APIError) MarshalJSON() ([]byte, error) {
	type Alias APIError
	return json.Marshal(&struct {
		*Alias
	}{
		Alias: (*Alias)(e),
	})
}

// NotFound creates a NOT_FOUND error
func NotFound(resource string) *APIError {
	return &APIError{
		Code:    ErrNotFound,
		Message: fmt.Sprintf("%s not found", resource),
		Status:  http.StatusNotFound,
	}
}

// Unauthorized creates an UNAUTHORIZED error
func Unauthorized(message string) *APIError {
	return &APIError{
		Code:    ErrUnauthorized,
		Message: message,
		Status:  http.StatusUnauthorized,
	}
}

// Forbidden creates a FORBIDDEN error
func Forbidden(message string) *APIError {
	return &APIError{
		Code:    ErrForbidden,
		Message: message,
		Status:  http.StatusForbidden,
	}
}

// Conflict creates a CONFLICT error
func Conflict(resource string) *APIError {
	return &APIError{
		Code:    ErrConflict,
		Message: fmt.Sprintf("%s already exists or is in an invalid state", resource),
		Status:  http.StatusConflict,
	}
}

// ValidationError creates a VALIDATION_ERROR
func ValidationError(field, message string) *APIError {
	return &APIError{
		Code:    ErrValidation,
		Message: message,
		Field:   field,
		Status:  http.StatusUnprocessableEntity,
	}
}

// BadRequest creates a BAD_REQUEST error
func BadRequest(message string) *APIError {
	return &APIError{
		Code:    ErrBadRequest,
		Message: message,
		Status:  http.StatusBadRequest,
	}
}

// InternalError creates an INTERNAL_ERROR
func InternalError(message string) *APIError {
	return &APIError{
		Code:    ErrInternalError,
		Message: message,
		Status:  http.StatusInternalServerError,
	}
}

// AlreadyExists creates an ALREADY_EXISTS error
func AlreadyExists(resource string) *APIError {
	return &APIError{
		Code:    ErrAlreadyExists,
		Message: fmt.Sprintf("%s already exists", resource),
		Status:  http.StatusConflict,
	}
}

// RateLimited creates a RATE_LIMITED error
func RateLimited(message string) *APIError {
	if message == "" {
		message = "rate limit exceeded"
	}
	return &APIError{
		Code:    ErrRateLimited,
		Message: message,
		Status:  http.StatusTooManyRequests,
	}
}

// ServiceUnavailable creates a SERVICE_UNAVAILABLE error
func ServiceUnavailable(service string) *APIError {
	return &APIError{
		Code:    ErrServiceUnavail,
		Message: fmt.Sprintf("%s is temporarily unavailable", service),
		Status:  http.StatusServiceUnavailable,
	}
}

// Timeout creates a TIMEOUT error
func Timeout(operation string) *APIError {
	return &APIError{
		Code:    ErrTimeout,
		Message: fmt.Sprintf("%s timed out", operation),
		Status:  http.StatusGatewayTimeout,
	}
}

// WithDetails adds additional details to an error
func (e *APIError) WithDetails(details string) *APIError {
	e.Details = details
	return e
}
