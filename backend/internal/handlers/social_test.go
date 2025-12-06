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
	"github.com/zfogg/sidechain/backend/internal/models"
)

// =============================================================================
// FOLLOW/UNFOLLOW TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestFollowUserRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestFollowUserMissingTargetUserID() {
	t := suite.T()

	body := map[string]interface{}{}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/follow", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func (suite *HandlersTestSuite) TestFollowUserSelfFollow() {
	suite.T().Skip("TODO: Requires mock stream client")
}

func (suite *HandlersTestSuite) TestUnfollowUserRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestUnfollowUserMissingTargetUserID() {
	t := suite.T()

	body := map[string]interface{}{}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/unfollow", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func (suite *HandlersTestSuite) TestFollowUserByIDRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestFollowUserByIDNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("POST", "/api/users/00000000-0000-0000-0000-000000000000/follow", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestUnfollowUserByIDRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestUnfollowUserByIDNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("DELETE", "/api/users/00000000-0000-0000-0000-000000000000/follow", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// =============================================================================
// LIKE/UNLIKE TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestLikePostRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestLikePostMissingActivityID() {
	t := suite.T()

	body := map[string]interface{}{}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/like", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func (suite *HandlersTestSuite) TestUnlikePostRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestUnlikePostMissingActivityID() {
	t := suite.T()

	body := map[string]interface{}{}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("DELETE", "/api/social/like", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// =============================================================================
// EMOJI REACTION TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestEmojiReactRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestEmojiReactMissingActivityID() {
	t := suite.T()

	body := map[string]interface{}{
		"emoji": "🔥",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/react", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func (suite *HandlersTestSuite) TestEmojiReactMissingEmoji() {
	t := suite.T()

	body := map[string]interface{}{
		"activity_id": "loop:test123",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/react", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// =============================================================================
// TRACKING TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestTrackPlayRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestTrackListenDurationRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
}

func (suite *HandlersTestSuite) TestTrackListenDurationMissingFields() {
	suite.T().Skip("TODO: Requires mock stream client")
}

// =============================================================================
// HELPER: Create other user for follow tests
// =============================================================================

func (suite *HandlersTestSuite) createOtherUser() *models.User {
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("otheruser%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(suite.T(), suite.db.Create(otherUser).Error)
	return otherUser
}
