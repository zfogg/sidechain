package service

import (
	"os"
	"testing"
)

// TestNewAudioValidator tests audio validator initialization
func TestNewAudioValidator(t *testing.T) {
	validator := NewAudioValidator()

	if validator == nil {
		t.Fatal("NewAudioValidator returned nil")
	}

	if validator.MaxFileSizeMB != 100 {
		t.Errorf("Expected MaxFileSizeMB to be 100, got %d", validator.MaxFileSizeMB)
	}
}

// TestValidateAudioFile_FileNotFound tests validation with non-existent file
func TestValidateAudioFile_FileNotFound(t *testing.T) {
	validator := NewAudioValidator()

	_, err := validator.ValidateAudioFile("/nonexistent/file.mp3")
	if err == nil {
		t.Fatal("Expected error for non-existent file, got nil")
	}

	if err.Error() != "file not found: /nonexistent/file.mp3" {
		t.Errorf("Unexpected error message: %v", err)
	}
}

// TestValidateAudioFile_UnsupportedFormat tests validation with unsupported format
func TestValidateAudioFile_UnsupportedFormat(t *testing.T) {
	// Create a temporary file with unsupported extension
	tmpFile, err := os.CreateTemp("", "test_*.xyz")
	if err != nil {
		t.Fatalf("Failed to create temp file: %v", err)
	}
	defer os.Remove(tmpFile.Name())
	tmpFile.Close()

	validator := NewAudioValidator()

	_, err = validator.ValidateAudioFile(tmpFile.Name())
	if err == nil {
		t.Fatal("Expected error for unsupported format, got nil")
	}

	if !contains(err.Error(), "unsupported audio format") {
		t.Errorf("Expected unsupported format error, got: %v", err)
	}
}

// TestValidateAudioFile_FileTooLarge tests validation with oversized file
func TestValidateAudioFile_FileTooLarge(t *testing.T) {
	// Create a temporary file larger than the default limit
	tmpFile, err := os.CreateTemp("", "test_*.mp3")
	if err != nil {
		t.Fatalf("Failed to create temp file: %v", err)
	}
	defer os.Remove(tmpFile.Name())

	// Write 101 MB of data to exceed the limit
	largeSize := int64(101 * 1024 * 1024)
	if err := tmpFile.Truncate(largeSize); err != nil {
		t.Fatalf("Failed to truncate file: %v", err)
	}
	tmpFile.Close()

	validator := NewAudioValidator()

	_, err = validator.ValidateAudioFile(tmpFile.Name())
	if err == nil {
		t.Fatal("Expected error for large file, got nil")
	}

	if !contains(err.Error(), "file too large") {
		t.Errorf("Expected file too large error, got: %v", err)
	}
}

// TestValidateAudioFile_ValidFile tests validation with valid file
func TestValidateAudioFile_ValidFile(t *testing.T) {
	// Create a temporary MP3 file
	tmpFile, err := os.CreateTemp("", "test_*.mp3")
	if err != nil {
		t.Fatalf("Failed to create temp file: %v", err)
	}
	defer os.Remove(tmpFile.Name())

	// Write some content (small file)
	tmpFile.WriteString("ID3")
	tmpFile.Close()

	validator := NewAudioValidator()

	metadata, err := validator.ValidateAudioFile(tmpFile.Name())
	if err != nil {
		t.Fatalf("Validation failed: %v", err)
	}

	if metadata == nil {
		t.Fatal("Expected metadata, got nil")
	}

	if metadata.Format != "mp3" {
		t.Errorf("Expected format 'mp3', got '%s'", metadata.Format)
	}

	if metadata.FileSize <= 0 {
		t.Errorf("Expected positive file size, got %d", metadata.FileSize)
	}
}

// TestValidateAudioFile_AllFormats tests all supported audio formats
func TestValidateAudioFile_AllFormats(t *testing.T) {
	formats := []string{"mp3", "wav", "flac", "ogg", "aac", "m4a", "opus", "weba"}

	for _, format := range formats {
		t.Run(format, func(t *testing.T) {
			tmpFile, err := os.CreateTemp("", "test_*."+format)
			if err != nil {
				t.Fatalf("Failed to create temp file: %v", err)
			}
			defer os.Remove(tmpFile.Name())

			tmpFile.WriteString("test")
			tmpFile.Close()

			validator := NewAudioValidator()
			metadata, err := validator.ValidateAudioFile(tmpFile.Name())

			if err != nil {
				t.Errorf("Validation failed for format %s: %v", format, err)
			}

			if metadata == nil {
				t.Errorf("Expected metadata for format %s, got nil", format)
			}

			if metadata.Format != format {
				t.Errorf("Expected format '%s', got '%s'", format, metadata.Format)
			}
		})
	}
}

// TestAudioMetadata_GetSizeReport tests file size reporting
func TestAudioMetadata_GetSizeReport(t *testing.T) {
	metadata := &AudioMetadata{
		FileSize: 5242880, // 5 MB
	}

	report := metadata.GetSizeReport()
	if !contains(report, "5.0") || !contains(report, "MB") {
		t.Errorf("Expected size report to contain '5.0 MB', got '%s'", report)
	}
}

// TestAudioMetadata_GetDurationReport tests duration reporting
func TestAudioMetadata_GetDurationReport(t *testing.T) {
	metadata := &AudioMetadata{}

	report := metadata.GetDurationReport()
	if report != "Unavailable" {
		t.Errorf("Expected 'Unavailable', got '%s'", report)
	}
}

// TestValidateAudioFileWithProgress tests progress callback
func TestValidateAudioFileWithProgress(t *testing.T) {
	tmpFile, err := os.CreateTemp("", "test_*.mp3")
	if err != nil {
		t.Fatalf("Failed to create temp file: %v", err)
	}
	defer os.Remove(tmpFile.Name())

	tmpFile.WriteString("test")
	tmpFile.Close()

	validator := NewAudioValidator()

	progressMessages := []string{}
	callback := func(msg string) {
		progressMessages = append(progressMessages, msg)
	}

	metadata, err := validator.ValidateAudioFileWithProgress(tmpFile.Name(), callback)

	if err != nil {
		t.Fatalf("Validation failed: %v", err)
	}

	if metadata == nil {
		t.Fatal("Expected metadata, got nil")
	}

	// Should have received at least some progress messages
	if len(progressMessages) == 0 {
		t.Fatal("Expected progress messages, got none")
	}

	// Check for specific messages
	foundComplete := false
	for _, msg := range progressMessages {
		if msg == "Validation complete" {
			foundComplete = true
			break
		}
	}

	if !foundComplete {
		t.Fatal("Expected 'Validation complete' message")
	}
}

// Helper function to check if string contains substring
func contains(str, substr string) bool {
	for i := 0; i <= len(str)-len(substr); i++ {
		if str[i:i+len(substr)] == substr {
			return true
		}
	}
	return false
}
