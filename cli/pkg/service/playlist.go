package service

import (
	"fmt"
	"strings"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// PlaylistService handles playlist operations
type PlaylistService struct{}

// NewPlaylistService creates a new playlist service
func NewPlaylistService() *PlaylistService {
	return &PlaylistService{}
}

// CreatePlaylist creates a new playlist
func (ps *PlaylistService) CreatePlaylist(name string, description string, isPublic bool) (*api.Playlist, error) {
	logger.Debug("Creating playlist", "name", name)

	if name == "" {
		return nil, fmt.Errorf("playlist name cannot be empty")
	}

	if len(name) > 100 {
		return nil, fmt.Errorf("playlist name exceeds maximum length (100 characters)")
	}

	if len(description) > 500 {
		return nil, fmt.Errorf("description exceeds maximum length (500 characters)")
	}

	playlist, err := api.CreatePlaylist(name, description, isPublic)
	if err != nil {
		logger.Error("Failed to create playlist", "error", err)
		return nil, err
	}

	logger.Debug("Playlist created successfully", "playlist_id", playlist.ID)
	fmt.Printf("✓ Playlist '%s' created\n", name)
	return playlist, nil
}

// ListPlaylists lists user's playlists
func (ps *PlaylistService) ListPlaylists(page, pageSize int) error {
	logger.Debug("Listing playlists", "page", page)

	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 100 {
		pageSize = 10
	}

	playlists, err := api.GetPlaylists(page, pageSize)
	if err != nil {
		logger.Error("Failed to fetch playlists", "error", err)
		return err
	}

	if len(playlists.Playlists) == 0 {
		fmt.Println("No playlists yet. Create one with `playlist create`")
		return nil
	}

	ps.displayPlaylistList(playlists)
	return nil
}

// ViewPlaylist displays playlist details and contents
func (ps *PlaylistService) ViewPlaylist(playlistID string) error {
	logger.Debug("Viewing playlist", "playlist_id", playlistID)

	playlist, err := api.GetPlaylist(playlistID)
	if err != nil {
		logger.Error("Failed to fetch playlist", "error", err)
		return err
	}

	fmt.Printf("\n📋 %s\n", playlist.Name)
	fmt.Println(strings.Repeat("─", 60))
	if playlist.Description != "" {
		fmt.Printf("Description: %s\n", playlist.Description)
	}
	fmt.Printf("Owner: @%s\n", playlist.Owner.Username)
	fmt.Printf("Posts: %d\n", playlist.PostCount)
	fmt.Printf("Visibility: %s\n", visibility(playlist.IsPublic))
	if len(playlist.Collaborators) > 0 {
		fmt.Printf("Collaborators: %d\n", len(playlist.Collaborators))
	}

	// Fetch and display first 10 entries
	entries, err := api.GetPlaylistEntries(playlistID, 1, 10)
	if err == nil && len(entries.Entries) > 0 {
		fmt.Println(strings.Repeat("─", 60))
		ps.displayPlaylistEntries(entries)
	}

	return nil
}

// AddPost adds a post to a playlist
func (ps *PlaylistService) AddPost(playlistID string, postID string) error {
	logger.Debug("Adding post to playlist", "post_id", postID)

	err := api.AddPostToPlaylist(playlistID, postID)
	if err != nil {
		logger.Error("Failed to add post to playlist", "error", err)
		return err
	}

	logger.Debug("Post added successfully")
	fmt.Println("✓ Post added to playlist")
	return nil
}

// RemovePost removes a post from a playlist
func (ps *PlaylistService) RemovePost(playlistID string, entryID string) error {
	logger.Debug("Removing post from playlist", "entry_id", entryID)

	err := api.RemovePostFromPlaylist(playlistID, entryID)
	if err != nil {
		logger.Error("Failed to remove post from playlist", "error", err)
		return err
	}

	logger.Debug("Post removed successfully")
	fmt.Println("✓ Post removed from playlist")
	return nil
}

// DeletePlaylist deletes a playlist
func (ps *PlaylistService) DeletePlaylist(playlistID string) error {
	logger.Debug("Deleting playlist", "playlist_id", playlistID)

	err := api.DeletePlaylist(playlistID)
	if err != nil {
		logger.Error("Failed to delete playlist", "error", err)
		return err
	}

	logger.Debug("Playlist deleted successfully")
	fmt.Println("✓ Playlist deleted")
	return nil
}

// ListPlaylisEntries lists posts in a playlist
func (ps *PlaylistService) ListPlaylistEntries(playlistID string, page, pageSize int) error {
	logger.Debug("Listing playlist entries", "playlist_id", playlistID, "page", page)

	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 100 {
		pageSize = 20
	}

	entries, err := api.GetPlaylistEntries(playlistID, page, pageSize)
	if err != nil {
		logger.Error("Failed to fetch playlist entries", "error", err)
		return err
	}

	if len(entries.Entries) == 0 {
		fmt.Println("No posts in this playlist yet.")
		return nil
	}

	ps.displayPlaylistEntries(entries)
	return nil
}

// displayPlaylistList displays a list of playlists
func (ps *PlaylistService) displayPlaylistList(playlists *api.PlaylistListResponse) {
	fmt.Printf("\n🎵 Your Playlists (Page %d)\n", playlists.Page)
	fmt.Println(strings.Repeat("─", 60))

	for i, pl := range playlists.Playlists {
		visibility := visibility(pl.IsPublic)
		fmt.Printf("%2d. %s\n", i+1, pl.Name)
		fmt.Printf("    Posts: %d | %s\n", pl.PostCount, visibility)
		if pl.Description != "" {
			desc := pl.Description
			if len(desc) > 45 {
				desc = desc[:42] + "..."
			}
			fmt.Printf("    %s\n", desc)
		}
		if i < len(playlists.Playlists)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nShowing %d of %d playlists\n\n", len(playlists.Playlists), playlists.TotalCount)
}

// displayPlaylistEntries displays posts in a playlist
func (ps *PlaylistService) displayPlaylistEntries(entries *api.PlaylistEntriesResponse) {
	fmt.Printf("\n📌 Playlist Contents (Page %d)\n", entries.Page)
	fmt.Println(strings.Repeat("─", 60))

	for i, entry := range entries.Entries {
		post := entry.Post
		fmt.Printf("%2d. %s\n", i+1, post.Title)
		fmt.Printf("    By @%s | 👍 %d | 💬 %d\n", post.AuthorUsername, post.LikeCount, post.CommentCount)
		if i < len(entries.Entries)-1 {
			fmt.Println(strings.Repeat("─", 60))
		}
	}

	fmt.Printf("\nShowing %d of %d posts\n\n", len(entries.Entries), entries.TotalCount)
}

// Helper function
func visibility(isPublic bool) string {
	if isPublic {
		return "🌐 Public"
	}
	return "🔒 Private"
}
