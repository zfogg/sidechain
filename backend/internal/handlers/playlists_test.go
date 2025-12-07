package handlers

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"

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

// PlaylistTestSuite tests playlist handlers
type PlaylistTestSuite struct {
	suite.Suite
	db       *gorm.DB
	router   *gin.Engine
	handlers *Handlers
	user1    *models.User
	user2    *models.User
	post1    *models.AudioPost
	post2    *models.AudioPost
}

// SetupSuite initializes test database and handlers
func (suite *PlaylistTestSuite) SetupSuite() {
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
		suite.T().Skipf("Skipping playlist tests: database not available (%v)", err)
		return
	}

	database.DB = db

	// Run migrations
	err = db.AutoMigrate(
		&models.User{},
		&models.AudioPost{},
		&models.Playlist{},
		&models.PlaylistEntry{},
		&models.PlaylistCollaborator{},
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

	// Create test posts
	suite.post1 = &models.AudioPost{
		UserID:   suite.user1.ID,
		AudioURL: "https://example.com/audio1.mp3",
		BPM:      120,
	}
	suite.db.Create(suite.post1)

	suite.post2 = &models.AudioPost{
		UserID:   suite.user2.ID,
		AudioURL: "https://example.com/audio2.mp3",
		BPM:      140,
	}
	suite.db.Create(suite.post2)
}

// setupRoutes configures the test router
func (suite *PlaylistTestSuite) setupRoutes() {
	authMiddleware := func(c *gin.Context) {
		userID := c.GetHeader("X-User-ID")
		if userID == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
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
	api.Use(authMiddleware)
	{
		api.POST("/playlists", suite.handlers.CreatePlaylist)
		api.GET("/playlists", suite.handlers.GetPlaylists)
		api.GET("/playlists/:id", suite.handlers.GetPlaylist)
		api.POST("/playlists/:id/entries", suite.handlers.AddPlaylistEntry)
		api.DELETE("/playlists/:id/entries/:entry_id", suite.handlers.DeletePlaylistEntry)
		api.POST("/playlists/:id/collaborators", suite.handlers.AddPlaylistCollaborator)
		api.DELETE("/playlists/:id/collaborators/:user_id", suite.handlers.DeletePlaylistCollaborator)
	}
}

// TearDownSuite cleans up test data
func (suite *PlaylistTestSuite) TearDownSuite() {
	if suite.db != nil {
		suite.db.Exec("TRUNCATE TABLE playlists, playlist_entries, playlist_collaborators, audio_posts, users CASCADE")
	}
}

// TestCreatePlaylist tests playlist creation
func (suite *PlaylistTestSuite) TestCreatePlaylist() {
	reqBody := map[string]interface{}{
		"name":             "My Playlist",
		"description":      "Test playlist",
		"is_collaborative": true,
		"is_public":        true,
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", "/api/v1/playlists", bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusCreated, w.Code)

	var response models.Playlist
	err := json.Unmarshal(w.Body.Bytes(), &response)
	require.NoError(suite.T(), err)
	assert.Equal(suite.T(), "My Playlist", response.Name)
	assert.Equal(suite.T(), suite.user1.ID, response.OwnerID)
	assert.True(suite.T(), response.IsCollaborative)
	assert.True(suite.T(), response.IsPublic)
}

// TestGetPlaylists tests listing playlists
func (suite *PlaylistTestSuite) TestGetPlaylists() {
	// Create a playlist
	playlist := &models.Playlist{
		Name:            "User1 Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: false,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	req := httptest.NewRequest("GET", "/api/v1/playlists?filter=owned", nil)
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)

	var response map[string]interface{}
	err := json.Unmarshal(w.Body.Bytes(), &response)
	require.NoError(suite.T(), err)
	assert.Contains(suite.T(), response, "playlists")
}

// TestGetPlaylist tests getting a single playlist
func (suite *PlaylistTestSuite) TestGetPlaylist() {
	playlist := &models.Playlist{
		Name:            "Test Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: false,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	req := httptest.NewRequest("GET", fmt.Sprintf("/api/v1/playlists/%s", playlist.ID), nil)
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)

	var response models.Playlist
	err := json.Unmarshal(w.Body.Bytes(), &response)
	require.NoError(suite.T(), err)
	assert.Equal(suite.T(), "Test Playlist", response.Name)
}

// TestAddPlaylistEntry tests adding a post to a playlist
func (suite *PlaylistTestSuite) TestAddPlaylistEntry() {
	playlist := &models.Playlist{
		Name:            "Test Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: true,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	reqBody := map[string]interface{}{
		"post_id": suite.post1.ID,
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/playlists/%s/entries", playlist.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusCreated, w.Code)

	var entry models.PlaylistEntry
	err := json.Unmarshal(w.Body.Bytes(), &entry)
	require.NoError(suite.T(), err)
	assert.Equal(suite.T(), playlist.ID, entry.PlaylistID)
	assert.Equal(suite.T(), suite.post1.ID, entry.PostID)
}

// TestDeletePlaylistEntry tests removing a post from a playlist
func (suite *PlaylistTestSuite) TestDeletePlaylistEntry() {
	playlist := &models.Playlist{
		Name:            "Test Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: true,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	entry := &models.PlaylistEntry{
		PlaylistID:    playlist.ID,
		PostID:        suite.post1.ID,
		AddedByUserID: suite.user1.ID,
		Position:      0,
	}
	suite.db.Create(entry)

	req := httptest.NewRequest("DELETE", fmt.Sprintf("/api/v1/playlists/%s/entries/%s", playlist.ID, entry.ID), nil)
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)

	// Verify entry was deleted
	var count int64
	suite.db.Model(&models.PlaylistEntry{}).Where("id = ?", entry.ID).Count(&count)
	assert.Equal(suite.T(), int64(0), count)
}

// TestAddPlaylistCollaborator tests adding a collaborator
func (suite *PlaylistTestSuite) TestAddPlaylistCollaborator() {
	playlist := &models.Playlist{
		Name:            "Test Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: true,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	reqBody := map[string]interface{}{
		"user_id": suite.user2.ID,
		"role":    "editor",
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/playlists/%s/collaborators", playlist.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusCreated, w.Code)

	var collab models.PlaylistCollaborator
	err := json.Unmarshal(w.Body.Bytes(), &collab)
	require.NoError(suite.T(), err)
	assert.Equal(suite.T(), playlist.ID, collab.PlaylistID)
	assert.Equal(suite.T(), suite.user2.ID, collab.UserID)
	assert.Equal(suite.T(), "editor", collab.Role)
}

// TestDeletePlaylistCollaborator tests removing a collaborator
func (suite *PlaylistTestSuite) TestDeletePlaylistCollaborator() {
	playlist := &models.Playlist{
		Name:            "Test Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: true,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	collab := &models.PlaylistCollaborator{
		PlaylistID: playlist.ID,
		UserID:     suite.user2.ID,
		Role:       "editor",
	}
	suite.db.Create(collab)

	req := httptest.NewRequest("DELETE", fmt.Sprintf("/api/v1/playlists/%s/collaborators/%s", playlist.ID, suite.user2.ID), nil)
	req.Header.Set("X-User-ID", suite.user1.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)

	// Verify collaborator was deleted
	var count int64
	suite.db.Model(&models.PlaylistCollaborator{}).Where("id = ?", collab.ID).Count(&count)
	assert.Equal(suite.T(), int64(0), count)
}

// TestPlaylistPermissions tests permission checks
func (suite *PlaylistTestSuite) TestPlaylistPermissions() {
	// Create private playlist
	playlist := &models.Playlist{
		Name:            "Private Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: false,
		IsPublic:        false,
	}
	suite.db.Create(playlist)

	// User2 should not be able to access private playlist
	req := httptest.NewRequest("GET", fmt.Sprintf("/api/v1/playlists/%s", playlist.ID), nil)
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusForbidden, w.Code)
}

// TestPlaylistEditorPermissions tests editor permissions
func (suite *PlaylistTestSuite) TestPlaylistEditorPermissions() {
	playlist := &models.Playlist{
		Name:            "Collaborative Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: true,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	// Add user2 as editor
	collab := &models.PlaylistCollaborator{
		PlaylistID: playlist.ID,
		UserID:     suite.user2.ID,
		Role:       "editor",
	}
	suite.db.Create(collab)

	// User2 should be able to add entries
	reqBody := map[string]interface{}{
		"post_id": suite.post2.ID,
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/playlists/%s/entries", playlist.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusCreated, w.Code)
}

// TestPlaylistViewerPermissions tests viewer (read-only) permissions
func (suite *PlaylistTestSuite) TestPlaylistViewerPermissions() {
	playlist := &models.Playlist{
		Name:            "Collaborative Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: true,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	// Add user2 as viewer
	collab := &models.PlaylistCollaborator{
		PlaylistID: playlist.ID,
		UserID:     suite.user2.ID,
		Role:       "viewer",
	}
	suite.db.Create(collab)

	// User2 should NOT be able to add entries (viewer is read-only)
	reqBody := map[string]interface{}{
		"post_id": suite.post2.ID,
	}
	body, _ := json.Marshal(reqBody)

	req := httptest.NewRequest("POST", fmt.Sprintf("/api/v1/playlists/%s/entries", playlist.ID), bytes.NewBuffer(body))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusForbidden, w.Code)
}

// TestPlaylistPublicAccess tests public playlist access
func (suite *PlaylistTestSuite) TestPlaylistPublicAccess() {
	playlist := &models.Playlist{
		Name:            "Public Playlist",
		OwnerID:         suite.user1.ID,
		IsCollaborative: false,
		IsPublic:        true,
	}
	suite.db.Create(playlist)

	// User2 should be able to view public playlist
	req := httptest.NewRequest("GET", fmt.Sprintf("/api/v1/playlists/%s", playlist.ID), nil)
	req.Header.Set("X-User-ID", suite.user2.ID)
	w := httptest.NewRecorder()

	suite.router.ServeHTTP(w, req)

	assert.Equal(suite.T(), http.StatusOK, w.Code)
}

// Run the test suite
func TestPlaylistTestSuite(t *testing.T) {
	suite.Run(t, new(PlaylistTestSuite))
}
