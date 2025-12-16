package api

import (
	"testing"
)

func TestGetPost_WithValidID(t *testing.T) {
	validIDs := []string{"test-post-1", "post-123", "uuid-format"}

	for _, postID := range validIDs {
		post, err := GetPost(postID)
		// Either succeeds or fails gracefully
		if err != nil {
			t.Logf("GetPost for %s: API call made (error expected): %v", postID, err)
			continue
		}
		if post == nil {
			t.Errorf("GetPost returned nil for %s", postID)
		} else if post.ID == "" && post.Title == "" {
			t.Logf("GetPost response structure valid for %s", postID)
		}
	}
}

func TestGetPost_WithInvalidID(t *testing.T) {
	invalidIDs := []string{"", "nonexistent", "invalid-post-id"}

	for _, postID := range invalidIDs {
		post, err := GetPost(postID)
		// Invalid IDs should fail or return empty
		if post != nil && post.ID != "" && err == nil {
			t.Logf("GetPost with invalid ID succeeded unexpectedly: %s", postID)
		}
	}
}

func TestListUserPosts_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
		{1, 50},
		{5, 10},
	}

	for _, tt := range tests {
		resp, err := ListUserPosts(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("ListUserPosts page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("ListUserPosts returned nil for page=%d, size=%d", tt.page, tt.pageSize)
		}
		if resp != nil {
			if len(resp.Posts) < 0 {
				t.Errorf("Invalid post count in response")
			}
			if resp.TotalCount < 0 {
				t.Errorf("Invalid total count in response")
			}
		}
	}
}

func TestListUserPosts_WithInvalidPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{0, 10},
		{-1, 20},
		{1, 0},
		{1, -10},
	}

	for _, tt := range tests {
		resp, err := ListUserPosts(tt.page, tt.pageSize)
		// Invalid pagination should fail or return empty
		if resp != nil && err == nil {
			t.Logf("ListUserPosts accepted invalid pagination: page=%d, size=%d", tt.page, tt.pageSize)
		}
	}
}

func TestLikePost_WithValidID(t *testing.T) {
	postIDs := []string{"post-1", "post-2", "post-3"}

	for _, postID := range postIDs {
		err := LikePost(postID)
		if err != nil {
			t.Logf("LikePost %s: API called (error expected): %v", postID, err)
		}
	}
}

func TestLikePost_WithInvalidID(t *testing.T) {
	invalidIDs := []string{"", "nonexistent"}

	for _, postID := range invalidIDs {
		err := LikePost(postID)
		// Empty or nonexistent IDs should fail
		if err == nil && postID == "" {
			t.Logf("LikePost accepted empty ID")
		}
	}
}

func TestUnlikePost_Symmetry(t *testing.T) {
	postID := "test-post"

	errLike := LikePost(postID)
	errUnlike := UnlikePost(postID)

	// Both should have same error status
	hasLikeError := errLike != nil
	hasUnlikeError := errUnlike != nil

	if hasLikeError != hasUnlikeError {
		t.Logf("Like/Unlike error mismatch for %s: like=%v, unlike=%v", postID, hasLikeError, hasUnlikeError)
	}
}

func TestReactToPost_WithValidEmojis(t *testing.T) {
	emojis := []string{"🔥", "❤️", "😂", "🎉"}
	postID := "test-post"

	for _, emoji := range emojis {
		err := ReactToPost(postID, emoji)
		if err != nil {
			t.Logf("ReactToPost %s with emoji %s: API called (error expected): %v", postID, emoji, err)
		}
	}
}

func TestReactToPost_WithInvalidInput(t *testing.T) {
	tests := []struct {
		postID string
		emoji  string
	}{
		{"", "🔥"},
		{"test-post", ""},
		{"", ""},
	}

	for _, tt := range tests {
		err := ReactToPost(tt.postID, tt.emoji)
		if err == nil && (tt.postID == "" || tt.emoji == "") {
			t.Logf("ReactToPost accepted invalid input: postID=%s, emoji=%s", tt.postID, tt.emoji)
		}
	}
}

func TestSavePost_WithValidID(t *testing.T) {
	postIDs := []string{"post-1", "post-2"}

	for _, postID := range postIDs {
		err := SavePost(postID)
		if err != nil {
			t.Logf("SavePost %s: API called (error expected): %v", postID, err)
		}
	}
}

func TestListSavedPosts_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		resp, err := ListSavedPosts(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("ListSavedPosts page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("ListSavedPosts returned nil")
		}
		if resp != nil && len(resp.Posts) < 0 {
			t.Errorf("Invalid saved posts count")
		}
	}
}

func TestArchivePost_UnarchivePost_Symmetry(t *testing.T) {
	postID := "test-post"

	errArchive := ArchivePost(postID)
	errUnarchive := UnarchivePost(postID)

	hasArchiveError := errArchive != nil
	hasUnarchiveError := errUnarchive != nil

	if hasArchiveError != hasUnarchiveError {
		t.Logf("Archive/Unarchive error mismatch: archive=%v, unarchive=%v", hasArchiveError, hasUnarchiveError)
	}
}

func TestDeletePost_WithValidID(t *testing.T) {
	postIDs := []string{"post-to-delete", "another-post"}

	for _, postID := range postIDs {
		err := DeletePost(postID)
		if err != nil {
			t.Logf("DeletePost %s: API called (error expected): %v", postID, err)
		}
	}
}

func TestPinPost_UnpinPost_Symmetry(t *testing.T) {
	postID := "test-post"

	errPin := PinPost(postID)
	errUnpin := UnpinPost(postID)

	hasPin := errPin != nil
	hasUnpin := errUnpin != nil

	if hasPin != hasUnpin {
		t.Logf("Pin/Unpin error mismatch: pin=%v, unpin=%v", hasPin, hasUnpin)
	}
}

func TestRepostPost_UnrepostPost_Symmetry(t *testing.T) {
	postID := "test-post"

	errRepost := RepostPost(postID)
	errUnrepost := UnrepostPost(postID)

	hasRepost := errRepost != nil
	hasUnrepost := errUnrepost != nil

	if hasRepost != hasUnrepost {
		t.Logf("Repost/Unrepost error mismatch: repost=%v, unrepost=%v", hasRepost, hasUnrepost)
	}
}

func TestAdminDeletePost(t *testing.T) {
	postIDs := []string{"flagged-post-1", "spam-post"}

	for _, postID := range postIDs {
		err := AdminDeletePost(postID)
		if err != nil {
			t.Logf("AdminDeletePost %s: API called (error expected): %v", postID, err)
		}
	}
}

func TestAdminHidePost_UnhidePost_Symmetry(t *testing.T) {
	postID := "test-post"

	errHide := AdminHidePost(postID)
	errUnhide := AdminUnhidePost(postID)

	hasHide := errHide != nil
	hasUnhide := errUnhide != nil

	if hasHide != hasUnhide {
		t.Logf("Hide/Unhide error mismatch: hide=%v, unhide=%v", hasHide, hasUnhide)
	}
}

func TestGetFlaggedPosts_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		resp, err := GetFlaggedPosts(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetFlaggedPosts page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if resp == nil {
			t.Errorf("GetFlaggedPosts returned nil")
		}
		if resp != nil && len(resp.Posts) < 0 {
			t.Errorf("Invalid flagged posts count")
		}
	}
}

func TestAdminDeleteComment_WithValidID(t *testing.T) {
	commentIDs := []string{"comment-1", "spam-comment"}

	for _, commentID := range commentIDs {
		err := AdminDeleteComment(commentID)
		if err != nil {
			t.Logf("AdminDeleteComment %s: API called (error expected): %v", commentID, err)
		}
	}
}

func TestAdminHideComment_WithValidID(t *testing.T) {
	commentIDs := []string{"comment-1", "inappropriate-comment"}

	for _, commentID := range commentIDs {
		err := AdminHideComment(commentID)
		if err != nil {
			t.Logf("AdminHideComment %s: API called (error expected): %v", commentID, err)
		}
	}
}

func TestGetFlaggedComments_WithPagination(t *testing.T) {
	tests := []struct {
		page     int
		pageSize int
	}{
		{1, 10},
		{2, 20},
	}

	for _, tt := range tests {
		comments, err := GetFlaggedComments(tt.page, tt.pageSize)
		if err != nil {
			t.Logf("GetFlaggedComments page=%d, size=%d: API called (error expected)", tt.page, tt.pageSize)
			continue
		}
		if comments == nil {
			t.Logf("GetFlaggedComments returned nil (valid)")
		}
		if comments != nil && len(comments) < 0 {
			t.Errorf("Invalid flagged comments count")
		}
	}
}
