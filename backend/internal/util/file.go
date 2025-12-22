package util

import (
	"fmt"
	"io"
	"mime/multipart"
	"os"
	"path/filepath"

	"github.com/google/uuid"
)

// SaveUploadedFile saves an uploaded multipart file to a temporary directory
// Returns the path to the saved file
func SaveUploadedFile(file *multipart.FileHeader) (string, error) {
	src, err := file.Open()
	if err != nil {
		return "", err
	}
	defer src.Close()

	// Create temp file
	tempDir := "/tmp/sidechain_uploads"
	if err := os.MkdirAll(tempDir, 0755); err != nil {
		return "", fmt.Errorf("failed to create temp directory: %w", err)
	}

	tempFilePath := filepath.Join(tempDir, uuid.New().String()+filepath.Ext(file.Filename))

	// Read and save file data using io.ReadFull to ensure complete read
	fileData := make([]byte, file.Size)
	_, err = io.ReadFull(src, fileData)
	if err != nil {
		return "", fmt.Errorf("failed to read file data: %w", err)
	}

	err = os.WriteFile(tempFilePath, fileData, 0644)
	if err != nil {
		return "", err
	}

	return tempFilePath, nil
}
