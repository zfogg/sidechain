package fingerprint

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"log"
	"math"
	"math/cmplx"
	"os/exec"
	"sort"

	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// FingerprintConfig contains configuration for audio fingerprinting
type FingerprintConfig struct {
	SampleRate      int     // Target sample rate for analysis (default: 8000)
	FFTSize         int     // FFT window size (default: 1024)
	HopSize         int     // Hop between windows (default: 256)
	FreqBands       int     // Number of frequency bands (default: 6)
	PeaksPerBand    int     // Max peaks per frequency band (default: 3)
	TargetZoneSize  int     // Time frames to look ahead for targets (default: 5)
	MinConfidence   float64 // Minimum match confidence (default: 0.7)
}

// DefaultFingerprintConfig returns sensible defaults for music fingerprinting
func DefaultFingerprintConfig() FingerprintConfig {
	return FingerprintConfig{
		SampleRate:     8000,  // Low sample rate is sufficient for fingerprinting
		FFTSize:        1024,  // ~128ms window at 8kHz
		HopSize:        256,   // ~32ms hop (75% overlap)
		FreqBands:      6,     // Divide spectrum into 6 bands
		PeaksPerBand:   3,     // Top 3 peaks per band
		TargetZoneSize: 5,     // Look 5 frames ahead for anchor-target pairs
		MinConfidence:  0.7,   // 70% minimum match confidence
	}
}

// Fingerprint represents a complete audio fingerprint
type Fingerprint struct {
	Hash       string            `json:"hash"`        // Primary hash for fast lookup
	Hashes     []uint32          `json:"hashes"`      // All hash values
	Timestamps []int             `json:"timestamps"`  // Frame indices for each hash
	Duration   float64           `json:"duration"`    // Audio duration in seconds
	Peaks      []Peak            `json:"peaks"`       // Detected peaks (for debugging)
}

// Peak represents a spectral peak in the spectrogram
type Peak struct {
	TimeFrame int     // Frame index
	FreqBin   int     // Frequency bin index
	Magnitude float64 // Peak magnitude
}

// FingerprintResult contains the result of fingerprint analysis
type FingerprintResult struct {
	Fingerprint    *Fingerprint `json:"fingerprint"`
	MatchedSoundID *string      `json:"matched_sound_id,omitempty"`
	MatchConfidence float64     `json:"match_confidence"`
	IsNewSound     bool         `json:"is_new_sound"`
}

// AudioFingerprinter handles audio fingerprint generation and matching
type AudioFingerprinter struct {
	config FingerprintConfig
}

// NewAudioFingerprinter creates a new fingerprinter with default config
func NewAudioFingerprinter() *AudioFingerprinter {
	return &AudioFingerprinter{
		config: DefaultFingerprintConfig(),
	}
}

// NewAudioFingerprinterWithConfig creates a fingerprinter with custom config
func NewAudioFingerprinterWithConfig(config FingerprintConfig) *AudioFingerprinter {
	return &AudioFingerprinter{config: config}
}

// GenerateFingerprint generates a fingerprint from an audio file
func (fp *AudioFingerprinter) GenerateFingerprint(ctx context.Context, audioPath string) (*Fingerprint, error) {
	// Step 1: Extract raw audio using FFmpeg (mono, low sample rate)
	samples, err := fp.extractAudioSamples(ctx, audioPath)
	if err != nil {
		return nil, fmt.Errorf("failed to extract audio samples: %w", err)
	}

	if len(samples) < fp.config.FFTSize {
		return nil, fmt.Errorf("audio too short for fingerprinting (need at least %d samples)", fp.config.FFTSize)
	}

	// Step 2: Generate spectrogram using FFT
	spectrogram := fp.computeSpectrogram(samples)

	// Step 3: Find peaks in spectrogram
	peaks := fp.findPeaks(spectrogram)

	if len(peaks) < 2 {
		return nil, fmt.Errorf("not enough spectral peaks detected (found %d)", len(peaks))
	}

	// Step 4: Generate fingerprint hashes using anchor-target pairs
	hashes, timestamps := fp.generateHashes(peaks)

	if len(hashes) == 0 {
		return nil, fmt.Errorf("no fingerprint hashes generated")
	}

	// Step 5: Compute primary hash (for fast lookup)
	primaryHash := fp.computePrimaryHash(hashes)

	duration := float64(len(samples)) / float64(fp.config.SampleRate)

	return &Fingerprint{
		Hash:       primaryHash,
		Hashes:     hashes,
		Timestamps: timestamps,
		Duration:   duration,
		Peaks:      peaks,
	}, nil
}

// extractAudioSamples uses FFmpeg to extract raw audio samples
func (fp *AudioFingerprinter) extractAudioSamples(ctx context.Context, audioPath string) ([]float64, error) {
	// FFmpeg command to extract mono audio at target sample rate as raw floats
	cmd := exec.CommandContext(ctx, "ffmpeg",
		"-i", audioPath,
		"-ac", "1",                           // Mono
		"-ar", fmt.Sprintf("%d", fp.config.SampleRate), // Target sample rate
		"-f", "f32le",                        // 32-bit float little endian
		"-",                                   // Output to stdout
	)

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err != nil {
		return nil, fmt.Errorf("ffmpeg failed: %v, stderr: %s", err, stderr.String())
	}

	// Convert raw bytes to float64 samples
	rawData := stdout.Bytes()
	numSamples := len(rawData) / 4 // 4 bytes per float32
	samples := make([]float64, numSamples)

	for i := 0; i < numSamples; i++ {
		bits := binary.LittleEndian.Uint32(rawData[i*4 : (i+1)*4])
		samples[i] = float64(math.Float32frombits(bits))
	}

	return samples, nil
}

// computeSpectrogram generates a spectrogram using FFT
func (fp *AudioFingerprinter) computeSpectrogram(samples []float64) [][]float64 {
	numFrames := (len(samples) - fp.config.FFTSize) / fp.config.HopSize
	if numFrames <= 0 {
		numFrames = 1
	}

	spectrogram := make([][]float64, numFrames)
	hannWindow := fp.createHannWindow(fp.config.FFTSize)

	for frame := 0; frame < numFrames; frame++ {
		startIdx := frame * fp.config.HopSize
		endIdx := startIdx + fp.config.FFTSize
		if endIdx > len(samples) {
			break
		}

		// Apply Hann window
		windowedSamples := make([]complex128, fp.config.FFTSize)
		for i := 0; i < fp.config.FFTSize; i++ {
			windowedSamples[i] = complex(samples[startIdx+i]*hannWindow[i], 0)
		}

		// Compute FFT
		spectrum := fp.fft(windowedSamples)

		// Compute magnitude spectrum (only positive frequencies)
		numBins := fp.config.FFTSize / 2
		magnitudes := make([]float64, numBins)
		for i := 0; i < numBins; i++ {
			magnitudes[i] = cmplx.Abs(spectrum[i])
		}

		spectrogram[frame] = magnitudes
	}

	return spectrogram
}

// createHannWindow creates a Hann window function
func (fp *AudioFingerprinter) createHannWindow(size int) []float64 {
	window := make([]float64, size)
	for i := 0; i < size; i++ {
		window[i] = 0.5 * (1 - math.Cos(2*math.Pi*float64(i)/float64(size-1)))
	}
	return window
}

// fft computes the Fast Fourier Transform (Cooley-Tukey)
func (fp *AudioFingerprinter) fft(x []complex128) []complex128 {
	n := len(x)
	if n <= 1 {
		return x
	}

	// Check if n is power of 2, if not pad with zeros
	if n&(n-1) != 0 {
		nextPow2 := 1
		for nextPow2 < n {
			nextPow2 <<= 1
		}
		padded := make([]complex128, nextPow2)
		copy(padded, x)
		x = padded
		n = nextPow2
	}

	// Bit reversal
	result := make([]complex128, n)
	bits := int(math.Log2(float64(n)))
	for i := 0; i < n; i++ {
		j := fp.reverseBits(i, bits)
		result[j] = x[i]
	}

	// Iterative FFT
	for s := 1; s <= bits; s++ {
		m := 1 << s
		wm := cmplx.Exp(complex(0, -2*math.Pi/float64(m)))

		for k := 0; k < n; k += m {
			w := complex(1, 0)
			for j := 0; j < m/2; j++ {
				t := w * result[k+j+m/2]
				u := result[k+j]
				result[k+j] = u + t
				result[k+j+m/2] = u - t
				w *= wm
			}
		}
	}

	return result
}

// reverseBits reverses the bits in a number
func (fp *AudioFingerprinter) reverseBits(num, bits int) int {
	result := 0
	for i := 0; i < bits; i++ {
		result = (result << 1) | (num & 1)
		num >>= 1
	}
	return result
}

// findPeaks finds spectral peaks in the spectrogram
func (fp *AudioFingerprinter) findPeaks(spectrogram [][]float64) []Peak {
	if len(spectrogram) == 0 {
		return nil
	}

	numBins := len(spectrogram[0])
	bandSize := numBins / fp.config.FreqBands
	if bandSize < 1 {
		bandSize = 1
	}

	var peaks []Peak

	for frame := 0; frame < len(spectrogram); frame++ {
		magnitudes := spectrogram[frame]

		// Find peaks in each frequency band
		for band := 0; band < fp.config.FreqBands; band++ {
			startBin := band * bandSize
			endBin := startBin + bandSize
			if endBin > numBins {
				endBin = numBins
			}

			// Find top peaks in this band
			bandPeaks := fp.findBandPeaks(magnitudes, startBin, endBin, frame)
			peaks = append(peaks, bandPeaks...)
		}
	}

	return peaks
}

// findBandPeaks finds peaks within a frequency band
func (fp *AudioFingerprinter) findBandPeaks(magnitudes []float64, startBin, endBin, frame int) []Peak {
	type binMag struct {
		bin int
		mag float64
	}

	var candidates []binMag

	// Find local maxima
	for bin := startBin + 1; bin < endBin-1; bin++ {
		if magnitudes[bin] > magnitudes[bin-1] && magnitudes[bin] > magnitudes[bin+1] {
			candidates = append(candidates, binMag{bin: bin, mag: magnitudes[bin]})
		}
	}

	// Sort by magnitude descending
	sort.Slice(candidates, func(i, j int) bool {
		return candidates[i].mag > candidates[j].mag
	})

	// Take top N peaks
	numPeaks := fp.config.PeaksPerBand
	if len(candidates) < numPeaks {
		numPeaks = len(candidates)
	}

	peaks := make([]Peak, numPeaks)
	for i := 0; i < numPeaks; i++ {
		peaks[i] = Peak{
			TimeFrame: frame,
			FreqBin:   candidates[i].bin,
			Magnitude: candidates[i].mag,
		}
	}

	return peaks
}

// generateHashes creates anchor-target hash pairs from peaks
func (fp *AudioFingerprinter) generateHashes(peaks []Peak) ([]uint32, []int) {
	// Sort peaks by time
	sort.Slice(peaks, func(i, j int) bool {
		return peaks[i].TimeFrame < peaks[j].TimeFrame
	})

	var hashes []uint32
	var timestamps []int

	// Generate anchor-target pairs
	for i, anchor := range peaks {
		// Find target peaks in the target zone
		for j := i + 1; j < len(peaks) && peaks[j].TimeFrame <= anchor.TimeFrame+fp.config.TargetZoneSize; j++ {
			target := peaks[j]

			// Create hash from anchor-target pair
			// Format: [anchor_freq:9 bits][target_freq:9 bits][delta_time:14 bits]
			deltaTime := target.TimeFrame - anchor.TimeFrame
			if deltaTime > 0 {
				hash := (uint32(anchor.FreqBin&0x1FF) << 23) |
					(uint32(target.FreqBin&0x1FF) << 14) |
					(uint32(deltaTime&0x3FFF))

				hashes = append(hashes, hash)
				timestamps = append(timestamps, anchor.TimeFrame)
			}
		}
	}

	return hashes, timestamps
}

// computePrimaryHash computes a single hash representing the fingerprint
func (fp *AudioFingerprinter) computePrimaryHash(hashes []uint32) string {
	// Use SHA256 of sorted hashes for consistency
	sorted := make([]uint32, len(hashes))
	copy(sorted, hashes)
	sort.Slice(sorted, func(i, j int) bool { return sorted[i] < sorted[j] })

	data := make([]byte, len(sorted)*4)
	for i, h := range sorted {
		binary.LittleEndian.PutUint32(data[i*4:], h)
	}

	hash := sha256.Sum256(data)
	return hex.EncodeToString(hash[:16]) // Use first 16 bytes (128 bits)
}

// FingerprintAndMatch generates a fingerprint and matches against existing sounds
func (fp *AudioFingerprinter) FingerprintAndMatch(ctx context.Context, audioPath string, postID string, userID string) (*FingerprintResult, error) {
	// Generate fingerprint
	fingerprint, err := fp.GenerateFingerprint(ctx, audioPath)
	if err != nil {
		return nil, err
	}

	// Serialize fingerprint data for storage
	fingerprintData, err := json.Marshal(fingerprint)
	if err != nil {
		return nil, fmt.Errorf("failed to serialize fingerprint: %w", err)
	}

	// Search for matching sounds
	matchedSound, confidence, err := fp.findMatchingSound(fingerprint)
	if err != nil {
		log.Printf("Warning: fingerprint matching failed: %v", err)
		// Continue even if matching fails
	}

	result := &FingerprintResult{
		Fingerprint:     fingerprint,
		MatchConfidence: confidence,
	}

	if matchedSound != nil && confidence >= fp.config.MinConfidence {
		// Found a match - link to existing sound
		result.MatchedSoundID = &matchedSound.ID
		result.IsNewSound = false

		// Create fingerprint record linked to existing sound
		audioFingerprint := &models.AudioFingerprint{
			AudioPostID:     &postID,
			SoundID:         &matchedSound.ID,
			FingerprintHash: fingerprint.Hash,
			FingerprintData: fingerprintData,
			MatchConfidence: confidence,
			Status:          "completed",
		}

		if err := database.DB.Create(audioFingerprint).Error; err != nil {
			return nil, fmt.Errorf("failed to create fingerprint record: %w", err)
		}

		// Create sound usage record
		soundUsage := &models.SoundUsage{
			SoundID:     matchedSound.ID,
			UserID:      userID,
			AudioPostID: &postID,
		}
		if err := database.DB.Create(soundUsage).Error; err != nil {
			log.Printf("Warning: failed to create sound usage record: %v", err)
		}

		// Increment usage count
		database.DB.Model(&models.Sound{}).
			Where("id = ?", matchedSound.ID).
			UpdateColumn("usage_count", database.DB.Raw("usage_count + 1"))

	} else {
		// No match - create new sound
		result.IsNewSound = true

		newSound := &models.Sound{
			Name:            fmt.Sprintf("Sound from %s", postID[:8]),
			OriginalPostID:  &postID,
			CreatorID:       userID,
			FingerprintHash: fingerprint.Hash,
			UsageCount:      1,
			IsPublic:        true,
			Duration:        fingerprint.Duration,
		}

		if err := database.DB.Create(newSound).Error; err != nil {
			return nil, fmt.Errorf("failed to create sound record: %w", err)
		}

		result.MatchedSoundID = &newSound.ID

		// Create fingerprint record
		audioFingerprint := &models.AudioFingerprint{
			AudioPostID:     &postID,
			SoundID:         &newSound.ID,
			FingerprintHash: fingerprint.Hash,
			FingerprintData: fingerprintData,
			MatchConfidence: 1.0, // Perfect match with itself
			Status:          "completed",
		}

		if err := database.DB.Create(audioFingerprint).Error; err != nil {
			return nil, fmt.Errorf("failed to create fingerprint record: %w", err)
		}

		// Create sound usage record
		soundUsage := &models.SoundUsage{
			SoundID:     newSound.ID,
			UserID:      userID,
			AudioPostID: &postID,
		}
		if err := database.DB.Create(soundUsage).Error; err != nil {
			log.Printf("Warning: failed to create sound usage record: %v", err)
		}
	}

	return result, nil
}

// findMatchingSound searches for a sound matching the fingerprint
func (fp *AudioFingerprinter) findMatchingSound(fingerprint *Fingerprint) (*models.Sound, float64, error) {
	if database.DB == nil {
		return nil, 0, nil
	}

	// First, try exact hash match
	var exactMatch models.Sound
	err := database.DB.Where("fingerprint_hash = ?", fingerprint.Hash).
		First(&exactMatch).Error

	if err == nil {
		return &exactMatch, 1.0, nil // Perfect match
	}

	// If no exact match, we could implement fuzzy matching by comparing
	// individual hashes across all sounds. For now, we'll rely on the
	// primary hash which should catch most duplicates.

	// TODO: Implement fuzzy matching for partial matches
	// This would involve:
	// 1. Query all audio_fingerprints
	// 2. Deserialize FingerprintData and compare hash counts
	// 3. Calculate Jaccard similarity between hash sets
	// 4. Return best match above threshold

	return nil, 0, nil
}

// ProcessPostFingerprint generates and stores fingerprint for a post
// This is called from the audio processing pipeline
func ProcessPostFingerprint(ctx context.Context, audioPath, postID, userID string) error {
	fingerprinter := NewAudioFingerprinter()

	result, err := fingerprinter.FingerprintAndMatch(ctx, audioPath, postID, userID)
	if err != nil {
		log.Printf("Warning: fingerprinting failed for post %s: %v", postID, err)
		// Mark as failed but don't block the upload
		if database.DB != nil {
			database.DB.Create(&models.AudioFingerprint{
				AudioPostID:     &postID,
				FingerprintHash: "",
				Status:          "failed",
			})
		}
		return nil // Don't fail the whole upload
	}

	log.Printf("✅ Fingerprinting complete for post %s: isNewSound=%v, confidence=%.2f",
		postID, result.IsNewSound, result.MatchConfidence)

	return nil
}
