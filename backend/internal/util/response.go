package util

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// ErrorResponse represents a standard error response
type ErrorResponse struct {
	Error   string `json:"error"`
	Message string `json:"message,omitempty"`
}

// RespondError sends a standardized error response
func RespondError(c *gin.Context, statusCode int, errorCode string, message ...string) {
	response := gin.H{"error": errorCode}
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
	RespondError(c, http.StatusUnauthorized, "unauthorized", msg)
}

// RespondNotFound sends a 404 Not Found response
func RespondNotFound(c *gin.Context, resource string) {
	RespondError(c, http.StatusNotFound, resource+"_not_found")
}

// RespondBadRequest sends a 400 Bad Request response
func RespondBadRequest(c *gin.Context, errorCode string, message ...string) {
	RespondError(c, http.StatusBadRequest, errorCode, message...)
}

// RespondForbidden sends a 403 Forbidden response
func RespondForbidden(c *gin.Context, errorCode string, message ...string) {
	RespondError(c, http.StatusForbidden, errorCode, message...)
}

// RespondInternalError sends a 500 Internal Server Error response
func RespondInternalError(c *gin.Context, errorCode string, message ...string) {
	RespondError(c, http.StatusInternalServerError, errorCode, message...)
}
