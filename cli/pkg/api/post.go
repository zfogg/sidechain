package api

import (
	"fmt"
	"os"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// CreatePostRequest is the request to create a new post
type CreatePostRequest struct {
	Title       string   `json:"title,omitempty"`
	Description string   `json:"description,omitempty"`
	Genre       []string `json:"genre,omitempty"`
	BPM         int      `json:"bpm,omitempty"`
	Key         string   `json:"key,omitempty"`
	DAW         string   `json:"daw,omitempty"`
}

// CreatePostResponse is the response when creating a post
type CreatePostResponse struct {
	Post Post `json:"post"`
}

// GetPost retrieves a post by ID
func GetPost(postID string) (*Post, error) {
	logger.Debug("Fetching post", "post_id", postID)

	var response struct {
		Post Post `json:"post"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/posts/%s", postID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		var errResp ErrorResponse
		if err := resp.Error().(*ErrorResponse); err != nil {
			return nil, fmt.Errorf("API error: %s", errResp.Message)
		}
		return nil, fmt.Errorf("failed to fetch post: %s", resp.Status())
	}

	return &response.Post, nil
}

// ListUserPosts retrieves a user's posts with pagination
func ListUserPosts(page, pageSize int) (*PostListResponse, error) {
	logger.Debug("Listing user posts", "page", page, "page_size", pageSize)

	var response PostListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/posts")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to list posts: %s", resp.Status())
	}

	return &response, nil
}

// UploadPost uploads an audio file and creates a post
func UploadPost(filePath string, title, description string, metadata map[string]interface{}) (*Post, error) {
	logger.Debug("Uploading post", "file", filePath)

	// Check if file exists
	if _, err := os.Stat(filePath); err != nil {
		return nil, fmt.Errorf("file not found: %s", filePath)
	}

	var response CreatePostResponse

	request := client.GetClient().R().
		SetResult(&response).
		SetFile("audio", filePath)

	// Add form fields
	if title != "" {
		request.SetFormData(map[string]string{
			"title": title,
		})
	}
	if description != "" {
		request.SetFormData(map[string]string{
			"description": description,
		})
	}

	// Add optional metadata
	if len(metadata) > 0 {
		for key, value := range metadata {
			request.SetFormData(map[string]string{
				key: fmt.Sprintf("%v", value),
			})
		}
	}

	resp, err := request.Post("/api/v1/posts/upload")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("upload failed: %s", resp.Status())
	}

	return &response.Post, nil
}

// DeletePost deletes a post by ID
func DeletePost(postID string) error {
	logger.Debug("Deleting post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/posts/%s", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete post: %s", resp.Status())
	}

	return nil
}

// ArchivePost archives a post
func ArchivePost(postID string) error {
	logger.Debug("Archiving post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Patch(fmt.Sprintf("/api/v1/posts/%s/archive", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to archive post: %s", resp.Status())
	}

	return nil
}

// UnarchivePost unarchives a post
func UnarchivePost(postID string) error {
	logger.Debug("Unarchiving post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Patch(fmt.Sprintf("/api/v1/posts/%s/unarchive", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to unarchive post: %s", resp.Status())
	}

	return nil
}

// LikePost adds a like to a post
func LikePost(postID string) error {
	logger.Debug("Liking post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/posts/%s/like", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to like post: %s", resp.Status())
	}

	return nil
}

// UnlikePost removes a like from a post
func UnlikePost(postID string) error {
	logger.Debug("Unliking post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/posts/%s/like", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to unlike post: %s", resp.Status())
	}

	return nil
}

// ReactToPost adds an emoji reaction to a post
func ReactToPost(postID, emoji string) error {
	logger.Debug("Reacting to post", "post_id", postID, "emoji", emoji)

	resp, err := client.GetClient().
		R().
		SetFormData(map[string]string{
			"emoji": emoji,
		}).
		Post(fmt.Sprintf("/api/v1/posts/%s/react", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to react to post: %s", resp.Status())
	}

	return nil
}

// SavePost saves a post
func SavePost(postID string) error {
	logger.Debug("Saving post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/posts/%s/save", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to save post: %s", resp.Status())
	}

	return nil
}

// UnsavePost unsaves a post
func UnsavePost(postID string) error {
	logger.Debug("Unsaving post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/posts/%s/save", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to unsave post: %s", resp.Status())
	}

	return nil
}

// ListSavedPosts retrieves saved posts with pagination
func ListSavedPosts(page, pageSize int) (*PostListResponse, error) {
	logger.Debug("Listing saved posts", "page", page, "page_size", pageSize)

	var response PostListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/posts/saved")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to list saved posts: %s", resp.Status())
	}

	return &response, nil
}

// PinPost pins a post to the user's profile
func PinPost(postID string) error {
	logger.Debug("Pinning post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/posts/%s/pin", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to pin post: %s", resp.Status())
	}

	return nil
}

// UnpinPost unpins a post from the user's profile
func UnpinPost(postID string) error {
	logger.Debug("Unpinning post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/posts/%s/pin", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to unpin post: %s", resp.Status())
	}

	return nil
}

// RepostPost reposts a post
func RepostPost(postID string) error {
	logger.Debug("Reposting post", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/posts/%s/repost", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to repost: %s", resp.Status())
	}

	return nil
}

// UnrepostPost undoes a repost
func UnrepostPost(postID string) error {
	logger.Debug("Undoing repost", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/posts/%s/repost", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to undo repost: %s", resp.Status())
	}

	return nil
}

// AdminComment is used for admin comment moderation operations
type AdminComment struct {
	ID             string    `json:"id"`
	PostID         string    `json:"post_id"`
	UserID         string    `json:"user_id"`
	AuthorUsername string    `json:"author_username,omitempty"`
	Content        string    `json:"content"`
	LikeCount      int       `json:"like_count"`
	CreatedAt      string    `json:"created_at"`
	UpdatedAt      string    `json:"updated_at"`
}

// Admin Post Moderation APIs

// AdminDeletePost deletes a post (admin only)
func AdminDeletePost(postID string) error {
	logger.Debug("Deleting post (admin)", "post_id", postID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/admin/posts/%s", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete post: %s", resp.Status())
	}

	return nil
}

// AdminHidePost hides a post temporarily (admin only)
func AdminHidePost(postID string) error {
	logger.Debug("Hiding post (admin)", "post_id", postID)

	request := struct {
		Action string `json:"action"`
	}{
		Action: "hide",
	}

	resp, err := client.GetClient().
		R().
		SetBody(request).
		Patch(fmt.Sprintf("/api/v1/admin/posts/%s", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to hide post: %s", resp.Status())
	}

	return nil
}

// AdminUnhidePost restores a hidden post (admin only)
func AdminUnhidePost(postID string) error {
	logger.Debug("Unhiding post (admin)", "post_id", postID)

	request := struct {
		Action string `json:"action"`
	}{
		Action: "unhide",
	}

	resp, err := client.GetClient().
		R().
		SetBody(request).
		Patch(fmt.Sprintf("/api/v1/admin/posts/%s", postID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to unhide post: %s", resp.Status())
	}

	return nil
}

// GetFlaggedPosts retrieves flagged posts for moderation (admin only)
func GetFlaggedPosts(page, pageSize int) (*FeedResponse, error) {
	logger.Debug("Fetching flagged posts", "page", page)

	var response FeedResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/admin/posts/flagged")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch flagged posts: %s", resp.Status())
	}

	return &response, nil
}

// Admin Comment Moderation APIs

// AdminDeleteComment deletes a comment (admin only)
func AdminDeleteComment(commentID string) error {
	logger.Debug("Deleting comment (admin)", "comment_id", commentID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/admin/comments/%s", commentID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete comment: %s", resp.Status())
	}

	return nil
}

// AdminHideComment hides a comment temporarily (admin only)
func AdminHideComment(commentID string) error {
	logger.Debug("Hiding comment (admin)", "comment_id", commentID)

	request := struct {
		Action string `json:"action"`
	}{
		Action: "hide",
	}

	resp, err := client.GetClient().
		R().
		SetBody(request).
		Patch(fmt.Sprintf("/api/v1/admin/comments/%s", commentID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to hide comment: %s", resp.Status())
	}

	return nil
}

// GetFlaggedComments retrieves flagged comments for moderation (admin only)
func GetFlaggedComments(page, pageSize int) ([]AdminComment, error) {
	logger.Debug("Fetching flagged comments", "page", page)

	var response struct {
		Comments []AdminComment `json:"comments"`
	}

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get("/api/v1/admin/comments/flagged")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch flagged comments: %s", resp.Status())
	}

	return response.Comments, nil
}
