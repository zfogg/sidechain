package handlers

import (
	"mime/multipart"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/google/uuid"
)

// Helper functions for parsing form data
func parseInt(s string, defaultValue int) int {
	if val, err := strconv.Atoi(s); err == nil {
		return val
	}
	return defaultValue
}

func parseFloat(s string, defaultValue float64) float64 {
	if val, err := strconv.ParseFloat(s, 64); err == nil {
		return val
	}
	return defaultValue
}

func parseGenreArray(s string) []string {
	if s == "" {
		return []string{}
	}
	// Simple comma-separated parsing - could be improved
	return []string{s}
}

func isValidAudioFile(filename string) bool {
	ext := strings.ToLower(filepath.Ext(filename))
	validExts := []string{".mp3", ".wav", ".aiff", ".aif", ".m4a", ".flac", ".ogg"}

	for _, validExt := range validExts {
		if ext == validExt {
			return true
		}
	}
	return false
}

func saveUploadedFile(file *multipart.FileHeader) (string, error) {
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

// extractMentions extracts @username mentions from comment content
func extractMentions(content string) []string {
	var mentions []string
	words := strings.Fields(content)
	seen := make(map[string]bool)

	for _, word := range words {
		if strings.HasPrefix(word, "@") && len(word) > 1 {
			// Clean the username (remove trailing punctuation)
			username := strings.TrimPrefix(word, "@")
			username = strings.TrimRight(username, ".,!?;:")
			username = strings.ToLower(username)

			if !seen[username] && len(username) >= 3 && len(username) <= 30 {
				seen[username] = true
				mentions = append(mentions, username)
			}
		}
	}
	return mentions
}
