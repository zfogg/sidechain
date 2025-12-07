package handlers

import (
	"net/http"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// CreateProjectFileRequest represents the request body for creating a project file
type CreateProjectFileRequest struct {
	Filename    string `json:"filename" binding:"required"`
	FileURL     string `json:"file_url" binding:"required"`
	FileSize    int64  `json:"file_size" binding:"required,gt=0"`
	DAWType     string `json:"daw_type"`           // Optional - auto-detected from filename if not provided
	Description string `json:"description"`        // Optional
	IsPublic    bool   `json:"is_public"`          // Defaults to true
	AudioPostID string `json:"audio_post_id"`      // Optional - link to a post
}

// CreateProjectFile handles POST /api/v1/project-files
// Creates a new project file record (after file is uploaded to CDN)
func (h *Handlers) CreateProjectFile(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}

	var req CreateProjectFileRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_request", "details": err.Error()})
		return
	}

	// Auto-detect DAW type from filename if not provided
	dawType := req.DAWType
	if dawType == "" {
		dawType = models.GetDAWTypeFromExtension(req.Filename)
	}

	// Validate file size
	if req.FileSize > models.MaxProjectFileSize {
		c.JSON(http.StatusBadRequest, gin.H{"error": "file_too_large", "max_size": models.MaxProjectFileSize})
		return
	}

	// Create the project file record
	projectFile := &models.ProjectFile{
		UserID:      userID.(string),
		Filename:    req.Filename,
		FileURL:     req.FileURL,
		DAWType:     dawType,
		FileSize:    req.FileSize,
		Description: req.Description,
		IsPublic:    true, // Default to public
	}

	// Set is_public from request if explicitly provided
	if c.Request.ContentLength > 0 {
		projectFile.IsPublic = req.IsPublic
	}

	// Link to audio post if provided
	if req.AudioPostID != "" {
		// Verify the post exists and belongs to the user
		var post models.AudioPost
		if err := database.DB.First(&post, "id = ? AND user_id = ?", req.AudioPostID, userID).Error; err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "invalid_post_id"})
			return
		}
		projectFile.AudioPostID = &req.AudioPostID
	}

	// Validate the project file
	if err := projectFile.Validate(); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Save to database
	if err := database.DB.Create(projectFile).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_create"})
		return
	}

	c.JSON(http.StatusCreated, projectFile)
}

// GetProjectFile handles GET /api/v1/project-files/:id
// Returns project file metadata with download URL
func (h *Handlers) GetProjectFile(c *gin.Context) {
	projectFileID := c.Param("id")
	userID, _ := c.Get("user_id")

	var projectFile models.ProjectFile
	if err := database.DB.Preload("User").First(&projectFile, "id = ?", projectFileID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "not_found"})
		return
	}

	// Check access - public files are accessible to all, private only to owner
	if !projectFile.IsPublic && (userID == nil || projectFile.UserID != userID.(string)) {
		c.JSON(http.StatusForbidden, gin.H{"error": "access_denied"})
		return
	}

	c.JSON(http.StatusOK, projectFile)
}

// DownloadProjectFile handles GET /api/v1/project-files/:id/download
// Returns a redirect to the file URL or the file content
func (h *Handlers) DownloadProjectFile(c *gin.Context) {
	projectFileID := c.Param("id")
	userID, _ := c.Get("user_id")

	var projectFile models.ProjectFile
	if err := database.DB.First(&projectFile, "id = ?", projectFileID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "not_found"})
		return
	}

	// Check access
	if !projectFile.IsPublic && (userID == nil || projectFile.UserID != userID.(string)) {
		c.JSON(http.StatusForbidden, gin.H{"error": "access_denied"})
		return
	}

	// Increment download count
	database.DB.Model(&projectFile).Update("download_count", projectFile.DownloadCount+1)

	// Set content-disposition header for download
	c.Header("Content-Disposition", "attachment; filename=\""+projectFile.Filename+"\"")

	// Redirect to the CDN URL
	c.Redirect(http.StatusFound, projectFile.FileURL)
}

// ListProjectFiles handles GET /api/v1/project-files
// Lists the current user's project files
func (h *Handlers) ListProjectFiles(c *gin.Context) {
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}

	// Parse pagination
	limit := 20
	offset := 0
	if l := c.Query("limit"); l != "" {
		if parsed, err := parseIntParam(l); err == nil && parsed > 0 && parsed <= 100 {
			limit = parsed
		}
	}
	if o := c.Query("offset"); o != "" {
		if parsed, err := parseIntParam(o); err == nil && parsed >= 0 {
			offset = parsed
		}
	}

	// Optional DAW type filter
	dawType := c.Query("daw_type")

	// Build query
	query := database.DB.Where("user_id = ?", userID)
	if dawType != "" {
		query = query.Where("daw_type = ?", dawType)
	}

	var projectFiles []models.ProjectFile
	if err := query.Order("created_at DESC").Limit(limit).Offset(offset).Find(&projectFiles).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_fetch"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"project_files": projectFiles,
		"limit":         limit,
		"offset":        offset,
	})
}

// DeleteProjectFile handles DELETE /api/v1/project-files/:id
// Soft deletes a project file (owner only)
func (h *Handlers) DeleteProjectFile(c *gin.Context) {
	projectFileID := c.Param("id")
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not_authenticated"})
		return
	}

	var projectFile models.ProjectFile
	if err := database.DB.First(&projectFile, "id = ?", projectFileID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "not_found"})
		return
	}

	// Check ownership
	if projectFile.UserID != userID.(string) {
		c.JSON(http.StatusForbidden, gin.H{"error": "not_owner"})
		return
	}

	// Soft delete
	if err := database.DB.Delete(&projectFile).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed_to_delete"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"deleted": true})
}

// GetPostProjectFile handles GET /api/v1/posts/:id/project-file
// Returns the project file associated with a post (if any)
func (h *Handlers) GetPostProjectFile(c *gin.Context) {
	postID := c.Param("id")

	var projectFile models.ProjectFile
	if err := database.DB.Preload("User").First(&projectFile, "audio_post_id = ?", postID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "no_project_file"})
		return
	}

	c.JSON(http.StatusOK, projectFile)
}

// parseIntParam is a helper to parse int from query string
func parseIntParam(s string) (int, error) {
	return strconv.Atoi(s)
}

// ValidateProjectFileExtension checks if a file extension is allowed
func ValidateProjectFileExtension(filename string) bool {
	ext := strings.ToLower(filepath.Ext(filename))
	_, exists := models.ProjectFileExtensions[ext]
	return exists
}
