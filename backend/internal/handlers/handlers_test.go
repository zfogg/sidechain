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
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// HandlersTestSuite contains handler tests that don't require getstream.io
type HandlersTestSuite struct {
	suite.Suite
	db       *gorm.DB
	router   *gin.Engine
	handlers *Handlers
	testUser *models.User
}

// SetupSuite initializes test database and handlers
func (suite *HandlersTestSuite) SetupSuite() {
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
		suite.T().Skipf("Skipping handler tests: database not available (%v)", err)
		return
	}

	database.DB = db

	// Check if tables already exist (migrations already run)
	// Only run AutoMigrate if users table doesn't exist
	var count int64
	db.Raw("SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'public' AND table_name = 'users'").Scan(&count)
	if count == 0 {
		// Create all test tables - order matters for foreign keys
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
	}

	suite.db = db
	suite.handlers = NewHandlers(nil, nil)

	gin.SetMode(gin.TestMode)
	suite.router = gin.New()
	suite.setupRoutes()
}

// setupRoutes configures the test router
func (suite *HandlersTestSuite) setupRoutes() {
	// Auth middleware that sets user_id and user from header
	authMiddleware := func(c *gin.Context) {
		userID := c.GetHeader("X-User-ID")
		if userID == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
			c.Abort()
			return
		}
		c.Set("user_id", userID)

		// Also load the user object for handlers that need it
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

	// Public routes (no auth)
	suite.router.POST("/api/devices/register", suite.handlers.RegisterDevice)
	// NOTE: ClaimDevice calls stream.CreateUser() which requires stream client
	// We test it separately or skip when stream is nil
	suite.router.GET("/api/verify", suite.handlers.VerifyToken)
	suite.router.GET("/api/search/users", suite.handlers.SearchUsers)
	suite.router.GET("/api/discover/trending", suite.handlers.GetTrendingUsers)
	suite.router.GET("/api/discover/featured", suite.handlers.GetFeaturedProducers)
	suite.router.GET("/api/discover/genre/:genre", suite.handlers.GetUsersByGenre)
	suite.router.GET("/api/discover/genres", suite.handlers.GetAvailableGenres)

	// Profile routes (some public)
	suite.router.GET("/api/users/:id/profile", suite.handlers.GetUserProfile)
	suite.router.GET("/api/users/:id/similar", suite.handlers.GetSimilarUsers)

	// Authenticated routes
	api := suite.router.Group("/api")
	api.Use(authMiddleware)

	api.GET("/profile", suite.handlers.GetMyProfile)
	api.PUT("/profile", suite.handlers.UpdateMyProfile)
	api.PUT("/users/username", suite.handlers.ChangeUsername)
	api.GET("/discover/suggested", suite.handlers.GetSuggestedUsers)

	// Feed routes (require getstream.io)
	api.GET("/feed/timeline", suite.handlers.GetTimeline)
	api.GET("/feed/timeline/enriched", suite.handlers.GetEnrichedTimeline)
	api.GET("/feed/timeline/aggregated", suite.handlers.GetAggregatedTimeline)
	api.GET("/feed/global", suite.handlers.GetGlobalFeed)
	api.GET("/feed/global/enriched", suite.handlers.GetEnrichedGlobalFeed)
	api.GET("/feed/trending", suite.handlers.GetTrendingFeed)
	api.POST("/feed/post", suite.handlers.CreatePost)

	// Social routes (require getstream.io)
	api.POST("/social/follow", suite.handlers.FollowUser)
	api.POST("/social/unfollow", suite.handlers.UnfollowUser)
	api.POST("/social/like", suite.handlers.LikePost)
	api.DELETE("/social/like", suite.handlers.UnlikePost)
	api.POST("/social/react", suite.handlers.EmojiReact)
	// TODO: Implement TrackPlay and TrackListenDuration handlers when needed
	// api.POST("/social/play", suite.handlers.TrackPlay)
	// api.POST("/social/listen-duration", suite.handlers.TrackListenDuration)

	// User routes (some require getstream.io)
	api.GET("/users/:id/followers", suite.handlers.GetUserFollowers)
	api.GET("/users/:id/following", suite.handlers.GetUserFollowing)
	api.GET("/users/:id/activity", suite.handlers.GetUserActivitySummary)
	api.POST("/users/:id/follow", suite.handlers.FollowUserByID)
	api.DELETE("/users/:id/follow", suite.handlers.UnfollowUserByID)

	// Audio routes
	api.POST("/audio/upload", suite.handlers.UploadAudio)
	api.GET("/audio/status/:job_id", suite.handlers.GetAudioProcessingStatus)
	api.GET("/audio/:id", suite.handlers.GetAudio)

	// Story routes (Phase 7.5)
	api.POST("/stories", suite.handlers.CreateStory)
	api.GET("/stories", suite.handlers.GetStories)
	api.GET("/stories/:id", suite.handlers.GetStory)
	api.POST("/stories/:id/view", suite.handlers.ViewStory)
	api.GET("/stories/:id/views", suite.handlers.GetStoryViews)
	api.GET("/users/:id/stories", suite.handlers.GetUserStories)

	// Notification routes (require getstream.io)
	api.GET("/notifications", suite.handlers.GetNotifications)
	api.GET("/notifications/counts", suite.handlers.GetNotificationCounts)
	api.POST("/notifications/read", suite.handlers.MarkNotificationsRead)
	api.POST("/notifications/seen", suite.handlers.MarkNotificationsSeen)
}

// TearDownSuite cleans up - only closes connection, doesn't drop tables
// to allow other test suites to run against the same database
func (suite *HandlersTestSuite) TearDownSuite() {
	sqlDB, _ := suite.db.DB()
	sqlDB.Close()
}

// SetupTest creates fresh test data before each test
func (suite *HandlersTestSuite) SetupTest() {
	// Only truncate tables that exist from AutoMigrate
	suite.db.Exec("TRUNCATE TABLE story_views, stories RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE comment_mentions, comments, audio_posts RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE users RESTART IDENTITY CASCADE")

	testID := fmt.Sprintf("%d", time.Now().UnixNano())
	suite.testUser = &models.User{
		Email:          fmt.Sprintf("testuser_%s@test.com", testID),
		Username:       fmt.Sprintf("testuser_%s", testID[:12]),
		DisplayName:    "Test User",
		StreamUserID:   fmt.Sprintf("stream_user_%s", testID),
		Bio:            "Test bio",
		Location:       "Test City",
		DAWPreference:  "Ableton",
		FollowerCount:  10,
		FollowingCount: 5,
		PostCount:      3,
	}
	err := suite.db.Create(suite.testUser).Error
	require.NoError(suite.T(), err, "Failed to create test user")
	require.NotEmpty(suite.T(), suite.testUser.ID, "Test user ID should be populated after create")
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

// NOTE: ClaimDevice tests are skipped because they require a getstream.io client.
// The handler calls h.stream.CreateUser() which panics with nil client.
// These should be tested in integration tests with a real getstream.io connection.

func (suite *HandlersTestSuite) TestClaimDeviceRequiresStreamClient() {
	// This test documents that ClaimDevice requires getstream.io
	// In a real setup, you would mock the stream client or use integration tests
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
// USER SEARCH AND DISCOVERY TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestSearchUsers() {
	t := suite.T()

	// Create searchable users
	for i := 0; i < 5; i++ {
		user := &models.User{
			Email:        fmt.Sprintf("searchuser%d_%d@test.com", i, time.Now().UnixNano()),
			Username:     fmt.Sprintf("beatmaker%d", i),
			DisplayName:  fmt.Sprintf("Beat Maker %d", i),
			StreamUserID: fmt.Sprintf("stream_search_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(user).Error)
	}

	req, _ := http.NewRequest("GET", "/api/search/users?q=beatmaker", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	users := response["users"].([]interface{})
	assert.GreaterOrEqual(t, len(users), 5)

	meta := response["meta"].(map[string]interface{})
	assert.Equal(t, "beatmaker", meta["query"])
}

func (suite *HandlersTestSuite) TestSearchUsersMissingQuery() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/search/users", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "search_query_required", response["error"])
}

func (suite *HandlersTestSuite) TestSearchUsersPagination() {
	t := suite.T()

	// Create many users
	for i := 0; i < 30; i++ {
		user := &models.User{
			Email:        fmt.Sprintf("producer%d_%d@test.com", i, time.Now().UnixNano()),
			Username:     fmt.Sprintf("producer%d", i),
			DisplayName:  fmt.Sprintf("Producer %d", i),
			StreamUserID: fmt.Sprintf("stream_producer_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(user).Error)
	}

	req, _ := http.NewRequest("GET", "/api/search/users?q=producer&limit=10&offset=0", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	users := response["users"].([]interface{})
	assert.LessOrEqual(t, len(users), 10)

	meta := response["meta"].(map[string]interface{})
	assert.Equal(t, float64(10), meta["limit"])
	assert.Equal(t, float64(0), meta["offset"])
}

func (suite *HandlersTestSuite) TestGetTrendingUsers() {
	t := suite.T()

	// Create users with posts
	for i := 0; i < 5; i++ {
		user := &models.User{
			Email:         fmt.Sprintf("trending%d_%d@test.com", i, time.Now().UnixNano()),
			Username:      fmt.Sprintf("trendinguser%d", i),
			DisplayName:   fmt.Sprintf("Trending User %d", i),
			StreamUserID:  fmt.Sprintf("stream_trending_%d_%d", i, time.Now().UnixNano()),
			FollowerCount: (5 - i) * 100,
		}
		require.NoError(t, suite.db.Create(user).Error)
	}

	req, _ := http.NewRequest("GET", "/api/discover/trending?period=week&limit=10", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "week", response["period"])
	assert.NotNil(t, response["users"])
}

func (suite *HandlersTestSuite) TestGetFeaturedProducers() {
	t := suite.T()

	// Create featured-worthy users (high follower count, active recently, has posts)
	now := time.Now()
	for i := 0; i < 3; i++ {
		user := &models.User{
			Email:         fmt.Sprintf("featured%d_%d@test.com", i, time.Now().UnixNano()),
			Username:      fmt.Sprintf("featureduser%d", i),
			DisplayName:   fmt.Sprintf("Featured User %d", i),
			StreamUserID:  fmt.Sprintf("stream_featured_%d_%d", i, time.Now().UnixNano()),
			FollowerCount: 100 + i*50,
			PostCount:     5 + i,
			LastActiveAt:  &now,
		}
		require.NoError(t, suite.db.Create(user).Error)
	}

	req, _ := http.NewRequest("GET", "/api/discover/featured?limit=10", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotNil(t, response["users"])
}

func (suite *HandlersTestSuite) TestGetUsersByGenre() {
	t := suite.T()

	// Create users with specific genre (using raw SQL for PostgreSQL array)
	for i := 0; i < 5; i++ {
		user := &models.User{
			Email:         fmt.Sprintf("houseuser%d_%d@test.com", i, time.Now().UnixNano()),
			Username:      fmt.Sprintf("houseprod%d", i),
			DisplayName:   fmt.Sprintf("House Producer %d", i),
			StreamUserID:  fmt.Sprintf("stream_house_%d_%d", i, time.Now().UnixNano()),
			FollowerCount: i * 10,
		}
		require.NoError(t, suite.db.Create(user).Error)
		suite.db.Exec("UPDATE users SET genre = '{House,Techno}' WHERE id = ?", user.ID)
	}

	req, _ := http.NewRequest("GET", "/api/discover/genre/House", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "House", response["genre"])
	users := response["users"].([]interface{})
	assert.GreaterOrEqual(t, len(users), 5)
}

func (suite *HandlersTestSuite) TestGetAvailableGenres() {
	t := suite.T()

	// Create users with various genres (using raw SQL for PostgreSQL array)
	genres := []string{
		"{Electronic,House}",
		"{House,Techno}",
		"{Hip-Hop,Trap}",
		"{Electronic,Ambient}",
	}

	for i, g := range genres {
		user := &models.User{
			Email:        fmt.Sprintf("genreuser%d_%d@test.com", i, time.Now().UnixNano()),
			Username:     fmt.Sprintf("genreuser%d", i),
			DisplayName:  fmt.Sprintf("Genre User %d", i),
			StreamUserID: fmt.Sprintf("stream_genre_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(user).Error)
		suite.db.Exec("UPDATE users SET genre = '"+g+"' WHERE id = ?", user.ID)
	}

	req, _ := http.NewRequest("GET", "/api/discover/genres", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotNil(t, response["genres"])
}

func (suite *HandlersTestSuite) TestGetSimilarUsersNoPosts() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/similar", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "no_posts_to_analyze", response["reason"])
}

func (suite *HandlersTestSuite) TestGetSimilarUsersWithPosts() {
	t := suite.T()

	// Create posts for the test user with unique StreamActivityID
	for i := 0; i < 5; i++ {
		post := &models.AudioPost{
			UserID:           suite.testUser.ID,
			AudioURL:         fmt.Sprintf("https://example.com/audio%d.mp3", i),
			OriginalFilename: fmt.Sprintf("loop%d.mp3", i),
			BPM:              120 + i*5,
			Key:              "C minor",
			ProcessingStatus: "complete",
			IsPublic:         true,
			StreamActivityID: fmt.Sprintf("stream_activity_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(post).Error)
	}

	// Create another user with similar posts
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("similar_%s@test.com", otherID),
		Username:     fmt.Sprintf("similaruser%s", otherID[:8]),
		DisplayName:  "Similar User",
		StreamUserID: fmt.Sprintf("stream_similar_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	for i := 0; i < 3; i++ {
		post := &models.AudioPost{
			UserID:           otherUser.ID,
			AudioURL:         fmt.Sprintf("https://example.com/similar%d.mp3", i),
			OriginalFilename: fmt.Sprintf("similar%d.mp3", i),
			BPM:              125, // Within range
			Key:              "C minor",
			ProcessingStatus: "complete",
			IsPublic:         true,
			StreamActivityID: fmt.Sprintf("stream_activity_similar_%d_%d", i, time.Now().UnixNano()),
		}
		require.NoError(t, suite.db.Create(post).Error)
	}

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/similar?limit=5", nil)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	// Should have BPM and key analysis
	assert.NotNil(t, response["analyzed_bpm"])
	assert.NotNil(t, response["analyzed_key"])
}

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
// FEED ENDPOINT TESTS (Priority 1)
// =============================================================================

// Note: These tests require a mock stream client or integration test setup
// TODO: Phase 4.5 - These "RequiresStream" tests are obsolete.
// The handlers now use stream.StreamClientInterface which should never be nil.
// Use FeedTestSuite with MockStreamClient for proper handler testing.
// These tests are skipped because calling methods on nil interface panics.

func (suite *HandlersTestSuite) TestGetTimelineRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	suite.T().Skip("Obsolete: handlers now require non-nil stream interface. See FeedTestSuite for proper tests.")
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
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/feed/global?limit=20&offset=0", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Should return 500 because stream client is nil
	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "failed to get global feed", response["error"])
}

func (suite *HandlersTestSuite) TestGetGlobalFeedPagination() {
	suite.T().Skip("TODO: Requires mock stream client")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/feed/global?limit=10&offset=5", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Even with nil stream, we can test that pagination params are parsed
	// The handler will fail at stream call, but we verify it gets that far
	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

func (suite *HandlersTestSuite) TestCreatePostRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	body := map[string]interface{}{
		"audio_url": "https://example.com/audio.mp3",
		"bpm":       128,
		"key":       "C major",
		"daw":       "Ableton",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/feed/post", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Should return 500 because stream client is nil
	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "failed to create post", response["error"])
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
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/feed/timeline/enriched?limit=20", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

func (suite *HandlersTestSuite) TestGetEnrichedGlobalFeedRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/feed/global/enriched?limit=20", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

// =============================================================================
// SOCIAL ENDPOINT TESTS (Priority 1)
// =============================================================================

func (suite *HandlersTestSuite) TestFollowUserRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	// Create another user to follow
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("otheruser%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	body := map[string]interface{}{
		"target_user_id": otherUser.StreamUserID,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/follow", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Should return 500 because stream client is nil
	assert.Equal(t, http.StatusInternalServerError, w.Code)
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
	t := suite.T()

	body := map[string]interface{}{
		"target_user_id": suite.testUser.StreamUserID, // Following self
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/follow", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Handler should check for self-follow before calling stream
	// Let's check what the handler does - it might return 400 or proceed to stream
	// Since stream is nil, we'll get 500, but we can test the validation separately
	assert.True(t, w.Code == http.StatusBadRequest || w.Code == http.StatusInternalServerError)
}

func (suite *HandlersTestSuite) TestUnfollowUserRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	// Create another user to unfollow
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("otheruser%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	body := map[string]interface{}{
		"target_user_id": otherUser.StreamUserID,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/unfollow", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
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
	t := suite.T()

	// Create another user to follow
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("otheruser%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	req, _ := http.NewRequest("POST", "/api/users/"+otherUser.ID+"/follow", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
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
	t := suite.T()

	// Create another user to unfollow
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("otheruser%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	req, _ := http.NewRequest("DELETE", "/api/users/"+otherUser.ID+"/follow", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

func (suite *HandlersTestSuite) TestUnfollowUserByIDNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("DELETE", "/api/users/00000000-0000-0000-0000-000000000000/follow", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestLikePostRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	body := map[string]interface{}{
		"activity_id": "loop:test123",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/like", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Handler checks for emoji first, then calls stream
	assert.True(t, w.Code == http.StatusOK || w.Code == http.StatusInternalServerError)
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
	t := suite.T()

	body := map[string]interface{}{
		"activity_id": "loop:test123",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("DELETE", "/api/social/like", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
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

func (suite *HandlersTestSuite) TestEmojiReactRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	body := map[string]interface{}{
		"activity_id": "loop:test123",
		"emoji":       "🔥",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/react", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
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
// NOTIFICATION ENDPOINT TESTS (Priority 2)
// =============================================================================

func (suite *HandlersTestSuite) TestGetNotificationsRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/notifications?limit=20&offset=0", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "failed_to_get_notifications", response["error"])
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
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/notifications/counts", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "failed_to_get_notification_counts", response["error"])
}

func (suite *HandlersTestSuite) TestMarkNotificationsReadRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("POST", "/api/notifications/read", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "failed_to_mark_read", response["error"])
}

func (suite *HandlersTestSuite) TestMarkNotificationsSeenRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("POST", "/api/notifications/seen", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "failed_to_mark_seen", response["error"])
}

// =============================================================================
// PROFILE ENDPOINT TESTS (Priority 2) - Additional tests beyond existing ones
// =============================================================================

func (suite *HandlersTestSuite) TestGetUserFollowersRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/followers?limit=20&offset=0", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

func (suite *HandlersTestSuite) TestGetUserFollowingRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/following?limit=20&offset=0", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
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
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/followers?limit=10&offset=5", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Will fail at stream call, but we verify pagination params are parsed
	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

// =============================================================================
// AUDIO ENDPOINT TESTS (Priority 3)
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
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/audio/status/nonexistent-job-id", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Handler should return 404 for non-existent job
	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestGetAudioNotFound() {
	suite.T().Skip("TODO: Requires mock stream client")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/audio/nonexistent-audio-id", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// =============================================================================
// AGGREGATED FEED ENDPOINT TESTS (Priority 3)
// =============================================================================

func (suite *HandlersTestSuite) TestGetAggregatedTimelineRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/feed/timeline/aggregated?limit=20", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

func (suite *HandlersTestSuite) TestGetTrendingFeedRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/feed/trending?genre=Electronic&limit=20", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

func (suite *HandlersTestSuite) TestGetUserActivitySummaryRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/"+suite.testUser.ID+"/activity?limit=20", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
}

func (suite *HandlersTestSuite) TestGetUserActivitySummaryNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/users/00000000-0000-0000-0000-000000000000/activity", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// =============================================================================
// TRACKING ENDPOINT TESTS
// =============================================================================

func (suite *HandlersTestSuite) TestTrackPlayRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()
	// TODO: Implement TrackPlay handler when needed - skipping test for now
	t.Skip("TrackPlay handler not yet implemented")
}

func (suite *HandlersTestSuite) TestTrackListenDurationRequiresStream() {
	suite.T().Skip("Obsolete: handlers require non-nil stream interface")
	t := suite.T()
	// TODO: Implement TrackListenDuration handler when needed - skipping test for now
	t.Skip("TrackListenDuration handler not yet implemented")
}

func (suite *HandlersTestSuite) TestTrackListenDurationMissingFields() {
	suite.T().Skip("TODO: Requires mock stream client")
	t := suite.T()

	body := map[string]interface{}{}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/social/listen-duration", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Should validate required fields
	assert.True(t, w.Code == http.StatusBadRequest || w.Code == http.StatusOK)
}

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
	suite.createTestStory(suite.testUser.ID, 30.0, time.Now().UTC().Add(24*time.Hour))    // Active
	suite.createTestStory(suite.testUser.ID, 25.0, time.Now().UTC().Add(-1*time.Hour))    // Expired

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
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/stories/nonexistent-story-id", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "not_found", response["error"])
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
	t := suite.T()

	req, _ := http.NewRequest("POST", "/api/stories/nonexistent-story-id/view", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
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

// =============================================================================
// RUN SUITE
// =============================================================================

func TestHandlersSuite(t *testing.T) {
	if os.Getenv("SKIP_DB_TESTS") == "true" {
		t.Skip("Skipping database tests")
	}

	suite.Run(t, new(HandlersTestSuite))
}

func getEnvOrDefault(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}
