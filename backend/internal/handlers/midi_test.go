package handlers

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// =============================================================================
// MIDI PATTERN ENDPOINT TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestCreateMIDIPatternSuccess() {
	t := suite.T()

	body := map[string]any{
		"name":        "Test Pattern",
		"description": "A test MIDI pattern",
		"events": []map[string]any{
			{"time": 0.0, "type": "note_on", "note": 60, "velocity": 100, "channel": 0},
			{"time": 0.5, "type": "note_off", "note": 60, "velocity": 0, "channel": 0},
			{"time": 0.5, "type": "note_on", "note": 64, "velocity": 80, "channel": 0},
			{"time": 1.0, "type": "note_off", "note": 64, "velocity": 0, "channel": 0},
		},
		"total_time":     2.0,
		"tempo":          120,
		"time_signature": []int{4, 4},
		"key":            "C major",
		"is_public":      true,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/midi", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code, "Response body: %s", w.Body.String())

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotEmpty(t, response["id"])
	assert.Equal(t, "Test Pattern", response["name"])
	assert.NotEmpty(t, response["download_url"])

	// Verify pattern was created in database
	var pattern models.MIDIPattern
	err := suite.db.First(&pattern, "id = ?", response["id"]).Error
	require.NoError(t, err)
	assert.Equal(t, suite.testUser.ID, pattern.UserID)
	assert.Equal(t, "Test Pattern", pattern.Name)
	assert.Equal(t, 120, pattern.Tempo)
	assert.Len(t, pattern.Events, 4)
}

func (suite *HandlersTestSuite) TestCreateMIDIPatternInvalidTempo() {
	t := suite.T()

	body := map[string]any{
		"events": []map[string]any{
			{"time": 0.0, "type": "note_on", "note": 60, "velocity": 100, "channel": 0},
		},
		"total_time":     1.0,
		"tempo":          0, // Invalid tempo
		"time_signature": []int{4, 4},
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/midi", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func (suite *HandlersTestSuite) TestCreateMIDIPatternInvalidTimeSignature() {
	t := suite.T()

	body := map[string]any{
		"events": []map[string]any{
			{"time": 0.0, "type": "note_on", "note": 60, "velocity": 100, "channel": 0},
		},
		"total_time":     1.0,
		"tempo":          120,
		"time_signature": []int{4}, // Invalid - should be [num, denom]
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/midi", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "invalid_time_signature", response["error"])
}

func (suite *HandlersTestSuite) TestGetMIDIPatternSuccess() {
	t := suite.T()

	// Create a pattern first
	pattern := &models.MIDIPattern{
		UserID:        suite.testUser.ID,
		Name:          "Get Test Pattern",
		Events:        []models.MIDIEvent{{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0}},
		TotalTime:     1.0,
		Tempo:         120,
		TimeSignature: []int{4, 4},
		IsPublic:      true,
	}
	err := suite.db.Create(pattern).Error
	require.NoError(t, err)

	req, _ := http.NewRequest("GET", "/api/midi/"+pattern.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, pattern.ID, response["id"])
	assert.Equal(t, "Get Test Pattern", response["name"])
	assert.Equal(t, float64(120), response["tempo"])
}

func (suite *HandlersTestSuite) TestGetMIDIPatternNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/midi/00000000-0000-0000-0000-000000000000", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestGetMIDIPatternPrivateAccessDenied() {
	t := suite.T()

	// Create another user with unique identifiers
	otherUser := &models.User{
		Email:        "private_test_user@test.com",
		Username:     "private_test_user",
		StreamUserID: "stream_private_test",
	}
	err := suite.db.Create(otherUser).Error
	require.NoError(t, err, "Failed to create other user")

	// Create a private pattern owned by other user
	pattern := &models.MIDIPattern{
		UserID:        otherUser.ID,
		Name:          "Private Pattern",
		Events:        []models.MIDIEvent{{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0}},
		TotalTime:     1.0,
		Tempo:         120,
		TimeSignature: []int{4, 4},
	}
	suite.db.Create(pattern)
	// Explicitly set to private (GORM ignores false due to default:true)
	suite.db.Model(pattern).Update("is_public", false)

	// Verify pattern was created correctly
	require.NotEmpty(t, pattern.ID, "Pattern ID should not be empty")
	require.NotEmpty(t, otherUser.ID, "Other user ID should not be empty")
	require.NotEqual(t, suite.testUser.ID, otherUser.ID, "Test user and other user should have different IDs")
	t.Logf("Pattern owner: %s, Test user: %s, Pattern public: %v", pattern.UserID, suite.testUser.ID, pattern.IsPublic)

	// Try to access with different user
	req, _ := http.NewRequest("GET", "/api/midi/"+pattern.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	t.Logf("Response: %s", w.Body.String())
	assert.Equal(t, http.StatusForbidden, w.Code)
}

func (suite *HandlersTestSuite) TestGetMIDIPatternFileSuccess() {
	t := suite.T()

	// Create a pattern with some notes
	pattern := &models.MIDIPattern{
		UserID: suite.testUser.ID,
		Name:   "Download Test",
		Events: []models.MIDIEvent{
			{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0},
			{Time: 0.5, Type: "note_off", Note: 60, Velocity: 0, Channel: 0},
			{Time: 0.5, Type: "note_on", Note: 64, Velocity: 80, Channel: 0},
			{Time: 1.0, Type: "note_off", Note: 64, Velocity: 0, Channel: 0},
		},
		TotalTime:     1.0,
		Tempo:         120,
		TimeSignature: []int{4, 4},
		IsPublic:      true,
	}
	err := suite.db.Create(pattern).Error
	require.NoError(t, err)

	req, _ := http.NewRequest("GET", "/api/midi/"+pattern.ID+"/file", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())
	assert.Equal(t, "audio/midi", w.Header().Get("Content-Type"))
	assert.Contains(t, w.Header().Get("Content-Disposition"), "Download Test.mid")

	// Verify it's a valid MIDI file (starts with "MThd")
	assert.True(t, bytes.HasPrefix(w.Body.Bytes(), []byte("MThd")), "Should be a valid MIDI file")

	// Verify download count was incremented
	var updatedPattern models.MIDIPattern
	suite.db.First(&updatedPattern, "id = ?", pattern.ID)
	assert.Equal(t, 1, updatedPattern.DownloadCount)
}

func (suite *HandlersTestSuite) TestListMIDIPatternsSuccess() {
	t := suite.T()

	// Create multiple patterns
	for i := 0; i < 3; i++ {
		pattern := &models.MIDIPattern{
			UserID:        suite.testUser.ID,
			Name:          "Pattern " + string(rune('A'+i)),
			Events:        []models.MIDIEvent{{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0}},
			TotalTime:     1.0,
			Tempo:         120,
			TimeSignature: []int{4, 4},
			IsPublic:      true,
		}
		suite.db.Create(pattern)
	}

	req, _ := http.NewRequest("GET", "/api/midi", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	patterns, ok := response["patterns"].([]any)
	require.True(t, ok, "patterns should be an array")
	assert.Len(t, patterns, 3)

	meta, ok := response["meta"].(map[string]any)
	require.True(t, ok, "meta should be a map")
	assert.Equal(t, float64(3), meta["total"])
}

func (suite *HandlersTestSuite) TestDeleteMIDIPatternSuccess() {
	t := suite.T()

	pattern := &models.MIDIPattern{
		UserID:        suite.testUser.ID,
		Name:          "Delete Me",
		Events:        []models.MIDIEvent{{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0}},
		TotalTime:     1.0,
		Tempo:         120,
		TimeSignature: []int{4, 4},
		IsPublic:      true,
	}
	suite.db.Create(pattern)

	req, _ := http.NewRequest("DELETE", "/api/midi/"+pattern.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	// Verify it's soft-deleted
	var deletedPattern models.MIDIPattern
	err := suite.db.Unscoped().First(&deletedPattern, "id = ?", pattern.ID).Error
	require.NoError(t, err)
	assert.NotNil(t, deletedPattern.DeletedAt)
}

func (suite *HandlersTestSuite) TestDeleteMIDIPatternNotOwner() {
	t := suite.T()

	// Create another user with unique identifiers
	otherUser := &models.User{
		Email:        "delete_test_owner@test.com",
		Username:     "delete_test_owner",
		StreamUserID: "stream_delete_test",
	}
	err := suite.db.Create(otherUser).Error
	require.NoError(t, err, "Failed to create other user")

	// Create pattern owned by other user
	pattern := &models.MIDIPattern{
		UserID:        otherUser.ID,
		Name:          "Not Yours",
		Events:        []models.MIDIEvent{{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0}},
		TotalTime:     1.0,
		Tempo:         120,
		TimeSignature: []int{4, 4},
		IsPublic:      true,
	}
	suite.db.Create(pattern)

	// Try to delete as different user
	req, _ := http.NewRequest("DELETE", "/api/midi/"+pattern.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

func (suite *HandlersTestSuite) TestUpdateMIDIPatternSuccess() {
	t := suite.T()

	pattern := &models.MIDIPattern{
		UserID:        suite.testUser.ID,
		Name:          "Original Name",
		Description:   "Original desc",
		Events:        []models.MIDIEvent{{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0}},
		TotalTime:     1.0,
		Tempo:         120,
		TimeSignature: []int{4, 4},
		IsPublic:      true,
	}
	suite.db.Create(pattern)

	body := map[string]any{
		"name":        "Updated Name",
		"description": "Updated description",
		"is_public":   false,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PATCH", "/api/midi/"+pattern.ID, bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code, "Response body: %s", w.Body.String())

	// Verify updates
	var updatedPattern models.MIDIPattern
	suite.db.First(&updatedPattern, "id = ?", pattern.ID)
	assert.Equal(t, "Updated Name", updatedPattern.Name)
	assert.Equal(t, "Updated description", updatedPattern.Description)
	assert.False(t, updatedPattern.IsPublic)
}

// =============================================================================
// MIDI FILE GENERATION TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestMIDIFileRoundTrip() {
	t := suite.T()

	// Create MIDI data
	originalData := &models.MIDIData{
		Events: []models.MIDIEvent{
			{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0},
			{Time: 0.5, Type: "note_off", Note: 60, Velocity: 0, Channel: 0},
			{Time: 0.5, Type: "note_on", Note: 64, Velocity: 80, Channel: 0},
			{Time: 1.0, Type: "note_off", Note: 64, Velocity: 0, Channel: 0},
			{Time: 1.0, Type: "note_on", Note: 67, Velocity: 90, Channel: 0},
			{Time: 1.5, Type: "note_off", Note: 67, Velocity: 0, Channel: 0},
		},
		TotalTime:     1.5,
		Tempo:         120,
		TimeSignature: []int{4, 4},
	}

	// Generate MIDI file
	midiBytes, err := GenerateMIDIFile(originalData, "Test")
	require.NoError(t, err)
	assert.NotEmpty(t, midiBytes)

	// Parse it back
	parsedData, err := ParseMIDIFile(midiBytes)
	require.NoError(t, err)

	// Verify parsed data matches original
	assert.Equal(t, originalData.Tempo, parsedData.Tempo)
	assert.Equal(t, originalData.TimeSignature, parsedData.TimeSignature)
	assert.Len(t, parsedData.Events, len(originalData.Events))

	// Verify events (allow small timing differences due to quantization)
	for i, origEvent := range originalData.Events {
		parsedEvent := parsedData.Events[i]
		assert.Equal(t, origEvent.Type, parsedEvent.Type)
		assert.Equal(t, origEvent.Note, parsedEvent.Note)
		assert.Equal(t, origEvent.Channel, parsedEvent.Channel)
		// Allow 1ms timing tolerance
		assert.InDelta(t, origEvent.Time, parsedEvent.Time, 0.01)
	}
}
