package queue

import (
	"context"
	"fmt"
	"log"
	"runtime"
	"sync"
	"time"

	"github.com/google/uuid"
)

// AudioJob represents an audio processing job
type AudioJob struct {
	ID           string                 `json:"id"`
	UserID       string                 `json:"user_id"`
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
	AudioURL      string `json:"audio_url"`
	WaveformURL   string `json:"waveform_url"`
	Duration      float64 `json:"duration"`
	ProcessedSize int64  `json:"processed_size"`
}

// AudioQueue manages background audio processing
type AudioQueue struct {
	jobs        chan *AudioJob
	results     map[string]*AudioJob
	resultsMux  sync.RWMutex
	workers     int
	ctx         context.Context
	cancel      context.CancelFunc
	
	// For testing: channels to signal job completion
	jobCompleted chan string
}

// NewAudioQueue creates a new audio processing queue
func NewAudioQueue() *AudioQueue {
	ctx, cancel := context.WithCancel(context.Background())
	
	// Use CPU count for worker pool size
	workers := runtime.NumCPU()
	if workers > 8 {
		workers = 8 // Cap at 8 workers to avoid overwhelming
	}
	
	return &AudioQueue{
		jobs:         make(chan *AudioJob, 100), // Buffer 100 jobs
		results:      make(map[string]*AudioJob),
		workers:      workers,
		ctx:          ctx,
		cancel:       cancel,
		jobCompleted: make(chan string, 100), // For testing
	}
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
func (q *AudioQueue) SubmitJob(userID, tempFilePath, filename string, metadata map[string]interface{}) (*AudioJob, error) {
	job := &AudioJob{
		ID:           uuid.New().String(),
		UserID:       userID,
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

// WaitForAllJobs waits for multiple jobs to complete (for testing)
func (q *AudioQueue) WaitForAllJobs(jobIDs []string, timeout time.Duration) error {
	completed := make(map[string]bool)
	timer := time.NewTimer(timeout)
	defer timer.Stop()

	for len(completed) < len(jobIDs) {
		select {
		case completedJobID := <-q.jobCompleted:
			for _, targetID := range jobIDs {
				if completedJobID == targetID {
					completed[targetID] = true
					break
				}
			}
		case <-timer.C:
			return fmt.Errorf("timeout waiting for jobs, completed %d/%d", len(completed), len(jobIDs))
		case <-q.ctx.Done():
			return fmt.Errorf("queue stopped")
		}
	}

	return nil
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

// processJob handles the actual audio processing
func (q *AudioQueue) processJob(workerID int, job *AudioJob) {
	log.Printf("🔧 Worker %d processing job %s", workerID, job.ID)
	
	// Update job status
	q.updateJobStatus(job.ID, "processing", nil, nil)
	
	// TODO: Implement actual audio processing
	// This would call the AudioProcessor with the job data
	// For now, simulate processing time
	time.Sleep(2 * time.Second)
	
	// Simulate successful result
	result := &AudioJobResult{
		AudioURL:      fmt.Sprintf("https://sidechain-media.s3.amazonaws.com/audio/%s/%s.mp3", job.UserID, job.ID),
		WaveformURL:   fmt.Sprintf("https://sidechain-media.s3.amazonaws.com/audio/%s/%s_waveform.svg", job.UserID, job.ID),
		Duration:      32.5,
		ProcessedSize: 1024 * 1024, // 1MB
	}
	
	// Update job with result
	q.updateJobStatus(job.ID, "complete", result, nil)
	
	// Signal completion for testing
	select {
	case q.jobCompleted <- job.ID:
	default:
		// Channel full, don't block
	}
	
	log.Printf("✅ Worker %d completed job %s", workerID, job.ID)
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