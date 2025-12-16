package api

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// Playlist represents a user's playlist
type Playlist struct {
	ID            string    `json:"id"`
	OwnerID       string    `json:"owner_id"`
	Owner         User      `json:"owner"`
	Name          string    `json:"name"`
	Description   string    `json:"description"`
	IsPublic      bool      `json:"is_public"`
	PostCount     int       `json:"post_count"`
	Collaborators []User    `json:"collaborators"`
	CreatedAt     time.Time `json:"created_at"`
	UpdatedAt     time.Time `json:"updated_at"`
}

// PlaylistEntry represents a post in a playlist
type PlaylistEntry struct {
	ID        string    `json:"id"`
	PostID    string    `json:"post_id"`
	Post      Post      `json:"post"`
	AddedAt   time.Time `json:"added_at"`
	AddedByID string    `json:"added_by_id"`
}

// PlaylistListResponse wraps a list of playlists
type PlaylistListResponse struct {
	Playlists  []Playlist `json:"playlists"`
	TotalCount int        `json:"total_count"`
	Page       int        `json:"page"`
	PageSize   int        `json:"page_size"`
}

// PlaylistEntriesResponse wraps playlist entries
type PlaylistEntriesResponse struct {
	Entries    []PlaylistEntry `json:"entries"`
	TotalCount int             `json:"total_count"`
	Page       int             `json:"page"`
	PageSize   int             `json:"page_size"`
}

// CreatePlaylistRequest is the request to create a playlist
type CreatePlaylistRequest struct {
	Name        string `json:"name"`
	Description string `json:"description"`
	IsPublic    bool   `json:"is_public"`
}

// CreatePlaylist creates a new playlist
func CreatePlaylist(name string, description string, isPublic bool) (*Playlist, error) {
	logger.Debug("Creating playlist", "name", name)

	req := CreatePlaylistRequest{
		Name:        name,
		Description: description,
		IsPublic:    isPublic,
	}

	var response struct {
		Playlist Playlist `json:"playlist"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post("/api/v1/playlists")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to create playlist: %s", resp.Status())
	}

	return &response.Playlist, nil
}

// GetPlaylists retrieves user's playlists with pagination
func GetPlaylists(page int, pageSize int) (*PlaylistListResponse, error) {
	logger.Debug("Fetching playlists", "page", page)

	var response PlaylistListResponse
	resp, err := client.GetClient().
		R().
		SetQueryParam("page", fmt.Sprintf("%d", page)).
		SetQueryParam("page_size", fmt.Sprintf("%d", pageSize)).
		SetResult(&response).
		Get("/api/v1/playlists")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch playlists: %s", resp.Status())
	}

	return &response, nil
}

// GetPlaylist retrieves a specific playlist's details
func GetPlaylist(playlistID string) (*Playlist, error) {
	logger.Debug("Fetching playlist", "playlist_id", playlistID)

	var response struct {
		Playlist Playlist `json:"playlist"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/playlists/%s", playlistID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch playlist: %s", resp.Status())
	}

	return &response.Playlist, nil
}

// GetPlaylistEntries retrieves posts in a playlist with pagination
func GetPlaylistEntries(playlistID string, page int, pageSize int) (*PlaylistEntriesResponse, error) {
	logger.Debug("Fetching playlist entries", "playlist_id", playlistID)

	var response PlaylistEntriesResponse
	resp, err := client.GetClient().
		R().
		SetQueryParam("page", fmt.Sprintf("%d", page)).
		SetQueryParam("page_size", fmt.Sprintf("%d", pageSize)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/playlists/%s/entries", playlistID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch playlist entries: %s", resp.Status())
	}

	return &response, nil
}

// AddPostToPlaylist adds a post to a playlist
func AddPostToPlaylist(playlistID string, postID string) error {
	logger.Debug("Adding post to playlist", "playlist_id", playlistID, "post_id", postID)

	req := struct {
		PostID string `json:"post_id"`
	}{
		PostID: postID,
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		Post(fmt.Sprintf("/api/v1/playlists/%s/entries", playlistID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to add post to playlist: %s", resp.Status())
	}

	return nil
}

// RemovePostFromPlaylist removes a post from a playlist
func RemovePostFromPlaylist(playlistID string, entryID string) error {
	logger.Debug("Removing post from playlist", "playlist_id", playlistID, "entry_id", entryID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/playlists/%s/entries/%s", playlistID, entryID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to remove post from playlist: %s", resp.Status())
	}

	return nil
}

// DeletePlaylist deletes a playlist
func DeletePlaylist(playlistID string) error {
	logger.Debug("Deleting playlist", "playlist_id", playlistID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/playlists/%s", playlistID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete playlist: %s", resp.Status())
	}

	return nil
}

// AddCollaborator adds a collaborator to a playlist
func AddCollaborator(playlistID string, userID string) error {
	logger.Debug("Adding collaborator to playlist", "playlist_id", playlistID, "user_id", userID)

	req := struct {
		UserID string `json:"user_id"`
	}{
		UserID: userID,
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		Post(fmt.Sprintf("/api/v1/playlists/%s/collaborators", playlistID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to add collaborator: %s", resp.Status())
	}

	return nil
}

// RemoveCollaborator removes a collaborator from a playlist
func RemoveCollaborator(playlistID string, userID string) error {
	logger.Debug("Removing collaborator from playlist", "playlist_id", playlistID, "user_id", userID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/playlists/%s/collaborators/%s", playlistID, userID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to remove collaborator: %s", resp.Status())
	}

	return nil
}

// UpdatePlaylist updates playlist metadata
func UpdatePlaylist(playlistID string, name string, description string, isPublic bool) (*Playlist, error) {
	logger.Debug("Updating playlist", "playlist_id", playlistID)

	req := CreatePlaylistRequest{
		Name:        name,
		Description: description,
		IsPublic:    isPublic,
	}

	var response struct {
		Playlist Playlist `json:"playlist"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Put(fmt.Sprintf("/api/v1/playlists/%s", playlistID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to update playlist: %s", resp.Status())
	}

	return &response.Playlist, nil
}
