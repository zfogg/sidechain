// Package analysis provides a client for the audio analysis microservice.
// The microservice uses Essentia to detect musical key and BPM from audio files.
package analysis

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"mime/multipart"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"github.com/zfogg/sidechain/backend/internal/logger"
	"go.uber.org/zap"
)

// Client provides methods to interact with the audio analysis service.
type Client struct {
	baseURL    string
	httpClient *http.Client
}

// KeyResult contains the key detection results.
type KeyResult struct {
	Value      string  `json:"value"`      // Key in standard notation (e.g., "A minor")
	Camelot    string  `json:"camelot"`    // Camelot wheel notation (e.g., "8A")
	Confidence float64 `json:"confidence"` // Detection confidence (0.0-1.0)
	Scale      string  `json:"scale"`      // Scale type: "major" or "minor"
	Root       string  `json:"root"`       // Root note (e.g., "A", "F#")
}

// BPMResult contains the BPM detection results.
type BPMResult struct {
	Value      float64   `json:"value"`      // Detected BPM
	Confidence float64   `json:"confidence"` // Detection confidence (0.0-1.0)
	Beats      []float64 `json:"beats"`      // Beat positions in seconds (first 100)
}

// TagItem represents a single tag with confidence score.
type TagItem struct {
	Name       string  `json:"name"`       // Tag name
	Confidence float64 `json:"confidence"` // Confidence score (0.0-1.0)
}

// TagResult contains audio tagging results from TensorFlow models.
type TagResult struct {
	TopTags                []TagItem `json:"top_tags,omitempty"`            // Top general audio tags (MagnaTagATune)
	Genres                 []string  `json:"genres,omitempty"`              // Detected genres (MTG-Jamendo)
	Moods                  []string  `json:"moods,omitempty"`               // Detected moods/themes
	Instruments            []string  `json:"instruments,omitempty"`         // Detected instruments
	HasVocals              *bool     `json:"has_vocals,omitempty"`          // Whether audio contains vocals
	VocalConfidence        *float64  `json:"vocal_confidence,omitempty"`    // Confidence for vocal detection
	IsDanceable            *bool     `json:"is_danceable,omitempty"`        // Whether audio is danceable
	DanceabilityConfidence *float64  `json:"danceability_confidence,omitempty"` // Confidence for danceability
	Arousal                *float64  `json:"arousal,omitempty"`             // Energy level (0=calm, 1=energetic)
	Valence                *float64  `json:"valence,omitempty"`             // Mood positivity (0=negative, 1=positive)
}

// AnalysisResult contains the full analysis results.
type AnalysisResult struct {
	Key            *KeyResult `json:"key,omitempty"`
	BPM            *BPMResult `json:"bpm,omitempty"`
	Tags           *TagResult `json:"tags,omitempty"`
	Duration       float64    `json:"duration"`
	AnalysisTimeMs int        `json:"analysis_time_ms"`
}

// AnalysisOptions configures what analysis to perform.
type AnalysisOptions struct {
	DetectKey  bool
	DetectBPM  bool
	DetectTags bool
	TagTopN    int
}

// HealthResponse contains the health check response.
type HealthResponse struct {
	Status          string  `json:"status"`
	EssentiaVersion string  `json:"essentia_version"`
	UptimeSeconds   float64 `json:"uptime_seconds"`
}

// NewClient creates a new audio analysis client.
func NewClient(baseURL string) *Client {
	return &Client{
		baseURL: baseURL,
		httpClient: &http.Client{
			Timeout: 60 * time.Second, // Match analysis timeout
		},
	}
}

// NewClientWithTimeout creates a new audio analysis client with custom timeout.
func NewClientWithTimeout(baseURL string, timeout time.Duration) *Client {
	return &Client{
		baseURL: baseURL,
		httpClient: &http.Client{
			Timeout: timeout,
		},
	}
}

// Health checks if the audio analysis service is healthy.
func (c *Client) Health(ctx context.Context) (*HealthResponse, error) {
	url := c.baseURL + "/health"

	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create health request: %w", err)
	}

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("health check failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("health check returned %d: %s", resp.StatusCode, string(body))
	}

	var health HealthResponse
	if err := json.NewDecoder(resp.Body).Decode(&health); err != nil {
		return nil, fmt.Errorf("failed to decode health response: %w", err)
	}

	return &health, nil
}

// DefaultAnalysisOptions returns the default analysis options (key + BPM, no tags).
func DefaultAnalysisOptions() AnalysisOptions {
	return AnalysisOptions{
		DetectKey:  true,
		DetectBPM:  true,
		DetectTags: false,
		TagTopN:    10,
	}
}

// FullAnalysisOptions returns options for full analysis including tags.
func FullAnalysisOptions() AnalysisOptions {
	return AnalysisOptions{
		DetectKey:  true,
		DetectBPM:  true,
		DetectTags: true,
		TagTopN:    10,
	}
}

// AnalyzeFile analyzes an audio file for key and BPM detection.
// The file is uploaded to the analysis service for processing.
func (c *Client) AnalyzeFile(ctx context.Context, filePath string) (*AnalysisResult, error) {
	return c.AnalyzeFileWithOptions(ctx, filePath, true, true)
}

// AnalyzeFileWithOptions analyzes an audio file with configurable detection options.
func (c *Client) AnalyzeFileWithOptions(ctx context.Context, filePath string, detectKey, detectBPM bool) (*AnalysisResult, error) {
	opts := AnalysisOptions{
		DetectKey:  detectKey,
		DetectBPM:  detectBPM,
		DetectTags: false,
		TagTopN:    10,
	}
	return c.AnalyzeFileWithFullOptions(ctx, filePath, opts)
}

// AnalyzeFileWithFullOptions analyzes an audio file with full configurable options including tagging.
func (c *Client) AnalyzeFileWithFullOptions(ctx context.Context, filePath string, opts AnalysisOptions) (*AnalysisResult, error) {
	url := c.baseURL + "/api/v1/analyze"

	// Open the file
	file, err := os.Open(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to open audio file: %w", err)
	}
	defer file.Close()

	// Create multipart form
	var body bytes.Buffer
	writer := multipart.NewWriter(&body)

	// Add the audio file
	part, err := writer.CreateFormFile("audio_file", filepath.Base(filePath))
	if err != nil {
		return nil, fmt.Errorf("failed to create form file: %w", err)
	}
	if _, err := io.Copy(part, file); err != nil {
		return nil, fmt.Errorf("failed to copy file to form: %w", err)
	}

	// Add detection options
	if err := writer.WriteField("detect_key", fmt.Sprintf("%t", opts.DetectKey)); err != nil {
		return nil, fmt.Errorf("failed to write detect_key field: %w", err)
	}
	if err := writer.WriteField("detect_bpm", fmt.Sprintf("%t", opts.DetectBPM)); err != nil {
		return nil, fmt.Errorf("failed to write detect_bpm field: %w", err)
	}
	if err := writer.WriteField("detect_tags", fmt.Sprintf("%t", opts.DetectTags)); err != nil {
		return nil, fmt.Errorf("failed to write detect_tags field: %w", err)
	}
	tagTopN := opts.TagTopN
	if tagTopN <= 0 {
		tagTopN = 10
	}
	if err := writer.WriteField("tag_top_n", fmt.Sprintf("%d", tagTopN)); err != nil {
		return nil, fmt.Errorf("failed to write tag_top_n field: %w", err)
	}

	if err := writer.Close(); err != nil {
		return nil, fmt.Errorf("failed to close multipart writer: %w", err)
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", url, &body)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}
	req.Header.Set("Content-Type", writer.FormDataContentType())

	// Send request
	startTime := time.Now()
	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("analysis request failed: %w", err)
	}
	defer resp.Body.Close()

	// Handle response
	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode != http.StatusOK {
		logger.Log.Warn("Audio analysis failed",
			zap.Int("status_code", resp.StatusCode),
			zap.String("response", string(respBody)),
			zap.Duration("duration", time.Since(startTime)),
		)
		return nil, fmt.Errorf("analysis failed with status %d: %s", resp.StatusCode, string(respBody))
	}

	var result AnalysisResult
	if err := json.Unmarshal(respBody, &result); err != nil {
		return nil, fmt.Errorf("failed to decode analysis result: %w", err)
	}

	logger.Log.Debug("Audio analysis completed",
		zap.String("file", filePath),
		zap.Duration("duration", time.Since(startTime)),
		zap.Int("analysis_time_ms", result.AnalysisTimeMs),
	)

	return &result, nil
}

// AnalyzeURL analyzes audio from a URL for key and BPM detection.
// The analysis service will download the audio from the URL.
func (c *Client) AnalyzeURL(ctx context.Context, audioURL string) (*AnalysisResult, error) {
	return c.AnalyzeURLWithOptions(ctx, audioURL, true, true)
}

// AnalyzeURLWithOptions analyzes audio from a URL with configurable options.
func (c *Client) AnalyzeURLWithOptions(ctx context.Context, audioURL string, detectKey, detectBPM bool) (*AnalysisResult, error) {
	opts := AnalysisOptions{
		DetectKey:  detectKey,
		DetectBPM:  detectBPM,
		DetectTags: false,
		TagTopN:    10,
	}
	return c.AnalyzeURLWithFullOptions(ctx, audioURL, opts)
}

// AnalyzeURLWithFullOptions analyzes audio from a URL with full configurable options including tagging.
func (c *Client) AnalyzeURLWithFullOptions(ctx context.Context, audioURL string, opts AnalysisOptions) (*AnalysisResult, error) {
	url := c.baseURL + "/api/v1/analyze/json"

	// Create JSON request body
	tagTopN := opts.TagTopN
	if tagTopN <= 0 {
		tagTopN = 10
	}
	reqBody := map[string]interface{}{
		"audio_url":   audioURL,
		"detect_key":  opts.DetectKey,
		"detect_bpm":  opts.DetectBPM,
		"detect_tags": opts.DetectTags,
		"tag_top_n":   tagTopN,
	}

	bodyBytes, err := json.Marshal(reqBody)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal request: %w", err)
	}

	// Create request
	req, err := http.NewRequestWithContext(ctx, "POST", url, bytes.NewReader(bodyBytes))
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")

	// Send request
	startTime := time.Now()
	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("analysis request failed: %w", err)
	}
	defer resp.Body.Close()

	// Handle response
	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read response: %w", err)
	}

	if resp.StatusCode != http.StatusOK {
		logger.Log.Warn("Audio analysis from URL failed",
			zap.Int("status_code", resp.StatusCode),
			zap.String("response", string(respBody)),
			zap.Duration("duration", time.Since(startTime)),
		)
		return nil, fmt.Errorf("analysis failed with status %d: %s", resp.StatusCode, string(respBody))
	}

	var result AnalysisResult
	if err := json.Unmarshal(respBody, &result); err != nil {
		return nil, fmt.Errorf("failed to decode analysis result: %w", err)
	}

	logger.Log.Debug("Audio analysis from URL completed",
		zap.String("url", audioURL),
		zap.Duration("duration", time.Since(startTime)),
		zap.Int("analysis_time_ms", result.AnalysisTimeMs),
	)

	return &result, nil
}

// IsAvailable checks if the analysis service is available and healthy.
func (c *Client) IsAvailable(ctx context.Context) bool {
	health, err := c.Health(ctx)
	if err != nil {
		return false
	}
	return health.Status == "healthy"
}
