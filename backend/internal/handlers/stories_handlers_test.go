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
// STORY ENDPOINT TESTS (Phase 7.5)
// =============================================================================

func (suite *HandlersTestSuite) TestCreateStorySuccess() {
	t := suite.T()

	body := map[string]any{
		"audio_url":      "https://cdn.example.com/story-audio.mp3",
		"audio_duration": 30.5,
		"bpm":            128,
		"key":            "C minor",
		"genre":          []string{"Electronic", "House"},
		"midi_data": map[string]any{
			"events": []map[string]any{
				{"time": 0.0, "type": "note_on", "note": 60, "velocity": 100, "channel": 0},
				{"time": 0.5, "type": "note_off", "note": 60, "channel": 0},
			},
			"total_time": 30.5,
			"tempo":      128,
		},
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/stories", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code, "Response body: %s", w.Body.String())

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotEmpty(t, response["story_id"])
	assert.NotEmpty(t, response["expires_at"])
	assert.Equal(t, "Story created successfully", response["message"])

	// Verify story was created in database
	var story models.Story
	err := suite.db.First(&story, "id = ?", response["story_id"]).Error
	require.NoError(t, err)
	assert.Equal(t, suite.testUser.ID, story.UserID)
	assert.Equal(t, 30.5, story.AudioDuration)
	assert.Equal(t, 128, *story.BPM)
	assert.Equal(t, "C minor", *story.Key)
}

func (suite *HandlersTestSuite) TestCreateStoryDurationTooShort() {
	t := suite.T()

	body := map[string]any{
		"audio_url":      "https://cdn.example.com/story-audio.mp3",
		"audio_duration": 3.0, // Less than 5 seconds
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/stories", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "invalid_duration", response["error"])
}

func (suite *HandlersTestSuite) TestCreateStoryDurationTooLong() {
	t := suite.T()

	body := map[string]any{
		"audio_url":      "https://cdn.example.com/story-audio.mp3",
		"audio_duration": 75.0, // More than 60 seconds
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/stories", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "invalid_duration", response["error"])
}

func (suite *HandlersTestSuite) TestCreateStoryMissingAudioURL() {
	t := suite.T()

	body := map[string]any{
		"audio_duration": 30.0,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/stories", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "invalid_request", response["error"])
}

func (suite *HandlersTestSuite) TestCreateStoryUnauthorized() {
	t := suite.T()

	body := map[string]any{
		"audio_url":      "https://cdn.example.com/story-audio.mp3",
		"audio_duration": 30.0,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/stories", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	// No X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func (suite *HandlersTestSuite) TestCreateStoryAudioOnlyNoMIDI() {
	t := suite.T()

	body := map[string]any{
		"audio_url":      "https://cdn.example.com/story-audio.mp3",
		"audio_duration": 25.0,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/stories", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.NotEmpty(t, response["story_id"])
}

// createTestStory is a helper to create stories for testing
func (suite *HandlersTestSuite) createTestStory(userID string, duration float64, expiresAt time.Time) *models.Story {
	bpm := 128
	key := "C major"
	story := &models.Story{
		UserID:        userID,
		AudioURL:      "https://cdn.example.com/test-story.mp3",
		AudioDuration: duration,
		BPM:           &bpm,
		Key:           &key,
		ExpiresAt:     expiresAt,
		ViewCount:     0,
	}
	require.NoError(suite.T(), suite.db.Create(story).Error)
	return story
}

func (suite *HandlersTestSuite) TestGetStoriesSuccess() {
	t := suite.T()

	// Create some stories for the test user
	suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))
	suite.createTestStory(suite.testUser.ID, 25.0, time.Now().UTC().Add(23*time.Hour))

	req, _ := http.NewRequest("GET", "/api/stories", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	stories := response["stories"].([]any)
	assert.Len(t, stories, 2)
	assert.Equal(t, float64(2), response["count"])
}

func (suite *HandlersTestSuite) TestGetStoriesFiltersExpired() {
	t := suite.T()

	// Create one active story and one expired story
	suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))  // Active
	suite.createTestStory(suite.testUser.ID, 25.0, time.Now().UTC().Add(-1*time.Hour)) // Expired

	req, _ := http.NewRequest("GET", "/api/stories", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	stories := response["stories"].([]any)
	assert.Len(t, stories, 1) // Only the active story
}

func (suite *HandlersTestSuite) TestGetStoriesUnauthorized() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/stories", nil)
	// No X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

func (suite *HandlersTestSuite) TestGetStorySuccess() {
	t := suite.T()

	story := suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))

	req, _ := http.NewRequest("GET", "/api/stories/"+story.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	storyResp := response["story"].(map[string]any)
	assert.Equal(t, story.ID, storyResp["id"])
	assert.Equal(t, float64(0), response["view_count"])
}

func (suite *HandlersTestSuite) TestGetStoryNotFound() {
	suite.T().Skip("TODO: Requires mock stream client")
}

func (suite *HandlersTestSuite) TestGetStoryExpired() {
	t := suite.T()

	// Create an expired story
	story := suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(-1*time.Hour))

	req, _ := http.NewRequest("GET", "/api/stories/"+story.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusGone, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "expired", response["error"])
}

func (suite *HandlersTestSuite) TestGetStoryOwnerSeesViewers() {
	t := suite.T()

	story := suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))

	// Create another user who viewed the story
	viewerID := fmt.Sprintf("%d", time.Now().UnixNano())
	viewer := &models.User{
		Email:        fmt.Sprintf("viewer_%s@test.com", viewerID),
		Username:     fmt.Sprintf("viewer%s", viewerID[:8]),
		DisplayName:  "Story Viewer",
		StreamUserID: fmt.Sprintf("stream_viewer_%s", viewerID),
	}
	require.NoError(t, suite.db.Create(viewer).Error)

	// Record a view
	view := &models.StoryView{
		StoryID:  story.ID,
		ViewerID: viewer.ID,
		ViewedAt: time.Now().UTC(),
	}
	require.NoError(t, suite.db.Create(view).Error)

	// Update view count
	suite.db.Model(story).Update("view_count", 1)

	req, _ := http.NewRequest("GET", "/api/stories/"+story.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID) // Story owner

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, float64(1), response["view_count"])
	viewers := response["viewers"].([]any)
	assert.Len(t, viewers, 1)
}

func (suite *HandlersTestSuite) TestGetUserStoriesSuccess() {
	t := suite.T()

	// Create stories for the test user
	suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))
	suite.createTestStory(suite.testUser.ID, 20.0, time.Now().UTC().Add(12*time.Hour))

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/stories", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	stories := response["stories"].([]any)
	assert.Len(t, stories, 2)
}

func (suite *HandlersTestSuite) TestGetUserStoriesEmpty() {
	t := suite.T()

	// Create another user with no stories
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("other%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	req, _ := http.NewRequest("GET", "/api/users/"+otherUser.ID+"/stories", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	stories := response["stories"].([]any)
	assert.Len(t, stories, 0)
}

func (suite *HandlersTestSuite) TestViewStorySuccess() {
	t := suite.T()

	// Create a story by another user
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("other%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	story := suite.createTestStory(otherUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))

	req, _ := http.NewRequest("POST", "/api/stories/"+story.ID+"/view", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, true, response["viewed"])
	assert.Equal(t, float64(1), response["view_count"])

	// Verify view was recorded
	var view models.StoryView
	err := suite.db.Where("story_id = ? AND viewer_id = ?", story.ID, suite.testUser.ID).First(&view).Error
	require.NoError(t, err)
}

func (suite *HandlersTestSuite) TestViewStoryDoesNotCountOwnView() {
	t := suite.T()

	story := suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))

	req, _ := http.NewRequest("POST", "/api/stories/"+story.ID+"/view", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, true, response["viewed"])
	assert.Equal(t, float64(0), response["view_count"]) // Should not increment for own story

	// Verify no view was recorded
	var count int64
	suite.db.Model(&models.StoryView{}).Where("story_id = ?", story.ID).Count(&count)
	assert.Equal(t, int64(0), count)
}

func (suite *HandlersTestSuite) TestViewStoryNotFound() {
	suite.T().Skip("TODO: Requires mock stream client")
}

func (suite *HandlersTestSuite) TestViewStoryExpired() {
	t := suite.T()

	// Create another user with an expired story
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("other%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	story := suite.createTestStory(otherUser.ID, 30.0, time.Now().UTC().Add(-1*time.Hour)) // Expired

	req, _ := http.NewRequest("POST", "/api/stories/"+story.ID+"/view", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusGone, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "expired", response["error"])
}

func (suite *HandlersTestSuite) TestViewStoryDuplicateViewNotCounted() {
	t := suite.T()

	// Create a story by another user
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("other%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	story := suite.createTestStory(otherUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))

	// First view
	req1, _ := http.NewRequest("POST", "/api/stories/"+story.ID+"/view", nil)
	req1.Header.Set("X-User-ID", suite.testUser.ID)
	w1 := httptest.NewRecorder()
	suite.router.ServeHTTP(w1, req1)
	assert.Equal(t, http.StatusOK, w1.Code)

	// Second view (should not increment count)
	req2, _ := http.NewRequest("POST", "/api/stories/"+story.ID+"/view", nil)
	req2.Header.Set("X-User-ID", suite.testUser.ID)
	w2 := httptest.NewRecorder()
	suite.router.ServeHTTP(w2, req2)
	assert.Equal(t, http.StatusOK, w2.Code)

	var response map[string]any
	json.Unmarshal(w2.Body.Bytes(), &response)

	// View count should still be 1 after duplicate view attempt
	assert.Equal(t, float64(1), response["view_count"])
}

func (suite *HandlersTestSuite) TestGetStoryViewsSuccess() {
	t := suite.T()

	story := suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))

	// Create viewers
	for i := 0; i < 3; i++ {
		viewerID := fmt.Sprintf("%d_%d", time.Now().UnixNano(), i)
		viewer := &models.User{
			Email:        fmt.Sprintf("viewer_%s@test.com", viewerID),
			Username:     fmt.Sprintf("viewer%d", i),
			DisplayName:  fmt.Sprintf("Viewer %d", i),
			StreamUserID: fmt.Sprintf("stream_viewer_%s", viewerID),
		}
		require.NoError(t, suite.db.Create(viewer).Error)

		view := &models.StoryView{
			StoryID:  story.ID,
			ViewerID: viewer.ID,
			ViewedAt: time.Now().UTC(),
		}
		require.NoError(t, suite.db.Create(view).Error)
	}

	req, _ := http.NewRequest("GET", "/api/stories/"+story.ID+"/views", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID) // Story owner

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	viewers := response["viewers"].([]any)
	assert.Len(t, viewers, 3)
	assert.Equal(t, float64(3), response["view_count"])
}

func (suite *HandlersTestSuite) TestGetStoryViewsNotOwner() {
	t := suite.T()

	// Create a story by another user
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("other%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	story := suite.createTestStory(otherUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))

	req, _ := http.NewRequest("GET", "/api/stories/"+story.ID+"/views", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID) // Not the story owner

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "forbidden", response["error"])
}

func (suite *HandlersTestSuite) TestGetStoryViewsNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/stories/nonexistent-story-id/views", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestGetStoryViewsUnauthorized() {
	t := suite.T()

	story := suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))

	req, _ := http.NewRequest("GET", "/api/stories/"+story.ID+"/views", nil)
	// No X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}
