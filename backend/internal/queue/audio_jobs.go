package queue

import (
	"context"
	"fmt"
	"os"
	"runtime"
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/analysis"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/fingerprint"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/storage"
	"go.uber.org/zap"
)

// AudioJob represents an audio processing job
type AudioJob struct {
	ID           string                 `json:"id"`
	UserID       string                 `json:"user_id"`
	PostID       string                 `json:"post_id"` // Link to AudioPost record
	TempFilePath string                 `json:"temp_file_path"`
	Filename     string                 `json:"filename"`
	Metadata     map[string]interface{} `json:"metadata"`
	Status       string                 `json:"status"` // pending, processing, complete, failed
	CreatedAt    time.Time              `json:"created_at"`
	CompletedAt  *time.Time             `json:"completed_at,omitempty"`
	ErrorMessage *string                `json:"error_message,omitempty"`
	Result       *AudioJobResult        `json:"result,omitempty"`
}

// AudioJobResult contains the processing result
type AudioJobResult struct {
	AudioURL      string  `json:"audio_url"`
	WaveformURL   string  `json:"waveform_url"`
	Duration      float64 `json:"duration"`
	ProcessedSize int64   `json:"processed_size"`

	// Audio analysis results (from Essentia service)
	DetectedBPM           *float64 `json:"detected_bpm,omitempty"`
	DetectedBPMConfidence *float64 `json:"detected_bpm_confidence,omitempty"`
	DetectedKey           *string  `json:"detected_key,omitempty"`
	DetectedKeyConfidence *float64 `json:"detected_key_confidence,omitempty"`
	DetectedKeyCamelot    *string  `json:"detected_key_camelot,omitempty"`

	// Audio tagging results (from Essentia TensorFlow models)
	DetectedTags        []string `json:"detected_tags,omitempty"`        // MagnaTagATune top tags
	DetectedGenres      []string `json:"detected_genres,omitempty"`      // MTG-Jamendo genres
	DetectedMoods       []string `json:"detected_moods,omitempty"`       // MTG-Jamendo moods
	DetectedInstruments []string `json:"detected_instruments,omitempty"` // MTG-Jamendo instruments
	HasVocals           *bool    `json:"has_vocals,omitempty"`           // Voice/Instrumental detection
	IsDanceable         *bool    `json:"is_danceable,omitempty"`         // Danceability detection
	Arousal             *float64 `json:"arousal,omitempty"`              // Energy level (0-1)
	Valence             *float64 `json:"valence,omitempty"`              // Mood positivity (0-1)
}

// AudioQueue manages background audio processing
type AudioQueue struct {
	jobs       chan *AudioJob
	results    map[string]*AudioJob
	resultsMux sync.RWMutex
	workers    int
	ctx        context.Context
	cancel     context.CancelFunc

	// Dependencies for actual processing
	s3Uploader    *storage.S3Uploader
	audioAnalyzer *analysis.Client
	tempDir       string

	// Callback for post indexing (protected by callbackMux to prevent data races)
	callbackMux    sync.RWMutex
	onPostComplete func(postID string)

	// For testing: channels to signal job completion
	jobCompleted chan string
}

// NewAudioQueue creates a new audio processing queue
func NewAudioQueue(s3Uploader *storage.S3Uploader) *AudioQueue {
	ctx, cancel := context.WithCancel(context.Background())

	// Use CPU count for worker pool size
	workers := runtime.NumCPU()
	if workers > 8 {
		workers = 8 // Cap at 8 workers to avoid overwhelming
	}

	tempDir := "/tmp/sidechain_audio"
	if err := os.MkdirAll(tempDir, 0755); err != nil {
		logger.Log.Warn("Failed to create temp directory", zap.String("temp_dir", tempDir), zap.Error(err))
	}

	return &AudioQueue{
		jobs:         make(chan *AudioJob, 100), // Buffer 100 jobs
		results:      make(map[string]*AudioJob),
		workers:      workers,
		ctx:          ctx,
		cancel:       cancel,
		s3Uploader:   s3Uploader,
		tempDir:      tempDir,
		jobCompleted: make(chan string, 100), // For testing
	}
}

// SetPostCompleteCallback sets a callback function to be called when a post processing completes
func (q *AudioQueue) SetPostCompleteCallback(callback func(postID string)) {
	q.callbackMux.Lock()
	defer q.callbackMux.Unlock()
	q.onPostComplete = callback
}

// SetAudioAnalyzer sets the audio analysis client for key/BPM detection
func (q *AudioQueue) SetAudioAnalyzer(analyzer *analysis.Client) {
	q.audioAnalyzer = analyzer
}

// Start begins processing jobs with worker pool
func (q *AudioQueue) Start() {
	logger.Log.Info("Starting audio queue", zap.Int("workers", q.workers))

	for i := 0; i < q.workers; i++ {
		go q.worker(i)
	}
}

// Stop gracefully shuts down the queue
func (q *AudioQueue) Stop() {
	q.cancel()
	close(q.jobs)
}

// SubmitJob submits a new audio processing job
func (q *AudioQueue) SubmitJob(userID, postID, tempFilePath, filename string, metadata map[string]interface{}) (*AudioJob, error) {
	job := &AudioJob{
		ID:           uuid.New().String(),
		UserID:       userID,
		PostID:       postID,
		TempFilePath: tempFilePath,
		Filename:     filename,
		Metadata:     metadata,
		Status:       "pending",
		CreatedAt:    time.Now(),
	}

	// Store job in results map
	q.resultsMux.Lock()
	q.results[job.ID] = job
	q.resultsMux.Unlock()

	// Submit to worker pool
	select {
	case q.jobs <- job:
		return job, nil
	default:
		return nil, fmt.Errorf("audio queue is full")
	}
}

// GetJobStatus returns the current status of a job
func (q *AudioQueue) GetJobStatus(jobID string) (*AudioJob, error) {
	q.resultsMux.RLock()
	defer q.resultsMux.RUnlock()

	job, exists := q.results[jobID]
	if !exists {
		return nil, fmt.Errorf("job not found")
	}

	return job, nil
}

// WaitForJobCompletion waits for a specific job to complete (for testing)
func (q *AudioQueue) WaitForJobCompletion(jobID string, timeout time.Duration) error {
	timer := time.NewTimer(timeout)
	defer timer.Stop()

	for {
		select {
		case completedJobID := <-q.jobCompleted:
			if completedJobID == jobID {
				return nil
			}
		case <-timer.C:
			return fmt.Errorf("timeout waiting for job %s", jobID)
		case <-q.ctx.Done():
			return fmt.Errorf("queue stopped")
		}
	}
}

// worker processes audio jobs from the queue
func (q *AudioQueue) worker(workerID int) {
	logger.Log.Info("Audio worker started", zap.Int("worker_id", workerID))

	for {
		select {
		case job := <-q.jobs:
			if job == nil {
				logger.Log.Info("Audio worker shutting down", zap.Int("worker_id", workerID))
				return
			}

			q.processJob(workerID, job)

		case <-q.ctx.Done():
			logger.Log.Info("Audio worker shutting down", zap.Int("worker_id", workerID))
			return
		}
	}
}

// processJob handles the actual audio processing with FFmpeg
func (q *AudioQueue) processJob(workerID int, job *AudioJob) {
	logger.Log.Info("Worker processing job", zap.Int("worker_id", workerID), zap.String("job_id", job.ID), zap.String("filename", job.Filename))
	startTime := time.Now()

	// Update job status to processing
	q.updateJobStatus(job.ID, "processing", nil, nil)
	q.updateAudioPostStatus(job.PostID, "processing")

	// Clean up temp file when done
	defer os.Remove(job.TempFilePath)

	// Create processing context with timeout (5 minutes max)
	ctx, cancel := context.WithTimeout(q.ctx, 5*time.Minute)
	defer cancel()

	// 1. Process audio with FFmpeg (normalize + encode to MP3)
	processedAudio, err := q.processAudioWithFFmpeg(ctx, job)
	if err != nil {
		errMsg := fmt.Sprintf("FFmpeg processing failed: %v", err)
		logger.Log.Error("Worker job failed", zap.Int("worker_id", workerID), zap.String("job_id", job.ID), zap.String("error", errMsg))
		q.updateJobStatus(job.ID, "failed", nil, &errMsg)
		q.updateAudioPostStatus(job.PostID, "failed")
		q.signalCompletion(job.ID)
		return
	}

	// 2. Upload processed MP3 to S3
	audioResult, err := q.s3Uploader.UploadAudio(ctx, processedAudio.Data, job.UserID, job.Filename)
	if err != nil {
		errMsg := fmt.Sprintf("S3 audio upload failed: %v", err)
		logger.Log.Error("Worker job failed", zap.Int("worker_id", workerID), zap.String("job_id", job.ID), zap.String("error", errMsg))
		q.updateJobStatus(job.ID, "failed", nil, &errMsg)
		q.updateAudioPostStatus(job.PostID, "failed")
		q.signalCompletion(job.ID)
		return
	}

	// 3. Upload waveform PNG to S3
	var waveformURL string
	if processedAudio.WaveformPNG != "" {
		waveformResult, err := q.s3Uploader.UploadWaveform(ctx, []byte(processedAudio.WaveformPNG), audioResult.Key)
		if err != nil {
			// Non-fatal - log and continue
			logger.Log.Warn("Waveform upload failed", zap.Int("worker_id", workerID), zap.String("job_id", job.ID), zap.Error(err))
		} else {
			waveformURL = waveformResult.URL
		}
	}

	// 4. Update database record with results
	result := &AudioJobResult{
		AudioURL:              audioResult.URL,
		WaveformURL:           waveformURL,
		Duration:              processedAudio.Duration,
		ProcessedSize:         audioResult.Size,
		DetectedBPM:           processedAudio.DetectedBPM,
		DetectedBPMConfidence: processedAudio.DetectedBPMConfidence,
		DetectedKey:           processedAudio.DetectedKey,
		DetectedKeyConfidence: processedAudio.DetectedKeyConfidence,
		DetectedKeyCamelot:    processedAudio.DetectedKeyCamelot,
		DetectedTags:          processedAudio.DetectedTags,
		DetectedGenres:        processedAudio.DetectedGenres,
		DetectedMoods:         processedAudio.DetectedMoods,
		DetectedInstruments:   processedAudio.DetectedInstruments,
		HasVocals:             processedAudio.HasVocals,
		IsDanceable:           processedAudio.IsDanceable,
		Arousal:               processedAudio.Arousal,
		Valence:               processedAudio.Valence,
	}

	err = q.updateAudioPostComplete(job.PostID, result)
	if err != nil {
		errMsg := fmt.Sprintf("Database update failed: %v", err)
		logger.Log.Error("Worker job failed", zap.Int("worker_id", workerID), zap.String("job_id", job.ID), zap.String("error", errMsg))
		q.updateJobStatus(job.ID, "failed", nil, &errMsg)
		q.updateAudioPostStatus(job.PostID, "failed")
		q.signalCompletion(job.ID)
		return
	}

	// 5. Generate audio fingerprint for Sound detection
	// Note: Fingerprinting is done in processAudioWithFFmpeg while the processed file is still available

	// Success!
	q.updateJobStatus(job.ID, "complete", result, nil)

	elapsed := time.Since(startTime)
	logger.Log.Info("Worker completed job", zap.Int("worker_id", workerID), zap.String("job_id", job.ID), zap.Duration("elapsed", elapsed), zap.Float64("duration", processedAudio.Duration), zap.Int64("size", audioResult.Size))

	// Trigger post indexing callback
	q.callbackMux.RLock()
	callback := q.onPostComplete
	q.callbackMux.RUnlock()
	if callback != nil {
		go callback(job.PostID)
	}

	q.signalCompletion(job.ID)
}

// ProcessedAudio contains the result of FFmpeg processing
type ProcessedAudio struct {
	Data        []byte
	WaveformPNG string
	Duration    float64
	FileSize    int64

	// Audio analysis results (from Essentia service)
	DetectedBPM           *float64
	DetectedBPMConfidence *float64
	DetectedKey           *string
	DetectedKeyConfidence *float64
	DetectedKeyCamelot    *string

	// Audio tagging results (from Essentia TensorFlow models)
	DetectedTags        []string
	DetectedGenres      []string
	DetectedMoods       []string
	DetectedInstruments []string
	HasVocals           *bool
	IsDanceable         *bool
	Arousal             *float64
	Valence             *float64
}

// processAudioWithFFmpeg runs the actual FFmpeg processing pipeline
func (q *AudioQueue) processAudioWithFFmpeg(ctx context.Context, job *AudioJob) (*ProcessedAudio, error) {
	// Use the FFmpeg processor from the audio package
	// For now, implement inline to avoid circular imports

	inputPath := job.TempFilePath
	outputPath := fmt.Sprintf("%s/%s_processed.mp3", q.tempDir, job.ID)
	defer os.Remove(outputPath) // Clean up processed file after reading

	// Step 1: Normalize and encode to MP3
	err := runFFmpegNormalize(ctx, inputPath, outputPath)
	if err != nil {
		return nil, fmt.Errorf("normalization failed: %w", err)
	}

	// Step 2: Generate waveform
	waveformPNG, err := generateWaveformPNG(ctx, outputPath)
	if err != nil {
		// Non-fatal, continue without waveform
		logger.Log.Warn("Waveform generation failed", zap.Error(err))
		waveformPNG = ""
	}

	// Step 3: Extract duration
	duration, err := extractAudioDuration(ctx, outputPath)
	if err != nil {
		// Use default if extraction fails
		logger.Log.Warn("Duration extraction failed", zap.Error(err))
		duration = 0
	}

	// Step 4: Generate audio fingerprint for Sound detection
	// This must happen while the processed file is still available
	fingerprintCtx, fingerprintCancel := context.WithTimeout(ctx, 30*time.Second)
	err = fingerprint.ProcessPostFingerprint(fingerprintCtx, outputPath, job.PostID, job.UserID)
	fingerprintCancel()
	if err != nil {
		// Non-fatal - fingerprinting failure shouldn't block upload
		logger.Log.Warn("Audio fingerprinting failed", zap.Error(err))
	}

	// Step 4.5: Analyze audio for key, BPM, and tags using Essentia service
	var detectedBPM, detectedBPMConfidence *float64
	var detectedKey, detectedKeyCamelot *string
	var detectedKeyConfidence *float64
	var detectedTags, detectedGenres, detectedMoods, detectedInstruments []string
	var hasVocals, isDanceable *bool
	var arousal, valence *float64

	if q.audioAnalyzer != nil {
		analysisCtx, analysisCancel := context.WithTimeout(ctx, 90*time.Second) // Increased timeout for tagging
		// Use full analysis options to include tagging
		analysisResult, analysisErr := q.audioAnalyzer.AnalyzeFileWithFullOptions(analysisCtx, outputPath, analysis.FullAnalysisOptions())
		analysisCancel()

		if analysisErr != nil {
			// Non-fatal - analysis failure shouldn't block upload
			logger.Log.Warn("Audio analysis failed",
				zap.String("job_id", job.ID),
				zap.Error(analysisErr))
		} else {
			// Store BPM results
			if analysisResult.BPM != nil {
				detectedBPM = &analysisResult.BPM.Value
				detectedBPMConfidence = &analysisResult.BPM.Confidence
				logger.Log.Debug("BPM detected",
					zap.String("job_id", job.ID),
					zap.Float64("bpm", analysisResult.BPM.Value),
					zap.Float64("confidence", analysisResult.BPM.Confidence))
			}

			// Store key results
			if analysisResult.Key != nil {
				detectedKey = &analysisResult.Key.Value
				detectedKeyConfidence = &analysisResult.Key.Confidence
				detectedKeyCamelot = &analysisResult.Key.Camelot
				logger.Log.Debug("Key detected",
					zap.String("job_id", job.ID),
					zap.String("key", analysisResult.Key.Value),
					zap.String("camelot", analysisResult.Key.Camelot),
					zap.Float64("confidence", analysisResult.Key.Confidence))
			}

			// Store tag results
			if analysisResult.Tags != nil {
				// Extract tag names from TagItems
				if len(analysisResult.Tags.TopTags) > 0 {
					detectedTags = make([]string, len(analysisResult.Tags.TopTags))
					for i, tag := range analysisResult.Tags.TopTags {
						detectedTags[i] = tag.Name
					}
				}
				detectedGenres = analysisResult.Tags.Genres
				detectedMoods = analysisResult.Tags.Moods
				detectedInstruments = analysisResult.Tags.Instruments
				hasVocals = analysisResult.Tags.HasVocals
				isDanceable = analysisResult.Tags.IsDanceable
				arousal = analysisResult.Tags.Arousal
				valence = analysisResult.Tags.Valence

				logger.Log.Debug("Tags detected",
					zap.String("job_id", job.ID),
					zap.Int("genres", len(detectedGenres)),
					zap.Int("moods", len(detectedMoods)),
					zap.Int("instruments", len(detectedInstruments)))
			}
		}
	}

	// Step 5: Read processed file
	data, err := os.ReadFile(outputPath)
	if err != nil {
		return nil, fmt.Errorf("failed to read processed file: %w", err)
	}

	return &ProcessedAudio{
		Data:                  data,
		WaveformPNG:           waveformPNG,
		Duration:              duration,
		FileSize:              int64(len(data)),
		DetectedBPM:           detectedBPM,
		DetectedBPMConfidence: detectedBPMConfidence,
		DetectedKey:           detectedKey,
		DetectedKeyConfidence: detectedKeyConfidence,
		DetectedKeyCamelot:    detectedKeyCamelot,
		DetectedTags:          detectedTags,
		DetectedGenres:        detectedGenres,
		DetectedMoods:         detectedMoods,
		DetectedInstruments:   detectedInstruments,
		HasVocals:             hasVocals,
		IsDanceable:           isDanceable,
		Arousal:               arousal,
		Valence:               valence,
	}, nil
}

// updateJobStatus updates job status thread-safely
func (q *AudioQueue) updateJobStatus(jobID, status string, result *AudioJobResult, errorMessage *string) {
	q.resultsMux.Lock()
	defer q.resultsMux.Unlock()

	job, exists := q.results[jobID]
	if !exists {
		return
	}

	job.Status = status
	job.Result = result
	job.ErrorMessage = errorMessage

	if status == "complete" || status == "failed" {
		now := time.Now()
		job.CompletedAt = &now
	}
}

// updateAudioPostStatus updates the AudioPost processing status in the database
func (q *AudioQueue) updateAudioPostStatus(postID, status string) {
	if database.DB == nil {
		return
	}

	database.DB.Model(&models.AudioPost{}).
		Where("id = ?", postID).
		Update("processing_status", status)
}

// updateAudioPostComplete updates the AudioPost with processing results
func (q *AudioQueue) updateAudioPostComplete(postID string, result *AudioJobResult) error {
	if database.DB == nil {
		return nil
	}

	updates := map[string]interface{}{
		"processing_status": "complete",
		"audio_url":         result.AudioURL,
		"waveform_url":      result.WaveformURL,
		"duration":          result.Duration,
		"file_size":         result.ProcessedSize,
	}

	// Add detected audio analysis values if available
	if result.DetectedBPM != nil {
		updates["detected_bpm"] = result.DetectedBPM
		updates["detected_bpm_confidence"] = result.DetectedBPMConfidence
	}
	if result.DetectedKey != nil {
		updates["detected_key"] = result.DetectedKey
		updates["detected_key_confidence"] = result.DetectedKeyConfidence
		updates["detected_key_camelot"] = result.DetectedKeyCamelot
	}

	// Add detected audio tagging values if available
	if len(result.DetectedTags) > 0 {
		updates["detected_tags"] = models.StringArray(result.DetectedTags)
	}
	if len(result.DetectedGenres) > 0 {
		updates["detected_genres"] = models.StringArray(result.DetectedGenres)
	}
	if len(result.DetectedMoods) > 0 {
		updates["detected_moods"] = models.StringArray(result.DetectedMoods)
	}
	if len(result.DetectedInstruments) > 0 {
		updates["detected_instruments"] = models.StringArray(result.DetectedInstruments)
	}
	if result.HasVocals != nil {
		updates["has_vocals"] = result.HasVocals
	}
	if result.IsDanceable != nil {
		updates["is_danceable"] = result.IsDanceable
	}
	if result.Arousal != nil {
		updates["arousal"] = result.Arousal
	}
	if result.Valence != nil {
		updates["valence"] = result.Valence
	}

	return database.DB.Model(&models.AudioPost{}).
		Where("id = ?", postID).
		Updates(updates).Error
}

// signalCompletion signals that a job has completed (for testing)
func (q *AudioQueue) signalCompletion(jobID string) {
	select {
	case q.jobCompleted <- jobID:
	default:
		// Channel full, don't block
	}
}
