package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// Comment represents a post comment
type Comment struct {
	ID               string     `json:"id"`
	PostID           string     `json:"post_id"`
	UserID           string     `json:"user_id"`
	User             User       `json:"user,omitempty"`
	Content          string     `json:"content"`
	ParentID         *string    `json:"parent_id,omitempty"`
	Replies          []Comment  `json:"replies,omitempty"`
	LikeCount        int        `json:"like_count"`
	IsEdited         bool       `json:"is_edited"`
	EditedAt         *string    `json:"edited_at,omitempty"`
	IsDeleted        bool       `json:"is_deleted"`
	CreatedAt        string     `json:"created_at"`
	UpdatedAt        string     `json:"updated_at"`
}

// CreateCommentRequest is the request to create a comment
type CreateCommentRequest struct {
	Content  string  `json:"content"`
	ParentID *string `json:"parent_id,omitempty"`
}

// UpdateCommentRequest is the request to update a comment
type UpdateCommentRequest struct {
	Content string `json:"content"`
}

// CommentsListResponse represents a list of comments
type CommentsListResponse struct {
	Comments []Comment `json:"comments"`
	Meta     struct {
		Total  int `json:"total"`
		Limit  int `json:"limit"`
		Offset int `json:"offset"`
	} `json:"meta"`
}

// ReportCommentRequest is the request to report a comment
type ReportCommentRequest struct {
	Reason      string `json:"reason"`
	Description string `json:"description,omitempty"`
}

// CreateComment creates a new comment on a post
func CreateComment(postID string, req CreateCommentRequest) (*Comment, error) {
	logger.Debug("Creating comment", "post_id", postID)

	var response struct {
		Comment Comment `json:"comment"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/posts/%s/comments", postID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to create comment: %s", resp.Status())
	}

	return &response.Comment, nil
}

// GetComments retrieves comments on a post
func GetComments(postID string, limit, offset int) (*CommentsListResponse, error) {
	logger.Debug("Getting comments", "post_id", postID, "limit", limit, "offset", offset)

	var response CommentsListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParam("limit", fmt.Sprintf("%d", limit)).
		SetQueryParam("offset", fmt.Sprintf("%d", offset)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/posts/%s/comments", postID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get comments: %s", resp.Status())
	}

	return &response, nil
}

// GetCommentReplies retrieves replies to a comment
func GetCommentReplies(commentID string, limit, offset int) (*CommentsListResponse, error) {
	logger.Debug("Getting comment replies", "comment_id", commentID, "limit", limit, "offset", offset)

	var response CommentsListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParam("limit", fmt.Sprintf("%d", limit)).
		SetQueryParam("offset", fmt.Sprintf("%d", offset)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/comments/%s/replies", commentID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get comment replies: %s", resp.Status())
	}

	return &response, nil
}

// UpdateComment updates a comment
func UpdateComment(commentID string, req UpdateCommentRequest) (*Comment, error) {
	logger.Debug("Updating comment", "comment_id", commentID)

	var response struct {
		Comment Comment `json:"comment"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Put(fmt.Sprintf("/api/v1/comments/%s", commentID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to update comment: %s", resp.Status())
	}

	return &response.Comment, nil
}

// DeleteComment deletes a comment
func DeleteComment(commentID string) error {
	logger.Debug("Deleting comment", "comment_id", commentID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/comments/%s", commentID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete comment: %s", resp.Status())
	}

	return nil
}

// LikeComment likes a comment
func LikeComment(commentID string) (int, error) {
	logger.Debug("Liking comment", "comment_id", commentID)

	var response struct {
		Message   string `json:"message"`
		LikeCount int    `json:"like_count"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/comments/%s/like", commentID))

	if err != nil {
		return 0, err
	}

	if !resp.IsSuccess() {
		return 0, fmt.Errorf("failed to like comment: %s", resp.Status())
	}

	return response.LikeCount, nil
}

// UnlikeComment unlikes a comment
func UnlikeComment(commentID string) (int, error) {
	logger.Debug("Unliking comment", "comment_id", commentID)

	var response struct {
		Message   string `json:"message"`
		LikeCount int    `json:"like_count"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Delete(fmt.Sprintf("/api/v1/comments/%s/like", commentID))

	if err != nil {
		return 0, err
	}

	if !resp.IsSuccess() {
		return 0, fmt.Errorf("failed to unlike comment: %s", resp.Status())
	}

	return response.LikeCount, nil
}

// ReportComment reports a comment
func ReportComment(commentID string, req ReportCommentRequest) (string, error) {
	logger.Debug("Reporting comment", "comment_id", commentID)

	var response struct {
		Message  string `json:"message"`
		ReportID string `json:"report_id"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/comments/%s/report", commentID))

	if err != nil {
		return "", err
	}

	if !resp.IsSuccess() {
		return "", fmt.Errorf("failed to report comment: %s", resp.Status())
	}

	return response.ReportID, nil
}
