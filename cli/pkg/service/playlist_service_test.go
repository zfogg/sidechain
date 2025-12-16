package service

import (
	"testing"
)

func TestNewPlaylistService(t *testing.T) {
	service := NewPlaylistService()
	if service == nil {
		t.Error("NewPlaylistService returned nil")
	}
}

func TestPlaylistService_CreatePlaylistDirect(t *testing.T) {
	service := NewPlaylistService()

	// CreatePlaylist with direct parameters
	playlist, err := service.CreatePlaylist("Test Playlist", "Test description", true)
	if err != nil {
		t.Logf("CreatePlaylist: error (expected): %v", err)
	}
	if playlist != nil && playlist.ID == "" {
		t.Logf("CreatePlaylist returned without ID")
	}
}

func TestPlaylistService_ListPlaylists(t *testing.T) {
	service := NewPlaylistService()

	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		err := service.ListPlaylists(tt.page, tt.pageSize)
		t.Logf("ListPlaylists page=%d, size=%d: %v", tt.page, tt.pageSize, err)
	}
}

func TestPlaylistService_ViewPlaylist(t *testing.T) {
	service := NewPlaylistService()

	playlistID := "playlist-1"
	err := service.ViewPlaylist(playlistID)
	t.Logf("ViewPlaylist %s: %v", playlistID, err)
}

func TestPlaylistService_DeletePlaylistDirect(t *testing.T) {
	service := NewPlaylistService()

	playlistID := "playlist-to-delete"
	err := service.DeletePlaylist(playlistID)
	t.Logf("DeletePlaylist %s: %v", playlistID, err)
}

func TestPlaylistService_AddPost(t *testing.T) {
	service := NewPlaylistService()

	playlistID := "playlist-1"
	postID := "post-1"
	err := service.AddPost(playlistID, postID)
	t.Logf("AddPost to %s: %v", playlistID, err)
}

func TestPlaylistService_RemovePost(t *testing.T) {
	service := NewPlaylistService()

	playlistID := "playlist-1"
	entryID := "entry-1"
	err := service.RemovePost(playlistID, entryID)
	t.Logf("RemovePost from %s: %v", playlistID, err)
}
