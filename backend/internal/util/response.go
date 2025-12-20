package util

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/errors"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
)

// ErrorResponse represents a standard error response
type ErrorResponse struct {
	Code    string `json:"code"`
	Message string `json:"message,omitempty"`
	Field   string `json:"field,omitempty"`
	Details string `json:"details,omitempty"`
}

// RespondWithAPIError sends a structured API error response
func RespondWithAPIError(c *gin.Context, apiErr *errors.APIError) {
	// Log the error
	if apiErr.Status >= http.StatusInternalServerError {
		logger.Log.Error("API error",
			zap.String("code", string(apiErr.Code)),
			zap.String("message", apiErr.Message),
			zap.String("field", apiErr.Field),
			zap.Int("status", apiErr.Status),
		)
	} else if apiErr.Status >= http.StatusBadRequest {
		logger.Log.Warn("API error",
			zap.String("code", string(apiErr.Code)),
			zap.String("message", apiErr.Message),
			zap.String("field", apiErr.Field),
		)
	}

	response := ErrorResponse{
		Code:    string(apiErr.Code),
		Message: apiErr.Message,
		Field:   apiErr.Field,
		Details: apiErr.Details,
	}
	c.JSON(apiErr.Status, response)
}

// RespondError sends a standardized error response (legacy - deprecated)
// Use RespondWithAPIError with new errors package instead
func RespondError(c *gin.Context, statusCode int, errorCode string, message ...string) {
	response := gin.H{"code": errorCode}
	if len(message) > 0 && message[0] != "" {
		response["message"] = message[0]
	}
	c.JSON(statusCode, response)
}

// RespondUnauthorized sends a 401 Unauthorized response
func RespondUnauthorized(c *gin.Context, message ...string) {
	msg := "user not authenticated"
	if len(message) > 0 && message[0] != "" {
		msg = message[0]
	}
	RespondWithAPIError(c, errors.Unauthorized(msg))
}

// RespondNotFound sends a 404 Not Found response
func RespondNotFound(c *gin.Context, resource string) {
	RespondWithAPIError(c, errors.NotFound(resource))
}

// RespondBadRequest sends a 400 Bad Request response
// Signature: RespondBadRequest(c, message) or RespondBadRequest(c, errorCode, message)
func RespondBadRequest(c *gin.Context, args ...string) {
	message := "bad request"
	if len(args) > 0 {
		// If called with (c, errorCode, message), use second arg
		if len(args) > 1 {
			message = args[1]
		} else {
			message = args[0]
		}
	}
	RespondWithAPIError(c, errors.BadRequest(message))
}

// RespondForbidden sends a 403 Forbidden response
func RespondForbidden(c *gin.Context, message ...string) {
	msg := "forbidden"
	if len(message) > 0 && message[0] != "" {
		msg = message[0]
	}
	RespondWithAPIError(c, errors.Forbidden(msg))
}

// RespondInternalError sends a 500 Internal Server Error response
// Signature: RespondInternalError(c, message) or RespondInternalError(c, errorCode, message)
func RespondInternalError(c *gin.Context, args ...string) {
	message := "internal server error"
	if len(args) > 0 {
		// If called with (c, errorCode, message), use second arg
		if len(args) > 1 {
			message = args[1]
		} else {
			message = args[0]
		}
	}
	RespondWithAPIError(c, errors.InternalError(message))
}

// RespondConflict sends a 409 Conflict response
func RespondConflict(c *gin.Context, resource string) {
	RespondWithAPIError(c, errors.Conflict(resource))
}

// RespondValidationError sends a 422 Unprocessable Entity response
func RespondValidationError(c *gin.Context, field, message string) {
	RespondWithAPIError(c, errors.ValidationError(field, message))
}
