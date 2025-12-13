package util

import (
	"errors"
	"path/filepath"
	"strings"

	"github.com/zfogg/sidechain/backend/internal/models"
)

// IsValidAudioFile checks if a filename has a valid audio extension
func IsValidAudioFile(filename string) bool {
	ext := strings.ToLower(filepath.Ext(filename))
	validExts := []string{".mp3", ".wav", ".aiff", ".aif", ".m4a", ".flac", ".ogg"}

	for _, validExt := range validExts {
		if ext == validExt {
			return true
		}
	}
	return false
}

// ValidateProjectFileExtension checks if a file extension is allowed for project files
func ValidateProjectFileExtension(filename string) bool {
	ext := strings.ToLower(filepath.Ext(filename))
	_, exists := models.ProjectFileExtensions[ext]
	return exists
}

// ValidateFilename checks if a display filename is valid
// Filename is required and cannot contain directory separators
// Must be <= 255 chars
func ValidateFilename(filename string) error {
	if filename == "" {
		return errors.New("filename is required")
	}
	if strings.Contains(filename, "/") || strings.Contains(filename, "\\") {
		return errors.New("filename cannot contain directory paths")
	}
	if len(filename) > 255 {
		return errors.New("filename too long (max 255 characters)")
	}
	return nil
}
