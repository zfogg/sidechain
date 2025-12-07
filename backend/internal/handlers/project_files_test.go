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
// PROJECT FILE ENDPOINT TESTS (R.3.4)
// =============================================================================

func (suite *HandlersTestSuite) TestCreateProjectFileSuccess() {
	t := suite.T()

	body := map[string]any{
		"filename":  "My Track.als",
		"file_url":  "https://cdn.sidechain.app/projects/test-123.als",
		"file_size": 1024 * 1024, // 1MB
		"daw_type":  "ableton",
		"is_public": true,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/project-files", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code, "Response body: %s", w.Body.String())

	var response models.ProjectFile
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotEmpty(t, response.ID)
	assert.Equal(t, "My Track.als", response.Filename)
	assert.Equal(t, "ableton", response.DAWType)
	assert.Equal(t, int64(1024*1024), response.FileSize)
	assert.True(t, response.IsPublic)

	// Verify in database
	var projectFile models.ProjectFile
	err := suite.db.First(&projectFile, "id = ?", response.ID).Error
	require.NoError(t, err)
	assert.Equal(t, suite.testUser.ID, projectFile.UserID)
}

func (suite *HandlersTestSuite) TestCreateProjectFileAutoDetectDAW() {
	t := suite.T()

	body := map[string]any{
		"filename":  "Beat.flp",
		"file_url":  "https://cdn.sidechain.app/projects/test-456.flp",
		"file_size": 512 * 1024, // 512KB
		// No daw_type - should auto-detect from extension
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/project-files", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code, "Response body: %s", w.Body.String())

	var response models.ProjectFile
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.Equal(t, "fl_studio", response.DAWType) // Auto-detected from .flp
}

func (suite *HandlersTestSuite) TestCreateProjectFileFileTooLarge() {
	t := suite.T()

	body := map[string]any{
		"filename":  "Huge Project.als",
		"file_url":  "https://cdn.sidechain.app/projects/huge.als",
		"file_size": 100 * 1024 * 1024, // 100MB - exceeds 50MB limit
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/project-files", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "file_too_large", response["error"])
}

func (suite *HandlersTestSuite) TestCreateProjectFileMissingFilename() {
	t := suite.T()

	body := map[string]any{
		"file_url":  "https://cdn.sidechain.app/projects/test.als",
		"file_size": 1024,
		// Missing filename
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/project-files", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)
}

func (suite *HandlersTestSuite) TestGetProjectFileSuccess() {
	t := suite.T()

	// Create a project file first
	projectFile := &models.ProjectFile{
		UserID:   suite.testUser.ID,
		Filename: "Test.als",
		FileURL:  "https://cdn.sidechain.app/projects/test.als",
		DAWType:  "ableton",
		FileSize: 1024,
		IsPublic: true,
	}
	suite.db.Create(projectFile)

	req, _ := http.NewRequest("GET", "/api/v1/project-files/"+projectFile.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response models.ProjectFile
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, projectFile.ID, response.ID)
	assert.Equal(t, "Test.als", response.Filename)
}

func (suite *HandlersTestSuite) TestGetProjectFileNotFound() {
	t := suite.T()

	req, _ := http.NewRequest("GET", "/api/v1/project-files/nonexistent-id", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)
}

func (suite *HandlersTestSuite) TestGetProjectFilePrivateAccessDenied() {
	t := suite.T()

	// Create a private project file owned by another user
	otherUser := models.User{Username: "otheruser2", Email: "other2@test.com", DisplayName: "Other User 2"}
	err := suite.db.Create(&otherUser).Error
	require.NoError(t, err)
	require.NotEmpty(t, otherUser.ID, "Other user ID should be set")
	require.NotEqual(t, suite.testUser.ID, otherUser.ID, "Other user should have different ID than test user")

	projectFile := &models.ProjectFile{
		UserID:   otherUser.ID,
		Filename: "Private.als",
		FileURL:  "https://cdn.sidechain.app/projects/private.als",
		DAWType:  "ableton",
		FileSize: 1024,
		IsPublic: true, // Will be updated to false
	}
	err = suite.db.Create(projectFile).Error
	require.NoError(t, err)
	// Update to private (GORM's default:true ignores false during Create)
	err = suite.db.Model(projectFile).Update("is_public", false).Error
	require.NoError(t, err)

	req, _ := http.NewRequest("GET", "/api/v1/project-files/"+projectFile.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code, "Should deny access to private file not owned by requester. Response: %s", w.Body.String())
}

func (suite *HandlersTestSuite) TestListProjectFilesSuccess() {
	t := suite.T()

	// Create some project files
	for i := 0; i < 3; i++ {
		projectFile := &models.ProjectFile{
			UserID:   suite.testUser.ID,
			Filename: "Project" + string(rune('A'+i)) + ".als",
			FileURL:  "https://cdn.sidechain.app/projects/test" + string(rune('0'+i)) + ".als",
			DAWType:  "ableton",
			FileSize: 1024,
			IsPublic: true,
		}
		suite.db.Create(projectFile)
	}

	req, _ := http.NewRequest("GET", "/api/v1/project-files", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	projectFiles := response["project_files"].([]any)
	assert.GreaterOrEqual(t, len(projectFiles), 3)
}

func (suite *HandlersTestSuite) TestListProjectFilesWithDAWFilter() {
	t := suite.T()

	// Create project files with different DAW types
	flpFile := &models.ProjectFile{
		UserID:   suite.testUser.ID,
		Filename: "Beat.flp",
		FileURL:  "https://cdn.sidechain.app/projects/beat.flp",
		DAWType:  "fl_studio",
		FileSize: 1024,
		IsPublic: true,
	}
	suite.db.Create(flpFile)

	alsFile := &models.ProjectFile{
		UserID:   suite.testUser.ID,
		Filename: "Track.als",
		FileURL:  "https://cdn.sidechain.app/projects/track.als",
		DAWType:  "ableton",
		FileSize: 1024,
		IsPublic: true,
	}
	suite.db.Create(alsFile)

	// Filter by FL Studio only
	req, _ := http.NewRequest("GET", "/api/v1/project-files?daw_type=fl_studio", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)

	projectFiles := response["project_files"].([]any)
	for _, pf := range projectFiles {
		pfMap := pf.(map[string]any)
		assert.Equal(t, "fl_studio", pfMap["daw_type"])
	}
}

func (suite *HandlersTestSuite) TestDeleteProjectFileSuccess() {
	t := suite.T()

	projectFile := &models.ProjectFile{
		UserID:   suite.testUser.ID,
		Filename: "ToDelete.als",
		FileURL:  "https://cdn.sidechain.app/projects/delete.als",
		DAWType:  "ableton",
		FileSize: 1024,
		IsPublic: true,
	}
	suite.db.Create(projectFile)

	req, _ := http.NewRequest("DELETE", "/api/v1/project-files/"+projectFile.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	// Verify soft deleted
	var deleted models.ProjectFile
	err := suite.db.Unscoped().First(&deleted, "id = ?", projectFile.ID).Error
	require.NoError(t, err)
	assert.NotNil(t, deleted.DeletedAt)
}

func (suite *HandlersTestSuite) TestDeleteProjectFileNotOwner() {
	t := suite.T()

	// Create file owned by another user
	otherUser := models.User{Username: "otheruser3", Email: "other3@test.com", DisplayName: "Other User 3"}
	suite.db.Create(&otherUser)

	projectFile := &models.ProjectFile{
		UserID:   otherUser.ID,
		Filename: "OthersFile.als",
		FileURL:  "https://cdn.sidechain.app/projects/others.als",
		DAWType:  "ableton",
		FileSize: 1024,
		IsPublic: true,
	}
	suite.db.Create(projectFile)

	req, _ := http.NewRequest("DELETE", "/api/v1/project-files/"+projectFile.ID, nil)
	req.Header.Set("X-User-ID", suite.testUser.ID) // Different user

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusForbidden, w.Code)
}

func (suite *HandlersTestSuite) TestDownloadProjectFileSuccess() {
	t := suite.T()

	projectFile := &models.ProjectFile{
		UserID:        suite.testUser.ID,
		Filename:      "Download.als",
		FileURL:       "https://cdn.sidechain.app/projects/download.als",
		DAWType:       "ableton",
		FileSize:      1024,
		IsPublic:      true,
		DownloadCount: 5,
	}
	suite.db.Create(projectFile)

	req, _ := http.NewRequest("GET", "/api/v1/project-files/"+projectFile.ID+"/download", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	// Should redirect to CDN URL
	assert.Equal(t, http.StatusFound, w.Code)
	assert.Equal(t, projectFile.FileURL, w.Header().Get("Location"))
	assert.Contains(t, w.Header().Get("Content-Disposition"), "Download.als")

	// Verify download count incremented
	var updated models.ProjectFile
	suite.db.First(&updated, "id = ?", projectFile.ID)
	assert.Equal(t, 6, updated.DownloadCount)
}

func (suite *HandlersTestSuite) TestGetPostProjectFileSuccess() {
	t := suite.T()

	// Create a post
	post := &models.AudioPost{
		UserID:           suite.testUser.ID,
		OriginalFilename: "track.mp3",
		FileSize:         1024,
		ProcessingStatus: "complete",
	}
	suite.db.Create(post)

	// Create project file linked to the post
	projectFile := &models.ProjectFile{
		UserID:      suite.testUser.ID,
		AudioPostID: &post.ID,
		Filename:    "LinkedProject.als",
		FileURL:     "https://cdn.sidechain.app/projects/linked.als",
		DAWType:     "ableton",
		FileSize:    1024,
		IsPublic:    true,
	}
	suite.db.Create(projectFile)

	req, _ := http.NewRequest("GET", "/api/v1/posts/"+post.ID+"/project-file", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusOK, w.Code)

	var response models.ProjectFile
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, projectFile.ID, response.ID)
	assert.Equal(t, post.ID, *response.AudioPostID)
}

func (suite *HandlersTestSuite) TestGetPostProjectFileNotFound() {
	t := suite.T()

	// Create a post without a project file
	post := &models.AudioPost{
		UserID:           suite.testUser.ID,
		OriginalFilename: "track.mp3",
		FileSize:         1024,
		ProcessingStatus: "complete",
	}
	suite.db.Create(post)

	req, _ := http.NewRequest("GET", "/api/v1/posts/"+post.ID+"/project-file", nil)
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusNotFound, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "no_project_file", response["error"])
}

func (suite *HandlersTestSuite) TestCreateProjectFileWithPostLink() {
	t := suite.T()

	// Create a post first
	post := &models.AudioPost{
		UserID:           suite.testUser.ID,
		OriginalFilename: "track.mp3",
		FileSize:         1024,
		ProcessingStatus: "complete",
	}
	suite.db.Create(post)

	body := map[string]any{
		"filename":      "Track Project.als",
		"file_url":      "https://cdn.sidechain.app/projects/track-project.als",
		"file_size":     2048,
		"audio_post_id": post.ID,
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/project-files", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusCreated, w.Code)

	var response models.ProjectFile
	json.Unmarshal(w.Body.Bytes(), &response)

	assert.NotNil(t, response.AudioPostID)
	assert.Equal(t, post.ID, *response.AudioPostID)
}

func (suite *HandlersTestSuite) TestCreateProjectFileWithInvalidPostLink() {
	t := suite.T()

	body := map[string]any{
		"filename":      "Track Project.als",
		"file_url":      "https://cdn.sidechain.app/projects/track-project.als",
		"file_size":     2048,
		"audio_post_id": "nonexistent-post-id",
	}
	jsonBody, _ := json.Marshal(body)

	req, _ := http.NewRequest("POST", "/api/v1/project-files", bytes.NewBuffer(jsonBody))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("X-User-ID", suite.testUser.ID)

	w := httptest.NewRecorder()
	suite.router.ServeHTTP(w, req)

	assert.Equal(t, http.StatusBadRequest, w.Code)

	var response map[string]any
	json.Unmarshal(w.Body.Bytes(), &response)
	assert.Equal(t, "invalid_post_id", response["error"])
}
