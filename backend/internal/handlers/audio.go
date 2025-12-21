package handlers

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/logger"
	"github.com/zfogg/sidechain/backend/internal/models"
	"github.com/zfogg/sidechain/backend/internal/util"
)

// UploadAudio handles audio file uploads with async processing
func (h *Handlers) UploadAudio(c *gin.Context) {
	currentUser, ok := util.GetUserFromContext(c)
	if !ok {
		return
	}

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
	if !util.IsValidAudioFile(file.Filename) {
		util.RespondBadRequest(c, "invalid_file_type", "Only .mp3, .wav, .aiff, .m4a files are supported")
		return
	}

	// Parse and validate display filename
	filename := c.PostForm("filename")
	if err := util.ValidateFilename(filename); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_filename",
			"message": err.Error(),
		})
		return
	}
	// Default to original filename if not provided
	if filename == "" {
		filename = file.Filename
	}

	// Parse and validate MIDI filename (used if MIDI data is provided)
	midiFilename := c.PostForm("midi_filename")
	// Only validate midi_filename if it's provided and non-empty
	if midiFilename != "" {
		if err := util.ValidateFilename(midiFilename); err != nil {
			c.JSON(http.StatusBadRequest, gin.H{
				"error":   "invalid_midi_filename",
				"message": err.Error(),
			})
			return
		}
	}

	// Parse and validate metadata from form
	bpm := util.ParseInt(c.PostForm("bpm"), 120)
	durationBars := util.ParseInt(c.PostForm("duration_bars"), 8)

	// Validate BPM (1-300 range)
	if bpm < 1 || bpm > 300 {
		logger.WarnWithFields(fmt.Sprintf("Invalid BPM value %d, using default 120", bpm), fmt.Errorf("bpm out of range"))
		bpm = 120
	}

	// Validate duration_bars (1-256 range)
	if durationBars < 1 || durationBars > 256 {
		logger.WarnWithFields(fmt.Sprintf("Invalid duration_bars value %d, using default 8", durationBars), fmt.Errorf("duration_bars out of range"))
		durationBars = 8
	}

	metadata := map[string]interface{}{
		"bpm":           bpm,
		"key":           c.DefaultPostForm("key", "C major"),
		"duration_bars": durationBars,
		"daw":           c.DefaultPostForm("daw", "Unknown"),
		"genre":         c.DefaultPostForm("genre", "Electronic"),
		"sample_rate":   util.ParseFloat(c.PostForm("sample_rate"), 44100.0),
	}

	// Save uploaded file temporarily
	tempFilePath, err := util.SaveUploadedFile(file)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "upload_failed",
			"message": "Failed to save uploaded file",
		})
		return
	}

	// Parse MIDI data if provided
	var midiPatternID *string
	midiDataStr := c.PostForm("midi_data")
	if midiDataStr != "" {
		var midiData models.MIDIData
		if err := json.Unmarshal([]byte(midiDataStr), &midiData); err == nil && len(midiData.Events) > 0 {
			// Create standalone MIDI pattern with filename
			pattern := &models.MIDIPattern{
				UserID:        currentUser.ID,
				Name:          "MIDI from upload",
				Filename:      midiFilename, // User-provided MIDI filename
				Events:        midiData.Events,
				Tempo:         midiData.Tempo,
				TimeSignature: midiData.TimeSignature,
				IsPublic:      true,
			}
			// DB-5: Validate MIDI pattern before saving (DB-5)
			if validationErr := pattern.Validate(); validationErr != nil {
				logger.WarnWithFields("Invalid MIDI pattern data, skipping MIDI creation", validationErr)
				// Continue without MIDI pattern - audio upload proceeds
			} else if err := database.DB.Create(pattern).Error; err != nil {
				logger.WarnWithFields("Failed to save MIDI pattern to database", err)
				// Continue without MIDI pattern - audio upload proceeds
			} else {
				midiPatternID = &pattern.ID
			}
		}
	}

	// Parse comment_audience (default to "everyone")
	commentAudience := c.DefaultPostForm("comment_audience", models.CommentAudienceEveryone)
	// Validate comment_audience value
	if commentAudience != models.CommentAudienceEveryone &&
		commentAudience != models.CommentAudienceFollowers &&
		commentAudience != models.CommentAudienceOff {
		commentAudience = models.CommentAudienceEveryone
	}

	// Create pending audio post in database FIRST to get the postID
	// This allows the background job to update this record when complete
	audioPost := &models.AudioPost{
		ID:               uuid.New().String(),
		UserID:           currentUser.ID,
		OriginalFilename: file.Filename,
		Filename:         filename, // User-provided display filename
		FileSize:         file.Size,
		BPM:              bpm, // Use validated BPM value
		Key:              c.DefaultPostForm("key", "C major"),
		DurationBars:     durationBars, // Use validated duration_bars value
		DAW:              c.DefaultPostForm("daw", "Unknown"),
		Genre:            util.ParseGenreArray(c.PostForm("genre")),
		ProcessingStatus: "pending",
		IsPublic:         true,
		MIDIPatternID:    midiPatternID, // Link to MIDI pattern if created
		CommentAudience:  commentAudience,
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
	job, err := h.kernel.AudioProcessor().SubmitProcessingJob(currentUser.ID, audioPost.ID, tempFilePath, file.Filename, metadata)
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
	response := gin.H{
		"message":     "Audio upload received - processing in background",
		"post_id":     audioPost.ID,
		"job_id":      job.ID,
		"status":      "processing",
		"eta_seconds": 10, // Estimate 10 seconds for processing
		"poll_url":    fmt.Sprintf("/api/v1/audio/status/%s", job.ID),
	}
	if midiPatternID != nil {
		response["midi_pattern_id"] = *midiPatternID
		response["has_midi"] = true
	}
	c.JSON(http.StatusAccepted, response)
}

// GetAudioProcessingStatus returns the status of an audio processing job
func (h *Handlers) GetAudioProcessingStatus(c *gin.Context) {
	jobID := c.Param("job_id")

	status, err := h.kernel.AudioProcessor().GetJobStatus(jobID)
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
