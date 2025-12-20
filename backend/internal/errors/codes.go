package errors

import "net/http"

// ErrorCode represents the type of error
type ErrorCode string

const (
	ErrNotFound         ErrorCode = "NOT_FOUND"
	ErrUnauthorized     ErrorCode = "UNAUTHORIZED"
	ErrForbidden        ErrorCode = "FORBIDDEN"
	ErrConflict         ErrorCode = "CONFLICT"
	ErrValidation       ErrorCode = "VALIDATION_ERROR"
	ErrBadRequest       ErrorCode = "BAD_REQUEST"
	ErrInternalError    ErrorCode = "INTERNAL_ERROR"
	ErrAlreadyExists    ErrorCode = "ALREADY_EXISTS"
	ErrRateLimited      ErrorCode = "RATE_LIMITED"
	ErrServiceUnavail   ErrorCode = "SERVICE_UNAVAILABLE"
	ErrTimeout          ErrorCode = "TIMEOUT"
)

// StatusCodeMap maps ErrorCode to HTTP status code
var StatusCodeMap = map[ErrorCode]int{
	ErrNotFound:       http.StatusNotFound,
	ErrUnauthorized:   http.StatusUnauthorized,
	ErrForbidden:      http.StatusForbidden,
	ErrConflict:       http.StatusConflict,
	ErrValidation:     http.StatusUnprocessableEntity,
	ErrBadRequest:     http.StatusBadRequest,
	ErrInternalError:  http.StatusInternalServerError,
	ErrAlreadyExists:  http.StatusConflict,
	ErrRateLimited:    http.StatusTooManyRequests,
	ErrServiceUnavail: http.StatusServiceUnavailable,
	ErrTimeout:        http.StatusGatewayTimeout,
}

// StatusCode returns the HTTP status code for this error code
func (e ErrorCode) StatusCode() int {
	if code, ok := StatusCodeMap[e]; ok {
		return code
	}
	return http.StatusInternalServerError
}
