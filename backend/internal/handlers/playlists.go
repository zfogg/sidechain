package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
	"gorm.io/gorm"
)

// CreatePlaylist creates a new playlist
// POST /api/v1/playlists
func (h *Handlers) CreatePlaylist(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var req struct {
		Name            string `json:"name" binding:"required,min=1,max=100"`
		Description     string `json:"description,omitempty" binding:"max=500"`
		IsCollaborative bool   `json:"is_collaborative"`
		IsPublic        bool   `json:"is_public"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	playlist := &models.Playlist{
		Name:            req.Name,
		Description:     req.Description,
		OwnerID:         currentUser.ID,
		IsCollaborative: req.IsCollaborative,
		IsPublic:        req.IsPublic,
	}

	if err := database.DB.Create(playlist).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "creation_failed",
			"message": "Failed to create playlist",
		})
		return
	}

	// Load owner relationship
	database.DB.Preload("Owner").First(playlist, playlist.ID)

	c.JSON(http.StatusCreated, gin.H{"playlist": playlist})
}

// GetPlaylists returns user's playlists
// GET /api/v1/playlists
func (h *Handlers) GetPlaylists(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	filter := c.DefaultQuery("filter", "all") // all, owned, collaborated, public

	var playlists []models.Playlist
	query := database.DB.Preload("Owner")

	switch filter {
	case "owned":
		query = query.Where("owner_id = ?", currentUser.ID)
	case "collaborated":
		// Get playlists where user is a collaborator (but not owner)
		var collaboratorPlaylistIDs []string
		database.DB.Model(&models.PlaylistCollaborator{}).
			Where("user_id = ?", currentUser.ID).
			Pluck("playlist_id", &collaboratorPlaylistIDs)
		query = query.Where("id IN ? AND owner_id != ?", collaboratorPlaylistIDs, currentUser.ID)
	case "public":
		query = query.Where("is_public = ?", true)
	default:
		// all: owned + collaborated + public
		var collaboratorPlaylistIDs []string
		database.DB.Model(&models.PlaylistCollaborator{}).
			Where("user_id = ?", currentUser.ID).
			Pluck("playlist_id", &collaboratorPlaylistIDs)
		query = query.Where("owner_id = ? OR id IN ? OR is_public = ?",
			currentUser.ID, collaboratorPlaylistIDs, true)
	}

	if err := query.Order("created_at DESC").Find(&playlists).Error; err != nil {
		util.RespondInternalError(c, "query_failed", "Failed to fetch playlists")
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"playlists": playlists,
		"count":     len(playlists),
	})
}

// GetPlaylist returns a single playlist with entries
// GET /api/v1/playlists/:id
func (h *Handlers) GetPlaylist(c *gin.Context) {
	playlistID := c.Param("id")
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var playlist models.Playlist
	if err := database.DB.
		Preload("Owner").
		Preload("Entries", func(db *gorm.DB) *gorm.DB {
			return db.Order("position ASC")
		}).
		Preload("Entries.Post.User").
		Preload("Entries.AddedByUser").
		Preload("Collaborators.User").
		First(&playlist, "id = ?", playlistID).Error; err != nil {
		if util.HandleDBError(c, err, "playlist") {
			return
		}
		util.RespondInternalError(c, "query_failed", "Failed to fetch playlist")
		return
	}

	// Check access permissions
	if !playlist.HasAccess(currentUser.ID) {
		util.RespondForbidden(c, "forbidden", "You don't have access to this playlist")
		return
	}

	// Sort entries by position
	// Note: GORM doesn't automatically sort, so we'll do it in the response
	// or we can add ORDER BY in the query

	c.JSON(http.StatusOK, gin.H{"playlist": playlist})
}

// UpdatePlaylist updates playlist metadata
// PUT /api/v1/playlists/:id
func (h *Handlers) UpdatePlaylist(c *gin.Context) {
	playlistID := c.Param("id")
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var playlist models.Playlist
	if err := database.DB.Preload("Owner").First(&playlist, "id = ?", playlistID).Error; err != nil {
		if util.HandleDBError(c, err, "playlist") {
			return
		}
		util.RespondInternalError(c, "query_failed", "Failed to fetch playlist")
		return
	}

	// Only owner can update playlist
	if playlist.OwnerID != currentUser.ID {
		util.RespondForbidden(c, "forbidden", "You don't have permission to update this playlist")
		return
	}

	var req struct {
		Name            *string `json:"name"`
		Description     *string `json:"description"`
		IsCollaborative *bool   `json:"is_collaborative"`
		IsPublic        *bool   `json:"is_public"`
		CoverUrl        *string `json:"cover_url"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Update only provided fields
	updates := map[string]interface{}{}
	if req.Name != nil {
		updates["name"] = *req.Name
	}
	if req.Description != nil {
		updates["description"] = *req.Description
	}
	if req.IsCollaborative != nil {
		updates["is_collaborative"] = *req.IsCollaborative
	}
	if req.IsPublic != nil {
		updates["is_public"] = *req.IsPublic
	}
	if req.CoverUrl != nil {
		updates["cover_url"] = *req.CoverUrl
	}

	if err := database.DB.Model(&playlist).Updates(updates).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "update_failed",
			"message": "Failed to update playlist",
		})
		return
	}

	// Reload with relationships
	database.DB.Preload("Owner").First(&playlist, playlistID)

	c.JSON(http.StatusOK, gin.H{"playlist": playlist})
}

// AddPlaylistEntry adds a post to a playlist
// POST /api/v1/playlists/:id/entries
func (h *Handlers) AddPlaylistEntry(c *gin.Context) {
	playlistID := c.Param("id")
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var req struct {
		PostID   string `json:"post_id" binding:"required"`
		Position *int   `json:"position,omitempty"` // Optional: insert at specific position
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Get playlist
	var playlist models.Playlist
	if err := database.DB.Preload("Collaborators").First(&playlist, "id = ?", playlistID).Error; err != nil {
		if util.HandleDBError(c, err, "playlist") {
			return
		}
		util.RespondInternalError(c, "query_failed")
		return
	}

	// Check permissions
	userRole := playlist.GetUserRole(currentUser.ID)
	if userRole != models.PlaylistRoleOwner && userRole != models.PlaylistRoleEditor {
		util.RespondForbidden(c, "forbidden", "You don't have permission to add entries to this playlist")
		return
	}

	// Verify post exists
	var post models.AudioPost
	if err := database.DB.First(&post, "id = ?", req.PostID).Error; err != nil {
		if util.HandleDBError(c, err, "post") {
			return
		}
		util.RespondInternalError(c, "query_failed")
		return
	}

	// Check if post is already in playlist
	var existingEntry models.PlaylistEntry
	if err := database.DB.Where("playlist_id = ? AND post_id = ?", playlistID, req.PostID).
		First(&existingEntry).Error; err == nil {
		c.JSON(http.StatusConflict, gin.H{
			"error":   "already_exists",
			"message": "Post is already in this playlist",
		})
		return
	}

	// Determine position
	position := 0
	if req.Position != nil {
		position = *req.Position
	} else {
		// Append to end: get max position + 1
		var maxPosition int
		database.DB.Model(&models.PlaylistEntry{}).
			Where("playlist_id = ?", playlistID).
			Select("COALESCE(MAX(position), -1)").
			Scan(&maxPosition)
		position = maxPosition + 1
	}

	// Shift existing entries if inserting at specific position
	if req.Position != nil {
		database.DB.Model(&models.PlaylistEntry{}).
			Where("playlist_id = ? AND position >= ?", playlistID, position).
			Update("position", gorm.Expr("position + 1"))
	}

	entry := &models.PlaylistEntry{
		PlaylistID:    playlistID,
		PostID:        req.PostID,
		AddedByUserID: currentUser.ID,
		Position:      position,
	}

	if err := database.DB.Create(entry).Error; err != nil {
		util.RespondInternalError(c, "creation_failed", "Failed to add entry to playlist")
		return
	}

	// Load relationships
	database.DB.Preload("Post").Preload("AddedByUser").First(entry, entry.ID)

	c.JSON(http.StatusCreated, entry)
}

// DeletePlaylistEntry removes a post from a playlist
// DELETE /api/v1/playlists/:id/entries/:entry_id
func (h *Handlers) DeletePlaylistEntry(c *gin.Context) {
	playlistID := c.Param("id")
	entryID := c.Param("entry_id")
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	// Get playlist
	var playlist models.Playlist
	if err := database.DB.Preload("Collaborators").First(&playlist, "id = ?", playlistID).Error; err != nil {
		if util.HandleDBError(c, err, "playlist") {
			return
		}
		util.RespondInternalError(c, "query_failed")
		return
	}

	// Check permissions
	userRole := playlist.GetUserRole(currentUser.ID)
	if userRole != models.PlaylistRoleOwner && userRole != models.PlaylistRoleEditor {
		util.RespondForbidden(c, "forbidden", "You don't have permission to remove entries from this playlist")
		return
	}

	// Get entry to find its position
	var entry models.PlaylistEntry
	if err := database.DB.First(&entry, "id = ? AND playlist_id = ?", entryID, playlistID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "entry_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "query_failed"})
		return
	}

	// Delete entry
	if err := database.DB.Delete(&entry).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "deletion_failed",
			"message": "Failed to remove entry from playlist",
		})
		return
	}

	// Reorder remaining entries (shift positions down)
	database.DB.Model(&models.PlaylistEntry{}).
		Where("playlist_id = ? AND position > ?", playlistID, entry.Position).
		Update("position", gorm.Expr("position - 1"))

	c.JSON(http.StatusOK, gin.H{"message": "Entry removed from playlist"})
}

// AddPlaylistCollaborator adds a collaborator to a playlist
// POST /api/v1/playlists/:id/collaborators
func (h *Handlers) AddPlaylistCollaborator(c *gin.Context) {
	playlistID := c.Param("id")
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	var req struct {
		UserID string `json:"user_id" binding:"required"`
		Role   string `json:"role" binding:"required,oneof=editor viewer"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		util.RespondBadRequest(c, "invalid_request", err.Error())
		return
	}

	// Get playlist
	var playlist models.Playlist
	if err := database.DB.First(&playlist, "id = ?", playlistID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "playlist_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "query_failed"})
		return
	}

	// Check permissions (owner only)
	if playlist.OwnerID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{
			"error":   "forbidden",
			"message": "Only the playlist owner can add collaborators",
		})
		return
	}

	// Verify user exists
	var targetUser models.User
	if err := database.DB.First(&targetUser, "id = ?", req.UserID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user_not_found"})
		return
	}

	// Don't allow adding owner as collaborator
	if req.UserID == playlist.OwnerID {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": "Playlist owner cannot be added as a collaborator",
		})
		return
	}

	// Check if already a collaborator
	var existing models.PlaylistCollaborator
	if err := database.DB.Where("playlist_id = ? AND user_id = ?", playlistID, req.UserID).
		First(&existing).Error; err == nil {
		// Update role
		existing.Role = models.PlaylistCollaboratorRole(req.Role)
		if err := database.DB.Save(&existing).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "update_failed"})
			return
		}
		c.JSON(http.StatusOK, existing)
		return
	}

	// Create new collaborator
	collaborator := &models.PlaylistCollaborator{
		PlaylistID: playlistID,
		UserID:     req.UserID,
		Role:       models.PlaylistCollaboratorRole(req.Role),
	}

	if err := database.DB.Create(collaborator).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "creation_failed",
			"message": "Failed to add collaborator",
		})
		return
	}

	// Load user relationship
	database.DB.Preload("User").First(collaborator, collaborator.ID)

	c.JSON(http.StatusCreated, collaborator)
}

// DeletePlaylistCollaborator removes a collaborator from a playlist
// DELETE /api/v1/playlists/:id/collaborators/:user_id
func (h *Handlers) DeletePlaylistCollaborator(c *gin.Context) {
	playlistID := c.Param("id")
	userID := c.Param("user_id")
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

	// Get playlist
	var playlist models.Playlist
	if err := database.DB.First(&playlist, "id = ?", playlistID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "playlist_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "query_failed"})
		return
	}

	// Check permissions (owner only)
	if playlist.OwnerID != currentUser.ID {
		c.JSON(http.StatusForbidden, gin.H{
			"error":   "forbidden",
			"message": "Only the playlist owner can remove collaborators",
		})
		return
	}

	// Delete collaborator
	result := database.DB.Where("playlist_id = ? AND user_id = ?", playlistID, userID).
		Delete(&models.PlaylistCollaborator{})

	if result.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "collaborator_not_found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Collaborator removed from playlist"})
}
