package handlers

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/stream"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// FeedTestSuite tests feed handlers with mocked Stream.io client
type FeedTestSuite struct {
	suite.Suite
	db         *gorm.DB
	router     *gin.Engine
	handlers   *Handlers
	mockStream *stream.MockStreamClient
	testUser   *models.User
}

// SetupSuite initializes test database and handlers with mock Stream client
func (suite *FeedTestSuite) SetupSuite() {
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
		suite.T().Skipf("Skipping feed tests: database not available (%v)", err)
		return
	}

	database.DB = db

	// Create all test tables
	err = db.AutoMigrate(
		&models.User{},
		&models.OAuthProvider{},
		&models.AudioPost{},
		&models.Comment{},
		&models.CommentMention{},
		&models.Story{},
		&models.StoryView{},
	)
	require.NoError(suite.T(), err)

	suite.db = db

	// Create mock Stream client
	suite.mockStream = stream.NewMockStreamClient()

	// Create handlers with mock stream client
	suite.handlers = NewHandlers(suite.mockStream, nil)

	gin.SetMode(gin.TestMode)
	suite.router = gin.New()
	suite.setupRoutes()
}

// setupRoutes configures the test router
func (suite *FeedTestSuite) setupRoutes() {
	authMiddleware := func(c *gin.Context) {
		userID := c.GetHeader("X-User-ID")
		if userID == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
			c.Abort()
			return
		}
		c.Set("user_id", userID)

		var user models.User
		if err := database.DB.First(&user, "id = ?", userID).Error; err == nil {
			c.Set("user", &user)
		} else {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
			c.Abort()
			return
		}
		c.Next()
	}

	api := suite.router.Group("/api")
	api.Use(authMiddleware)

	// Feed routes
	api.GET("/feed/timeline", suite.handlers.GetTimeline)
	api.GET("/feed/timeline/enriched", suite.handlers.GetEnrichedTimeline)
	api.GET("/feed/timeline/aggregated", suite.handlers.GetAggregatedTimeline)
	api.GET("/feed/global", suite.handlers.GetGlobalFeed)
	api.GET("/feed/global/enriched", suite.handlers.GetEnrichedGlobalFeed)
	api.GET("/feed/trending", suite.handlers.GetTrendingFeed)
	api.POST("/feed/post", suite.handlers.CreatePost)

	// Social routes
	api.POST("/social/follow", suite.handlers.FollowUser)
	api.POST("/social/unfollow", suite.handlers.UnfollowUser)
	api.POST("/social/like", suite.handlers.LikePost)
	api.DELETE("/social/like", suite.handlers.UnlikePost)
	api.POST("/social/react", suite.handlers.EmojiReact)

	// User follower routes
	api.GET("/users/:id/followers", suite.handlers.GetUserFollowers)
	api.GET("/users/:id/following", suite.handlers.GetUserFollowing)
	api.GET("/users/:id/activity", suite.handlers.GetUserActivitySummary)
	api.POST("/users/:id/follow", suite.handlers.FollowUserByID)
	api.DELETE("/users/:id/follow", suite.handlers.UnfollowUserByID)

	// Notification routes
	api.GET("/notifications", suite.handlers.GetNotifications)
	api.GET("/notifications/counts", suite.handlers.GetNotificationCounts)
	api.POST("/notifications/read", suite.handlers.MarkNotificationsRead)
	api.POST("/notifications/seen", suite.handlers.MarkNotificationsSeen)
}

// TearDownSuite cleans up
func (suite *FeedTestSuite) TearDownSuite() {
	sqlDB, _ := suite.db.DB()
	sqlDB.Close()
}

// SetupTest creates fresh test data before each test
func (suite *FeedTestSuite) SetupTest() {
	suite.db.Exec("TRUNCATE TABLE story_views, stories RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE comment_mentions, comments, audio_posts RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE users RESTART IDENTITY CASCADE")

	// Reset mock client
	suite.mockStream.Reset()

	testID := fmt.Sprintf("%d", time.Now().UnixNano())
	suite.testUser = &models.User{
		Email:          fmt.Sprintf("feedtest_%s@test.com", testID),
		Username:       fmt.Sprintf("feedtest_%s", testID[:12]),
		DisplayName:    "Feed Test User",
		StreamUserID:   fmt.Sprintf("stream_feed_%s", testID),
		Bio:            "Test bio",
		Location:       "Test City",
		DAWPreference:  "Ableton",
		FollowerCount:  10,
		FollowingCount: 5,
		PostCount:      3,
	}
	err := suite.db.Create(suite.testUser).Error
	require.NoError(suite.T(), err, "Failed to create test user")
}

// =============================================================================
// TIMELINE TESTS
// =============================================================================

func (suite *FeedTestSuite) TestGetTimeline() {
	t := suite.T()

	// Configure mock to return sample activities
	suite.mockStream.GetUserTimelineFunc = func(userID string, limit, offset int) ([]*stream.Activity, error) {
		return []*stream.Activity{
			{
				ID:       "activity_1",
				Actor:    "user:123",
				Verb:     "posted",
				Object:   "loop:abc",
				AudioURL: "https://cdn.example.com/loop1.mp3",
				BPM:      120,
				Key:      "Am",
			},
			{
				ID:       "activity_2",
				Actor:    "user:456",
				Verb:     "posted",
				Object:   "loop:def",
				AudioURL: "https://cdn.example.com/loop2.mp3",
				BPM:      140,
				Key:      "Fm",
			},
		}, nil
	}

	req, _ := http.NewRequest("GET", "/api/feed/timeline?limit=10&offset=0", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	activities := response["activities"].([]interface{})
	assert.Len(t, activities, 2)

	// Verify mock was called with correct parameters
	assert.True(t, suite.mockStream.AssertCalled("GetUserTimeline"))
	calls := suite.mockStream.GetCallsForMethod("GetUserTimeline")
	assert.Len(t, calls, 1)
	assert.Equal(t, suite.testUser.StreamUserID, calls[0].Args[0])
}

func (suite *FeedTestSuite) TestGetTimelineEmpty() {
	t := suite.T()

	// Mock returns empty activities
	suite.mockStream.GetUserTimelineFunc = func(userID string, limit, offset int) ([]*stream.Activity, error) {
		return []*stream.Activity{}, nil
	}

	req, _ := http.NewRequest("GET", "/api/feed/timeline", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	activities := response["activities"].([]interface{})
	assert.Len(t, activities, 0)
}

func (suite *FeedTestSuite) TestGetTimelineUnauthorized() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/feed/timeline", nil)
	// No X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

// =============================================================================
// GLOBAL FEED TESTS
// =============================================================================

func (suite *FeedTestSuite) TestGetGlobalFeed() {
	t := suite.T()

	suite.mockStream.GetGlobalFeedFunc = func(limit, offset int) ([]*stream.Activity, error) {
		return []*stream.Activity{
			{ID: "global_1", Actor: "user:100", Verb: "posted", Object: "loop:xyz", BPM: 128},
			{ID: "global_2", Actor: "user:200", Verb: "posted", Object: "loop:uvw", BPM: 90},
		}, nil
	}

	req, _ := http.NewRequest("GET", "/api/feed/global?limit=20&offset=0", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	activities := response["activities"].([]interface{})
	assert.Len(t, activities, 2)

	assert.True(t, suite.mockStream.AssertCalled("GetGlobalFeed"))
}

func (suite *FeedTestSuite) TestGetGlobalFeedPagination() {
	t := suite.T()

	// Track what parameters were passed
	var capturedLimit, capturedOffset int
	suite.mockStream.GetGlobalFeedFunc = func(limit, offset int) ([]*stream.Activity, error) {
		capturedLimit = limit
		capturedOffset = offset
		return []*stream.Activity{}, nil
	}

	req, _ := http.NewRequest("GET", "/api/feed/global?limit=50&offset=25", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Equal(t, 50, capturedLimit)
	assert.Equal(t, 25, capturedOffset)
}

// =============================================================================
// CREATE POST TESTS
// =============================================================================

func (suite *FeedTestSuite) TestCreatePost() {
	t := suite.T()

	// Configure mock to return success
	suite.mockStream.CreateLoopActivityFunc = func(userID string, activity *stream.Activity) error {
		activity.ID = "new_activity_123"
		return nil
	}

	body := map[string]interface{}{
		"audio_url":     "https://cdn.example.com/newloop.mp3",
		"bpm":           128,
		"key":           "Cm",
		"duration_bars": 8,
		"genre":         []string{"house", "techno"},
		"title":         "My New Loop",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/feed/post", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "post_created", response["message"])
	assert.NotEmpty(t, response["post"])

	// Verify mock was called
	assert.True(t, suite.mockStream.AssertCalled("CreateLoopActivity"))
}

func (suite *FeedTestSuite) TestCreatePostMissingAudioURL() {
	t := suite.T()

	body := map[string]interface{}{
		"bpm": 128,
		"key": "Cm",
		// Missing audio_url
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/feed/post", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response["error"], "audio_url")
}

// =============================================================================
// SOCIAL INTERACTION TESTS
// =============================================================================

func (suite *FeedTestSuite) TestFollowUser() {
	t := suite.T()

	// Create target user
	targetUser := &models.User{
		Email:        fmt.Sprintf("target_%d@test.com", time.Now().UnixNano()),
		Username:     "targetuser",
		DisplayName:  "Target User",
		StreamUserID: fmt.Sprintf("stream_target_%d", time.Now().UnixNano()),
	}
	require.NoError(t, suite.db.Create(targetUser).Error)

	suite.mockStream.FollowUserFunc = func(userID, targetUserID string) error {
		return nil
	}

	body := map[string]interface{}{
		"target_user_id": targetUser.StreamUserID,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/follow", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "followed", response["message"])

	assert.True(t, suite.mockStream.AssertCalled("FollowUser"))
}

func (suite *FeedTestSuite) TestUnfollowUser() {
	t := suite.T()

	// Create target user
	targetUser := &models.User{
		Email:        fmt.Sprintf("target2_%d@test.com", time.Now().UnixNano()),
		Username:     "targetuser2",
		DisplayName:  "Target User 2",
		StreamUserID: fmt.Sprintf("stream_target2_%d", time.Now().UnixNano()),
	}
	require.NoError(t, suite.db.Create(targetUser).Error)

	suite.mockStream.UnfollowUserFunc = func(userID, targetUserID string) error {
		return nil
	}

	body := map[string]interface{}{
		"target_user_id": targetUser.StreamUserID,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/unfollow", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "unfollowed", response["message"])

	assert.True(t, suite.mockStream.AssertCalled("UnfollowUser"))
}

func (suite *FeedTestSuite) TestLikePost() {
	t := suite.T()

	suite.mockStream.AddReactionWithNotificationFunc = func(kind, actorUserID, activityID, targetUserID, loopID, emoji string) error {
		return nil
	}

	body := map[string]interface{}{
		"activity_id":    "activity_to_like",
		"target_user_id": "user123",
		"loop_id":        "loop456",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/like", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "liked", response["message"])

	assert.True(t, suite.mockStream.AssertCalled("AddReactionWithNotification"))
}

func (suite *FeedTestSuite) TestUnlikePost() {
	t := suite.T()

	suite.mockStream.RemoveReactionByActivityAndUserFunc = func(activityID, userID, kind string) error {
		return nil
	}

	body := map[string]interface{}{
		"activity_id": "activity_to_unlike",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("DELETE", "/api/social/like", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "unliked", response["message"])

	assert.True(t, suite.mockStream.AssertCalled("RemoveReactionByActivityAndUser"))
}

// =============================================================================
// NOTIFICATION TESTS
// =============================================================================

func (suite *FeedTestSuite) TestGetNotifications() {
	t := suite.T()

	suite.mockStream.GetNotificationsFunc = func(userID string, limit, offset int) (*stream.NotificationResponse, error) {
		return &stream.NotificationResponse{
			Groups: []*stream.NotificationGroup{
				{
					ID:            "group_1",
					Verb:          "like",
					ActivityCount: 3,
					ActorCount:    2,
					IsRead:        false,
					IsSeen:        false,
				},
			},
			Unseen: 1,
			Unread: 1,
		}, nil
	}

	req, _ := http.NewRequest("GET", "/api/notifications?limit=20", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotNil(t, response["notifications"])
	assert.Equal(t, float64(1), response["unseen"])
	assert.Equal(t, float64(1), response["unread"])

	assert.True(t, suite.mockStream.AssertCalled("GetNotifications"))
}

func (suite *FeedTestSuite) TestGetNotificationCounts() {
	t := suite.T()

	suite.mockStream.GetNotificationCountsFunc = func(userID string) (unseen, unread int, err error) {
		return 5, 3, nil
	}

	req, _ := http.NewRequest("GET", "/api/notifications/counts", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, float64(5), response["unseen"])
	assert.Equal(t, float64(3), response["unread"])

	assert.True(t, suite.mockStream.AssertCalled("GetNotificationCounts"))
}

func (suite *FeedTestSuite) TestMarkNotificationsRead() {
	t := suite.T()

	suite.mockStream.MarkNotificationsReadFunc = func(userID string) error {
		return nil
	}

	req, _ := http.NewRequest("POST", "/api/notifications/read", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	assert.True(t, suite.mockStream.AssertCalled("MarkNotificationsRead"))
}

func (suite *FeedTestSuite) TestMarkNotificationsSeen() {
	t := suite.T()

	suite.mockStream.MarkNotificationsSeenFunc = func(userID string) error {
		return nil
	}

	req, _ := http.NewRequest("POST", "/api/notifications/seen", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	assert.True(t, suite.mockStream.AssertCalled("MarkNotificationsSeen"))
}

// =============================================================================
// FOLLOWER/FOLLOWING TESTS
// =============================================================================

func (suite *FeedTestSuite) TestGetUserFollowers() {
	t := suite.T()

	suite.mockStream.GetFollowersFunc = func(userID string, limit, offset int) ([]*stream.FollowRelation, error) {
		return []*stream.FollowRelation{
			{UserID: "follower_1", FeedID: "user:follower_1", TargetID: "user:" + userID},
			{UserID: "follower_2", FeedID: "user:follower_2", TargetID: "user:" + userID},
		}, nil
	}

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/followers?limit=10", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	followers := response["followers"].([]interface{})
	assert.Len(t, followers, 2)

	assert.True(t, suite.mockStream.AssertCalled("GetFollowers"))
}

func (suite *FeedTestSuite) TestGetUserFollowing() {
	t := suite.T()

	suite.mockStream.GetFollowingFunc = func(userID string, limit, offset int) ([]*stream.FollowRelation, error) {
		return []*stream.FollowRelation{
			{UserID: "following_1", FeedID: "timeline:" + userID, TargetID: "user:following_1"},
		}, nil
	}

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/following?limit=10", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	following := response["following"].([]interface{})
	assert.Len(t, following, 1)

	assert.True(t, suite.mockStream.AssertCalled("GetFollowing"))
}

// =============================================================================
// RUN SUITE
// =============================================================================

func TestFeedSuite(t *testing.T) {
	if os.Getenv("SKIP_DB_TESTS") == "true" {
		t.Skip("Skipping database tests")
	}

	suite.Run(t, new(FeedTestSuite))
}
