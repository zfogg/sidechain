package handlers

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// =============================================================================
// PROFILE TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestGetUserProfile() {
	t := suite.T()

	// Verify the user exists in database before making request
	var verifyUser models.User
	err := database.DB.First(&verifyUser, "id = ?", suite.testUser.ID).Error
	require.NoError(t, err, "User should exist in database.DB before API call")
	require.Equal(t, suite.testUser.ID, verifyUser.ID, "User ID should match")

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/profile", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, suite.testUser.ID, response["id"])
	assert.Equal(t, suite.testUser.Username, response["username"])
	assert.Equal(t, suite.testUser.DisplayName, response["display_name"])
	assert.Equal(t, suite.testUser.Bio, response["bio"])
}

func (suite *HandlersTestSuite) TestGetUserProfileNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/00000000-0000-0000-0000-000000000000/profile", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestGetMyProfile() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/profile", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, suite.testUser.ID, response["id"])
	assert.Equal(t, suite.testUser.Email, response["email"])
	assert.Equal(t, suite.testUser.Username, response["username"])
}

func (suite *HandlersTestSuite) TestGetMyProfileUnauthorized() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/profile", nil)
	// No X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func (suite *HandlersTestSuite) TestUpdateMyProfile() {
	t := suite.T()

	body := map[string]interface{}{
		"display_name":   "Updated Name",
		"bio":            "Updated bio",
		"location":       "New City",
		"daw_preference": "FL Studio",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/profile", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "profile_updated", response["message"])

	user := response["user"].(map[string]interface{})
	assert.Equal(t, "Updated Name", user["display_name"])
	assert.Equal(t, "Updated bio", user["bio"])
	assert.Equal(t, "New City", user["location"])
	assert.Equal(t, "FL Studio", user["daw_preference"])

	// Verify in database
	var dbUser models.User
	suite.db.First(&dbUser, "id = ?", suite.testUser.ID)
	assert.Equal(t, "Updated Name", dbUser.DisplayName)
}

func (suite *HandlersTestSuite) TestUpdateMyProfileNoFields() {
	t := suite.T()

	body := map[string]interface{}{}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/profile", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "no_fields_to_update", response["error"])
}

// =============================================================================
// USERNAME CHANGE TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestChangeUsernameSuccess() {
	t := suite.T()

	body := map[string]interface{}{
		"username": "newusername123",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/users/username", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "username_changed", response["message"])
	assert.Equal(t, "newusername123", response["username"])
	assert.Equal(t, suite.testUser.Username, response["old_username"])

	// Verify in database
	var dbUser models.User
	suite.db.First(&dbUser, "id = ?", suite.testUser.ID)
	assert.Equal(t, "newusername123", dbUser.Username)
}

func (suite *HandlersTestSuite) TestChangeUsernameTooShort() {
	t := suite.T()

	body := map[string]interface{}{
		"username": "ab",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/users/username", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "username_too_short", response["error"])
}

func (suite *HandlersTestSuite) TestChangeUsernameTooLong() {
	t := suite.T()

	body := map[string]interface{}{
		"username": "this_username_is_way_too_long_for_the_system",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/users/username", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "username_too_long", response["error"])
}

func (suite *HandlersTestSuite) TestChangeUsernameInvalidChars() {
	t := suite.T()

	body := map[string]interface{}{
		"username": "user@name!",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/users/username", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "username_invalid_chars", response["error"])
}

func (suite *HandlersTestSuite) TestChangeUsernameTaken() {
	t := suite.T()

	// Create another user with a known username
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     "takenusername",
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	body := map[string]interface{}{
		"username": "takenusername",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/users/username", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusConflict, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "username_taken", response["error"])
}

func (suite *HandlersTestSuite) TestChangeUsernameUnchanged() {
	t := suite.T()

	body := map[string]interface{}{
		"username": suite.testUser.Username,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/users/username", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "username_unchanged", response["message"])
}

// =============================================================================
// USER FOLLOWERS/FOLLOWING TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestGetUserFollowersRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetUserFollowingRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetUserFollowersNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/00000000-0000-0000-0000-000000000000/followers", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestGetUserFollowingNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/00000000-0000-0000-0000-000000000000/following", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestGetUserFollowersPagination() {
	suite.T().Skip("TODO: Requires mock stream client")
}

func (suite *HandlersTestSuite) TestGetUserActivitySummaryRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestGetUserActivitySummaryNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/00000000-0000-0000-0000-000000000000/activity", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}
