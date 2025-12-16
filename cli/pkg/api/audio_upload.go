package api

import (
	"bytes"
	"fmt"
	"io"
	"mime/multipart"
	"os"
	"path/filepath"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// AudioUploadResponse represents the response from audio upload
type AudioUploadResponse struct {
	JobID   string `json:"job_id"`
	AudioID string `json:"audio_id"`
	Message string `json:"message"`
}

// AudioProcessingStatus represents the status of audio processing
type AudioProcessingStatus struct {
	JobID      string `json:"job_id"`
	AudioID    string `json:"audio_id"`
	Status     string `json:"status"` // pending, processing, completed, failed
	Progress   int    `json:"progress"`
	Message    string `json:"message"`
	CreatedAt  string `json:"created_at"`
	UpdatedAt  string `json:"updated_at"`
}

// AudioFile represents an uploaded audio file
type AudioFile struct {
	ID       string `json:"id"`
	Filename string `json:"filename"`
	URL      string `json:"url"`
	Duration int    `json:"duration"`
	Size     int64  `json:"size"`
	MimeType string `json:"mime_type"`
	Metadata map[string]interface{} `json:"metadata"`
	CreatedAt string `json:"created_at"`
}

// UploadAudio uploads an audio file
func UploadAudio(filePath string) (*AudioUploadResponse, error) {
	logger.Debug("Uploading audio", "file_path", filePath)

	file, err := os.Open(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to open file: %w", err)
	}
	defer file.Close()

	// Get file stats
	fileInfo, err := file.Stat()
	if err != nil {
		return nil, fmt.Errorf("failed to stat file: %w", err)
	}

	// Create multipart form
	body := &bytes.Buffer{}
	writer := multipart.NewWriter(body)

	// Add file field
	part, err := writer.CreateFormFile("audio", filepath.Base(filePath))
	if err != nil {
		return nil, fmt.Errorf("failed to create form file: %w", err)
	}

	_, err = io.Copy(part, file)
	if err != nil {
		return nil, fmt.Errorf("failed to copy file: %w", err)
	}

	err = writer.Close()
	if err != nil {
		return nil, fmt.Errorf("failed to close writer: %w", err)
	}

	var result AudioUploadResponse
	resp, err := client.GetClient().
		R().
		SetHeader("Content-Type", writer.FormDataContentType()).
		SetBody(body.Bytes()).
		SetResult(&result).
		Post("/api/v1/audio/upload")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to upload audio: %s", resp.Status())
	}

	logger.Debug("Audio uploaded", "job_id", result.JobID, "file_size", fileInfo.Size())
	return &result, nil
}

// GetAudioProcessingStatus retrieves the status of audio processing
func GetAudioProcessingStatus(jobID string) (*AudioProcessingStatus, error) {
	logger.Debug("Getting audio processing status", "job_id", jobID)

	var result AudioProcessingStatus
	resp, err := client.GetClient().
		R().
		SetResult(&result).
		Get(fmt.Sprintf("/api/v1/audio/jobs/%s", jobID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get audio status: %s", resp.Status())
	}

	return &result, nil
}

// GetAudio retrieves an audio file
func GetAudio(audioID string) (*AudioFile, error) {
	logger.Debug("Getting audio", "audio_id", audioID)

	var result AudioFile
	resp, err := client.GetClient().
		R().
		SetResult(&result).
		Get(fmt.Sprintf("/api/v1/audio/%s", audioID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to get audio: %s", resp.Status())
	}

	return &result, nil
}

// GetAudioDownloadURL gets the direct download URL for an audio file
func GetAudioDownloadURL(audioID string) (string, error) {
	logger.Debug("Getting audio download URL", "audio_id", audioID)

	audio, err := GetAudio(audioID)
	if err != nil {
		return "", err
	}

	return audio.URL, nil
}
