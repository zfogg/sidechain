package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// RemixRequest is the request to create a remix
type RemixRequest struct {
	OriginalPostID string `json:"original_post_id"`
	Title          string `json:"title,omitempty"`
	Description    string `json:"description,omitempty"`
}

// RemixResponse is the response when creating a remix
type RemixResponse struct {
	RemixID string `json:"remix_id"`
	PostID  string `json:"post_id"`
}

// GetPostDownloadURL returns the URL to download a post's audio
func GetPostDownloadURL(postID string) (string, error) {
	logger.Debug("Getting post download URL", "post_id", postID)
	return fmt.Sprintf("/api/v1/posts/%s/download", postID), nil
}

// CreateRemix creates a remix of a post
func CreateRemix(remixRequest RemixRequest) (*RemixResponse, error) {
	logger.Debug("Creating remix", "original_post_id", remixRequest.OriginalPostID)

	var response struct {
		Remix RemixResponse `json:"remix"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(remixRequest).
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/posts/%s/remix", remixRequest.OriginalPostID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to create remix: %s", resp.Status())
	}

	return &response.Remix, nil
}

// GetRemixes retrieves remixes of a post
func GetRemixes(postID string, page, pageSize int) (*PostListResponse, error) {
	logger.Debug("Getting remixes", "post_id", postID, "page", page)

	var response PostListResponse

	resp, err := client.GetClient().
		R().
		SetQueryParams(map[string]string{
			"page":      fmt.Sprintf("%d", page),
			"page_size": fmt.Sprintf("%d", pageSize),
		}).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/posts/%s/remixes", postID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get remixes: %s", resp.Status())
	}

	return &response, nil
}

// ReportPost reports a post for moderation
func ReportPost(postID, reason, description string) error {
	logger.Debug("Reporting post", "post_id", postID, "reason", reason)
	return ReportContent("post", postID, reason, description)
}

// GetPostStats retrieves engagement statistics for a post
func GetPostStats(postID string) (map[string]int, error) {
	logger.Debug("Getting post stats", "post_id", postID)

	var response struct {
		Stats map[string]int `json:"stats"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/posts/%s/stats", postID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get post stats: %s", resp.Status())
	}

	return response.Stats, nil
}
