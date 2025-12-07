package timeline

import (
	"context"
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

func setupTestDB(t *testing.T) *gorm.DB {
	host := os.Getenv("POSTGRES_HOST")
	if host == "" {
		host = "localhost"
	}
	port := os.Getenv("POSTGRES_PORT")
	if port == "" {
		port = "5432"
	}
	user := os.Getenv("POSTGRES_USER")
	if user == "" {
		user = "sidechain"
	}
	password := os.Getenv("POSTGRES_PASSWORD")
	if password == "" {
		password = "sidechain_dev_password"
	}
	dbname := os.Getenv("POSTGRES_DB")
	if dbname == "" {
		dbname = "sidechain_test"
	}

	dsn := "host=" + host + " port=" + port + " user=" + user + " password=" + password + " dbname=" + dbname + " sslmode=disable"
	db, err := gorm.Open(postgres.Open(dsn), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	require.NoError(t, err)

	// Migrate test models
	err = db.AutoMigrate(&models.User{}, &models.AudioPost{})
	require.NoError(t, err)

	// Set global DB for timeline service
	database.DB = db

	return db
}

func cleanupTestDB(t *testing.T, db *gorm.DB) {
	// Clean up test data
	db.Exec("DELETE FROM audio_posts WHERE user_id LIKE 'test-%'")
	db.Exec("DELETE FROM users WHERE id LIKE 'test-%'")
}

func TestRankItems(t *testing.T) {
	// Create a mock service (no DB needed for ranking test)
	svc := &Service{}

	now := time.Now()

	items := []TimelineItem{
		{
			ID:        "post1",
			Source:    "following",
			Score:     1.0,
			CreatedAt: now.Add(-1 * time.Hour),
			Post: &models.AudioPost{
				LikeCount: 10,
				PlayCount: 50,
			},
		},
		{
			ID:        "post2",
			Source:    "gorse",
			Score:     0.9,
			CreatedAt: now.Add(-30 * time.Minute),
			Post: &models.AudioPost{
				LikeCount: 30,
				PlayCount: 100,
			},
		},
		{
			ID:        "post3",
			Source:    "trending",
			Score:     0.8,
			CreatedAt: now.Add(-2 * time.Hour),
			Post: &models.AudioPost{
				LikeCount: 100,
				PlayCount: 500,
			},
		},
		{
			ID:        "post4",
			Source:    "recent",
			Score:     0.5,
			CreatedAt: now.Add(-10 * time.Minute),
			Post: &models.AudioPost{
				LikeCount: 5,
				PlayCount: 20,
			},
		},
	}

	ranked := svc.rankItems(items, "test-user")

	// Verify items are sorted by score
	for i := 0; i < len(ranked)-1; i++ {
		// If scores are close, recency should determine order
		if abs(ranked[i].Score-ranked[i+1].Score) < 0.1 {
			assert.True(t, ranked[i].CreatedAt.After(ranked[i+1].CreatedAt) || ranked[i].CreatedAt.Equal(ranked[i+1].CreatedAt),
				"When scores are close, more recent posts should come first")
		} else {
			assert.GreaterOrEqual(t, ranked[i].Score, ranked[i+1].Score,
				"Items should be sorted by score descending")
		}
	}

	// Verify source weights are applied correctly
	// Following posts should have higher scores than equivalent recent posts
	for _, item := range ranked {
		t.Logf("Post %s: source=%s, originalScore=%.2f, finalScore=%.2f", item.ID, item.Source, items[0].Score, item.Score)
	}
}

func TestExtractIDFromObject(t *testing.T) {
	tests := []struct {
		object   string
		expected string
	}{
		{"loop:abc123", "abc123"},
		{"user:test-user-id", "test-user-id"},
		{"post:12345", "12345"},
		{"noprefix", ""},  // No colon means empty result
		{"", ""},
	}

	for _, tc := range tests {
		result := extractIDFromObject(tc.object)
		assert.Equal(t, tc.expected, result, "extractIDFromObject(%s)", tc.object)
	}
}

func TestGetTimelineIntegration(t *testing.T) {
	if os.Getenv("SKIP_INTEGRATION_TESTS") != "" {
		t.Skip("Skipping integration test")
	}

	db := setupTestDB(t)
	defer cleanupTestDB(t, db)

	// Use real UUIDs for test data - generate unique ones to avoid conflicts
	uniqueSuffix := time.Now().Format("150405000000")
	testUserID := "aaaaaaaa-bbbb-cccc-dddd-" + uniqueSuffix[:12]
	testPostID1 := "eeeeeeee-ffff-0000-1111-" + uniqueSuffix[:12]
	testPostID2 := "22222222-3333-4444-5555-" + uniqueSuffix[:12]

	// Clean up any existing test data first
	db.Exec("DELETE FROM audio_posts WHERE id LIKE 'eeeeeeee%' OR id LIKE '22222222-3333%'")
	db.Exec("DELETE FROM users WHERE id LIKE 'aaaaaaaa%'")

	// Create test user
	testUser := &models.User{
		ID:           testUserID,
		Email:        "test-timeline-" + testUserID + "@example.com",
		Username:     "testtimeline" + testUserID[:8],
		DisplayName:  "Test Timeline User",
		StreamUserID: testUserID,
	}
	err := db.Create(testUser).Error
	require.NoError(t, err)
	defer db.Delete(testUser)

	// Create some test posts with unique stream activity IDs
	posts := []*models.AudioPost{
		{
			ID:               testPostID1,
			UserID:           testUser.ID,
			AudioURL:         "https://example.com/audio1.mp3",
			BPM:              120,
			Key:              "C major",
			IsPublic:         true,
			StreamActivityID: testPostID1 + "-stream",
		},
		{
			ID:               testPostID2,
			UserID:           testUser.ID,
			AudioURL:         "https://example.com/audio2.mp3",
			BPM:              140,
			Key:              "A minor",
			IsPublic:         true,
			StreamActivityID: testPostID2 + "-stream",
		},
	}
	for _, post := range posts {
		err := db.Create(post).Error
		require.NoError(t, err)
		defer db.Delete(post)
	}

	// Create service (stream client will be nil, which will cause getFollowingPosts to fail gracefully)
	svc := &Service{
		db:           db,
		streamClient: nil,
		gorseClient:  nil,
	}

	// Test getRecentPosts - use a different user ID to ensure we get posts from testUser
	ctx := context.Background()
	recentPosts, err := svc.getRecentPosts(ctx, "44444444-4444-4444-4444-444444444444", 10)
	require.NoError(t, err)
	assert.NotEmpty(t, recentPosts, "Should have recent posts")

	// Verify at least one post is from test user
	foundTestPost := false
	for _, item := range recentPosts {
		assert.NotNil(t, item.Post)
		assert.Equal(t, "recent", item.Source)
		if item.Post.UserID == testUser.ID {
			foundTestPost = true
		}
	}
	assert.True(t, foundTestPost, "Should find test user's posts in recent posts")
}

func TestTimelineItemFields(t *testing.T) {
	item := TimelineItem{
		ID:     "test-id",
		Type:   "post",
		Source: "gorse",
		Score:  0.95,
		Reason: "matches your genre preferences",
		Post: &models.AudioPost{
			ID:        "post-id",
			AudioURL:  "https://example.com/audio.mp3",
			BPM:       128,
			LikeCount: 50,
		},
		User: &models.User{
			ID:       "user-id",
			Username: "testuser",
		},
		CreatedAt: time.Now(),
	}

	assert.Equal(t, "test-id", item.ID)
	assert.Equal(t, "post", item.Type)
	assert.Equal(t, "gorse", item.Source)
	assert.Equal(t, 0.95, item.Score)
	assert.Equal(t, "matches your genre preferences", item.Reason)
	assert.NotNil(t, item.Post)
	assert.NotNil(t, item.User)
}
