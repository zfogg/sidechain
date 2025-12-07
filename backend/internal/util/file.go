package util

import (
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
	os.MkdirAll(tempDir, 0755)

	tempFilePath := filepath.Join(tempDir, uuid.New().String()+filepath.Ext(file.Filename))

	// Read and save file data
	fileData := make([]byte, file.Size)
	_, err = src.Read(fileData)
	if err != nil {
		return "", err
	}

	err = os.WriteFile(tempFilePath, fileData, 0644)
	if err != nil {
		return "", err
	}

	return tempFilePath, nil
}
