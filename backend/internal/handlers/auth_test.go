package handlers

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"mime/multipart"
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
	"github.com/zfogg/sidechain/backend/internal/storage"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// MockS3Uploader is a mock implementation of storage.ProfilePictureUploader for testing
type MockS3Uploader struct {
	UploadedFiles    []MockUploadedFile
	ShouldFail       bool
	FailError        error
	ReturnURL        string
	ValidateFileType bool // If true, validates file extension like the real implementation
}

type MockUploadedFile struct {
	UserID   string
	Filename string
	Size     int64
	Data     []byte
}

func NewMockS3Uploader() *MockS3Uploader {
	return &MockS3Uploader{
		UploadedFiles:    make([]MockUploadedFile, 0),
		ReturnURL:        "https://test-bucket.s3.amazonaws.com/profile-pics/test-user/test-image.jpg",
		ValidateFileType: true,
	}
}

func (m *MockS3Uploader) UploadProfilePicture(ctx context.Context, file multipart.File, header *multipart.FileHeader, userID string) (*storage.UploadResult, error) {
	if m.ShouldFail {
		if m.FailError != nil {
			return nil, m.FailError
		}
		return nil, fmt.Errorf("mock upload failure")
	}

	// Optionally validate file type like the real implementation
	if m.ValidateFileType {
		ext := getFileExtension(header.Filename)
		allowedExts := map[string]bool{
			".jpg":  true,
			".jpeg": true,
			".png":  true,
			".gif":  true,
			".webp": true,
		}
		if !allowedExts[ext] {
			return nil, fmt.Errorf("unsupported file type: %s", ext)
		}
	}

	// Read file data
	data, err := io.ReadAll(file)
	if err != nil {
		return nil, fmt.Errorf("failed to read file: %w", err)
	}

	m.UploadedFiles = append(m.UploadedFiles, MockUploadedFile{
		UserID:   userID,
		Filename: header.Filename,
		Size:     header.Size,
		Data:     data,
	})

	return &storage.UploadResult{
		Key:    fmt.Sprintf("profile-pics/%s/%s", userID, header.Filename),
		URL:    m.ReturnURL,
		Bucket: "test-bucket",
		Region: "us-east-1",
		Size:   header.Size,
	}, nil
}

func getFileExtension(filename string) string {
	for i := len(filename) - 1; i >= 0; i-- {
		if filename[i] == '.' {
			return filename[i:]
		}
	}
	return ""
}

// AuthTestSuite contains auth handler tests
type AuthTestSuite struct {
	suite.Suite
	db           *gorm.DB
	router       *gin.Engine
	authHandlers *AuthHandlers
	mockUploader *MockS3Uploader
	testUser     *models.User
}

// SetupSuite initializes test database and handlers
func (suite *AuthTestSuite) SetupSuite() {
	// Build test DSN from environment or use defaults
	host := getEnvOrDefaultAuth("POSTGRES_HOST", "localhost")
	port := getEnvOrDefaultAuth("POSTGRES_PORT", "5432")
	user := getEnvOrDefaultAuth("POSTGRES_USER", "postgres")
	password := getEnvOrDefaultAuth("POSTGRES_PASSWORD", "")
	dbname := getEnvOrDefaultAuth("POSTGRES_DB", "sidechain_test")

	testDSN := fmt.Sprintf("host=%s port=%s user=%s dbname=%s sslmode=disable", host, port, user, dbname)
	if password != "" {
		testDSN = fmt.Sprintf("host=%s port=%s user=%s password=%s dbname=%s sslmode=disable", host, port, user, password, dbname)
	}

	db, err := gorm.Open(postgres.Open(testDSN), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		suite.T().Skipf("Skipping auth tests: database not available (%v)", err)
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

	// Initialize mock S3 uploader
	suite.mockUploader = NewMockS3Uploader()

	// Initialize auth handlers with nil auth service (we'll test handlers directly)
	suite.authHandlers = NewAuthHandlers(nil, suite.mockUploader, nil)

	// Setup Gin router
	gin.SetMode(gin.TestMode)
	suite.router = gin.New()
	suite.setupRoutes()
}

// setupRoutes configures the test router with auth routes
func (suite *AuthTestSuite) setupRoutes() {
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

	users := api.Group("/users")
	users.Use(authMiddleware)
	users.POST("/upload-profile-picture", suite.authHandlers.UploadProfilePicture)
}

// TearDownSuite cleans up - only closes connection, doesn't drop tables
// to allow other test suites to run against the same database
func (suite *AuthTestSuite) TearDownSuite() {
	if suite.db != nil {
		sqlDB, _ := suite.db.DB()
		sqlDB.Close()
	}
}

// SetupTest creates fresh test data before each test
func (suite *AuthTestSuite) SetupTest() {
	// Reset mock uploader state
	suite.mockUploader.UploadedFiles = make([]MockUploadedFile, 0)
	suite.mockUploader.ShouldFail = false
	suite.mockUploader.FailError = nil

	// Clean existing data
	suite.db.Exec("TRUNCATE TABLE oauth_providers RESTART IDENTITY CASCADE")
	suite.db.Exec("TRUNCATE TABLE users RESTART IDENTITY CASCADE")

	// Create test user with unique identifiers per test
	testID := fmt.Sprintf("%d", time.Now().UnixNano())
	suite.testUser = &models.User{
		Email:        fmt.Sprintf("testuser_%s@test.com", testID),
		Username:     fmt.Sprintf("testuser_%s", testID),
		DisplayName:  "Test User",
		StreamUserID: fmt.Sprintf("stream_test_user_%s", testID),
	}
	require.NoError(suite.T(), suite.db.Create(suite.testUser).Error)
}

// =============================================================================
// PROFILE PICTURE UPLOAD TESTS
// =============================================================================

// TestUploadProfilePictureSuccess tests successful profile picture upload
func (suite *AuthTestSuite) TestUploadProfilePictureSuccess() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	// Create multipart form with image
	body, contentType := createMultipartForm(t, "profile_picture", "test-image.jpg", []byte("fake jpeg data"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "Profile picture uploaded successfully", response["message"])
	assert.NotEmpty(t, response["url"])
	assert.NotEmpty(t, response["profile_picture_url"])

	// Verify mock received the upload
	assert.Len(t, suite.mockUploader.UploadedFiles, 1)
	assert.Equal(t, suite.testUser.ID, suite.mockUploader.UploadedFiles[0].UserID)
	assert.Equal(t, "test-image.jpg", suite.mockUploader.UploadedFiles[0].Filename)

	// Verify user's profile_picture_url was updated in database
	var updatedUser models.User
	suite.db.First(&updatedUser, "id = ?", suite.testUser.ID)
	assert.Equal(t, suite.mockUploader.ReturnURL, updatedUser.ProfilePictureURL)
}

// TestUploadProfilePicturePNG tests PNG upload
func (suite *AuthTestSuite) TestUploadProfilePicturePNG() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	body, contentType := createMultipartForm(t, "profile_picture", "avatar.png", []byte("fake png data"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
	assert.Len(t, suite.mockUploader.UploadedFiles, 1)
	assert.Equal(t, "avatar.png", suite.mockUploader.UploadedFiles[0].Filename)
}

// TestUploadProfilePictureGIF tests GIF upload
func (suite *AuthTestSuite) TestUploadProfilePictureGIF() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	body, contentType := createMultipartForm(t, "profile_picture", "animated.gif", []byte("fake gif data"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// TestUploadProfilePictureWebP tests WebP upload
func (suite *AuthTestSuite) TestUploadProfilePictureWebP() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	body, contentType := createMultipartForm(t, "profile_picture", "modern.webp", []byte("fake webp data"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)
}

// TestUploadProfilePictureInvalidFileType tests rejection of unsupported file types
func (suite *AuthTestSuite) TestUploadProfilePictureInvalidFileType() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	// Try to upload a .txt file
	body, contentType := createMultipartForm(t, "profile_picture", "document.txt", []byte("not an image"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Contains(t, response["message"], "unsupported file type")

	// Verify no file was uploaded
	assert.Len(t, suite.mockUploader.UploadedFiles, 0)
}

// TestUploadProfilePictureInvalidPDF tests rejection of PDF files
func (suite *AuthTestSuite) TestUploadProfilePictureInvalidPDF() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	body, contentType := createMultipartForm(t, "profile_picture", "resume.pdf", []byte("pdf content"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)
	assert.Len(t, suite.mockUploader.UploadedFiles, 0)
}

// TestUploadProfilePictureNoFile tests error when no file is provided
func (suite *AuthTestSuite) TestUploadProfilePictureNoFile() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	// Create empty multipart form without file
	body := &bytes.Buffer{}
	writer := multipart.NewWriter(body)
	writer.Close()

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", writer.FormDataContentType())
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "missing_file", response["error"])
}

// TestUploadProfilePictureUnauthenticated tests 401 when not authenticated
func (suite *AuthTestSuite) TestUploadProfilePictureUnauthenticated() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	body, contentType := createMultipartForm(t, "profile_picture", "test.jpg", []byte("data"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	// Note: NOT setting X-User-ID header

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusUnauthorized, w.Code)
}

// TestUploadProfilePictureS3Failure tests handling of S3 upload failure
func (suite *AuthTestSuite) TestUploadProfilePictureS3Failure() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	// Configure mock to fail
	suite.mockUploader.ShouldFail = true
	suite.mockUploader.FailError = fmt.Errorf("S3 connection timeout")
	suite.mockUploader.ValidateFileType = false // Skip validation to test S3 failure

	body, contentType := createMultipartForm(t, "profile_picture", "test.jpg", []byte("image data"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "upload_failed", response["error"])
}

// TestUploadProfilePictureUserNotFound tests error when user doesn't exist
func (suite *AuthTestSuite) TestUploadProfilePictureUserNotFound() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	body, contentType := createMultipartForm(t, "profile_picture", "test.jpg", []byte("image data"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", "non-existent-user-id")

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Upload succeeds but database update fails
	assert.Equal(t, http.StatusInternalServerError, w.Code)

	var response map[string]interface{}
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "user_not_found", response["error"])
}

// TestUploadProfilePictureDatabaseUpdateFailsButReturnsURL tests partial failure handling
func (suite *AuthTestSuite) TestUploadProfilePictureUpdatesCorrectUser() {
	suite.T().Skip("TODO: Fix multipart form handling in tests")
	t := suite.T()

	// Create a second user
	testID := fmt.Sprintf("%d", time.Now().UnixNano())
	secondUser := &models.User{
		Email:        fmt.Sprintf("seconduser_%s@test.com", testID),
		Username:     fmt.Sprintf("seconduser_%s", testID),
		DisplayName:  "Second User",
		StreamUserID: fmt.Sprintf("stream_second_user_%s", testID),
	}
	require.NoError(t, suite.db.Create(secondUser).Error)

	// Upload for first user
	body, contentType := createMultipartForm(t, "profile_picture", "first-user.jpg", []byte("data"))

	req, _ := http.NewRequest("POST", "/api/v1/users/upload-profile-picture", body)
	req.Header.Set("Content-Type", contentType)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)
	assert.Equal(t, http.StatusOK, w.Code)

	// Verify only first user was updated
	var firstUserUpdated models.User
	var secondUserUpdated models.User
	suite.db.First(&firstUserUpdated, "id = ?", suite.testUser.ID)
	suite.db.First(&secondUserUpdated, "id = ?", secondUser.ID)

	assert.Equal(t, suite.mockUploader.ReturnURL, firstUserUpdated.ProfilePictureURL)
	assert.Empty(t, secondUserUpdated.ProfilePictureURL)
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

// createMultipartForm creates a multipart form with a file for testing
func createMultipartForm(t *testing.T, fieldName, filename string, fileContent []byte) (*bytes.Buffer, string) {
	body := &bytes.Buffer{}
	writer := multipart.NewWriter(body)

	part, err := writer.CreateFormFile(fieldName, filename)
	require.NoError(t, err)

	_, err = part.Write(fileContent)
	require.NoError(t, err)

	err = writer.Close()
	require.NoError(t, err)

	return body, writer.FormDataContentType()
}

// getEnvOrDefaultAuth returns environment variable or default value
func getEnvOrDefaultAuth(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

// =============================================================================
// RUN THE TEST SUITE
// =============================================================================

func TestAuthSuite(t *testing.T) {
	// Skip if no test database available
	if os.Getenv("SKIP_DB_TESTS") == "true" {
		t.Skip("Skipping database tests")
	}

	suite.Run(t, new(AuthTestSuite))
}
