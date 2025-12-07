package models

import (
	"time"

	"gorm.io/gorm"
)

// Playlist represents a collection of audio posts (R.3.1 Collaborative Playlists)
type Playlist struct {
	ID              string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	Name            string `gorm:"not null" json:"name"`
	Description     string `gorm:"type:text" json:"description"`
	OwnerID         string `gorm:"not null;index" json:"owner_id"`
	Owner           User   `gorm:"foreignKey:OwnerID" json:"owner,omitempty"`
	IsCollaborative bool   `gorm:"default:false" json:"is_collaborative"`
	IsPublic        bool   `gorm:"default:true" json:"is_public"`

	// Relationships
	Entries       []PlaylistEntry        `gorm:"foreignKey:PlaylistID" json:"entries,omitempty"`
	Collaborators []PlaylistCollaborator `gorm:"foreignKey:PlaylistID" json:"collaborators,omitempty"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName overrides the default table name
func (Playlist) TableName() string {
	return "playlists"
}

// PlaylistEntry represents a post in a playlist (R.3.1.1.2)
type PlaylistEntry struct {
	ID            string    `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	PlaylistID    string    `gorm:"not null;index" json:"playlist_id"`
	Playlist      Playlist  `gorm:"foreignKey:PlaylistID" json:"playlist,omitempty"`
	PostID        string    `gorm:"not null;index" json:"post_id"`
	Post          AudioPost `gorm:"foreignKey:PostID" json:"post,omitempty"`
	AddedByUserID string    `gorm:"not null;index" json:"added_by_user_id"`
	AddedByUser   User      `gorm:"foreignKey:AddedByUserID" json:"added_by_user,omitempty"`
	Position      int       `gorm:"not null;default:0" json:"position"` // Order in playlist (0-based)

	// GORM fields
	AddedAt   time.Time      `json:"added_at"`
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName overrides the default table name
func (PlaylistEntry) TableName() string {
	return "playlist_entries"
}

// PlaylistCollaboratorRole represents the role of a collaborator
type PlaylistCollaboratorRole string

const (
	PlaylistRoleOwner  PlaylistCollaboratorRole = "owner"
	PlaylistRoleEditor PlaylistCollaboratorRole = "editor"
	PlaylistRoleViewer PlaylistCollaboratorRole = "viewer"
)

// PlaylistCollaborator represents a user who can collaborate on a playlist (R.3.1.1.3)
type PlaylistCollaborator struct {
	ID         string                   `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	PlaylistID string                   `gorm:"not null;index" json:"playlist_id"`
	Playlist   Playlist                 `gorm:"foreignKey:PlaylistID" json:"playlist,omitempty"`
	UserID     string                   `gorm:"not null;index" json:"user_id"`
	User       User                     `gorm:"foreignKey:UserID" json:"user,omitempty"`
	Role       PlaylistCollaboratorRole `gorm:"not null;default:'viewer'" json:"role"` // owner, editor, viewer

	// GORM fields
	AddedAt   time.Time      `json:"added_at"`
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName overrides the default table name
func (PlaylistCollaborator) TableName() string {
	return "playlist_collaborators"
}

// CanEdit returns true if the collaborator can add/remove entries
func (c *PlaylistCollaborator) CanEdit() bool {
	return c.Role == PlaylistRoleOwner || c.Role == PlaylistRoleEditor
}

// CanManageCollaborators returns true if the collaborator can add/remove other collaborators
func (c *PlaylistCollaborator) CanManageCollaborators() bool {
	return c.Role == PlaylistRoleOwner
}

// HasAccess checks if a user has access to a playlist (owner, collaborator, or public)
func (p *Playlist) HasAccess(userID string) bool {
	// Owner always has access
	if p.OwnerID == userID {
		return true
	}

	// Public playlists are accessible to everyone
	if p.IsPublic {
		return true
	}

	// Check if user is a collaborator
	for _, collab := range p.Collaborators {
		if collab.UserID == userID {
			return true
		}
	}

	return false
}

// GetUserRole returns the role of a user in the playlist
func (p *Playlist) GetUserRole(userID string) PlaylistCollaboratorRole {
	// Owner
	if p.OwnerID == userID {
		return PlaylistRoleOwner
	}

	// Check collaborators
	for _, collab := range p.Collaborators {
		if collab.UserID == userID {
			return collab.Role
		}
	}

	return "" // No access
}

// GetEntryCount returns the number of entries in the playlist
func (p *Playlist) GetEntryCount() int {
	return len(p.Entries)
}
