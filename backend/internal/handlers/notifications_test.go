package handlers

import (
	"net/http"
	"net/http/httptest"

	"github.com/stretchr/testify/assert"
)

// =============================================================================
// NOTIFICATION ENDPOINT TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestGetNotificationsRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetNotificationsUnauthorized() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/notifications", nil)
	// No X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func (suite *HandlersTestSuite) TestGetNotificationCountsRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestMarkNotificationsReadRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestMarkNotificationsSeenRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}
