package queue

import (
	"bytes"
	"context"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"math"
	"os/exec"
)

// runFFmpegNormalize normalizes audio to -14 LUFS and encodes to MP3
func runFFmpegNormalize(ctx context.Context, inputPath, outputPath string) error {
	// FFmpeg command for loudness normalization + MP3 encoding
	args := []string{
		"-i", inputPath,

		// Audio filters for loudness normalization to -14 LUFS (streaming standard)
		"-af", "loudnorm=I=-14:TP=-1:LRA=7",

		// MP3 encoding settings optimized for music streaming
		"-codec:a", "libmp3lame",
		"-b:a", "128k",  // 128kbps for good quality/size balance
		"-ar", "44100",  // Standard sample rate
		"-ac", "2",      // Stereo

		// Quality settings
		"-q:a", "2", // High quality VBR

		// Overwrite output file
		"-y",

		outputPath,
	}

	cmd := exec.CommandContext(ctx, "ffmpeg", args...)

	var stderr bytes.Buffer
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err != nil {
		return fmt.Errorf("ffmpeg failed: %v, stderr: %s", err, stderr.String())
	}

	return nil
}

// generateWaveformSVG creates an SVG waveform visualization from audio
func generateWaveformSVG(ctx context.Context, audioPath string) (string, error) {
	// Use FFmpeg to extract raw audio samples for waveform
	cmd := exec.CommandContext(ctx, "ffmpeg",
		"-i", audioPath,
		"-ac", "1",           // Convert to mono
		"-ar", "4000",        // Low sample rate for peak extraction
		"-f", "f32le",        // 32-bit float little endian
		"-acodec", "pcm_f32le",
		"-",                  // Output to stdout
	)

	var stdout bytes.Buffer
	var stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err != nil {
		return "", fmt.Errorf("waveform extraction failed: %v", err)
	}

	// Generate SVG from audio samples
	return generateSVGFromSamples(stdout.Bytes(), 400, 100), nil
}

// generateSVGFromSamples creates an SVG waveform from raw float32 audio samples
func generateSVGFromSamples(audioData []byte, width, height int) string {
	if len(audioData) < 4 {
		return fmt.Sprintf(`<svg width="%d" height="%d" xmlns="http://www.w3.org/2000/svg"><rect width="100%%" height="100%%" fill="#1a1a1e"/></svg>`, width, height)
	}

	// Parse float32 samples
	numSamples := len(audioData) / 4
	samples := make([]float32, numSamples)
	for i := 0; i < numSamples; i++ {
		bits := binary.LittleEndian.Uint32(audioData[i*4 : (i+1)*4])
		samples[i] = math.Float32frombits(bits)
	}

	// Calculate peaks for each pixel column
	samplesPerPixel := numSamples / width
	if samplesPerPixel < 1 {
		samplesPerPixel = 1
	}

	peaks := make([]float64, width)
	for x := 0; x < width; x++ {
		startIdx := x * samplesPerPixel
		endIdx := startIdx + samplesPerPixel
		if endIdx > numSamples {
			endIdx = numSamples
		}

		// Find max absolute value in this segment
		maxAbs := float64(0)
		for i := startIdx; i < endIdx; i++ {
			abs := math.Abs(float64(samples[i]))
			if abs > maxAbs {
				maxAbs = abs
			}
		}
		peaks[x] = maxAbs
	}

	// Normalize peaks to 0-1 range
	maxPeak := float64(0)
	for _, p := range peaks {
		if p > maxPeak {
			maxPeak = p
		}
	}
	if maxPeak > 0 {
		for i := range peaks {
			peaks[i] /= maxPeak
		}
	}

	// Generate SVG
	svg := fmt.Sprintf(`<svg width="%d" height="%d" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d">`, width, height, width, height)
	svg += `<rect width="100%" height="100%" fill="#1a1a1e"/>`

	// Draw waveform bars (mirrored style)
	halfHeight := float64(height) / 2
	barWidth := 2
	gap := 1

	for x := 0; x < width; x += barWidth + gap {
		peakIdx := x
		if peakIdx >= len(peaks) {
			break
		}

		barHeight := peaks[peakIdx] * halfHeight * 0.9 // 90% max to leave padding
		if barHeight < 1 {
			barHeight = 1
		}

		y := halfHeight - barHeight
		fullBarHeight := barHeight * 2

		// Draw bar centered on midline
		svg += fmt.Sprintf(`<rect x="%d" y="%.1f" width="%d" height="%.1f" fill="#00d4ff" rx="1"/>`,
			x, y, barWidth, fullBarHeight)
	}

	// Add center line
	svg += fmt.Sprintf(`<line x1="0" y1="%d" x2="%d" y2="%d" stroke="#333" stroke-width="0.5"/>`, height/2, width, height/2)

	svg += `</svg>`

	return svg
}

// extractAudioDuration gets the duration of an audio file using ffprobe
func extractAudioDuration(ctx context.Context, audioPath string) (float64, error) {
	cmd := exec.CommandContext(ctx, "ffprobe",
		"-v", "quiet",
		"-print_format", "json",
		"-show_format",
		audioPath,
	)

	var stdout bytes.Buffer
	cmd.Stdout = &stdout

	err := cmd.Run()
	if err != nil {
		return 0, fmt.Errorf("ffprobe failed: %w", err)
	}

	// Parse JSON output
	var result struct {
		Format struct {
			Duration string `json:"duration"`
		} `json:"format"`
	}

	err = json.Unmarshal(stdout.Bytes(), &result)
	if err != nil {
		return 0, fmt.Errorf("failed to parse ffprobe output: %w", err)
	}

	// Parse duration string to float
	var duration float64
	fmt.Sscanf(result.Format.Duration, "%f", &duration)

	return duration, nil
}

// CheckFFmpegAvailable verifies FFmpeg is installed and working
func CheckFFmpegAvailable() error {
	// Check FFmpeg
	cmd := exec.Command("ffmpeg", "-version")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("ffmpeg not found - install with: sudo pacman -S ffmpeg (Arch) or brew install ffmpeg (macOS)")
	}

	// Check FFprobe
	cmd = exec.Command("ffprobe", "-version")
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("ffprobe not found - it should be included with ffmpeg")
	}

	// Check for LAME MP3 encoder
	cmd = exec.Command("ffmpeg", "-codecs")
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Run()

	if !bytes.Contains(stdout.Bytes(), []byte("libmp3lame")) {
		return fmt.Errorf("libmp3lame codec not found - ensure ffmpeg was built with LAME support")
	}

	return nil
}
