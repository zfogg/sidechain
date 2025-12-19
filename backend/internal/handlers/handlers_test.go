package handlers

import (
	"fmt"
	"net/http"
	"os"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
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

	// Always run AutoMigrate to ensure all tables exist
	// GORM's AutoMigrate is idempotent - it only creates tables that don't exist
	err = db.AutoMigrate(
		&models.User{},
		&models.OAuthProvider{},
		&models.MIDIPattern{}, // Must come before AudioPost and Story
		&models.AudioPost{},
		&models.Comment{},
		&models.CommentMention{},
		&models.Story{},
		&models.StoryView{},
		&models.ProjectFile{}, // Project File Exchange
	)
	require.NoError(suite.T(), err)

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

	// Story routes
	api.POST("/stories", suite.handlers.CreateStory)
	api.GET("/stories", suite.handlers.GetStories)
	api.GET("/stories/:id", suite.handlers.GetStory)
	api.POST("/stories/:id/view", suite.handlers.ViewStory)
	api.GET("/stories/:id/views", suite.handlers.GetStoryViews)
	api.POST("/stories/:id/download", suite.handlers.DownloadStory)
	api.GET("/users/:id/stories", suite.handlers.GetUserStories)

	// MIDI routes
	api.POST("/midi", suite.handlers.CreateMIDIPattern)
	api.GET("/midi", suite.handlers.ListMIDIPatterns)
	api.GET("/midi/:id", suite.handlers.GetMIDIPattern)
	api.GET("/midi/:id/file", suite.handlers.GetMIDIPatternFile)
	api.PATCH("/midi/:id", suite.handlers.UpdateMIDIPattern)
	api.DELETE("/midi/:id", suite.handlers.DeleteMIDIPattern)

	// Project file routes
	api.POST("/v1/project-files", suite.handlers.CreateProjectFile)
	api.GET("/v1/project-files", suite.handlers.ListProjectFiles)
	api.GET("/v1/project-files/:id", suite.handlers.GetProjectFile)
	api.GET("/v1/project-files/:id/download", suite.handlers.DownloadProjectFile)
	api.DELETE("/v1/project-files/:id", suite.handlers.DeleteProjectFile)
	api.GET("/v1/posts/:id/project-file", suite.handlers.GetPostProjectFile)

	// Notification routes (require getstream.io)
	api.GET("/notifications", suite.handlers.GetNotifications)
	api.GET("/notifications/counts", suite.handlers.GetNotificationCounts)
	api.POST("/notifications/read", suite.handlers.MarkNotificationsRead)
	api.POST("/notifications/seen", suite.handlers.MarkNotificationsSeen)
}

// TearDownSuite cleans up - only closes connection, doesn't drop tables
func (suite *HandlersTestSuite) TearDownSuite() {
	sqlDB, _ := suite.db.DB()
	sqlDB.Close()
}

// SetupTest creates fresh test data before each test
func (suite *HandlersTestSuite) SetupTest() {
	// Only truncate tables that exist from AutoMigrate
	suite.db.Exec("TRUNCATE TABLE project_files RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE story_views, stories RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE comment_mentions, comments, audio_posts RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE midi_patterns RESTART IDENTITY CASCADE")
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
