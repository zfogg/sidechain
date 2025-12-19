package search

import (
	"github.com/zfogg/sidechain/backend/internal/models"
)

// UserSearchDoc represents a user document for Elasticsearch indexing
type UserSearchDoc struct {
	ID            string   `json:"id"`
	Username      string   `json:"username"`
	DisplayName   string   `json:"display_name"`
	Bio           string   `json:"bio"`
	Genre         []string `json:"genre,omitempty"`
	FollowerCount int      `json:"follower_count"`
	CreatedAt     string   `json:"created_at"`
}

// PostSearchDoc represents a post document for Elasticsearch indexing
type PostSearchDoc struct {
	ID               string   `json:"id"`
	UserID           string   `json:"user_id"`
	Username         string   `json:"username"`
	Filename         string   `json:"filename"`
	OriginalFilename string   `json:"original_filename"`
	Genre            []string `json:"genre,omitempty"`
	BPM              int      `json:"bpm"`
	Key              string   `json:"key"`
	DAW              string   `json:"daw"`
	LikeCount        int      `json:"like_count"`
	PlayCount        int      `json:"play_count"`
	CommentCount     int      `json:"comment_count"`
	CreatedAt        string   `json:"created_at"`
}

// StorySearchDoc represents a story document for Elasticsearch indexing
type StorySearchDoc struct {
	ID        string `json:"id"`
	UserID    string `json:"user_id"`
	Username  string `json:"username"`
	CreatedAt string `json:"created_at"`
	ExpiresAt string `json:"expires_at"`
}

// UserToSearchDoc converts a User model to a search document
func UserToSearchDoc(user models.User) map[string]interface{} {
	return map[string]interface{}{
		"id":              user.ID,
		"username":        user.Username,
		"display_name":    user.DisplayName,
		"bio":             user.Bio,
		"genre":           user.Genre,
		"follower_count":  user.FollowerCount,
		"created_at":      user.CreatedAt,
	}
}

// AudioPostToSearchDoc converts an AudioPost model to a search document
// Includes current engagement counts from the cached fields
func AudioPostToSearchDoc(post models.AudioPost, username string) map[string]interface{} {
	return map[string]interface{}{
		"id":                 post.ID,
		"user_id":            post.UserID,
		"username":           username,
		"filename":           post.Filename,
		"original_filename":  post.OriginalFilename,
		"genre":              post.Genre,
		"bpm":                post.BPM,
		"key":                post.Key,
		"daw":                post.DAW,
		"like_count":         post.LikeCount,
		"play_count":         post.PlayCount,
		"comment_count":      post.CommentCount,
		"created_at":         post.CreatedAt,
	}
}
