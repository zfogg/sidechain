package api

import (
	"fmt"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// ProjectFile represents a project file
type ProjectFile struct {
	ID          string `json:"id"`
	UserID      string `json:"user_id"`
	AudioPostID *string `json:"audio_post_id,omitempty"`
	Filename    string `json:"filename"`
	FileURL     string `json:"file_url"`
	DAWType     string `json:"daw_type"`
	FileSize    int64  `json:"file_size"`
	Description string `json:"description,omitempty"`
	IsPublic    bool   `json:"is_public"`
	DownloadCount int  `json:"download_count"`
	CreatedAt   string `json:"created_at"`
	UpdatedAt   string `json:"updated_at"`
}

// CreateProjectFileRequest is the request to create a project file
type CreateProjectFileRequest struct {
	Filename    string `json:"filename"`
	FileURL     string `json:"file_url"`
	FileSize    int64  `json:"file_size"`
	DAWType     string `json:"daw_type,omitempty"`
	Description string `json:"description,omitempty"`
	IsPublic    bool   `json:"is_public"`
	AudioPostID *string `json:"audio_post_id,omitempty"`
}

// ProjectFileListResponse represents a list of project files
type ProjectFileListResponse struct {
	Files      []ProjectFile `json:"files"`
	TotalCount int           `json:"total_count"`
	Limit      int           `json:"limit"`
	Offset     int           `json:"offset"`
}

// CreateProjectFile creates a new project file record
func CreateProjectFile(req CreateProjectFileRequest) (*ProjectFile, error) {
	logger.Debug("Creating project file", "filename", req.Filename)

	var response struct {
		File ProjectFile `json:"file"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post("/api/v1/project-files")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to create project file: %s", resp.Status())
	}

	return &response.File, nil
}

// ListProjectFiles lists project files with optional filtering
func ListProjectFiles(limit, offset int, dawType string) (*ProjectFileListResponse, error) {
	logger.Debug("Listing project files", "limit", limit, "offset", offset, "daw_type", dawType)

	var response ProjectFileListResponse

	req := client.GetClient().
		R().
		SetQueryParam("limit", fmt.Sprintf("%d", limit)).
		SetQueryParam("offset", fmt.Sprintf("%d", offset))

	if dawType != "" {
		req = req.SetQueryParam("daw_type", dawType)
	}

	resp, err := req.
		SetResult(&response).
		Get("/api/v1/project-files")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to list project files: %s", resp.Status())
	}

	return &response, nil
}

// GetProjectFile retrieves a specific project file
func GetProjectFile(fileID string) (*ProjectFile, error) {
	logger.Debug("Getting project file", "file_id", fileID)

	var response struct {
		File ProjectFile `json:"file"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/project-files/%s", fileID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get project file: %s", resp.Status())
	}

	return &response.File, nil
}

// DeleteProjectFile deletes a project file
func DeleteProjectFile(fileID string) error {
	logger.Debug("Deleting project file", "file_id", fileID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/project-files/%s", fileID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete project file: %s", resp.Status())
	}

	return nil
}

// GetDownloadURL returns the download URL for a project file
func GetDownloadURL(fileID string) (string, error) {
	logger.Debug("Getting download URL", "file_id", fileID)

	return fmt.Sprintf("/api/v1/project-files/%s/download", fileID), nil
}
