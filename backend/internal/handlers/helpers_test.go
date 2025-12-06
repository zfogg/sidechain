package handlers

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/stretchr/testify/assert"
)

// =============================================================================
// HELPER FUNCTION TESTS
// =============================================================================

func TestIsValidAudioFile(t *testing.T) {
	testCases := []struct {
		filename string
		expected bool
	}{
		{"loop.mp3", true},
		{"loop.wav", true},
		{"loop.aiff", true},
		{"loop.aif", true},
		{"loop.m4a", true},
		{"loop.flac", true},
		{"loop.ogg", true},
		{"loop.txt", false},
		{"loop.exe", false},
		{"loop.MP3", true}, // Case insensitive
		{"loop.WAV", true},
		{"", false},
		{"noextension", false},
	}

	for _, tc := range testCases {
		t.Run(tc.filename, func(t *testing.T) {
			result := isValidAudioFile(tc.filename)
			assert.Equal(t, tc.expected, result, "Expected %v for %s", tc.expected, tc.filename)
		})
	}
}

func TestParseInt(t *testing.T) {
	testCases := []struct {
		input        string
		defaultValue int
		expected     int
	}{
		{"123", 0, 123},
		{"", 100, 100},
		{"invalid", 50, 50},
		{"-10", 0, -10},
		{"0", 100, 0},
	}

	for _, tc := range testCases {
		t.Run(tc.input, func(t *testing.T) {
			result := parseInt(tc.input, tc.defaultValue)
			assert.Equal(t, tc.expected, result)
		})
	}
}

func TestParseFloat(t *testing.T) {
	testCases := []struct {
		input        string
		defaultValue float64
		expected     float64
	}{
		{"44100.0", 0, 44100.0},
		{"", 48000.0, 48000.0},
		{"invalid", 44100.0, 44100.0},
		{"48000", 0, 48000.0},
	}

	for _, tc := range testCases {
		t.Run(tc.input, func(t *testing.T) {
			result := parseFloat(tc.input, tc.defaultValue)
			assert.Equal(t, tc.expected, result)
		})
	}
}

func TestParseGenreArray(t *testing.T) {
	testCases := []struct {
		input    string
		expected []string
	}{
		{"Electronic", []string{"Electronic"}},
		{"", []string{}},
		{"House, Techno", []string{"House, Techno"}}, // Current impl just wraps
	}

	for _, tc := range testCases {
		t.Run(tc.input, func(t *testing.T) {
			result := parseGenreArray(tc.input)
			assert.Equal(t, tc.expected, result)
		})
	}
}

// =============================================================================
// DEVICE REGISTRATION TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestRegisterDevice() {
	t := suite.T()

	req, _ := http.NewRequest("POST", "/api/devices/register", nil)
	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotEmpty(t, response["device_id"])
	assert.Equal(t, "pending_claim", response["status"])
	assert.Contains(t, response["claim_url"], "/auth/claim/")
}

func (suite *HandlersTestSuite) TestClaimDeviceRequiresStreamClient() {
	suite.T().Skip("ClaimDevice requires getstream.io client - test in integration tests")
}

// =============================================================================
// TOKEN VERIFICATION TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestVerifyTokenValid() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/verify", nil)
	req.Header.Set("Authorization", "Bearer very_long_valid_token_12345")

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, true, response["valid"])
}

func (suite *HandlersTestSuite) TestVerifyTokenMissing() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/verify", nil)
	// No Authorization header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func (suite *HandlersTestSuite) TestVerifyTokenTooShort() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/verify", nil)
	req.Header.Set("Authorization", "short")

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, false, response["valid"])
}

// =============================================================================
// FEED ENDPOINT TESTS (Obsolete - use FeedTestSuite with mock)
// =============================================================================

func (suite *HandlersTestSuite) TestGetTimelineRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetTimelineUnauthorized() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/feed/timeline", nil)
	// No X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func (suite *HandlersTestSuite) TestGetGlobalFeedRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetGlobalFeedPagination() {
	suite.T().Skip("TODO: Requires mock stream client")
}

func (suite *HandlersTestSuite) TestCreatePostRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestCreatePostMissingAudioURL() {
	t := suite.T()

	body := map[string]interface{}{
		"bpm": 128,
		"key": "C major",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/feed/post", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func (suite *HandlersTestSuite) TestCreatePostInvalidJSON() {
	t := suite.T()

	req, _ := http.NewRequest("POST", "/api/feed/post", bytes.NewBufferString("invalid json"))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func (suite *HandlersTestSuite) TestGetEnrichedTimelineRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetEnrichedGlobalFeedRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetAggregatedTimelineRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetTrendingFeedRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}
