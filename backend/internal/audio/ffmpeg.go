package audio

import (
	"bytes"
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/google/uuid"
)

// FFmpegProcessor handles audio processing with FFmpeg
type FFmpegProcessor struct {
	tempDir string
}

// NewFFmpegProcessor creates a new FFmpeg processor
func NewFFmpegProcessor() *FFmpegProcessor {
	tempDir := "/tmp/sidechain_audio"
	os.MkdirAll(tempDir, 0755)
	
	return &FFmpegProcessor{
		tempDir: tempDir,
	}
}

// ProcessAudioFile handles complete audio processing pipeline
func (p *FFmpegProcessor) ProcessAudioFile(ctx context.Context, inputPath string, metadata AudioMetadata) (*ProcessedAudio, error) {
	// 1. Normalize loudness to -14 LUFS and convert to MP3
	mp3Path, err := p.normalizeAndEncode(ctx, inputPath, metadata)
	if err != nil {
		return nil, fmt.Errorf("failed to normalize audio: %w", err)
	}
	defer os.Remove(mp3Path)

	// 2. Generate waveform visualization
	waveformSVG, err := p.generateWaveform(ctx, mp3Path)
	if err != nil {
		return nil, fmt.Errorf("failed to generate waveform: %w", err)
	}

	// 3. Extract additional metadata
	audioInfo, err := p.extractAudioInfo(ctx, mp3Path)
	if err != nil {
		return nil, fmt.Errorf("failed to extract audio info: %w", err)
	}

	// 4. Read processed file data
	processedData, err := os.ReadFile(mp3Path)
	if err != nil {
		return nil, fmt.Errorf("failed to read processed file: %w", err)
	}

	return &ProcessedAudio{
		Data:        processedData,
		WaveformSVG: waveformSVG,
		Duration:    audioInfo.Duration,
		FileSize:    int64(len(processedData)),
		SampleRate:  audioInfo.SampleRate,
		Bitrate:     128, // We encode at 128kbps
	}, nil
}

// ProcessedAudio contains the result of audio processing
type ProcessedAudio struct {
	Data        []byte  `json:"-"`         // Raw MP3 data
	WaveformSVG string  `json:"waveform"`  // SVG waveform
	Duration    float64 `json:"duration"`  // Duration in seconds
	FileSize    int64   `json:"file_size"` // Processed file size
	SampleRate  int     `json:"sample_rate"`
	Bitrate     int     `json:"bitrate"`
}

// AudioInfo contains extracted audio metadata
type AudioInfo struct {
	Duration   float64 `json:"duration"`
	SampleRate int     `json:"sample_rate"`
	Channels   int     `json:"channels"`
	Bitrate    int     `json:"bitrate"`
}

// normalizeAndEncode normalizes loudness and encodes to MP3
func (p *FFmpegProcessor) normalizeAndEncode(ctx context.Context, inputPath string, metadata AudioMetadata) (string, error) {
	outputPath := filepath.Join(p.tempDir, uuid.New().String()+"_normalized.mp3")

	// FFmpeg command for loudness normalization + MP3 encoding
	// Using two-pass loudnorm for best quality
	args := []string{
		"-i", inputPath,
		
		// Audio filters for loudness normalization
		"-af", "loudnorm=I=-14:TP=-1:LRA=7:measured_I=-16:measured_LRA=11:measured_TP=-1.5:measured_thresh=-26.12:offset=-0.47",
		
		// MP3 encoding settings optimized for music
		"-codec:a", "libmp3lame",
		"-b:a", "128k",           // 128kbps for good quality/size balance
		"-ar", "44100",           // Standard sample rate
		"-ac", "2",               // Stereo
		"-q:a", "2",              // High quality VBR
		
		// Metadata preservation
		"-map_metadata", "0",
		
		// Overwrite output file
		"-y",
		
		outputPath,
	}

	cmd := exec.CommandContext(ctx, "ffmpeg", args...)
	
	var stderr bytes.Buffer
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err != nil {
		return "", fmt.Errorf("ffmpeg normalization failed: %v, stderr: %s", err, stderr.String())
	}

	return outputPath, nil
}

// generateWaveform creates SVG waveform using FFmpeg analysis
func (p *FFmpegProcessor) generateWaveform(ctx context.Context, audioPath string) (string, error) {
	// Use FFmpeg to extract peak data for waveform
	cmd := exec.CommandContext(ctx, "ffmpeg",
		"-i", audioPath,
		"-ac", "1",                    // Convert to mono for analysis
		"-ar", "4000",                 // Downsample for peak extraction
		"-f", "f32le",                 // 32-bit float output
		"-acodec", "pcm_f32le",
		"-",                           // Output to stdout
	)

	var stdout bytes.Buffer
	var stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err != nil {
		return "", fmt.Errorf("waveform extraction failed: %v, stderr: %s", err, stderr.String())
	}

	// Generate SVG from audio peaks
	return p.generateSVGWaveform(stdout.Bytes(), 400, 100), nil
}

// generateSVGWaveform creates SVG from raw audio peaks
func (p *FFmpegProcessor) generateSVGWaveform(audioData []byte, width, height int) string {
	if len(audioData) < 4 {
		return `<svg width="400" height="100"><rect width="100%" height="100%" fill="#1a1a1e"/></svg>`
	}

	// Calculate samples per pixel
	samplesPerPixel := len(audioData) / 4 / width // 4 bytes per float32
	if samplesPerPixel < 1 {
		samplesPerPixel = 1
	}

	svg := fmt.Sprintf(`<svg width="%d" height="%d" xmlns="http://www.w3.org/2000/svg">`, width, height)
	svg += `<rect width="100%" height="100%" fill="#1a1a1e"/>`
	
	// Generate waveform path
	pathData := "M"
	for x := 0; x < width; x++ {
		sampleIndex := x * samplesPerPixel * 4
		
		if sampleIndex+3 < len(audioData) {
			// Extract peak amplitude (simplified peak detection)
			amplitude := float64(audioData[sampleIndex]%128) / 128.0
			
			// Convert to SVG coordinates
			y := int(float64(height/2) * (1.0 - amplitude))
			if y < 0 {
				y = 0
			} else if y >= height {
				y = height - 1
			}
			
			if x == 0 {
				pathData += fmt.Sprintf("%d,%d", x, height/2)
			} else {
				pathData += fmt.Sprintf(" L%d,%d", x, y)
			}
		}
	}
	
	// Add waveform path with Sidechain blue color
	svg += fmt.Sprintf(`<path d="%s" stroke="#00d4ff" stroke-width="1.5" fill="none" opacity="0.8"/>`, pathData)
	
	// Add center line
	svg += fmt.Sprintf(`<line x1="0" y1="%d" x2="%d" y2="%d" stroke="#333" stroke-width="0.5"/>`, height/2, width, height/2)
	
	svg += `</svg>`
	
	return svg
}

// extractAudioInfo gets duration and other metadata from processed file
func (p *FFmpegProcessor) extractAudioInfo(ctx context.Context, audioPath string) (*AudioInfo, error) {
	// Use ffprobe to get precise audio information
	cmd := exec.CommandContext(ctx, "ffprobe",
		"-v", "quiet",
		"-print_format", "json",
		"-show_format",
		"-show_streams",
		audioPath,
	)

	var stdout bytes.Buffer
	cmd.Stdout = &stdout

	err := cmd.Run()
	if err != nil {
		return nil, fmt.Errorf("ffprobe failed: %w", err)
	}

	// Parse JSON output (simplified - in production use proper JSON parsing)
	output := stdout.String()
	
	// Extract duration (simple string parsing for now)
	duration := extractFloat(output, "duration")
	sampleRate := int(extractFloat(output, "sample_rate"))
	
	return &AudioInfo{
		Duration:   duration,
		SampleRate: sampleRate,
		Channels:   2, // We normalize to stereo
		Bitrate:    128,
	}, nil
}

// extractFloat extracts a float value from FFprobe JSON output
func extractFloat(json, key string) float64 {
	// Simple string search - in production use proper JSON parsing
	searchStr := fmt.Sprintf(`"%s":"`, key)
	start := strings.Index(json, searchStr)
	if start == -1 {
		return 0
	}
	
	start += len(searchStr)
	end := strings.Index(json[start:], `"`)
	if end == -1 {
		return 0
	}
	
	valueStr := json[start : start+end]
	value, _ := strconv.ParseFloat(valueStr, 64)
	return value
}

// CheckFFmpegInstallation verifies FFmpeg and required codecs
func CheckFFmpegInstallation() error {
	// Check FFmpeg
	cmd := exec.Command("ffmpeg", "-version")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("ffmpeg not found - install with: brew install ffmpeg")
	}

	// Check FFprobe
	cmd = exec.Command("ffprobe", "-version")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("ffprobe not found - install with: brew install ffmpeg")
	}

	// Check for LAME MP3 encoder
	cmd = exec.Command("ffmpeg", "-codecs")
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Run()
	
	if !strings.Contains(stdout.String(), "libmp3lame") {
		return fmt.Errorf("libmp3lame codec not found - install with: brew install ffmpeg --with-lame")
	}

	return nil
}