package handlers

import (
	"net/http"
	"net/http/httptest"

	"github.com/stretchr/testify/assert"
)

// =============================================================================
// AUDIO ENDPOINT TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestUploadAudioUnauthorized() {
	t := suite.T()

	req, _ := http.NewRequest("POST", "/api/audio/upload", nil)
	// No X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func (suite *HandlersTestSuite) TestGetAudioProcessingStatusNotFound() {
	suite.T().Skip("TODO: Requires mock stream client")
}

func (suite *HandlersTestSuite) TestGetAudioNotFound() {
	suite.T().Skip("TODO: Requires mock stream client")
}
