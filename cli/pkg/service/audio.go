package service

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/dhowden/tag"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// AudioValidator provides audio file validation
type AudioValidator struct {
	MaxFileSizeMB int // Maximum file size in MB (default: 100)
}

// AudioMetadata contains extracted audio metadata
type AudioMetadata struct {
	Format   string
	BPM      int
	Key      string
	Artist   string
	Title    string
	Album    string
	FileSize int64
}

// NewAudioValidator creates a new audio validator with defaults
func NewAudioValidator() *AudioValidator {
	return &AudioValidator{
		MaxFileSizeMB: 100,
	}
}

// ValidateAudioFile validates an audio file and extracts metadata
func (av *AudioValidator) ValidateAudioFile(filePath string) (*AudioMetadata, error) {
	logger.Debug("Validating audio file", "path", filePath)

	// Check if file exists
	fileInfo, err := os.Stat(filePath)
	if err != nil {
		return nil, fmt.Errorf("file not found: %s", filePath)
	}

	// Check file size
	fileSizeBytes := fileInfo.Size()
	fileSizeMB := float64(fileSizeBytes) / (1024 * 1024)
	if int(fileSizeMB) > av.MaxFileSizeMB {
		return nil, fmt.Errorf("file too large: %.1f MB (max: %d MB)", fileSizeMB, av.MaxFileSizeMB)
	}

	// Validate format
	ext := strings.ToLower(filepath.Ext(filePath))
	format := strings.TrimPrefix(ext, ".")

	validFormats := map[string]bool{
		"mp3":  true,
		"wav":  true,
		"flac": true,
		"ogg":  true,
		"aac":  true,
		"m4a":  true,
		"opus": true,
		"weba": true,
	}

	if !validFormats[format] {
		return nil, fmt.Errorf("unsupported audio format: .%s (supported: mp3, wav, flac, ogg, aac, m4a, opus)", format)
	}

	// Extract metadata
	metadata := &AudioMetadata{
		Format:   format,
		FileSize: fileSizeBytes,
	}

	// Read metadata from file
	if err := av.extractMetadata(filePath, metadata); err != nil {
		logger.Debug("Warning: could not extract full metadata", "error", err.Error())
		// Don't fail on metadata extraction errors - these are optional
	}

	logger.Debug("Audio validation successful", "format", format, "size_mb", fileSizeMB)
	return metadata, nil
}

// extractMetadata extracts metadata from audio file
func (av *AudioValidator) extractMetadata(filePath string, metadata *AudioMetadata) error {
	file, err := os.Open(filePath)
	if err != nil {
		return fmt.Errorf("could not open file: %w", err)
	}
	defer file.Close()

	// Try to read metadata
	m, err := tag.ReadFrom(file)
	if err != nil {
		logger.Debug("Could not read metadata from file", "error", err.Error())
		return nil // Not all audio files have readable metadata
	}

	// Extract available metadata
	if m.Title() != "" {
		metadata.Title = m.Title()
	}
	if m.Artist() != "" {
		metadata.Artist = m.Artist()
	}
	if m.Album() != "" {
		metadata.Album = m.Album()
	}

	// Try to extract BPM and Key from comments/raw metadata
	// This is format-specific and may not be available in all files
	raw := m.Raw()
	if raw != nil {
		// Some ID3v2 tags store BPM in TBPM frame
		if bpmVal, ok := raw["TBPM"]; ok {
			// Parse BPM from the value if available
			if bpmStr, ok := bpmVal.(string); ok {
				fmt.Sscanf(bpmStr, "%d", &metadata.BPM)
			}
		}

		// Try to extract key/initial key from ID3v2 tags
		if keyVal, ok := raw["TIT1"]; ok {
			// TIT1 or TKEY might contain key information
			if keyStr, ok := keyVal.(string); ok && keyStr != "" {
				metadata.Key = keyStr
			}
		}
	}

	return nil
}

// ValidateAudioFileWithProgress validates a file and shows progress
func (av *AudioValidator) ValidateAudioFileWithProgress(filePath string, onProgress func(string)) (*AudioMetadata, error) {
	onProgress("Checking file...")

	// Check existence and size
	fileInfo, err := os.Stat(filePath)
	if err != nil {
		return nil, fmt.Errorf("file not found: %s", filePath)
	}

	fileSizeBytes := fileInfo.Size()
	fileSizeMB := float64(fileSizeBytes) / (1024 * 1024)
	onProgress(fmt.Sprintf("File size: %.1f MB", fileSizeMB))

	// Validate format
	ext := strings.ToLower(filepath.Ext(filePath))
	format := strings.TrimPrefix(ext, ".")

	validFormats := map[string]bool{
		"mp3":  true,
		"wav":  true,
		"flac": true,
		"ogg":  true,
		"aac":  true,
		"m4a":  true,
		"opus": true,
		"weba": true,
	}

	if !validFormats[format] {
		return nil, fmt.Errorf("unsupported audio format: .%s", format)
	}
	onProgress(fmt.Sprintf("Format: %s", format))

	// Check file size
	if int(fileSizeMB) > av.MaxFileSizeMB {
		return nil, fmt.Errorf("file too large: %.1f MB (max: %d MB)", fileSizeMB, av.MaxFileSizeMB)
	}

	onProgress("Extracting metadata...")

	// Extract metadata
	metadata := &AudioMetadata{
		Format:   format,
		FileSize: fileSizeBytes,
	}

	if err := av.extractMetadata(filePath, metadata); err != nil {
		logger.Debug("Warning: could not extract metadata", "error", err.Error())
	}

	onProgress("Validation complete")
	return metadata, nil
}

// GetSizeReport returns a human-readable file size description
func (m *AudioMetadata) GetSizeReport() string {
	mb := float64(m.FileSize) / (1024 * 1024)
	return fmt.Sprintf("%.1f MB", mb)
}

// GetDurationReport returns a human-readable duration description
func (m *AudioMetadata) GetDurationReport() string {
	return "Unavailable"
}

// DisplayMetadata prints the extracted metadata
func (m *AudioMetadata) DisplayMetadata() {
	fmt.Printf("\n📊 Audio Metadata\n")
	fmt.Printf("  Format:   %s\n", strings.ToUpper(m.Format))
	fmt.Printf("  Size:     %s\n", m.GetSizeReport())
	if m.Title != "" {
		fmt.Printf("  Title:    %s\n", m.Title)
	}
	if m.Artist != "" {
		fmt.Printf("  Artist:   %s\n", m.Artist)
	}
	if m.Album != "" {
		fmt.Printf("  Album:    %s\n", m.Album)
	}
	if m.BPM > 0 {
		fmt.Printf("  BPM:      %d\n", m.BPM)
	}
	if m.Key != "" {
		fmt.Printf("  Key:      %s\n", m.Key)
	}
	fmt.Printf("\n")
}
