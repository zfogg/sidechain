package queue

import (
	"fmt"
	"sync"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestAudioQueue tests the background job processing (fast version)
func TestAudioQueue(t *testing.T) {
	// Pass nil for S3Uploader in tests - we're testing queue mechanics, not uploads
	queue := NewAudioQueue(nil)

	// Test job creation and submission (no workers needed)
	metadata := map[string]interface{}{
		"bpm": 128,
		"key": "C major",
	}

	job, err := queue.SubmitJob("test-user", "test-post-id", "/tmp/test.wav", "test.wav", metadata)
	require.NoError(t, err)
	require.NotNil(t, job)

	assert.NotEmpty(t, job.ID)
	assert.Equal(t, "test-user", job.UserID)
	assert.Equal(t, "test-post-id", job.PostID)
	assert.Equal(t, "pending", job.Status)
	assert.Equal(t, "/tmp/test.wav", job.TempFilePath)
	assert.Equal(t, "test.wav", job.Filename)

	// Test status retrieval
	retrievedJob, err := queue.GetJobStatus(job.ID)
	require.NoError(t, err)
	assert.Equal(t, job.ID, retrievedJob.ID)
	assert.Equal(t, "pending", retrievedJob.Status)

	// Test manual status update (simulate worker completion)
	result := &AudioJobResult{
		AudioURL:    "https://test.com/audio.mp3",
		WaveformURL: "https://test.com/waveform.svg",
		Duration:    30.5,
	}
	queue.updateJobStatus(job.ID, "complete", result, nil)

	finalJob, err := queue.GetJobStatus(job.ID)
	require.NoError(t, err)
	assert.Equal(t, "complete", finalJob.Status)
	assert.Equal(t, result, finalJob.Result)
}

// TestQueueConcurrency tests concurrent job submission (fast version)
func TestQueueConcurrency(t *testing.T) {
	queue := NewAudioQueue(nil)

	const numJobs = 10
	var wg sync.WaitGroup
	jobIDs := make([]string, numJobs)
	errors := make([]error, numJobs)

	// Submit multiple jobs concurrently
	for i := 0; i < numJobs; i++ {
		wg.Add(1)
		go func(index int) {
			defer wg.Done()

			metadata := map[string]interface{}{
				"bpm": 120 + index*10,
				"key": "C major",
			}

			job, err := queue.SubmitJob(
				fmt.Sprintf("user-%d", index),
				fmt.Sprintf("post-%d", index),
				fmt.Sprintf("/tmp/test%d.wav", index),
				fmt.Sprintf("test%d.wav", index),
				metadata,
			)

			errors[index] = err
			if err == nil {
				jobIDs[index] = job.ID
			}
		}(i)
	}

	wg.Wait()

	// Verify all jobs were submitted successfully
	for i, err := range errors {
		assert.NoError(t, err, "Job submission %d should succeed", i)
	}

	// Verify job IDs are unique
	idSet := make(map[string]bool)
	for _, jobID := range jobIDs {
		assert.False(t, idSet[jobID], "Job ID should be unique: %s", jobID)
		idSet[jobID] = true
	}
}

// TestQueueOverflow tests queue capacity limits
func TestQueueOverflow(t *testing.T) {
	// Create queue with small buffer for testing
	queue := &AudioQueue{
		jobs:         make(chan *AudioJob, 2), // Only 2 job buffer
		results:      make(map[string]*AudioJob),
		workers:      1,
		jobCompleted: make(chan string, 10),
	}

	// Fill the queue buffer
	metadata := map[string]interface{}{"test": "data"}

	_, err := queue.SubmitJob("user1", "post1", "/tmp/test1.wav", "test1.wav", metadata)
	assert.NoError(t, err)

	_, err = queue.SubmitJob("user2", "post2", "/tmp/test2.wav", "test2.wav", metadata)
	assert.NoError(t, err)

	// Third job should fail (queue full)
	_, err = queue.SubmitJob("user3", "post3", "/tmp/test3.wav", "test3.wav", metadata)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "queue is full")
}

// TestJobStatusTracking tests job status updates
func TestJobStatusTracking(t *testing.T) {
	queue := NewAudioQueue(nil)

	// Submit job
	metadata := map[string]interface{}{"test": "data"}
	job, err := queue.SubmitJob("test-user", "test-post", "/tmp/test.wav", "test.wav", metadata)
	require.NoError(t, err)

	// Initial status should be pending
	status, err := queue.GetJobStatus(job.ID)
	require.NoError(t, err)
	assert.Equal(t, "pending", status.Status)
	assert.Nil(t, status.CompletedAt)

	// Update status manually for testing
	queue.updateJobStatus(job.ID, "processing", nil, nil)

	status, err = queue.GetJobStatus(job.ID)
	require.NoError(t, err)
	assert.Equal(t, "processing", status.Status)

	// Complete the job
	result := &AudioJobResult{
		AudioURL:    "https://test.com/audio.mp3",
		WaveformURL: "https://test.com/waveform.svg",
		Duration:    30.5,
	}

	queue.updateJobStatus(job.ID, "complete", result, nil)

	status, err = queue.GetJobStatus(job.ID)
	require.NoError(t, err)
	assert.Equal(t, "complete", status.Status)
	assert.Equal(t, result, status.Result)
	assert.NotNil(t, status.CompletedAt)
}

// TestInvalidJobID tests error handling for non-existent jobs
func TestInvalidJobID(t *testing.T) {
	queue := NewAudioQueue(nil)

	_, err := queue.GetJobStatus("non-existent-job-id")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "job not found")
}

// BenchmarkJobSubmission benchmarks job submission performance
func BenchmarkJobSubmission(b *testing.B) {
	queue := NewAudioQueue(nil)
	metadata := map[string]interface{}{"bpm": 128}

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		_, err := queue.SubmitJob(
			fmt.Sprintf("user-%d", i),
			fmt.Sprintf("post-%d", i),
			fmt.Sprintf("/tmp/test%d.wav", i),
			fmt.Sprintf("test%d.wav", i),
			metadata,
		)
		if err != nil {
			b.Fatalf("Job submission failed: %v", err)
		}
	}
}
