// +build integration

package integration

import (
	"context"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zfogg/sidechain/backend/internal/audio"
)

// TestFFmpegIntegration tests actual FFmpeg processing (slow)
func TestFFmpegIntegration(t *testing.T) {
	// Skip if FFmpeg not available
	if err := audio.CheckFFmpegInstallation(); err != nil {
		t.Skipf("FFmpeg not available: %v", err)
	}

	processor := audio.NewFFmpegProcessor()
	
	// Create test audio file
	testAudioPath := createTestAudioFile(t)
	defer os.Remove(testAudioPath)

	metadata := audio.AudioMetadata{
		BPM:          128,
		Key:          "C major",
		DurationBars: 4,
		DAW:          "Test DAW",
		Genre:        []string{"Test"},
		SampleRate:   44100,
	}

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	// Test actual audio processing
	result, err := processor.ProcessAudioFile(ctx, testAudioPath, metadata)
	require.NoError(t, err)
	require.NotNil(t, result)

	// Verify results
	assert.Greater(t, len(result.Data), 0, "Processed audio data should not be empty")
	assert.Greater(t, result.Duration, 0.0, "Duration should be positive")
	assert.Equal(t, 128, result.Bitrate, "Bitrate should be 128kbps")
	assert.NotEmpty(t, result.WaveformSVG, "Waveform SVG should be generated")
	assert.Contains(t, result.WaveformSVG, "<svg", "Should be valid SVG")
	assert.Contains(t, result.WaveformSVG, "#00d4ff", "Should use Sidechain blue color")
}

// createTestAudioFile creates a simple test audio file using FFmpeg
func createTestAudioFile(t *testing.T) string {
	tempPath := filepath.Join(os.TempDir(), "integration_test_audio_"+time.Now().Format("20060102_150405")+".wav")
	
	// Generate 2-second sine wave at 440Hz using FFmpeg
	cmd := exec.Command("ffmpeg",
		"-f", "lavfi",
		"-i", "sine=frequency=440:duration=2",
		"-ar", "44100",
		"-ac", "2",
		"-y",
		tempPath,
	)

	err := cmd.Run()
	if err != nil {
		t.Fatalf("Failed to create test audio file: %v", err)
	}

	return tempPath
}

// BenchmarkAudioProcessing benchmarks the actual audio processing pipeline
func BenchmarkAudioProcessing(b *testing.B) {
	if err := audio.CheckFFmpegInstallation(); err != nil {
		b.Skipf("FFmpeg not available: %v", err)
	}

	processor := audio.NewFFmpegProcessor()
	testAudioPath := createTestAudioFile(&testing.T{})
	defer os.Remove(testAudioPath)

	metadata := audio.AudioMetadata{
		BPM:          128,
		Key:          "C major", 
		DurationBars: 8,
		DAW:          "Benchmark DAW",
		Genre:        []string{"Electronic"},
		SampleRate:   44100,
	}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		_, err := processor.ProcessAudioFile(ctx, testAudioPath, metadata)
		cancel()
		
		if err != nil {
			b.Fatalf("Processing failed: %v", err)
		}
	}
}