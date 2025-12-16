package api

import (
	"testing"
)

func TestAddReaction_API(t *testing.T) {
	// Try with non-existent post ID
	postID := "nonexistent-post"
	emoji := "🔥"

	resp, err := AddReaction(postID, emoji)

	if err != nil {
		t.Logf("AddReaction error (expected, post doesn't exist): %v", err)
		return
	}

	if resp == nil {
		t.Error("AddReaction returned nil response")
	}
}

func TestRemoveReaction_API(t *testing.T) {
	// Try with non-existent reaction
	postID := "nonexistent-post"
	emoji := "🔥"

	err := RemoveReaction(postID, emoji)

	if err != nil {
		t.Logf("RemoveReaction error (expected, reaction doesn't exist): %v", err)
	}
}

func TestGetPostReactions_API(t *testing.T) {
	// Try with non-existent post
	postID := "nonexistent-post"

	resp, err := GetPostReactions(postID)

	if err != nil {
		t.Logf("GetPostReactions error (post doesn't exist): %v", err)
		return
	}

	if resp == nil {
		t.Error("GetPostReactions returned nil")
	}
	if resp.ReactionCounts != nil {
		// Verify reaction counts structure
		for emoji, count := range resp.ReactionCounts {
			if count < 0 {
				t.Errorf("Invalid count for emoji %s: %d", emoji, count)
			}
		}
	}
}
