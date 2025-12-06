package handlers

import (
	"fmt"
	"net/http"
	"os"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
)

// UploadAudio handles audio file uploads with async processing
func (h *Handlers) UploadAudio(c *gin.Context) {
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}

	currentUser := user.(*models.User)

	// Get uploaded file
	file, err := c.FormFile("audio")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "no_audio_file",
			"message": "No audio file provided in 'audio' field",
		})
		return
	}

	// Validate file size (max 50MB)
	const maxFileSize = 50 * 1024 * 1024
	if file.Size > maxFileSize {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "file_too_large",
			"message": "Audio file must be under 50MB",
		})
		return
	}

	// Validate file type
	if !isValidAudioFile(file.Filename) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_file_type",
			"message": "Only .mp3, .wav, .aiff, .m4a files are supported",
		})
		return
	}

	// Parse metadata from form
	metadata := map[string]interface{}{
		"bpm":           parseInt(c.PostForm("bpm"), 120),
		"key":           c.DefaultPostForm("key", "C major"),
		"duration_bars": parseInt(c.PostForm("duration_bars"), 8),
		"daw":           c.DefaultPostForm("daw", "Unknown"),
		"genre":         c.DefaultPostForm("genre", "Electronic"),
		"sample_rate":   parseFloat(c.PostForm("sample_rate"), 44100.0),
	}

	// Save uploaded file temporarily
	tempFilePath, err := saveUploadedFile(file)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "upload_failed",
			"message": "Failed to save uploaded file",
		})
		return
	}

	// Create pending audio post in database FIRST to get the postID
	// This allows the background job to update this record when complete
	audioPost := &models.AudioPost{
		ID:               uuid.New().String(),
		UserID:           currentUser.ID,
		OriginalFilename: file.Filename,
		FileSize:         file.Size,
		BPM:              parseInt(c.PostForm("bpm"), 120),
		Key:              c.DefaultPostForm("key", "C major"),
		DurationBars:     parseInt(c.PostForm("duration_bars"), 8),
		DAW:              c.DefaultPostForm("daw", "Unknown"),
		Genre:            parseGenreArray(c.PostForm("genre")),
		ProcessingStatus: "pending",
		IsPublic:         true,
	}

	err = database.DB.Create(audioPost).Error
	if err != nil {
		os.Remove(tempFilePath)
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "database_failed",
			"message": "Failed to save audio post",
		})
		return
	}

	// Submit to background processing queue with postID so it can update the record
	job, err := h.audioProcessor.SubmitProcessingJob(currentUser.ID, audioPost.ID, tempFilePath, file.Filename, metadata)
	if err != nil {
		os.Remove(tempFilePath)
		// Update the post to failed status
		database.DB.Model(audioPost).Update("processing_status", "failed")
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "queue_failed",
			"message": "Failed to queue audio for processing",
		})
		return
	}

	// Return immediate response - processing happens in background
	c.JSON(http.StatusAccepted, gin.H{
		"message":     "Audio upload received - processing in background",
		"post_id":     audioPost.ID,
		"job_id":      job.ID,
		"status":      "processing",
		"eta_seconds": 10, // Estimate 10 seconds for processing
		"poll_url":    fmt.Sprintf("/api/v1/audio/status/%s", job.ID),
	})
}

// GetAudioProcessingStatus returns the status of an audio processing job
func (h *Handlers) GetAudioProcessingStatus(c *gin.Context) {
	jobID := c.Param("job_id")

	status, err := h.audioProcessor.GetJobStatus(jobID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{
			"error":   "job_not_found",
			"message": "Processing job not found",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"job_id": jobID,
		"status": status.Status,
		"result": status.Result,
		"error":  status.ErrorMessage,
	})
}

// GetAudio retrieves audio file metadata
func (h *Handlers) GetAudio(c *gin.Context) {
	audioID := c.Param("id")

	// In production, fetch from database
	c.JSON(http.StatusOK, gin.H{
		"id":       audioID,
		"url":      "https://cdn.sidechain.app/audio/" + audioID + ".mp3",
		"duration": 32.5,
		"waveform": "data:image/svg+xml;base64,PHN2Zz48L3N2Zz4=",
		"metadata": gin.H{
			"bpm":           128,
			"key":           "F minor",
			"duration_bars": 8,
		},
	})
}
