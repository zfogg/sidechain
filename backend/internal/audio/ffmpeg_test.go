package audio

import (
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestFFmpegProcessor tests basic processor creation (fast)
func TestFFmpegProcessor(t *testing.T) {
	processor := NewFFmpegProcessor()
	require.NotNil(t, processor)
	assert.NotEmpty(t, processor.tempDir)

	// Test that temp directory exists
	_, err := os.Stat(processor.tempDir)
	assert.NoError(t, err, "Temp directory should be created")
}

// TestWaveformGeneration tests SVG waveform generation
func TestWaveformGeneration(t *testing.T) {
	processor := NewFFmpegProcessor()

	// Test with mock audio data
	mockAudioData := make([]byte, 1600) // 400 samples * 4 bytes
	for i := 0; i < len(mockAudioData); i += 4 {
		// Create simple sine wave pattern
		mockAudioData[i] = byte(i % 128)
	}

	svg := processor.generateSVGWaveform(mockAudioData, 400, 100)

	assert.NotEmpty(t, svg)
	assert.Contains(t, svg, "<svg width=\"400\" height=\"100\"")
	assert.Contains(t, svg, "#00d4ff") // Sidechain blue
	assert.Contains(t, svg, "</svg>")
	assert.Contains(t, svg, "stroke-width=\"1.5\"")
}

// TestFFmpegInstallationCheck tests FFmpeg availability check
func TestFFmpegInstallationCheck(t *testing.T) {
	// This test just verifies the check function works, not FFmpeg itself
	err := CheckFFmpegInstallation()

	// Either FFmpeg is available or it's not - both are valid test outcomes
	if err != nil {
		t.Logf("FFmpeg not available (expected in CI): %v", err)
		assert.Contains(t, err.Error(), "ffmpeg")
	} else {
		t.Log("FFmpeg available")
	}
}
