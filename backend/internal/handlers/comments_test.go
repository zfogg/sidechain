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
	"github.com/zfogg/sidechain/backend/internal/util"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// CommentTestSuite contains comment handler tests
type CommentTestSuite struct {
	suite.Suite
	db       *gorm.DB
	router   *gin.Engine
	handlers *Handlers
	testUser *models.User
	testPost *models.AudioPost
}

// SetupSuite initializes test database and handlers
func (suite *CommentTestSuite) SetupSuite() {
	// Build test DSN from environment or use defaults
	host := getEnvOrDefaultComment("POSTGRES_HOST", "localhost")
	port := getEnvOrDefaultComment("POSTGRES_PORT", "5432")
	user := getEnvOrDefaultComment("POSTGRES_USER", "postgres")
	password := getEnvOrDefaultComment("POSTGRES_PASSWORD", "")
	dbname := getEnvOrDefaultComment("POSTGRES_DB", "sidechain_test")

	testDSN := fmt.Sprintf("host=%s port=%s user=%s dbname=%s sslmode=disable", host, port, user, dbname)
	if password != "" {
		testDSN = fmt.Sprintf("host=%s port=%s user=%s password=%s dbname=%s sslmode=disable", host, port, user, password, dbname)
	}

	db, err := gorm.Open(postgres.Open(testDSN), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		suite.T().Skipf("Skipping comment tests: database not available (%v)", err)
		return
	}

	// Set global DB for database package
	database.DB = db

	// Check if tables already exist (migrations already run)
	var count int64
	db.Raw("SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = 'public' AND table_name = 'users'").Scan(&count)
	if count == 0 {
		err = db.AutoMigrate(
			&models.User{},
			&models.OAuthProvider{},
			&models.AudioPost{},
			&models.Comment{},
			&models.CommentMention{},
		)
		require.NoError(suite.T(), err)
	}

	suite.db = db

	// Initialize handlers (nil stream client for tests)
	suite.handlers = NewHandlers(nil, nil)

	// Setup Gin router
	gin.SetMode(gin.TestMode)
	suite.router = gin.New()
	suite.setupRoutes()
}

// setupRoutes configures the test router with comment routes
func (suite *CommentTestSuite) setupRoutes() {
	api := suite.router.Group("/api/v1")

	// Mock auth middleware that sets user_id from header
	authMiddleware := func(c *gin.Context) {
		userID := c.GetHeader("X-User-ID")
		if userID == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
			c.Abort()
			return
		}
		c.Set("user_id", userID)
		c.Next()
	}

	posts := api.Group("/posts")
	posts.Use(authMiddleware)
	posts.POST("/:id/comments", suite.handlers.CreateComment)
	posts.GET("/:id/comments", suite.handlers.GetComments)

	comments := api.Group("/comments")
	comments.Use(authMiddleware)
	comments.GET("/:id/replies", suite.handlers.GetCommentReplies)
	comments.PUT("/:id", suite.handlers.UpdateComment)
	comments.DELETE("/:id", suite.handlers.DeleteComment)
	comments.POST("/:id/like", suite.handlers.LikeComment)
	comments.DELETE("/:id/like", suite.handlers.UnlikeComment)
}

// TearDownSuite cleans up - only closes connection, doesn't drop tables
// to allow other test suites to run against the same database
func (suite *CommentTestSuite) TearDownSuite() {
	sqlDB, _ := suite.db.DB()
	sqlDB.Close()
}

// SetupTest creates fresh test data before each test
func (suite *CommentTestSuite) SetupTest() {
	// Use TRUNCATE CASCADE for proper cleanup (only tables from AutoMigrate)
	suite.db.Exec("TRUNCATE TABLE comment_mentions, comments, audio_posts RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE users RESTART IDENTITY CASCADE")

	// Create test user with unique identifiers per test
	testID := fmt.Sprintf("%d", time.Now().UnixNano())
	suite.testUser = &models.User{
		Email:        fmt.Sprintf("commenter_%s@test.com", testID),
		Username:     fmt.Sprintf("testcommenter_%s", testID),
		DisplayName:  "Test Commenter",
		StreamUserID: fmt.Sprintf("stream_test_user_%s", testID),
	}
	require.NoError(suite.T(), suite.db.Create(suite.testUser).Error)

	// Create test post
	suite.testPost = &models.AudioPost{
		UserID:           suite.testUser.ID,
		AudioURL:         "https://example.com/audio.mp3",
		OriginalFilename: "test.mp3",
		ProcessingStatus: "complete",
		IsPublic:         true,
	}
	require.NoError(suite.T(), suite.db.Create(suite.testPost).Error)
}

// =============================================================================
// UNIT TESTS FOR extractMentions
// =============================================================================

func TestExtractMentions(t *testing.T) {
	testCases := []struct {
		name     string
		content  string
		expected []string
	}{
		{
			name:     "single mention",
			content:  "Hey @producer nice loop!",
			expected: []string{"producer"},
		},
		{
			name:     "multiple mentions",
			content:  "@user1 and @user2 should check this out",
			expected: []string{"user1", "user2"},
		},
		{
			name:     "mention with punctuation",
			content:  "Thanks @beatmaker! You're awesome @musician.",
			expected: []string{"beatmaker", "musician"},
		},
		{
			name:     "duplicate mentions deduplicated",
			content:  "Hey @user1 what do you think @user1?",
			expected: []string{"user1"},
		},
		{
			name:     "no mentions",
			content:  "This is a regular comment",
			expected: []string(nil),
		},
		{
			name:     "at symbol alone",
			content:  "Email me @ my email",
			expected: []string(nil),
		},
		{
			name:     "mention at start",
			content:  "@firstuser hello!",
			expected: []string{"firstuser"},
		},
		{
			name:     "case insensitive",
			content:  "@UPPERCASE and @lowercase",
			expected: []string{"uppercase", "lowercase"},
		},
		{
			name:     "username too short",
			content:  "@ab is too short, @abc is ok",
			expected: []string{"abc"},
		},
		{
			name:     "mention with underscores and hyphens",
			content:  "Check out @beat_maker-2024 style",
			expected: []string{"beat_maker-2024"},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			result := util.ExtractMentions(tc.content)
			assert.Equal(t, tc.expected, result)
		})
	}
}

// =============================================================================
// INTEGRATION TESTS FOR COMMENT HANDLERS
// =============================================================================

// TestCreateComment tests creating a new comment
func (suite *CommentTestSuite) TestCreateComment() {
	t := suite.T()

	// Test successful comment creation
	body := map[string]interface{}{
		"content": "This is a great loop!",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/posts/"+suite.testPost.ID+"/comments", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	comment := response["comment"].(map[string]interface{})
	assert.Equal(t, "This is a great loop!", comment["content"])
	assert.Equal(t, suite.testPost.ID, comment["post_id"])
	assert.Equal(t, suite.testUser.ID, comment["user_id"])

	// Verify post comment count was incremented
	var post models.AudioPost
	suite.db.First(&post, "id = ?", suite.testPost.ID)
	assert.Equal(t, 1, post.CommentCount)
}

// TestCreateCommentWithInvalidPost tests comment creation on non-existent post
func (suite *CommentTestSuite) TestCreateCommentWithInvalidPost() {
	t := suite.T()

	body := map[string]interface{}{
		"content": "Test comment",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/posts/00000000-0000-0000-0000-000000000000/comments", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

// TestCreateCommentWithEmptyContent tests validation for empty content
func (suite *CommentTestSuite) TestCreateCommentWithEmptyContent() {
	t := suite.T()

	body := map[string]interface{}{
		"content": "",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/posts/"+suite.testPost.ID+"/comments", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

// TestCreateReplyComment tests creating a reply to another comment
func (suite *CommentTestSuite) TestCreateReplyComment() {
	t := suite.T()

	// Create parent comment first
	parentComment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Parent comment",
	}
	require.NoError(t, suite.db.Create(parentComment).Error)

	// Create reply
	body := map[string]interface{}{
		"content":   "This is a reply",
		"parent_id": parentComment.ID,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/posts/"+suite.testPost.ID+"/comments", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	comment := response["comment"].(map[string]interface{})
	assert.Equal(t, parentComment.ID, comment["parent_id"])
}

// TestNestedRepliesFlatten tests that deeply nested replies are flattened to 1 level
func (suite *CommentTestSuite) TestNestedRepliesFlatten() {
	t := suite.T()

	// Create parent comment
	parentComment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Parent comment",
	}
	require.NoError(t, suite.db.Create(parentComment).Error)

	// Create child (reply to parent)
	childComment := &models.Comment{
		PostID:   suite.testPost.ID,
		UserID:   suite.testUser.ID,
		Content:  "Child comment",
		ParentID: &parentComment.ID,
	}
	require.NoError(t, suite.db.Create(childComment).Error)

	// Try to reply to the child - should become reply to parent instead
	body := map[string]interface{}{
		"content":   "Grandchild comment",
		"parent_id": childComment.ID,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/posts/"+suite.testPost.ID+"/comments", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	comment := response["comment"].(map[string]interface{})
	// Grandchild should become child of parent (flattened to 1 level)
	assert.Equal(t, parentComment.ID, comment["parent_id"])
}

// TestGetComments tests retrieving comments for a post
func (suite *CommentTestSuite) TestGetComments() {
	t := suite.T()

	// Create test comments
	for i := 0; i < 5; i++ {
		comment := &models.Comment{
			PostID:  suite.testPost.ID,
			UserID:  suite.testUser.ID,
			Content: fmt.Sprintf("Comment %d", i),
		}
		require.NoError(t, suite.db.Create(comment).Error)
	}

	req, _ := http.NewRequest("GET", "/api/v1/posts/"+suite.testPost.ID+"/comments", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	comments := response["comments"].([]interface{})
	assert.Len(t, comments, 5)

	meta := response["meta"].(map[string]interface{})
	assert.Equal(t, float64(5), meta["total"])
}

// TestGetCommentsPagination tests pagination for comments
func (suite *CommentTestSuite) TestGetCommentsPagination() {
	t := suite.T()

	// Create 15 comments
	for i := 0; i < 15; i++ {
		comment := &models.Comment{
			PostID:  suite.testPost.ID,
			UserID:  suite.testUser.ID,
			Content: fmt.Sprintf("Comment %d", i),
		}
		require.NoError(t, suite.db.Create(comment).Error)
	}

	// Get first page (limit 10)
	req, _ := http.NewRequest("GET", "/api/v1/posts/"+suite.testPost.ID+"/comments?limit=10&offset=0", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	comments := response["comments"].([]interface{})
	assert.Len(t, comments, 10)

	meta := response["meta"].(map[string]interface{})
	assert.Equal(t, float64(15), meta["total"])

	// Get second page
	req, _ = http.NewRequest("GET", "/api/v1/posts/"+suite.testPost.ID+"/comments?limit=10&offset=10", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w = httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	json.Unmarshal(w.Body.Bytes(), &response)

	comments = response["comments"].([]interface{})
	assert.Len(t, comments, 5)
}

// TestGetCommentReplies tests retrieving replies to a comment
func (suite *CommentTestSuite) TestGetCommentReplies() {
	t := suite.T()

	// Create parent comment
	parentComment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Parent comment",
	}
	require.NoError(t, suite.db.Create(parentComment).Error)

	// Create replies
	for i := 0; i < 3; i++ {
		reply := &models.Comment{
			PostID:   suite.testPost.ID,
			UserID:   suite.testUser.ID,
			Content:  fmt.Sprintf("Reply %d", i),
			ParentID: &parentComment.ID,
		}
		require.NoError(t, suite.db.Create(reply).Error)
	}

	req, _ := http.NewRequest("GET", "/api/v1/comments/"+parentComment.ID+"/replies", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	replies := response["replies"].([]interface{})
	assert.Len(t, replies, 3)
}

// TestUpdateComment tests editing a comment
func (suite *CommentTestSuite) TestUpdateComment() {
	t := suite.T()

	// Create comment
	comment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Original content",
	}
	require.NoError(t, suite.db.Create(comment).Error)

	// Update within 5 minutes
	body := map[string]interface{}{
		"content": "Updated content",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/v1/comments/"+comment.ID, bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	updatedComment := response["comment"].(map[string]interface{})
	assert.Equal(t, "Updated content", updatedComment["content"])
	assert.True(t, updatedComment["is_edited"].(bool))
	assert.NotNil(t, updatedComment["edited_at"])
}

// TestUpdateCommentNotOwner tests that non-owners can't edit
func (suite *CommentTestSuite) TestUpdateCommentNotOwner() {
	t := suite.T()

	// Create another user with unique ID
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("otheruser_%s", otherID),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_user_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	// Create comment by test user
	comment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Original content",
	}
	require.NoError(t, suite.db.Create(comment).Error)

	// Try to update as other user
	body := map[string]interface{}{
		"content": "Hacked content",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/v1/comments/"+comment.ID, bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", otherUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

// TestUpdateCommentExpiredWindow tests the 5-minute edit window
func (suite *CommentTestSuite) TestUpdateCommentExpiredWindow() {
	t := suite.T()

	// Create comment with old timestamp
	comment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Original content",
	}
	require.NoError(t, suite.db.Create(comment).Error)

	// Manually update created_at to 10 minutes ago
	oldTime := time.Now().Add(-10 * time.Minute)
	suite.db.Model(comment).Update("created_at", oldTime)

	// Try to update
	body := map[string]interface{}{
		"content": "Updated content",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("PUT", "/api/v1/comments/"+comment.ID, bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "edit_window_expired", response["error"])
}

// TestDeleteComment tests soft deleting a comment
func (suite *CommentTestSuite) TestDeleteComment() {
	t := suite.T()

	// Create comment
	comment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "To be deleted",
	}
	require.NoError(t, suite.db.Create(comment).Error)

	// Update post comment count
	suite.db.Model(&suite.testPost).Update("comment_count", 1)

	req, _ := http.NewRequest("DELETE", "/api/v1/comments/"+comment.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify soft delete
	var deletedComment models.Comment
	suite.db.First(&deletedComment, "id = ?", comment.ID)
	assert.True(t, deletedComment.IsDeleted)
	assert.Equal(t, "[Comment deleted]", deletedComment.Content)

	// Verify post comment count was decremented
	var post models.AudioPost
	suite.db.First(&post, "id = ?", suite.testPost.ID)
	assert.Equal(t, 0, post.CommentCount)
}

// TestDeleteCommentNotOwner tests that non-owners can't delete
func (suite *CommentTestSuite) TestDeleteCommentNotOwner() {
	t := suite.T()

	// Create another user with unique ID
	otherID := fmt.Sprintf("%d", time.Now().UnixNano())
	otherUser := &models.User{
		Email:        fmt.Sprintf("other_%s@test.com", otherID),
		Username:     fmt.Sprintf("otheruser_%s", otherID),
		DisplayName:  "Other User",
		StreamUserID: fmt.Sprintf("stream_other_user_%s", otherID),
	}
	require.NoError(t, suite.db.Create(otherUser).Error)

	// Create comment by test user
	comment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Not deletable by others",
	}
	require.NoError(t, suite.db.Create(comment).Error)

	req, _ := http.NewRequest("DELETE", "/api/v1/comments/"+comment.ID, nil)
	req.Header.Set("X-User-ID", otherUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

// TestLikeComment tests liking a comment
func (suite *CommentTestSuite) TestLikeComment() {
	t := suite.T()

	// Create comment
	comment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Likeable comment",
	}
	require.NoError(t, suite.db.Create(comment).Error)

	req, _ := http.NewRequest("POST", "/api/v1/comments/"+comment.ID+"/like", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "comment_liked", response["message"])
	assert.Equal(t, float64(1), response["like_count"])

	// Verify in database
	var likedComment models.Comment
	suite.db.First(&likedComment, "id = ?", comment.ID)
	assert.Equal(t, 1, likedComment.LikeCount)
}

// TestUnlikeComment tests unliking a comment
func (suite *CommentTestSuite) TestUnlikeComment() {
	t := suite.T()

	// Create comment with 1 like
	comment := &models.Comment{
		PostID:    suite.testPost.ID,
		UserID:    suite.testUser.ID,
		Content:   "Liked comment",
		LikeCount: 1,
	}
	require.NoError(t, suite.db.Create(comment).Error)

	req, _ := http.NewRequest("DELETE", "/api/v1/comments/"+comment.ID+"/like", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "comment_unliked", response["message"])
	assert.Equal(t, float64(0), response["like_count"])

	// Verify in database
	var unlikedComment models.Comment
	suite.db.First(&unlikedComment, "id = ?", comment.ID)
	assert.Equal(t, 0, unlikedComment.LikeCount)
}

// TestLikeDeletedComment tests that deleted comments can't be liked
func (suite *CommentTestSuite) TestLikeDeletedComment() {
	t := suite.T()

	// Create deleted comment
	comment := &models.Comment{
		PostID:    suite.testPost.ID,
		UserID:    suite.testUser.ID,
		Content:   "[Comment deleted]",
		IsDeleted: true,
	}
	require.NoError(t, suite.db.Create(comment).Error)

	req, _ := http.NewRequest("POST", "/api/v1/comments/"+comment.ID+"/like", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "cannot_like_deleted_comment", response["error"])
}

// TestCommentMentions tests that @mentions are extracted and stored
func (suite *CommentTestSuite) TestCommentMentions() {
	t := suite.T()

	// Create user to mention with unique ID
	mentionID := fmt.Sprintf("%d", time.Now().UnixNano())
	mentionedUser := &models.User{
		Email:        fmt.Sprintf("mentioned_%s@test.com", mentionID),
		Username:     fmt.Sprintf("mentionme%s", mentionID[:8]), // Use shorter unique suffix for username
		DisplayName:  "Mention Me",
		StreamUserID: fmt.Sprintf("stream_mention_user_%s", mentionID),
	}
	require.NoError(t, suite.db.Create(mentionedUser).Error)

	// Create comment with mention using the actual username
	body := map[string]interface{}{
		"content": fmt.Sprintf("Hey @%s check this out!", mentionedUser.Username),
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/posts/"+suite.testPost.ID+"/comments", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	// Give async mention processing time to complete
	time.Sleep(100 * time.Millisecond)

	// Verify mention was created
	var mention models.CommentMention
	err := suite.db.Where("mentioned_user_id = ?", mentionedUser.ID).First(&mention).Error
	assert.NoError(t, err)
	assert.Equal(t, mentionedUser.ID, mention.MentionedUserID)
}

// TestDeletedCommentsExcludedFromListing tests that deleted comments are hidden
func (suite *CommentTestSuite) TestDeletedCommentsExcludedFromListing() {
	t := suite.T()

	// Create visible comment
	visibleComment := &models.Comment{
		PostID:  suite.testPost.ID,
		UserID:  suite.testUser.ID,
		Content: "Visible comment",
	}
	require.NoError(t, suite.db.Create(visibleComment).Error)

	// Create deleted comment
	deletedComment := &models.Comment{
		PostID:    suite.testPost.ID,
		UserID:    suite.testUser.ID,
		Content:   "[Comment deleted]",
		IsDeleted: true,
	}
	require.NoError(t, suite.db.Create(deletedComment).Error)

	req, _ := http.NewRequest("GET", "/api/v1/posts/"+suite.testPost.ID+"/comments", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	comments := response["comments"].([]interface{})
	assert.Len(t, comments, 1)

	// Verify only visible comment is returned
	firstComment := comments[0].(map[string]interface{})
	assert.Equal(t, "Visible comment", firstComment["content"])
}

// Run the test suite
func TestCommentSuite(t *testing.T) {
	// Skip if no test database available
	if os.Getenv("SKIP_DB_TESTS") == "true" {
		t.Skip("Skipping database tests")
	}

	suite.Run(t, new(CommentTestSuite))
}

// getEnvOrDefaultComment returns environment variable or default value
func getEnvOrDefaultComment(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}
