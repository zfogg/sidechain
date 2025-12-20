package util

import (
	"errors"
	"net/mail"
	"path/filepath"
	"regexp"
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

// ValidateEmail validates email format
func ValidateEmail(email string) error {
	if email == "" {
		return errors.New("email is required")
	}
	if len(email) > 254 {
		return errors.New("email is too long")
	}
	_, err := mail.ParseAddress(email)
	if err != nil {
		return errors.New("invalid email format")
	}
	return nil
}

// ValidatePassword validates password strength
// Requirements: min 8 chars, at least 1 uppercase, 1 lowercase, 1 number, 1 special char
func ValidatePassword(password string) error {
	if password == "" {
		return errors.New("password is required")
	}
	if len(password) < 8 {
		return errors.New("password must be at least 8 characters")
	}
	if len(password) > 128 {
		return errors.New("password is too long")
	}

	hasUpper := regexp.MustCompile(`[A-Z]`).MatchString(password)
	hasLower := regexp.MustCompile(`[a-z]`).MatchString(password)
	hasNumber := regexp.MustCompile(`[0-9]`).MatchString(password)
	hasSpecial := regexp.MustCompile(`[!@#$%^&*()_+\-=\[\]{};':"\\|,.<>\/?]`).MatchString(password)

	if !hasUpper {
		return errors.New("password must contain at least one uppercase letter")
	}
	if !hasLower {
		return errors.New("password must contain at least one lowercase letter")
	}
	if !hasNumber {
		return errors.New("password must contain at least one number")
	}
	if !hasSpecial {
		return errors.New("password must contain at least one special character")
	}
	return nil
}

// ValidateUsername validates username format
// Requirements: 3-30 alphanumeric + underscore, no consecutive underscores
func ValidateUsername(username string) error {
	if username == "" {
		return errors.New("username is required")
	}
	if len(username) < 3 {
		return errors.New("username must be at least 3 characters")
	}
	if len(username) > 30 {
		return errors.New("username must be at most 30 characters")
	}

	if !regexp.MustCompile(`^[a-zA-Z0-9_]+$`).MatchString(username) {
		return errors.New("username can only contain letters, numbers, and underscores")
	}
	if strings.Contains(username, "__") {
		return errors.New("username cannot contain consecutive underscores")
	}
	if strings.HasPrefix(username, "_") || strings.HasSuffix(username, "_") {
		return errors.New("username cannot start or end with underscore")
	}
	return nil
}

// ValidateString validates a string length
func ValidateString(value, fieldName string, minLen, maxLen int) error {
	if minLen > 0 && len(value) < minLen {
		return errors.New(fieldName + " must be at least " + string(rune(minLen)) + " characters")
	}
	if maxLen > 0 && len(value) > maxLen {
		return errors.New(fieldName + " must be at most " + string(rune(maxLen)) + " characters")
	}
	return nil
}

// ValidateRange validates a numeric range
func ValidateRange(value int, fieldName string, min, max int) error {
	if value < min {
		return errors.New(fieldName + " must be at least " + string(rune(min)))
	}
	if value > max {
		return errors.New(fieldName + " must be at most " + string(rune(max)))
	}
	return nil
}

// ValidateUUID validates UUID format (basic check)
func ValidateUUID(id string) error {
	if id == "" {
		return errors.New("id is required")
	}
	if len(id) != 36 {
		return errors.New("invalid id format")
	}
	if !regexp.MustCompile(`^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$`).MatchString(id) {
		return errors.New("invalid id format")
	}
	return nil
}

// ValidateVisibility validates visibility value
func ValidateVisibility(visibility string) error {
	validVisibilities := map[string]bool{
		"public":  true,
		"private": true,
		"friends": true,
	}
	if !validVisibilities[visibility] {
		return errors.New("visibility must be public, private, or friends")
	}
	return nil
}

// ValidateBPM validates BPM (beats per minute) range
func ValidateBPM(bpm int) error {
	if bpm < 40 {
		return errors.New("BPM must be at least 40")
	}
	if bpm > 300 {
		return errors.New("BPM must be at most 300")
	}
	return nil
}

// ValidateDAW validates DAW type
func ValidateDAW(daw string) error {
	validDAWs := map[string]bool{
		"ableton":    true,
		"fl-studio": true,
		"logic":      true,
		"reaper":     true,
		"studio-one": true,
		"cubase":     true,
		"reason":     true,
		"live":       true,
		"other":      true,
	}
	if daw == "" {
		return nil // Optional field
	}
	if !validDAWs[strings.ToLower(daw)] {
		return errors.New("invalid DAW type")
	}
	return nil
}

// ValidatePaginationLimit validates pagination limit
func ValidatePaginationLimit(limit int64) error {
	if limit < 1 {
		return errors.New("limit must be at least 1")
	}
	if limit > 1000 {
		return errors.New("limit must be at most 1000")
	}
	return nil
}

// ValidatePaginationOffset validates pagination offset
func ValidatePaginationOffset(offset int64) error {
	if offset < 0 {
		return errors.New("offset must be non-negative")
	}
	return nil
}
