package waveform

import (
	"bytes"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"io"
	"math"

	"github.com/go-audio/audio"
	"github.com/go-audio/wav"
)

// Generator creates waveform images from audio data
type Generator struct {
	Width       int
	Height      int
	Background  color.Color
	Foreground  color.Color
	SampleRate  int
}

// NewGenerator creates a new waveform generator with default settings
func NewGenerator() *Generator {
	return &Generator{
		Width:       1200,
		Height:      200,
		Background:  color.RGBA{38, 38, 44, 255},  // #26262C
		Foreground:  color.RGBA{180, 228, 255, 255}, // #B4E4FF (skyBlue)
		SampleRate:  44100,
	}
}

// GenerateFromWAV generates a waveform PNG from WAV audio data
func (g *Generator) GenerateFromWAV(audioData io.ReadSeeker) ([]byte, error) {
	// Decode WAV file
	decoder := wav.NewDecoder(audioData)
	if !decoder.IsValidFile() {
		return nil, fmt.Errorf("invalid WAV file")
	}

	// Read audio buffer
	buf, err := decoder.FullPCMBuffer()
	if err != nil {
		return nil, fmt.Errorf("failed to read audio buffer: %w", err)
	}

	if buf == nil || len(buf.Data) == 0 {
		return nil, fmt.Errorf("empty audio buffer")
	}

	// Generate waveform image
	img := g.generateImage(buf)

	// Encode to PNG
	var pngBuf bytes.Buffer
	if err := png.Encode(&pngBuf, img); err != nil {
		return nil, fmt.Errorf("failed to encode PNG: %w", err)
	}

	return pngBuf.Bytes(), nil
}

// generateImage creates the waveform image from audio buffer
func (g *Generator) generateImage(buf *audio.IntBuffer) image.Image {
	img := image.NewRGBA(image.Rect(0, 0, g.Width, g.Height))

	// Fill background
	for y := 0; y < g.Height; y++ {
		for x := 0; x < g.Width; x++ {
			img.Set(x, y, g.Background)
		}
	}

	if len(buf.Data) == 0 {
		return img
	}

	// Calculate samples per pixel
	samplesPerPixel := len(buf.Data) / g.Width
	if samplesPerPixel < 1 {
		samplesPerPixel = 1
	}

	// Draw waveform
	centerY := g.Height / 2

	for x := 0; x < g.Width; x++ {
		// Get samples for this pixel
		startSample := x * samplesPerPixel
		endSample := startSample + samplesPerPixel
		if endSample > len(buf.Data) {
			endSample = len(buf.Data)
		}

		if startSample >= len(buf.Data) {
			break
		}

		// Calculate RMS (root mean square) for this chunk
		var sum float64
		count := 0
		for i := startSample; i < endSample; i++ {
			sample := float64(buf.Data[i])
			sum += sample * sample
			count++
		}

		if count == 0 {
			continue
		}

		rms := math.Sqrt(sum / float64(count))

		// Normalize to 0-1 range (assuming 16-bit audio)
		normalized := rms / float64(math.MaxInt16)
		if normalized > 1.0 {
			normalized = 1.0
		}

		// Calculate height of waveform bar
		barHeight := int(normalized * float64(g.Height/2))

		// Draw vertical line from center
		for y := centerY - barHeight; y <= centerY+barHeight; y++ {
			if y >= 0 && y < g.Height {
				img.Set(x, y, g.Foreground)
			}
		}
	}

	return img
}

// SetColors sets custom colors for the waveform
func (g *Generator) SetColors(background, foreground color.Color) {
	g.Background = background
	g.Foreground = foreground
}

// SetDimensions sets custom dimensions for the waveform
func (g *Generator) SetDimensions(width, height int) {
	g.Width = width
	g.Height = height
}
