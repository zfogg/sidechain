package util

import (
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// HandleDBError handles database errors and sends appropriate HTTP responses
// Returns true if the error was handled (and response was sent), false otherwise
func HandleDBError(c *gin.Context, err error, resourceName string) bool {
	if err == nil {
		return false
	}

	if err == gorm.ErrRecordNotFound {
		RespondNotFound(c, resourceName)
		return true
	}

	// For other database errors, return internal server error
	RespondInternalError(c, "Failed to fetch "+resourceName)
	return true
}
