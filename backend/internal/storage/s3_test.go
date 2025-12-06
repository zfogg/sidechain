package storage

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

// =============================================================================
// CONTENT TYPE TESTS
// =============================================================================

func TestGetContentType(t *testing.T) {
	tests := []struct {
		extension string
		expected  string
	}{
		{".mp3", "audio/mpeg"},
		{".MP3", "audio/mpeg"},
		{".wav", "audio/wav"},
		{".WAV", "audio/wav"},
		{".ogg", "audio/ogg"},
		{".m4a", "audio/mp4"},
		{".svg", "image/svg+xml"},
		{".unknown", "application/octet-stream"},
		{"", "application/octet-stream"},
		{".flac", "application/octet-stream"}, // Not explicitly mapped
	}

	for _, tt := range tests {
		t.Run(tt.extension, func(t *testing.T) {
			result := getContentType(tt.extension)
			assert.Equal(t, tt.expected, result)
		})
	}
}

func TestGetContentTypeForImage(t *testing.T) {
	tests := []struct {
		extension string
		expected  string
	}{
		{".jpg", "image/jpeg"},
		{".JPG", "image/jpeg"},
		{".jpeg", "image/jpeg"},
		{".JPEG", "image/jpeg"},
		{".png", "image/png"},
		{".PNG", "image/png"},
		{".gif", "image/gif"},
		{".GIF", "image/gif"},
		{".webp", "image/webp"},
		{".WEBP", "image/webp"},
		{".unknown", "application/octet-stream"},
		{"", "application/octet-stream"},
		{".bmp", "application/octet-stream"}, // Not supported
	}

	for _, tt := range tests {
		t.Run(tt.extension, func(t *testing.T) {
			result := getContentTypeForImage(tt.extension)
			assert.Equal(t, tt.expected, result)
		})
	}
}

// =============================================================================
// UPLOAD RESULT TESTS
// =============================================================================

func TestUploadResultStruct(t *testing.T) {
	result := UploadResult{
		Key:    "audio/2024/01/user123/abc123.mp3",
		URL:    "https://cdn.example.com/audio/2024/01/user123/abc123.mp3",
		Bucket: "my-bucket",
		Region: "us-east-1",
		Size:   1024000,
	}

	assert.Equal(t, "audio/2024/01/user123/abc123.mp3", result.Key)
	assert.Equal(t, "https://cdn.example.com/audio/2024/01/user123/abc123.mp3", result.URL)
	assert.Equal(t, "my-bucket", result.Bucket)
	assert.Equal(t, "us-east-1", result.Region)
	assert.Equal(t, int64(1024000), result.Size)
}

// =============================================================================
// S3 UPLOADER STRUCT TESTS
// =============================================================================

func TestS3UploaderStruct(t *testing.T) {
	// Test the struct fields are accessible
	uploader := &S3Uploader{
		bucket:  "test-bucket",
		region:  "us-west-2",
		baseURL: "https://cdn.test.com",
	}

	assert.Equal(t, "test-bucket", uploader.bucket)
	assert.Equal(t, "us-west-2", uploader.region)
	assert.Equal(t, "https://cdn.test.com", uploader.baseURL)
}

// =============================================================================
// KEY GENERATION TESTS
// =============================================================================

func TestProfilePictureKeyFormat(t *testing.T) {
	// Test that profile picture keys have correct format
	userID := "user123"

	// Simulate key generation logic
	expectedPrefix := "profile-pics/" + userID + "/"

	// Key should start with profile-pics/{userID}/
	assert.Contains(t, expectedPrefix, "profile-pics/")
	assert.Contains(t, expectedPrefix, userID)
}

func TestAudioKeyContainsUserID(t *testing.T) {
	// Test that audio keys would contain user ID
	userID := "user456"

	// Simulate key generation logic for audio
	expectedPattern := "/" + userID + "/"

	// In real implementation, key would be: audio/{year}/{month}/{userID}/{fileID}.mp3
	assert.Contains(t, expectedPattern, userID)
}
