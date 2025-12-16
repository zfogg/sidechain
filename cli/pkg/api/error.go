package api

import (
	"fmt"
	"io"

	"github.com/go-resty/resty/v2"
	json "github.com/json-iterator/go"
)

// APIError represents an API error response
type APIError struct {
	Code       string
	Message    string
	StatusCode int
	Details    map[string]interface{}
}

func (e *APIError) Error() string {
	if e.Details != nil {
		return fmt.Sprintf("[%d] %s: %s (details: %v)", e.StatusCode, e.Code, e.Message, e.Details)
	}
	return fmt.Sprintf("[%d] %s: %s", e.StatusCode, e.Code, e.Message)
}

// ParseError parses an error response from the API
func ParseError(resp *resty.Response) error {
	statusCode := resp.StatusCode()

	// Try to parse as JSON error response
	var errResp ErrorResponse
	if err := json.Unmarshal(resp.Body(), &errResp); err == nil && errResp.Code != "" {
		return &APIError{
			Code:       errResp.Code,
			Message:    errResp.Message,
			StatusCode: statusCode,
			Details:    errResp.Details,
		}
	}

	// Fallback to generic error
	return &APIError{
		Code:       "unknown_error",
		Message:    string(resp.Body()),
		StatusCode: statusCode,
	}
}

// IsUnauthorized checks if error is due to missing/invalid authentication
func IsUnauthorized(err error) bool {
	if apiErr, ok := err.(*APIError); ok {
		return apiErr.StatusCode == 401
	}
	return false
}

// IsForbidden checks if error is due to insufficient permissions
func IsForbidden(err error) bool {
	if apiErr, ok := err.(*APIError); ok {
		return apiErr.StatusCode == 403
	}
	return false
}

// IsNotFound checks if error is due to resource not found
func IsNotFound(err error) bool {
	if apiErr, ok := err.(*APIError); ok {
		return apiErr.StatusCode == 404
	}
	return false
}

// IsServerError checks if error is due to server error (5xx)
func IsServerError(err error) bool {
	if apiErr, ok := err.(*APIError); ok {
		return apiErr.StatusCode >= 500
	}
	return false
}

// CheckResponse checks if response is successful and returns error if not
func CheckResponse(resp *resty.Response, err error) error {
	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return ParseError(resp)
	}

	return nil
}

// ParseResponseBody parses response body into target interface
func ParseResponseBody(body []byte, target interface{}) error {
	return json.Unmarshal(body, target)
}

// ReadResponseBody reads and closes response body
func ReadResponseBody(resp *resty.Response) ([]byte, error) {
	defer func() {
		if resp.RawResponse != nil && resp.RawResponse.Body != nil {
			io.ReadAll(resp.RawResponse.Body)
			resp.RawResponse.Body.Close()
		}
	}()
	return resp.Body(), nil
}
