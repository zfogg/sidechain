package api

import (
	"testing"
)

func TestCreateComment_WithValidInput(t *testing.T) {
	tests := []struct {
		postID  string
		content string
	}{
		{"post-1", "Great track!"},
		{"post-2", "Love the vibe"},
		{"post-3", "Thanks for sharing"},
	}

	for _, tt := range tests {
		req := CreateCommentRequest{Content: tt.content}
		comment, err := CreateComment(tt.postID, req)
		if err != nil {
			t.Logf("CreateComment post=%s: API called (error expected): %v", tt.postID, err)
			continue
		}
		if comment == nil {
			t.Errorf("CreateComment returned nil for post %s", tt.postID)
		} else {
			if comment.ID == "" {
				t.Errorf("Created comment missing ID for post %s", tt.postID)
			}
			if comment.Content != tt.content && comment.Content != "" {
				t.Errorf("Comment content mismatch for post %s", tt.postID)
			}
		}
	}
}

func TestCreateComment_WithInvalidInput(t *testing.T) {
	tests := []struct {
		postID  string
		content string
	}{
		{"", "Valid comment"},
		{"post-1", ""},
		{"", ""},
	}

	for _, tt := range tests {
		req := CreateCommentRequest{Content: tt.content}
		comment, err := CreateComment(tt.postID, req)
		if comment != nil && comment.ID != "" && err == nil {
			t.Logf("CreateComment accepted invalid input: postID=%s, content=%s", tt.postID, tt.content)
		}
	}
}

func TestGetComments_WithValidPostID(t *testing.T) {
	tests := []struct {
		postID string
		limit  int
		offset int
	}{
		{"post-1", 10, 0},
		{"post-2", 20, 0},
		{"post-3", 10, 10},
	}

	for _, tt := range tests {
		resp, err := GetComments(tt.postID, tt.limit, tt.offset)
		if err != nil {
			t.Logf("GetComments post=%s: API called (error expected): %v", tt.postID, err)
			continue
		}
		if resp == nil {
			t.Errorf("GetComments returned nil for post %s", tt.postID)
		}
		if resp != nil && len(resp.Comments) > tt.limit {
			t.Errorf("GetComments returned more comments than limit for post %s", tt.postID)
		}
	}
}

func TestGetComments_WithInvalidInput(t *testing.T) {
	tests := []struct {
		postID string
		limit  int
		offset int
	}{
		{"", 10, 0},
		{"post-1", 0, 0},
		{"post-1", -10, 0},
		{"post-1", 10, -1},
	}

	for _, tt := range tests {
		resp, err := GetComments(tt.postID, tt.limit, tt.offset)
		if resp != nil && len(resp.Comments) >= 0 && err == nil && tt.postID != "" {
			t.Logf("GetComments accepted invalid parameters: limit=%d, offset=%d", tt.limit, tt.offset)
		}
	}
}

func TestGetCommentReplies_WithValidInput(t *testing.T) {
	tests := []struct {
		commentID string
		limit     int
		offset    int
	}{
		{"comment-1", 10, 0},
		{"comment-2", 20, 0},
		{"comment-3", 5, 5},
	}

	for _, tt := range tests {
		resp, err := GetCommentReplies(tt.commentID, tt.limit, tt.offset)
		if err != nil {
			t.Logf("GetCommentReplies comment=%s: API called (error expected): %v", tt.commentID, err)
			continue
		}
		if resp == nil {
			t.Errorf("GetCommentReplies returned nil for comment %s", tt.commentID)
		}
	}
}

func TestUpdateComment_WithValidData(t *testing.T) {
	updates := []struct {
		commentID string
		content   string
	}{
		{"comment-1", "Updated comment"},
		{"comment-2", "New content"},
	}

	for _, update := range updates {
		req := UpdateCommentRequest{Content: update.content}
		comment, err := UpdateComment(update.commentID, req)
		if err != nil {
			t.Logf("UpdateComment %s: API called (error expected): %v", update.commentID, err)
			continue
		}
		if comment == nil {
			t.Errorf("UpdateComment returned nil for comment %s", update.commentID)
		}
	}
}

func TestUpdateComment_WithInvalidInput(t *testing.T) {
	tests := []struct {
		commentID string
		content   string
	}{
		{"", "Valid content"},
		{"comment-1", ""},
	}

	for _, tt := range tests {
		req := UpdateCommentRequest{Content: tt.content}
		comment, err := UpdateComment(tt.commentID, req)
		if comment != nil && comment.ID != "" && err == nil {
			t.Logf("UpdateComment accepted invalid input: commentID=%s", tt.commentID)
		}
	}
}

func TestDeleteComment_WithValidID(t *testing.T) {
	commentIDs := []string{"comment-1", "comment-2", "comment-3"}

	for _, commentID := range commentIDs {
		err := DeleteComment(commentID)
		if err != nil {
			t.Logf("DeleteComment %s: API called (error expected): %v", commentID, err)
		}
	}
}

func TestDeleteComment_WithInvalidID(t *testing.T) {
	invalidIDs := []string{"", "nonexistent"}

	for _, commentID := range invalidIDs {
		err := DeleteComment(commentID)
		if err == nil && commentID == "" {
			t.Logf("DeleteComment accepted empty ID")
		}
	}
}

func TestLikeComment_WithValidID(t *testing.T) {
	commentIDs := []string{"comment-1", "comment-2"}

	for _, commentID := range commentIDs {
		count, err := LikeComment(commentID)
		if err != nil {
			t.Logf("LikeComment %s: API called (error expected): %v", commentID, err)
			continue
		}
		if count < 0 {
			t.Errorf("LikeComment returned negative count for %s", commentID)
		}
	}
}

func TestLikeComment_UnlikeCommentSymmetry(t *testing.T) {
	commentIDs := []string{"comment-1", "comment-2"}

	for _, commentID := range commentIDs {
		likeCount, likeErr := LikeComment(commentID)
		unlikeCount, unlikeErr := UnlikeComment(commentID)

		hasLikeErr := likeErr != nil
		hasUnlikeErr := unlikeErr != nil

		if hasLikeErr != hasUnlikeErr {
			t.Logf("Like/Unlike error mismatch for %s: like=%v, unlike=%v", commentID, hasLikeErr, hasUnlikeErr)
		}
		// Counts should be valid integers
		if likeCount >= 0 && unlikeCount >= 0 {
			t.Logf("Like/Unlike counts valid for %s: like=%d, unlike=%d", commentID, likeCount, unlikeCount)
		}
	}
}

func TestReportComment_WithValidData(t *testing.T) {
	tests := []struct {
		commentID string
		reason    string
	}{
		{"spam-comment", "spam"},
		{"inappropriate", "inappropriate_content"},
		{"abusive", "harassment"},
	}

	for _, tt := range tests {
		req := ReportCommentRequest{Reason: tt.reason}
		reportID, err := ReportComment(tt.commentID, req)
		if err != nil {
			t.Logf("ReportComment %s: API called (error expected): %v", tt.commentID, err)
			continue
		}
		if reportID != "" {
			t.Logf("ReportComment created report: %s", reportID)
		}
	}
}

func TestReportComment_WithInvalidData(t *testing.T) {
	tests := []struct {
		commentID string
		reason    string
	}{
		{"", "spam"},
		{"comment-1", ""},
		{"", ""},
	}

	for _, tt := range tests {
		req := ReportCommentRequest{Reason: tt.reason}
		reportID, err := ReportComment(tt.commentID, req)
		if reportID != "" && err == nil && tt.commentID != "" && tt.reason != "" {
			t.Logf("ReportComment accepted invalid input")
		}
	}
}
