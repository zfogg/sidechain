package stories

import (
	"context"
	"fmt"
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// MockFileDeleter implements FileDeleter for testing
type MockFileDeleter struct {
	DeletedKeys []string
	ShouldFail  bool
}

func (m *MockFileDeleter) DeleteFile(ctx context.Context, key string) error {
	if m.ShouldFail {
		return fmt.Errorf("mock delete failure")
	}
	m.DeletedKeys = append(m.DeletedKeys, key)
	return nil
}

// CleanupTestSuite contains cleanup service tests
type CleanupTestSuite struct {
	suite.Suite
	db          *gorm.DB
	fileDeleter *MockFileDeleter
	testUser    *models.User
}

func getEnvOrDefault(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

// SetupSuite initializes test database
func (suite *CleanupTestSuite) SetupSuite() {
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
		suite.T().Skipf("Skipping cleanup tests: database not available (%v)", err)
		return
	}

	database.DB = db

	// Check if tables already exist (migrations already run)
	var count int64
	db.Raw("SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'public' AND table_name = 'users'").Scan(&count)
	if count == 0 {
		err = db.AutoMigrate(
			&models.User{},
			&models.Story{},
			&models.StoryView{},
		)
		require.NoError(suite.T(), err)
	}

	suite.db = db
}

// TearDownSuite cleans up
func (suite *CleanupTestSuite) TearDownSuite() {
	if suite.db != nil {
		sqlDB, _ := suite.db.DB()
		sqlDB.Close()
	}
}

// SetupTest creates fresh test data before each test
func (suite *CleanupTestSuite) SetupTest() {
	// Ensure tables exist before truncating (handles parallel test runs)
	suite.db.AutoMigrate(&models.User{}, &models.Story{}, &models.StoryView{})

	// Truncate tables (safe now that we know they exist)
	suite.db.Exec("TRUNCATE TABLE story_views, stories RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE users RESTART IDENTITY CASCADE")

	// Create mock file deleter
	suite.fileDeleter = &MockFileDeleter{
		DeletedKeys: []string{},
		ShouldFail:  false,
	}

	// Create test user
	testID := fmt.Sprintf("%d", time.Now().UnixNano())
	suite.testUser = &models.User{
		Email:        fmt.Sprintf("testuser_%s@test.com", testID),
		Username:     fmt.Sprintf("testuser_%s", testID[:12]),
		DisplayName:  "Test User",
		StreamUserID: fmt.Sprintf("stream_user_%s", testID),
	}
	require.NoError(suite.T(), suite.db.Create(suite.testUser).Error)
}

// createTestStory is a helper to create stories for testing
func (suite *CleanupTestSuite) createTestStory(userID string, duration float64, expiresAt time.Time, audioURL string) *models.Story {
	bpm := 128
	key := "C major"
	story := &models.Story{
		UserID:        userID,
		AudioURL:      audioURL,
		AudioDuration: duration,
		BPM:           &bpm,
		Key:           &key,
		ExpiresAt:     expiresAt,
		ViewCount:     0,
	}
	require.NoError(suite.T(), suite.db.Create(story).Error)
	return story
}

// =============================================================================
// CLEANUP SERVICE TESTS (7.5.7.1.4)
// =============================================================================

func (suite *CleanupTestSuite) TestCleanupExpiredStoriesDeletesFromDatabase() {
	t := suite.T()

	// Create expired stories
	expiredStory1 := suite.createTestStory(
		suite.testUser.ID,
		30.0,
		time.Now().UTC().Add(-2*time.Hour), // Expired 2 hours ago
		"https://cdn.example.com/audio/story1.mp3",
	)
	expiredStory2 := suite.createTestStory(
		suite.testUser.ID,
		25.0,
		time.Now().UTC().Add(-1*time.Hour), // Expired 1 hour ago
		"https://cdn.example.com/audio/story2.mp3",
	)

	// Create active story (should NOT be deleted)
	activeStory := suite.createTestStory(
		suite.testUser.ID,
		20.0,
		time.Now().UTC().Add(23*time.Hour), // Expires in 23 hours
		"https://cdn.example.com/audio/active.mp3",
	)

	// Run cleanup
	service := NewCleanupService(suite.fileDeleter, time.Hour)
	service.cleanupExpiredStories()

	// Verify expired stories are deleted
	var count int64
	suite.db.Model(&models.Story{}).Where("id = ?", expiredStory1.ID).Count(&count)
	assert.Equal(t, int64(0), count, "Expired story 1 should be deleted")

	suite.db.Model(&models.Story{}).Where("id = ?", expiredStory2.ID).Count(&count)
	assert.Equal(t, int64(0), count, "Expired story 2 should be deleted")

	// Verify active story is NOT deleted
	suite.db.Model(&models.Story{}).Where("id = ?", activeStory.ID).Count(&count)
	assert.Equal(t, int64(1), count, "Active story should NOT be deleted")
}

func (suite *CleanupTestSuite) TestCleanupDeletesAssociatedViews() {
	t := suite.T()

	// Create expired story
	expiredStory := suite.createTestStory(
		suite.testUser.ID,
		30.0,
		time.Now().UTC().Add(-1*time.Hour),
		"https://cdn.example.com/audio/story.mp3",
	)

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
			StoryID:  expiredStory.ID,
			ViewerID: viewer.ID,
			ViewedAt: time.Now().UTC(),
		}
		require.NoError(t, suite.db.Create(view).Error)
	}

	// Verify views exist before cleanup
	var viewCount int64
	suite.db.Model(&models.StoryView{}).Where("story_id = ?", expiredStory.ID).Count(&viewCount)
	assert.Equal(t, int64(3), viewCount, "Should have 3 views before cleanup")

	// Run cleanup
	service := NewCleanupService(suite.fileDeleter, time.Hour)
	service.cleanupExpiredStories()

	// Verify views are deleted
	suite.db.Model(&models.StoryView{}).Where("story_id = ?", expiredStory.ID).Count(&viewCount)
	assert.Equal(t, int64(0), viewCount, "Views should be deleted with story")
}

func (suite *CleanupTestSuite) TestCleanupDeletesAudioFromS3() {
	t := suite.T()

	// Create expired story with S3 audio URL
	suite.createTestStory(
		suite.testUser.ID,
		30.0,
		time.Now().UTC().Add(-1*time.Hour),
		"https://cdn.example.com/audio/2024/12/user123/story.mp3",
	)

	// Run cleanup
	service := NewCleanupService(suite.fileDeleter, time.Hour)
	service.cleanupExpiredStories()

	// Verify file deleter was called
	assert.Len(t, suite.fileDeleter.DeletedKeys, 1, "Should have deleted 1 audio file")
	assert.Equal(t, "audio/2024/12/user123/story.mp3", suite.fileDeleter.DeletedKeys[0])
}

func (suite *CleanupTestSuite) TestCleanupContinuesOnS3DeleteFailure() {
	t := suite.T()

	// Make file deleter fail
	suite.fileDeleter.ShouldFail = true

	// Create expired story
	expiredStory := suite.createTestStory(
		suite.testUser.ID,
		30.0,
		time.Now().UTC().Add(-1*time.Hour),
		"https://cdn.example.com/audio/story.mp3",
	)

	// Run cleanup
	service := NewCleanupService(suite.fileDeleter, time.Hour)
	service.cleanupExpiredStories()

	// Verify story is STILL deleted from database even though S3 delete failed
	var count int64
	suite.db.Model(&models.Story{}).Where("id = ?", expiredStory.ID).Count(&count)
	assert.Equal(t, int64(0), count, "Story should be deleted even if S3 delete fails")
}

func (suite *CleanupTestSuite) TestCleanupNoOpWhenNoExpiredStories() {
	t := suite.T()

	// Create only active stories
	activeStory1 := suite.createTestStory(
		suite.testUser.ID,
		30.0,
		time.Now().UTC().Add(24*time.Hour),
		"https://cdn.example.com/audio/active1.mp3",
	)
	activeStory2 := suite.createTestStory(
		suite.testUser.ID,
		25.0,
		time.Now().UTC().Add(12*time.Hour),
		"https://cdn.example.com/audio/active2.mp3",
	)

	// Run cleanup
	service := NewCleanupService(suite.fileDeleter, time.Hour)
	service.cleanupExpiredStories()

	// Verify all stories still exist
	var count int64
	suite.db.Model(&models.Story{}).Where("id IN ?", []string{activeStory1.ID, activeStory2.ID}).Count(&count)
	assert.Equal(t, int64(2), count, "Active stories should not be deleted")

	// Verify no S3 deletes
	assert.Len(t, suite.fileDeleter.DeletedKeys, 0, "No files should be deleted")
}

func (suite *CleanupTestSuite) TestCleanupWorksWithNilFileDeleter() {
	t := suite.T()

	// Create expired story
	expiredStory := suite.createTestStory(
		suite.testUser.ID,
		30.0,
		time.Now().UTC().Add(-1*time.Hour),
		"https://cdn.example.com/audio/story.mp3",
	)

	// Run cleanup with nil file deleter (simulates no S3 configured)
	service := NewCleanupService(nil, time.Hour)
	service.cleanupExpiredStories()

	// Verify story is still deleted from database
	var count int64
	suite.db.Model(&models.Story{}).Where("id = ?", expiredStory.ID).Count(&count)
	assert.Equal(t, int64(0), count, "Story should be deleted even without file deleter")
}

func (suite *CleanupTestSuite) TestCleanupMultipleUsersStories() {
	t := suite.T()

	// Create second user
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("other%s", otherID[:8]),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	// Create expired stories for both users
	expiredUser1 := suite.createTestStory(
		suite.testUser.ID,
		30.0,
		time.Now().UTC().Add(-1*time.Hour),
		"https://cdn.example.com/audio/user1.mp3",
	)
	expiredUser2 := suite.createTestStory(
		otherUser.ID,
		25.0,
		time.Now().UTC().Add(-2*time.Hour),
		"https://cdn.example.com/audio/user2.mp3",
	)

	// Create active stories for both users
	activeUser1 := suite.createTestStory(
		suite.testUser.ID,
		20.0,
		time.Now().UTC().Add(24*time.Hour),
		"https://cdn.example.com/audio/active1.mp3",
	)
	activeUser2 := suite.createTestStory(
		otherUser.ID,
		15.0,
		time.Now().UTC().Add(12*time.Hour),
		"https://cdn.example.com/audio/active2.mp3",
	)

	// Run cleanup
	service := NewCleanupService(suite.fileDeleter, time.Hour)
	service.cleanupExpiredStories()

	// Verify expired stories are deleted
	var count int64
	suite.db.Model(&models.Story{}).Where("id = ?", expiredUser1.ID).Count(&count)
	assert.Equal(t, int64(0), count, "Expired story from user 1 should be deleted")

	suite.db.Model(&models.Story{}).Where("id = ?", expiredUser2.ID).Count(&count)
	assert.Equal(t, int64(0), count, "Expired story from user 2 should be deleted")

	// Verify active stories remain
	suite.db.Model(&models.Story{}).Where("id = ?", activeUser1.ID).Count(&count)
	assert.Equal(t, int64(1), count, "Active story from user 1 should remain")

	suite.db.Model(&models.Story{}).Where("id = ?", activeUser2.ID).Count(&count)
	assert.Equal(t, int64(1), count, "Active story from user 2 should remain")
}

// =============================================================================
// EXTRACT S3 KEY TESTS
// =============================================================================

func TestExtractS3KeyFromURL(t *testing.T) {
	testCases := []struct {
		name     string
		url      string
		expected string
	}{
		{
			name:     "standard CDN URL with audio path",
			url:      "https://cdn.example.com/audio/2024/12/user123/story.mp3",
			expected: "audio/2024/12/user123/story.mp3",
		},
		{
			name:     "CDN URL with stories path",
			url:      "https://cdn.example.com/stories/user456/recording.mp3",
			expected: "stories/user456/recording.mp3",
		},
		{
			name:     "URL without recognized path prefix",
			url:      "https://cdn.example.com/other/path/file.mp3",
			expected: "",
		},
		{
			name:     "short URL",
			url:      "https://cdn.example.com",
			expected: "",
		},
		{
			name:     "empty URL",
			url:      "",
			expected: "",
		},
		{
			name:     "S3 direct URL with audio path",
			url:      "https://bucket.s3.amazonaws.com/audio/user/file.mp3",
			expected: "audio/user/file.mp3",
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			result := extractS3KeyFromURL(tc.url)
			assert.Equal(t, tc.expected, result)
		})
	}
}

// =============================================================================
// SERVICE START/STOP TESTS
// =============================================================================

func (suite *CleanupTestSuite) TestServiceStartAndStop() {
	// Create service with short interval
	service := NewCleanupService(suite.fileDeleter, 100*time.Millisecond)

	// Start service
	service.Start()

	// Let it run briefly
	time.Sleep(50 * time.Millisecond)

	// Stop service
	service.Stop()

	// Should not panic or hang
}

// =============================================================================
// RUN SUITE
// =============================================================================

func TestCleanupSuite(t *testing.T) {
	if os.Getenv("SKIP_DB_TESTS") == "true" {
		t.Skip("Skipping database tests")
	}

	suite.Run(t, new(CleanupTestSuite))
}
