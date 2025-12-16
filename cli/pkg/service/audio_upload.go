package service

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/api"
	"github.com/zfogg/sidechain/cli/pkg/formatter"
)

// AudioUploadService provides audio upload operations
type AudioUploadService struct{}

// NewAudioUploadService creates a new audio upload service
func NewAudioUploadService() *AudioUploadService {
	return &AudioUploadService{}
}

// UploadAudio handles interactive audio file upload
func (aus *AudioUploadService) UploadAudio() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Audio file path: ")
	filePath, _ := reader.ReadString('\n')
	filePath = strings.TrimSpace(filePath)

	if filePath == "" {
		return fmt.Errorf("file path cannot be empty")
	}

	// Verify file exists
	_, err := os.Stat(filePath)
	if err != nil {
		return fmt.Errorf("file not found: %s", filePath)
	}

	// Check file extension
	ext := strings.ToLower(filepath.Ext(filePath))
	validExts := []string{".mp3", ".wav", ".aac", ".flac", ".m4a"}
	isValid := false
	for _, validExt := range validExts {
		if ext == validExt {
			isValid = true
			break
		}
	}

	if !isValid {
		return fmt.Errorf("unsupported audio format: %s (supported: %v)", ext, validExts)
	}

	formatter.PrintInfo("⏳ Uploading audio file...")
	fmt.Printf("File: %s\n\n", filepath.Base(filePath))

	// Upload
	result, err := api.UploadAudio(filePath)
	if err != nil {
		return err
	}

	formatter.PrintSuccess("Audio uploaded successfully!")
	fmt.Printf("\nJob ID: %s\n", result.JobID)
	fmt.Printf("Audio ID: %s\n\n", result.AudioID)

	// Offer to check status
	fmt.Print("Check processing status now? (y/n): ")
	check, _ := reader.ReadString('\n')
	check = strings.TrimSpace(strings.ToLower(check))

	if check == "y" {
		return aus.CheckAudioStatus(result.JobID)
	}

	fmt.Printf("\nYou can check status later using: sidechain-cli audio status %s\n", result.JobID)
	return nil
}

// CheckAudioStatus polls and displays audio processing status
func (aus *AudioUploadService) CheckAudioStatus(jobID string) error {
	if jobID == "" {
		reader := bufio.NewReader(os.Stdin)
		fmt.Print("Job ID: ")
		var err error
		jobID, err = reader.ReadString('\n')
		if err != nil {
			return err
		}
		jobID = strings.TrimSpace(jobID)
	}

	if jobID == "" {
		return fmt.Errorf("job ID cannot be empty")
	}

	status, err := api.GetAudioProcessingStatus(jobID)
	if err != nil {
		return err
	}

	formatter.PrintInfo("🎵 Audio Processing Status")
	fmt.Printf("\n")

	statusInfo := map[string]interface{}{
		"Job ID":     status.JobID,
		"Audio ID":   status.AudioID,
		"Status":     aus.formatStatus(status.Status),
		"Progress":   fmt.Sprintf("%d%%", status.Progress),
		"Message":    status.Message,
		"Updated":    status.UpdatedAt,
	}

	formatter.PrintKeyValue(statusInfo)

	// Auto-refresh if still processing
	if status.Status == "pending" || status.Status == "processing" {
		fmt.Print("\nCheck status again? (y/n): ")
		reader := bufio.NewReader(os.Stdin)
		again, _ := reader.ReadString('\n')
		again = strings.TrimSpace(strings.ToLower(again))

		if again == "y" {
			time.Sleep(2 * time.Second) // Wait before retrying
			return aus.CheckAudioStatus(jobID)
		}
	}

	return nil
}

// GetAudio retrieves and displays audio file information
func (aus *AudioUploadService) GetAudio() error {
	reader := bufio.NewReader(os.Stdin)

	fmt.Print("Audio ID: ")
	audioID, _ := reader.ReadString('\n')
	audioID = strings.TrimSpace(audioID)

	if audioID == "" {
		return fmt.Errorf("audio ID cannot be empty")
	}

	audio, err := api.GetAudio(audioID)
	if err != nil {
		return err
	}

	formatter.PrintInfo("🎵 Audio File")
	fmt.Printf("\n")

	// Format file size
	var sizeStr string
	bytes := audio.Size
	switch {
	case bytes < 1024:
		sizeStr = fmt.Sprintf("%d B", bytes)
	case bytes < 1024*1024:
		sizeStr = fmt.Sprintf("%.2f KB", float64(bytes)/1024)
	case bytes < 1024*1024*1024:
		sizeStr = fmt.Sprintf("%.2f MB", float64(bytes)/(1024*1024))
	default:
		sizeStr = fmt.Sprintf("%.2f GB", float64(bytes)/(1024*1024*1024))
	}

	audioInfo := map[string]interface{}{
		"ID":         audio.ID,
		"Filename":   audio.Filename,
		"Duration":   fmt.Sprintf("%d seconds", audio.Duration),
		"Size":       sizeStr,
		"Type":       audio.MimeType,
		"Created":    audio.CreatedAt,
		"URL":        audio.URL,
	}

	formatter.PrintKeyValue(audioInfo)

	// Offer download
	if audio.URL != "" {
		fmt.Printf("\nDownload URL available. To download:\n")
		fmt.Printf("  curl -o audio.mp3 '%s'\n", audio.URL)
	}

	return nil
}

// Helper functions

func (aus *AudioUploadService) formatStatus(status string) string {
	statusMap := map[string]string{
		"pending":    "⏳ Pending",
		"processing": "🔄 Processing",
		"completed":  "✅ Completed",
		"failed":     "❌ Failed",
	}

	if formatted, ok := statusMap[status]; ok {
		return formatted
	}
	return status
}
