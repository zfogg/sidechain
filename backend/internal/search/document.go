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
	ID           string   `json:"id"`
	UserID       string   `json:"user_id"`
	Username     string   `json:"username"`
	Genre        []string `json:"genre,omitempty"`
	BPM          int      `json:"bpm"`
	Key          string   `json:"key"`
	DAW          string   `json:"daw"`
	LikeCount    int      `json:"like_count"`
	PlayCount    int      `json:"play_count"`
	CommentCount int      `json:"comment_count"`
	CreatedAt    string   `json:"created_at"`
}

// StorySearchDoc represents a story document for Elasticsearch indexing
type StorySearchDoc struct {
	ID        string `json:"id"`
	UserID    string `json:"user_id"`
	Username  string `json:"username"`
	CreatedAt string `json:"created_at"`
	ExpiresAt string `json:"expires_at"`
}

// UserToSearchDoc converts a User model to a search document (Phase 0.3)
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

// PostToSearchDoc converts a Post model to a search document (Phase 0.4)
// Note: Implement when Post model is defined
func PostToSearchDoc(postID, userID, username string, bpm int, key, daw string, genres []string) map[string]interface{} {
	return map[string]interface{}{
		"id":            postID,
		"user_id":       userID,
		"username":      username,
		"genre":         genres,
		"bpm":           bpm,
		"key":           key,
		"daw":           daw,
		"like_count":    0,
		"play_count":    0,
		"comment_count": 0,
		"created_at":    nil,
	}
}

// StoryToSearchDoc converts a Story model to a search document (Phase 0.5)
// Note: Implement when Story model is finalized
func StoryToSearchDoc(storyID, userID, username, createdAt, expiresAt string) map[string]interface{} {
	return map[string]interface{}{
		"id":         storyID,
		"user_id":    userID,
		"username":   username,
		"created_at": createdAt,
		"expires_at": expiresAt,
	}
}
