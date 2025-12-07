package util

import (
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
