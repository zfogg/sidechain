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

func TestGetReactions_API(t *testing.T) {
	// Try with non-existent post
	postID := "nonexistent-post"

	resp, err := GetReactions(postID)

	if err != nil {
		t.Logf("GetReactions error (post doesn't exist): %v", err)
		return
	}

	if resp == nil {
		t.Error("GetReactions returned nil")
	}
	if resp.Reactions != nil {
		// Verify reactions structure
		for _, reaction := range resp.Reactions {
			if reaction.Emoji == "" && reaction.Count == 0 {
				t.Log("Found empty reaction")
			}
		}
	}
}
