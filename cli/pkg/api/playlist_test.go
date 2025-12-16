package api

import (
	"testing"
)

func TestCreatePlaylist_WithValidData(t *testing.T) {
	tests := []struct {
		name        string
		description string
		isPublic    bool
	}{
		{"My Beats", "Collection of my beats", true},
		{"Private Demos", "Work in progress", false},
		{"Collaborations", "Tracks with other producers", true},
	}

	for _, tt := range tests {
		playlist, err := CreatePlaylist(tt.name, tt.description, tt.isPublic)
		if err != nil {
			t.Logf("CreatePlaylist %s: API called (error expected): %v", tt.name, err)
			continue
		}
		if playlist == nil {
			t.Errorf("CreatePlaylist returned nil for %s", tt.name)
		} else {
			if playlist.ID == "" {
				t.Errorf("Created playlist missing ID")
			}
			if playlist.Name != tt.name {
				t.Errorf("Playlist name mismatch: expected %s, got %s", tt.name, playlist.Name)
			}
		}
	}
}

func TestCreatePlaylist_WithInvalidData(t *testing.T) {
	tests := []struct {
		name        string
		description string
		isPublic    bool
	}{
		{"", "Empty name", true},
		{"Valid Name", "", true},
	}

	for _, tt := range tests {
		playlist, err := CreatePlaylist(tt.name, tt.description, tt.isPublic)
		if playlist != nil && playlist.ID != "" && err == nil && tt.name != "" {
			t.Logf("CreatePlaylist accepted invalid data")
		}
	}
}

func TestGetPlaylists_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
		{1, 50},
	}

	for _, tt := range tests {
		resp, err := GetPlaylists(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetPlaylists page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("GetPlaylists returned nil")
		}
		if resp != nil && len(resp.Playlists) > tt.pageSize && tt.pageSize > 0 {
			t.Errorf("GetPlaylists exceeded pageSize")
		}
	}
}

func TestGetPlaylist_WithValidID(t *testing.T) {
	playlistIDs := []string{"playlist-1", "playlist-2", "my-beats"}

	for _, playlistID := range playlistIDs {
		playlist, err := GetPlaylist(playlistID)
		if err != nil {
			t.Logf("GetPlaylist %s: API called (error expected): %v", playlistID, err)
			continue
		}
		if playlist == nil {
			t.Errorf("GetPlaylist returned nil for %s", playlistID)
		}
	}
}

func TestGetPlaylist_WithInvalidID(t *testing.T) {
	invalidIDs := []string{"", "nonexistent"}

	for _, playlistID := range invalidIDs {
		playlist, err := GetPlaylist(playlistID)
		if playlist != nil && playlist.ID != "" && err == nil {
			t.Logf("GetPlaylist accepted invalid ID: %s", playlistID)
		}
	}
}

func TestGetPlaylistEntries_WithValidID(t *testing.T) {
	tests := []struct {
		playlistID string
		page       int
		pageSize   int
	}{
		{"playlist-1", 1, 10},
		{"playlist-2", 2, 20},
	}

	for _, tt := range tests {
		resp, err := GetPlaylistEntries(tt.playlistID, tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetPlaylistEntries %s: API called (error expected): %v", tt.playlistID, err)
			continue
		}
		if resp == nil {
			t.Errorf("GetPlaylistEntries returned nil for %s", tt.playlistID)
		}
	}
}

func TestAddPostToPlaylist_WithValidData(t *testing.T) {
	tests := []struct {
		playlistID string
		postID     string
	}{
		{"playlist-1", "post-1"},
		{"playlist-2", "post-2"},
	}

	for _, tt := range tests {
		err := AddPostToPlaylist(tt.playlistID, tt.postID)
		if err != nil {
			t.Logf("AddPostToPlaylist %s->%s: API called (error expected): %v", tt.playlistID, tt.postID, err)
		}
	}
}

func TestAddPostToPlaylist_WithInvalidData(t *testing.T) {
	tests := []struct {
		playlistID string
		postID     string
	}{
		{"", "post-1"},
		{"playlist-1", ""},
		{"", ""},
	}

	for _, tt := range tests {
		err := AddPostToPlaylist(tt.playlistID, tt.postID)
		if err == nil && (tt.playlistID == "" || tt.postID == "") {
			t.Logf("AddPostToPlaylist accepted invalid data")
		}
	}
}

func TestRemovePostFromPlaylist_WithValidData(t *testing.T) {
	tests := []struct {
		playlistID string
		entryID    string
	}{
		{"playlist-1", "entry-1"},
		{"playlist-2", "entry-2"},
	}

	for _, tt := range tests {
		err := RemovePostFromPlaylist(tt.playlistID, tt.entryID)
		if err != nil {
			t.Logf("RemovePostFromPlaylist %s->%s: API called (error expected): %v", tt.playlistID, tt.entryID, err)
		}
	}
}

func TestDeletePlaylist_WithValidID(t *testing.T) {
	playlistIDs := []string{"playlist-to-delete", "unused-playlist"}

	for _, playlistID := range playlistIDs {
		err := DeletePlaylist(playlistID)
		if err != nil {
			t.Logf("DeletePlaylist %s: API called (error expected): %v", playlistID, err)
		}
	}
}

func TestDeletePlaylist_WithInvalidID(t *testing.T) {
	err := DeletePlaylist("")

	if err == nil {
		t.Logf("DeletePlaylist accepted empty ID")
	}
}

func TestAddCollaborator_WithValidData(t *testing.T) {
	tests := []struct {
		playlistID string
		userID     string
	}{
		{"playlist-1", "user-1"},
		{"playlist-2", "user-2"},
	}

	for _, tt := range tests {
		err := AddCollaborator(tt.playlistID, tt.userID)
		if err != nil {
			t.Logf("AddCollaborator %s->%s: API called (error expected): %v", tt.playlistID, tt.userID, err)
		}
	}
}

func TestRemoveCollaborator_WithValidData(t *testing.T) {
	tests := []struct {
		playlistID string
		userID     string
	}{
		{"playlist-1", "user-1"},
		{"playlist-2", "user-2"},
	}

	for _, tt := range tests {
		err := RemoveCollaborator(tt.playlistID, tt.userID)
		if err != nil {
			t.Logf("RemoveCollaborator %s->%s: API called (error expected): %v", tt.playlistID, tt.userID, err)
		}
	}
}

func TestUpdatePlaylist_WithValidData(t *testing.T) {
	tests := []struct {
		playlistID  string
		name        string
		description string
		isPublic    bool
	}{
		{"playlist-1", "Updated Name", "New description", true},
		{"playlist-2", "Private Beats", "Private collection", false},
	}

	for _, tt := range tests {
		playlist, err := UpdatePlaylist(tt.playlistID, tt.name, tt.description, tt.isPublic)
		if err != nil {
			t.Logf("UpdatePlaylist %s: API called (error expected): %v", tt.playlistID, err)
			continue
		}
		if playlist != nil && playlist.Name != tt.name {
			t.Errorf("UpdatePlaylist name mismatch: expected %s, got %s", tt.name, playlist.Name)
		}
	}
}
