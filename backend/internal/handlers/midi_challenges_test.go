package handlers

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// MIDIChallengeTestSuite tests MIDI challenge handlers
type MIDIChallengeTestSuite struct {
	suite.Suite
	db        *gorm.DB
	router    *gin.Engine
	handlers  *Handlers
	user1     *models.User
	user2     *models.User
	user3     *models.User
	post1     *models.AudioPost
	midi1     *models.MIDIPattern
	challenge *models.MIDIChallenge
}

// SetupSuite initializes test database and handlers
func (suite *MIDIChallengeTestSuite) SetupSuite() {
	host := getEnvOrDefault("POSTGRES_HOST", "localhost")
	port := getEnvOrDefault("POSTGRES_PORT", "5432")
	user := getEnvOrDefault("POSTGRES_USER", "postgres")
	password := getEnvOrDefault("POSTGRES_PASSWORD", "")
	dbname := getEnvOrDefault("POSTGRES_DB", "sidechain_test")

	testDSN := fmt.Sprintf("host=%s port=%s user=%s dbname=%s sslmode=disable", host, port, user, dbname)
	if password != "" {
		testDSN = fmt.Sprintf("host=%s port=%s user=%s password=%s dbname=%s sslmode=disable", host, port, user, password, dbname)
	}

	db, err := gorm.Open(postgres.Open(testDSN), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		suite.T().Skipf("Skipping MIDI challenge tests: database not available (%v)", err)
		return
	}

	database.DB = db

	// Run migrations
	err = db.AutoMigrate(
		&models.User{},
		&models.AudioPost{},
		&models.MIDIPattern{},
		&models.MIDIChallenge{},
		&models.MIDIChallengeEntry{},
		&models.MIDIChallengeVote{},
	)
	require.NoError(suite.T(), err)

	suite.db = db
	suite.handlers = NewHandlers(nil, nil)

	gin.SetMode(gin.TestMode)
	suite.router = gin.New()
	suite.setupRoutes()

	// Create test users
	suite.user1 = &models.User{
		Username: "testuser1",
		Email:    "test1@example.com",
	}
	suite.db.Create(suite.user1)

	suite.user2 = &models.User{
		Username: "testuser2",
		Email:    "test2@example.com",
	}
	suite.db.Create(suite.user2)

	suite.user3 = &models.User{
		Username: "testuser3",
		Email:    "test3@example.com",
	}
	suite.db.Create(suite.user3)

	// Create test post
	suite.post1 = &models.AudioPost{
		UserID:   suite.user1.ID,
		AudioURL: "https://example.com/audio1.mp3",
		BPM:      120,
	}
	suite.db.Create(suite.post1)

	// Create test MIDI pattern
	suite.midi1 = &models.MIDIPattern{
		UserID:        suite.user1.ID,
		Name:          "Test MIDI",
		Events:        []models.MIDIEvent{{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0}},
		TotalTime:     4.0,
		Tempo:         120,
		TimeSignature: []int{4, 4},
		Key:           "C",
		IsPublic:      true,
	}
	suite.db.Create(suite.midi1)

	// Link MIDI to post
	suite.post1.MIDIPatternID = &suite.midi1.ID
	suite.db.Save(suite.post1)

	// Create active challenge
	now := time.Now()
	bpmMin := 100
	bpmMax := 140
	suite.challenge = &models.MIDIChallenge{
		Title:       "Test Challenge",
		Description: "A test MIDI challenge",
		Constraints: models.MIDIChallengeConstraints{
			BPMMin: &bpmMin,
			BPMMax: &bpmMax,
		},
		StartDate:     now.Add(-24 * time.Hour), // Started yesterday
		EndDate:       now.Add(24 * time.Hour),  // Ends tomorrow
		VotingEndDate: now.Add(48 * time.Hour),  // Voting ends day after
	}
	suite.db.Create(suite.challenge)
}

// setupRoutes configures the test router
func (suite *MIDIChallengeTestSuite) setupRoutes() {
	authMiddleware := func(c *gin.Context) {
		userID := c.GetHeader("X-User-ID")
		if userID == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
			c.Abort()
			return
		}

		var user models.User
		if err := suite.db.First(&user, "id = ?", userID).Error; err != nil {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "user not found"})
			c.Abort()
			return
		}
		c.Set("user", &user)
		c.Next()
	}

	api := suite.router.Group("/api/v1")
	{
		// Public routes
		midiChallenges := api.Group("/midi-challenges")
		{
			midiChallenges.GET("", suite.handlers.GetMIDIChallenges)
			midiChallenges.GET("/:id", suite.handlers.GetMIDIChallenge)
			midiChallenges.GET("/:id/entries", suite.handlers.GetMIDIChallengeEntries)

			// Protected routes
			midiChallenges.Use(authMiddleware)
			midiChallenges.POST("", suite.handlers.CreateMIDIChallenge)
			midiChallenges.POST("/:id/entries", suite.handlers.CreateMIDIChallengeEntry)
			midiChallenges.POST("/:id/entries/:entry_id/vote", suite.handlers.VoteMIDIChallengeEntry)
		}
	}
}

// TearDownSuite cleans up test data
func (suite *MIDIChallengeTestSuite) TearDownSuite() {
	if suite.db != nil {
		suite.db.Exec("TRUNCATE TABLE midi_challenge_votes, midi_challenge_entries, midi_challenges, midi_patterns, audio_posts, users CASCADE")
	}
}

// TestGetMIDIChallenges tests listing challenges with status filter
func (suite *MIDIChallengeTestSuite) TestGetMIDIChallenges() {
	// Test getting all challenges
	req := httptest.NewRequest("GET", "/api/v1/midi-challenges", nil)
	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)

	var challenges []models.MIDIChallenge
	err := json.Unmarshal(w.Body.Bytes(), &challenges)
	require.NoError(suite.T(), err)
	assert.GreaterOrEqual(suite.T(), len(challenges), 1)

	// Test filtering by status=active
	req = httptest.NewRequest("GET", "/api/v1/midi-challenges?status=active", nil)
	w = httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)
	err = json.Unmarshal(w.Body.Bytes(), &challenges)
	require.NoError(suite.T(), err)
	// Should include our active challenge
	found := false
	for _, ch := range challenges {
		if ch.ID == suite.challenge.ID {
			found = true
			assert.Equal(suite.T(), models.MIDIChallengeStatusActive, ch.Status)
			break
		}
	}
	assert.True(suite.T(), found, "Active challenge should be in results")
}

// TestGetMIDIChallenge tests getting a single challenge
func (suite *MIDIChallengeTestSuite) TestGetMIDIChallenge() {
	req := httptest.NewRequest("GET", fmt.Sprintf("/api/v1/midi-challenges/%s", suite.challenge.ID), nil)
	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	require.NoError(suite.T(), err)
	assert.Contains(suite.T(), response, "challenge")
	assert.Contains(suite.T(), response, "entry_count")
}

// TestCreateMIDIChallenge tests challenge creation
func (suite *MIDIChallengeTestSuite) TestCreateMIDIChallenge() {
	now := time.Now()
	bpmMin := 100
	bpmMax := 150

	reqBody := map[string]interface{}{
		"title":       "New Challenge",
		"description": "A new test challenge",
		"constraints": map[string]interface{}{
			"bpm_min": bpmMin,
			"bpm_max": bpmMax,
		},
		"start_date":      now.Add(24 * time.Hour).Format(time.RFC3339),
		"end_date":        now.Add(48 * time.Hour).Format(time.RFC3339),
		"voting_end_date": now.Add(72 * time.Hour).Format(time.RFC3339),
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", "/api/v1/midi-challenges", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusCreated, w.Code)

	var challenge models.MIDIChallenge
	err := json.Unmarshal(w.Body.Bytes(), &challenge)
	require.NoError(suite.T(), err)
	assert.Equal(suite.T(), "New Challenge", challenge.Title)
	assert.NotNil(suite.T(), challenge.Constraints.BPMMin)
	assert.Equal(suite.T(), bpmMin, *challenge.Constraints.BPMMin)
}

// TestCreateMIDIChallengeEntry tests entry submission
func (suite *MIDIChallengeTestSuite) TestCreateMIDIChallengeEntry() {
	reqBody := map[string]interface{}{
		"post_id": suite.post1.ID,
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries", suite.challenge.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusCreated, w.Code)

	var entry models.MIDIChallengeEntry
	err := json.Unmarshal(w.Body.Bytes(), &entry)
	require.NoError(suite.T(), err)
	assert.Equal(suite.T(), suite.challenge.ID, entry.ChallengeID)
	assert.Equal(suite.T(), suite.user1.ID, entry.UserID)
	assert.Equal(suite.T(), suite.post1.ID, *entry.PostID)
}

// TestCreateMIDIChallengeEntryWithMIDIData tests entry submission with direct MIDI data
func (suite *MIDIChallengeTestSuite) TestCreateMIDIChallengeEntryWithMIDIData() {
	midiData := models.MIDIData{
		Events: []models.MIDIEvent{
			{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0},
			{Time: 1.0, Type: "note_off", Note: 60, Velocity: 0, Channel: 0},
		},
		TotalTime:     4.0,
		Tempo:         120,
		TimeSignature: []int{4, 4},
	}

	reqBody := map[string]interface{}{
		"audio_url": "https://example.com/audio2.mp3",
		"midi_data": midiData,
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries", suite.challenge.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusCreated, w.Code)

	var entry models.MIDIChallengeEntry
	err := json.Unmarshal(w.Body.Bytes(), &entry)
	require.NoError(suite.T(), err)
	assert.Equal(suite.T(), suite.challenge.ID, entry.ChallengeID)
	assert.Equal(suite.T(), suite.user2.ID, entry.UserID)
}

// TestCreateMIDIChallengeEntryDuplicate tests that users can't submit twice
func (suite *MIDIChallengeTestSuite) TestCreateMIDIChallengeEntryDuplicate() {
	// Create first entry
	entry := &models.MIDIChallengeEntry{
		ChallengeID: suite.challenge.ID,
		UserID:      suite.user1.ID,
		AudioURL:    "https://example.com/audio1.mp3",
		SubmittedAt: time.Now(),
	}
	suite.db.Create(entry)

	// Try to create duplicate
	reqBody := map[string]interface{}{
		"audio_url": "https://example.com/audio2.mp3",
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries", suite.challenge.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusConflict, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(suite.T(), "already_submitted", response["error"])
}

// TestCreateMIDIChallengeEntryConstraintValidation tests constraint validation
func (suite *MIDIChallengeTestSuite) TestCreateMIDIChallengeEntryConstraintValidation() {
	// Create challenge with BPM constraint
	now := time.Now()
	bpmMin := 120
	bpmMax := 130
	challenge := &models.MIDIChallenge{
		Title:       "BPM Constraint Challenge",
		Description: "Must be 120-130 BPM",
		Constraints: models.MIDIChallengeConstraints{
			BPMMin: &bpmMin,
			BPMMax: &bpmMax,
		},
		StartDate:     now.Add(-24 * time.Hour),
		EndDate:       now.Add(24 * time.Hour),
		VotingEndDate: now.Add(48 * time.Hour),
	}
	suite.db.Create(challenge)

	// Create MIDI with BPM outside range
	midiData := models.MIDIData{
		Events: []models.MIDIEvent{
			{Time: 0.0, Type: "note_on", Note: 60, Velocity: 100, Channel: 0},
		},
		TotalTime:     4.0,
		Tempo:         140, // Outside range
		TimeSignature: []int{4, 4},
	}

	reqBody := map[string]interface{}{
		"audio_url": "https://example.com/audio.mp3",
		"midi_data": midiData,
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries", challenge.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(suite.T(), "constraint_violation", response["error"])
	assert.Contains(suite.T(), response["message"].(string), "BPM")
}

// TestGetMIDIChallengeEntries tests listing challenge entries
func (suite *MIDIChallengeTestSuite) TestGetMIDIChallengeEntries() {
	// Create entries
	entry1 := &models.MIDIChallengeEntry{
		ChallengeID: suite.challenge.ID,
		UserID:      suite.user1.ID,
		AudioURL:    "https://example.com/audio1.mp3",
		VoteCount:   5,
		SubmittedAt: time.Now(),
	}
	suite.db.Create(entry1)

	entry2 := &models.MIDIChallengeEntry{
		ChallengeID: suite.challenge.ID,
		UserID:      suite.user2.ID,
		AudioURL:    "https://example.com/audio2.mp3",
		VoteCount:   10,
		SubmittedAt: time.Now(),
	}
	suite.db.Create(entry2)

	// Test default sort (by votes)
	req := httptest.NewRequest("GET", fmt.Sprintf("/api/v1/midi-challenges/%s/entries", suite.challenge.ID), nil)
	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)

	var entries []models.MIDIChallengeEntry
	err := json.Unmarshal(w.Body.Bytes(), &entries)
	require.NoError(suite.T(), err)
	assert.GreaterOrEqual(suite.T(), len(entries), 2)
	// Should be sorted by vote count (descending)
	if len(entries) >= 2 {
		assert.GreaterOrEqual(suite.T(), entries[0].VoteCount, entries[1].VoteCount)
	}
}

// TestVoteMIDIChallengeEntry tests voting on an entry
func (suite *MIDIChallengeTestSuite) TestVoteMIDIChallengeEntry() {
	// Create entry
	entry := &models.MIDIChallengeEntry{
		ChallengeID: suite.challenge.ID,
		UserID:      suite.user1.ID,
		AudioURL:    "https://example.com/audio1.mp3",
		VoteCount:   0,
		SubmittedAt: time.Now(),
	}
	suite.db.Create(entry)

	// Move challenge to voting period
	now := time.Now()
	suite.challenge.EndDate = now.Add(-1 * time.Hour)
	suite.challenge.VotingEndDate = now.Add(24 * time.Hour)
	suite.db.Save(suite.challenge)

	// Vote on entry
	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries/%s/vote", suite.challenge.ID, entry.ID), nil)
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)

	// Verify vote was recorded
	var vote models.MIDIChallengeVote
	err := suite.db.Where("challenge_id = ? AND entry_id = ? AND voter_id = ?", suite.challenge.ID, entry.ID, suite.user2.ID).
		First(&vote).Error
	require.NoError(suite.T(), err)

	// Verify vote count increased
	suite.db.First(&entry, entry.ID)
	assert.Equal(suite.T(), 1, entry.VoteCount)
}

// TestVoteMIDIChallengeEntryDuplicate tests that users can't vote twice
func (suite *MIDIChallengeTestSuite) TestVoteMIDIChallengeEntryDuplicate() {
	// Create entry
	entry := &models.MIDIChallengeEntry{
		ChallengeID: suite.challenge.ID,
		UserID:      suite.user1.ID,
		AudioURL:    "https://example.com/audio1.mp3",
		VoteCount:   0,
		SubmittedAt: time.Now(),
	}
	suite.db.Create(entry)

	// Move challenge to voting period
	now := time.Now()
	suite.challenge.EndDate = now.Add(-1 * time.Hour)
	suite.challenge.VotingEndDate = now.Add(24 * time.Hour)
	suite.db.Save(suite.challenge)

	// Create vote
	vote := &models.MIDIChallengeVote{
		ChallengeID: suite.challenge.ID,
		EntryID:     entry.ID,
		VoterID:     suite.user2.ID,
		VotedAt:     time.Now(),
	}
	suite.db.Create(vote)
	entry.VoteCount = 1
	suite.db.Save(entry)

	// Try to vote again
	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries/%s/vote", suite.challenge.ID, entry.ID), nil)
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusConflict, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(suite.T(), "already_voted", response["error"])
}

// TestMIDIChallengeStatusTransitions tests status calculation
func (suite *MIDIChallengeTestSuite) TestMIDIChallengeStatusTransitions() {
	now := time.Now()

	// Test upcoming challenge
	upcoming := &models.MIDIChallenge{
		Title:         "Upcoming Challenge",
		StartDate:     now.Add(24 * time.Hour),
		EndDate:       now.Add(48 * time.Hour),
		VotingEndDate: now.Add(72 * time.Hour),
	}
	assert.Equal(suite.T(), models.MIDIChallengeStatusUpcoming, upcoming.CalculateStatus())

	// Test active challenge
	active := &models.MIDIChallenge{
		Title:         "Active Challenge",
		StartDate:     now.Add(-24 * time.Hour),
		EndDate:       now.Add(24 * time.Hour),
		VotingEndDate: now.Add(48 * time.Hour),
	}
	assert.Equal(suite.T(), models.MIDIChallengeStatusActive, active.CalculateStatus())

	// Test voting challenge
	voting := &models.MIDIChallenge{
		Title:         "Voting Challenge",
		StartDate:     now.Add(-48 * time.Hour),
		EndDate:       now.Add(-1 * time.Hour),
		VotingEndDate: now.Add(24 * time.Hour),
	}
	assert.Equal(suite.T(), models.MIDIChallengeStatusVoting, voting.CalculateStatus())

	// Test ended challenge
	ended := &models.MIDIChallenge{
		Title:         "Ended Challenge",
		StartDate:     now.Add(-72 * time.Hour),
		EndDate:       now.Add(-48 * time.Hour),
		VotingEndDate: now.Add(-24 * time.Hour),
	}
	assert.Equal(suite.T(), models.MIDIChallengeStatusEnded, ended.CalculateStatus())
}

// TestCreateMIDIChallengeEntryBeforeStart tests that entries can't be submitted before challenge starts
func (suite *MIDIChallengeTestSuite) TestCreateMIDIChallengeEntryBeforeStart() {
	// Create upcoming challenge
	now := time.Now()
	challenge := &models.MIDIChallenge{
		Title:         "Future Challenge",
		StartDate:     now.Add(24 * time.Hour),
		EndDate:       now.Add(48 * time.Hour),
		VotingEndDate: now.Add(72 * time.Hour),
	}
	suite.db.Create(challenge)

	reqBody := map[string]interface{}{
		"audio_url": "https://example.com/audio.mp3",
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries", challenge.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(suite.T(), "challenge_not_started", response["error"])
}

// TestCreateMIDIChallengeEntryAfterEnd tests that entries can't be submitted after deadline
func (suite *MIDIChallengeTestSuite) TestCreateMIDIChallengeEntryAfterEnd() {
	// Create ended challenge
	now := time.Now()
	challenge := &models.MIDIChallenge{
		Title:         "Past Challenge",
		StartDate:     now.Add(-48 * time.Hour),
		EndDate:       now.Add(-24 * time.Hour),
		VotingEndDate: now.Add(-12 * time.Hour),
	}
	suite.db.Create(challenge)

	reqBody := map[string]interface{}{
		"audio_url": "https://example.com/audio.mp3",
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries", challenge.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(suite.T(), "challenge_ended", response["error"])
}

// TestVoteMIDIChallengeEntryBeforeVoting tests that voting can't happen before voting period
func (suite *MIDIChallengeTestSuite) TestVoteMIDIChallengeEntryBeforeVoting() {
	// Create entry
	entry := &models.MIDIChallengeEntry{
		ChallengeID: suite.challenge.ID,
		UserID:      suite.user1.ID,
		AudioURL:    "https://example.com/audio1.mp3",
		SubmittedAt: time.Now(),
	}
	suite.db.Create(entry)

	// Challenge is still in submission period
	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/midi-challenges/%s/entries/%s/vote", suite.challenge.ID, entry.ID), nil)
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(suite.T(), "voting_not_started", response["error"])
}

// Run the test suite
func TestMIDIChallengeTestSuite(t *testing.T) {
	suite.Run(t, new(MIDIChallengeTestSuite))
}
