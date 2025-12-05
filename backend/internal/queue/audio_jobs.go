package queue

import (
	"context"
	"fmt"
	"log"
	"os"
	"runtime"
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/storage"
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
	s3Uploader *storage.S3Uploader
	tempDir    string

	// Callback for post indexing (7.1.4)
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
	os.MkdirAll(tempDir, 0755)

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

// SetPostCompleteCallback sets a callback function to be called when a post processing completes (7.1.4)
func (q *AudioQueue) SetPostCompleteCallback(callback func(postID string)) {
	q.onPostComplete = callback
}

// Start begins processing jobs with worker pool
func (q *AudioQueue) Start() {
	log.Printf("🎵 Starting audio queue with %d workers", q.workers)

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
	log.Printf("🔧 Audio worker %d started", workerID)

	for {
		select {
		case job := <-q.jobs:
			if job == nil {
				log.Printf("🔧 Audio worker %d shutting down", workerID)
				return
			}

			q.processJob(workerID, job)

		case <-q.ctx.Done():
			log.Printf("🔧 Audio worker %d shutting down", workerID)
			return
		}
	}
}

// processJob handles the actual audio processing with FFmpeg
func (q *AudioQueue) processJob(workerID int, job *AudioJob) {
	log.Printf("🔧 Worker %d processing job %s (file: %s)", workerID, job.ID, job.Filename)
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
		log.Printf("❌ Worker %d job %s failed: %s", workerID, job.ID, errMsg)
		q.updateJobStatus(job.ID, "failed", nil, &errMsg)
		q.updateAudioPostStatus(job.PostID, "failed")
		q.signalCompletion(job.ID)
		return
	}

	// 2. Upload processed MP3 to S3
	audioResult, err := q.s3Uploader.UploadAudio(ctx, processedAudio.Data, job.UserID, job.Filename)
	if err != nil {
		errMsg := fmt.Sprintf("S3 audio upload failed: %v", err)
		log.Printf("❌ Worker %d job %s failed: %s", workerID, job.ID, errMsg)
		q.updateJobStatus(job.ID, "failed", nil, &errMsg)
		q.updateAudioPostStatus(job.PostID, "failed")
		q.signalCompletion(job.ID)
		return
	}

	// 3. Upload waveform SVG to S3
	var waveformURL string
	if processedAudio.WaveformSVG != "" {
		waveformResult, err := q.s3Uploader.UploadWaveform(ctx, []byte(processedAudio.WaveformSVG), audioResult.Key)
		if err != nil {
			// Non-fatal - log and continue
			log.Printf("⚠️ Worker %d job %s: waveform upload failed: %v", workerID, job.ID, err)
		} else {
			waveformURL = waveformResult.URL
		}
	}

	// 4. Update database record with results
	result := &AudioJobResult{
		AudioURL:      audioResult.URL,
		WaveformURL:   waveformURL,
		Duration:      processedAudio.Duration,
		ProcessedSize: audioResult.Size,
	}

	err = q.updateAudioPostComplete(job.PostID, result)
	if err != nil {
		errMsg := fmt.Sprintf("Database update failed: %v", err)
		log.Printf("❌ Worker %d job %s failed: %s", workerID, job.ID, errMsg)
		q.updateJobStatus(job.ID, "failed", nil, &errMsg)
		q.updateAudioPostStatus(job.PostID, "failed")
		q.signalCompletion(job.ID)
		return
	}

	// Success!
	q.updateJobStatus(job.ID, "complete", result, nil)

	elapsed := time.Since(startTime)
	log.Printf("✅ Worker %d completed job %s in %v (duration: %.1fs, size: %d bytes)",
		workerID, job.ID, elapsed, processedAudio.Duration, audioResult.Size)

	// Trigger post indexing callback (7.1.4)
	if q.onPostComplete != nil {
		go q.onPostComplete(job.PostID)
	}

	q.signalCompletion(job.ID)
}

// ProcessedAudio contains the result of FFmpeg processing
type ProcessedAudio struct {
	Data        []byte
	WaveformSVG string
	Duration    float64
	FileSize    int64
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
	waveformSVG, err := generateWaveformSVG(ctx, outputPath)
	if err != nil {
		// Non-fatal, continue without waveform
		log.Printf("⚠️ Waveform generation failed: %v", err)
		waveformSVG = ""
	}

	// Step 3: Extract duration
	duration, err := extractAudioDuration(ctx, outputPath)
	if err != nil {
		// Use default if extraction fails
		log.Printf("⚠️ Duration extraction failed: %v", err)
		duration = 0
	}

	// Step 4: Read processed file
	data, err := os.ReadFile(outputPath)
	if err != nil {
		return nil, fmt.Errorf("failed to read processed file: %w", err)
	}

	return &ProcessedAudio{
		Data:        data,
		WaveformSVG: waveformSVG,
		Duration:    duration,
		FileSize:    int64(len(data)),
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
		"waveform_svg":      result.WaveformURL,
		"duration":          result.Duration,
		"file_size":         result.ProcessedSize,
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
