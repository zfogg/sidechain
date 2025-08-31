package audio

import (
	"bytes"
	"context"
	"fmt"
	"mime/multipart"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/storage"
)

// Processor handles audio upload and processing pipeline
type Processor struct {
	s3Uploader *storage.S3Uploader
	tempDir    string
	// Note: Job queue integration will be added when we connect the pieces
}

// NewProcessor creates a new audio processor
func NewProcessor(s3Uploader *storage.S3Uploader) *Processor {
	tempDir := "/tmp/sidechain_audio"
	os.MkdirAll(tempDir, 0755)
	
	return &Processor{
		s3Uploader: s3Uploader,
		tempDir:    tempDir,
	}
}

// ProcessUpload handles complete audio upload pipeline
func (p *Processor) ProcessUpload(ctx context.Context, file *multipart.FileHeader, userID string, metadata AudioMetadata) (*models.AudioPost, error) {
	// 1. Save uploaded file temporarily
	tempFilePath, err := p.saveTemporaryFile(file)
	if err != nil {
		return nil, fmt.Errorf("failed to save temporary file: %w", err)
	}
	defer os.Remove(tempFilePath)

	// 2. Process audio (normalize loudness, convert to MP3)
	processedFilePath, err := p.processAudio(tempFilePath, metadata)
	if err != nil {
		return nil, fmt.Errorf("failed to process audio: %w", err)
	}
	defer os.Remove(processedFilePath)

	// 3. Generate waveform visualization
	waveformSVG, err := p.generateWaveform(processedFilePath)
	if err != nil {
		return nil, fmt.Errorf("failed to generate waveform: %w", err)
	}

	// 4. Read processed audio data
	processedData, err := os.ReadFile(processedFilePath)
	if err != nil {
		return nil, fmt.Errorf("failed to read processed audio: %w", err)
	}

	// 5. Upload to S3
	uploadResult, err := p.s3Uploader.UploadAudio(ctx, processedData, userID, file.Filename)
	if err != nil {
		return nil, fmt.Errorf("failed to upload to S3: %w", err)
	}

	// 6. Upload waveform to S3
	waveformResult, err := p.s3Uploader.UploadWaveform(ctx, []byte(waveformSVG), uploadResult.Key)
	if err != nil {
		// Non-fatal error - audio upload succeeded
		fmt.Printf("Warning: Failed to upload waveform: %v\n", err)
		waveformSVG = "" // Use empty string if upload failed
	}

	// 7. Create database record
	audioPost := &models.AudioPost{
		ID:              uuid.New().String(),
		UserID:          userID,
		AudioURL:        uploadResult.URL,
		OriginalFilename: file.Filename,
		FileSize:        uploadResult.Size,
		BPM:             metadata.BPM,
		Key:             metadata.Key,
		DurationBars:    metadata.DurationBars,
		DAW:             metadata.DAW,
		Genre:           metadata.Genre,
		WaveformSVG:     waveformSVG,
		ProcessingStatus: "complete",
		IsPublic:        true,
	}

	if waveformResult != nil {
		audioPost.WaveformSVG = waveformResult.URL
	}

	err = database.DB.Create(audioPost).Error
	if err != nil {
		return nil, fmt.Errorf("failed to save audio post: %w", err)
	}

	return audioPost, nil
}

// AudioMetadata represents metadata from the VST
type AudioMetadata struct {
	BPM          int      `json:"bpm"`
	Key          string   `json:"key"`
	DurationBars int      `json:"duration_bars"`
	DAW          string   `json:"daw"`
	Genre        []string `json:"genre"`
	SampleRate   float64  `json:"sample_rate"`
}

// saveTemporaryFile saves uploaded multipart file to temp directory
func (p *Processor) saveTemporaryFile(file *multipart.FileHeader) (string, error) {
	src, err := file.Open()
	if err != nil {
		return "", err
	}
	defer src.Close()

	// Create temporary file
	tempFilePath := filepath.Join(p.tempDir, uuid.New().String()+filepath.Ext(file.Filename))
	
	// Read file data
	fileData := make([]byte, file.Size)
	_, err = src.Read(fileData)
	if err != nil {
		return "", err
	}

	// Write to temp file
	err = os.WriteFile(tempFilePath, fileData, 0644)
	if err != nil {
		return "", err
	}

	return tempFilePath, nil
}

// processAudio normalizes loudness and converts to web-optimized MP3
func (p *Processor) processAudio(inputPath string, metadata AudioMetadata) (string, error) {
	outputPath := strings.Replace(inputPath, filepath.Ext(inputPath), "_processed.mp3", 1)

	// FFmpeg command for loudness normalization to -14 LUFS and MP3 encoding
	cmd := exec.Command("ffmpeg",
		"-i", inputPath,
		"-af", "loudnorm=I=-14:TP=-1:LRA=7",  // Normalize to -14 LUFS
		"-codec:a", "libmp3lame",             // MP3 encoder
		"-b:a", "128k",                       // 128kbps bitrate
		"-ar", "44100",                       // 44.1kHz sample rate
		"-channels", "2",                     // Stereo
		"-y",                                 // Overwrite output
		outputPath,
	)

	var stderr bytes.Buffer
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err != nil {
		return "", fmt.Errorf("ffmpeg failed: %v, stderr: %s", err, stderr.String())
	}

	return outputPath, nil
}

// generateWaveform creates SVG waveform visualization
func (p *Processor) generateWaveform(audioPath string) (string, error) {
	// Use FFmpeg to extract audio peaks for waveform generation
	cmd := exec.Command("ffmpeg",
		"-i", audioPath,
		"-ac", "1",                    // Convert to mono
		"-ar", "8000",                 // Low sample rate for peak extraction
		"-f", "f32le",                 // 32-bit float little endian
		"-",                           // Output to stdout
	)

	var stdout bytes.Buffer
	cmd.Stdout = &stdout

	err := cmd.Run()
	if err != nil {
		return "", fmt.Errorf("failed to extract audio data: %w", err)
	}

	// Convert float32 audio data to waveform SVG
	audioData := stdout.Bytes()
	svg := generateSVGFromAudioData(audioData, 400, 100)

	return svg, nil
}

// SubmitProcessingJob submits an audio processing job (placeholder)
func (p *Processor) SubmitProcessingJob(userID, tempFilePath, filename string, metadata map[string]interface{}) (*JobStatus, error) {
	// TODO: Integrate with actual job queue
	return &JobStatus{
		ID:     fmt.Sprintf("job_%s_%d", userID, time.Now().Unix()),
		Status: "pending",
	}, nil
}

// GetJobStatus gets the status of a processing job (placeholder)
func (p *Processor) GetJobStatus(jobID string) (*JobStatus, error) {
	// TODO: Integrate with actual job queue
	return &JobStatus{
		ID:     jobID,
		Status: "complete",
		Result: &JobResult{
			AudioURL: "https://mock-url.com/audio.mp3",
		},
	}, nil
}

// JobStatus represents the status of an audio processing job
type JobStatus struct {
	ID           string     `json:"id"`
	Status       string     `json:"status"`
	ErrorMessage *string    `json:"error_message,omitempty"`
	Result       *JobResult `json:"result,omitempty"`
}

// JobResult contains the result of audio processing
type JobResult struct {
	AudioURL    string `json:"audio_url"`
	WaveformURL string `json:"waveform_url"`
}

// generateSVGFromAudioData creates SVG waveform from raw audio data
func generateSVGFromAudioData(audioData []byte, width, height int) string {
	if len(audioData) == 0 {
		return ""
	}

	// Calculate samples per pixel
	samplesPerPixel := len(audioData) / 4 / width // 4 bytes per float32
	if samplesPerPixel < 1 {
		samplesPerPixel = 1
	}

	svg := fmt.Sprintf(`<svg width="%d" height="%d" xmlns="http://www.w3.org/2000/svg">`, width, height)
	svg += `<rect width="100%" height="100%" fill="#1a1a1e"/>`
	
	pathData := "M"
	for x := 0; x < width; x++ {
		sampleIndex := x * samplesPerPixel * 4 // 4 bytes per float32
		
		if sampleIndex+3 < len(audioData) {
			// Extract float32 sample (simplified - proper implementation would use binary.Read)
			// For now, use a simple amplitude calculation
			amplitude := float64(audioData[sampleIndex]) / 255.0
			
			// Convert to SVG Y coordinate
			y := int(float64(height/2) * (1.0 - amplitude))
			if y < 0 {
				y = 0
			} else if y >= height {
				y = height - 1
			}
			
			if x == 0 {
				pathData += fmt.Sprintf("%d,%d", x, y)
			} else {
				pathData += fmt.Sprintf(" L%d,%d", x, y)
			}
		}
	}
	
	svg += fmt.Sprintf(`<path d="%s" stroke="#00d4ff" stroke-width="1" fill="none"/>`, pathData)
	svg += `</svg>`
	
	return svg
}

// CheckFFmpegAvailable verifies FFmpeg is installed and working
func CheckFFmpegAvailable() error {
	cmd := exec.Command("ffmpeg", "-version")
	err := cmd.Run()
	if err != nil {
		return fmt.Errorf("ffmpeg not found - please install FFmpeg: %w", err)
	}
	return nil
}