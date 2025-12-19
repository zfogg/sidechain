package handlers

import (
	"fmt"
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/zfogg/sidechain/backend/internal/challenges"
	"github.com/zfogg/sidechain/backend/internal/database"
	"github.com/zfogg/sidechain/backend/internal/models"
	"gorm.io/gorm"
)

// GetMIDIChallenges returns a list of MIDI challenges
// GET /api/v1/midi-challenges?status=active|past|upcoming
func (h *Handlers) GetMIDIChallenges(c *gin.Context) {
	statusFilter := c.DefaultQuery("status", "") // active, past, upcoming, voting

	var challenges []models.MIDIChallenge
	query := database.DB

	now := time.Now()

	switch statusFilter {
	case "active":
		// Challenges where now is between start_date and end_date
		query = query.Where("start_date <= ? AND end_date >= ?", now, now)
	case "voting":
		// Challenges where now is between end_date and voting_end_date
		query = query.Where("end_date < ? AND voting_end_date >= ?", now, now)
	case "past":
		// Challenges where voting has ended
		query = query.Where("voting_end_date < ?", now)
	case "upcoming":
		// Challenges that haven't started yet
		query = query.Where("start_date > ?", now)
	default:
		// All challenges
	}

	// Calculate status for each challenge
	if err := query.Order("created_at DESC").Find(&challenges).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch MIDI challenges",
		})
		return
	}

	// Calculate and set status for each challenge
	for i := range challenges {
		challenges[i].Status = challenges[i].CalculateStatus()
	}

	c.JSON(http.StatusOK, challenges)
}

// GetMIDIChallenge returns a single MIDI challenge with entries
// GET /api/v1/midi-challenges/:id
func (h *Handlers) GetMIDIChallenge(c *gin.Context) {
	challengeID := c.Param("id")

	var challenge models.MIDIChallenge
	if err := database.DB.
		Preload("Entries", func(db *gorm.DB) *gorm.DB {
			return db.Order("vote_count DESC, submitted_at DESC")
		}).
		Preload("Entries.User").
		Preload("Entries.Post").
		Preload("Entries.MIDIPattern").
		First(&challenge, "id = ?", challengeID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "challenge_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch MIDI challenge",
		})
		return
	}

	// Calculate status
	challenge.Status = challenge.CalculateStatus()

	// Get entry count
	var entryCount int64
	database.DB.Model(&models.MIDIChallengeEntry{}).
		Where("challenge_id = ?", challengeID).
		Count(&entryCount)

	response := gin.H{
		"challenge":   challenge,
		"entry_count": entryCount,
	}

	c.JSON(http.StatusOK, response)
}

// CreateMIDIChallengeEntry submits an entry to a MIDI challenge
// POST /api/v1/midi-challenges/:id/entries
func (h *Handlers) CreateMIDIChallengeEntry(c *gin.Context) {
	challengeID := c.Param("id")
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	// Get challenge
	var challenge models.MIDIChallenge
	if err := database.DB.First(&challenge, "id = ?", challengeID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "challenge_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "query_failed"})
		return
	}

	// Check if challenge is active
	now := time.Now()
	if now.Before(challenge.StartDate) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "challenge_not_started",
			"message": "This challenge has not started yet",
		})
		return
	}
	if now.After(challenge.EndDate) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "challenge_ended",
			"message": "The submission deadline for this challenge has passed",
		})
		return
	}

	// Check if user already submitted an entry
	var existingEntry models.MIDIChallengeEntry
	if err := database.DB.Where("challenge_id = ? AND user_id = ?", challengeID, currentUser.ID).
		First(&existingEntry).Error; err == nil {
		c.JSON(http.StatusConflict, gin.H{
			"error":   "already_submitted",
			"message": "You have already submitted an entry to this challenge",
		})
		return
	}

	var req struct {
		PostID        *string          `json:"post_id"`         // Link to existing AudioPost
		AudioURL      string           `json:"audio_url"`       // Or provide audio URL directly
		MIDIPatternID *string          `json:"midi_pattern_id"` // Link to existing MIDIPattern
		MIDIData      *models.MIDIData `json:"midi_data"`       // Or provide MIDI data directly
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	// Validate that we have either post_id or audio_url
	if req.PostID == nil && req.AudioURL == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "missing_audio",
			"message": "Either post_id or audio_url must be provided",
		})
		return
	}

	// If post_id is provided, verify it exists and belongs to user
	if req.PostID != nil {
		var post models.AudioPost
		if err := database.DB.First(&post, "id = ?", *req.PostID).Error; err != nil {
			c.JSON(http.StatusNotFound, gin.H{
				"error":   "post_not_found",
				"message": "The specified post does not exist",
			})
			return
		}
		if post.UserID != currentUser.ID {
			c.JSON(http.StatusForbidden, gin.H{
				"error":   "forbidden",
				"message": "You can only submit your own posts",
			})
			return
		}
	}

	// Validate constraints
	if err := h.validateChallengeConstraints(&challenge, req.PostID, req.MIDIPatternID, req.MIDIData); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "constraint_violation",
			"message": err.Error(),
		})
		return
	}

	// Create entry
	entry := &models.MIDIChallengeEntry{
		ChallengeID: challengeID,
		UserID:      currentUser.ID,
		SubmittedAt: now,
	}

	if req.PostID != nil {
		entry.PostID = req.PostID
		// Get audio URL from post
		var post models.AudioPost
		database.DB.First(&post, "id = ?", *req.PostID)
		entry.AudioURL = post.AudioURL
		// Link MIDI if post has it
		if post.MIDIPatternID != nil {
			entry.MIDIPatternID = post.MIDIPatternID
		}
	} else {
		entry.AudioURL = req.AudioURL
	}

	if req.MIDIPatternID != nil {
		entry.MIDIPatternID = req.MIDIPatternID
	} else if req.MIDIData != nil {
		// Create MIDI pattern from data
		midiPattern := models.FromMIDIData(req.MIDIData, currentUser.ID)
		if err := database.DB.Create(midiPattern).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{
				"error":   "midi_creation_failed",
				"message": "Failed to create MIDI pattern",
			})
			return
		}
		entry.MIDIPatternID = &midiPattern.ID
	}

	if err := database.DB.Create(entry).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "creation_failed",
			"message": "Failed to create challenge entry",
		})
		return
	}

	// Load relationships
	database.DB.Preload("User").Preload("Post").Preload("MIDIPattern").First(entry, entry.ID)

	c.JSON(http.StatusCreated, entry)
}

// GetMIDIChallengeEntries returns all entries for a challenge
// GET /api/v1/midi-challenges/:id/entries?sort=votes|date
func (h *Handlers) GetMIDIChallengeEntries(c *gin.Context) {
	challengeID := c.Param("id")
	sortBy := c.DefaultQuery("sort", "votes") // votes or date

	// Verify challenge exists
	var challenge models.MIDIChallenge
	if err := database.DB.First(&challenge, "id = ?", challengeID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "challenge_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "query_failed"})
		return
	}

	var entries []models.MIDIChallengeEntry
	query := database.DB.Where("challenge_id = ?", challengeID).
		Preload("User").
		Preload("Post").
		Preload("MIDIPattern")

	switch sortBy {
	case "date":
		query = query.Order("submitted_at DESC")
	default:
		query = query.Order("vote_count DESC, submitted_at DESC")
	}

	if err := query.Find(&entries).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "query_failed",
			"message": "Failed to fetch challenge entries",
		})
		return
	}

	c.JSON(http.StatusOK, entries)
}

// VoteMIDIChallengeEntry votes for a challenge entry
// POST /api/v1/midi-challenges/:id/entries/:entry_id/vote
func (h *Handlers) VoteMIDIChallengeEntry(c *gin.Context) {
	challengeID := c.Param("id")
	entryID := c.Param("entry_id")
	user, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	currentUser := user.(*models.User)

	// Get challenge
	var challenge models.MIDIChallenge
	if err := database.DB.First(&challenge, "id = ?", challengeID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "challenge_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "query_failed"})
		return
	}

	// Check if challenge is in voting period
	now := time.Now()
	if now.Before(challenge.EndDate) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "voting_not_started",
			"message": "Voting has not started yet",
		})
		return
	}
	if now.After(challenge.VotingEndDate) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "voting_ended",
			"message": "Voting has ended",
		})
		return
	}

	// Verify entry exists and belongs to challenge
	var entry models.MIDIChallengeEntry
	if err := database.DB.First(&entry, "id = ? AND challenge_id = ?", entryID, challengeID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "entry_not_found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "query_failed"})
		return
	}

	// Check if user already voted for this challenge
	var existingVote models.MIDIChallengeVote
	if err := database.DB.Where("challenge_id = ? AND voter_id = ?", challengeID, currentUser.ID).
		First(&existingVote).Error; err == nil {
		c.JSON(http.StatusConflict, gin.H{
			"error":   "already_voted",
			"message": "You have already voted in this challenge",
		})
		return
	}

	// Create vote
	vote := &models.MIDIChallengeVote{
		ChallengeID: challengeID,
		EntryID:     entryID,
		VoterID:     currentUser.ID,
		VotedAt:     now,
	}

	if err := database.DB.Create(vote).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "vote_failed",
			"message": "Failed to record vote",
		})
		return
	}

	// Update entry vote count
	database.DB.Model(&entry).Update("vote_count", gorm.Expr("vote_count + 1"))

	c.JSON(http.StatusOK, gin.H{
		"message": "Vote recorded successfully",
		"vote":    vote,
	})
}

// CreateMIDIChallenge creates a new MIDI challenge (admin only)
// POST /api/v1/midi-challenges
func (h *Handlers) CreateMIDIChallenge(c *gin.Context) {
	userInterface, exists := c.Get("user")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}

	// Verify user is admin - only admins can create challenges
	currentUser, ok := userInterface.(*models.User)
	if !ok || !currentUser.IsAdmin {
		c.JSON(http.StatusForbidden, gin.H{"error": "admin_only", "message": "only administrators can create MIDI challenges"})
		return
	}

	var req struct {
		Title         string                          `json:"title" binding:"required,min=1,max=200"`
		Description   string                          `json:"description" binding:"max=2000"`
		Constraints   models.MIDIChallengeConstraints `json:"constraints"`
		StartDate     time.Time                       `json:"start_date" binding:"required"`
		EndDate       time.Time                       `json:"end_date" binding:"required"`
		VotingEndDate time.Time                       `json:"voting_end_date" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	// Validate dates
	now := time.Now()
	if req.StartDate.Before(now) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_start_date",
			"message": "Start date must be in the future",
		})
		return
	}
	if req.EndDate.Before(req.StartDate) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_end_date",
			"message": "End date must be after start date",
		})
		return
	}
	if req.VotingEndDate.Before(req.EndDate) {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_voting_end_date",
			"message": "Voting end date must be after submission end date",
		})
		return
	}

	challenge := &models.MIDIChallenge{
		Title:         req.Title,
		Description:   req.Description,
		Constraints:   req.Constraints,
		StartDate:     req.StartDate,
		EndDate:       req.EndDate,
		VotingEndDate: req.VotingEndDate,
	}

	if err := database.DB.Create(challenge).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "creation_failed",
			"message": "Failed to create MIDI challenge",
		})
		return
	}

	// Calculate status
	challenge.Status = challenge.CalculateStatus()

	// Notify all users about new challenge
	// Run in goroutine to avoid blocking the response
	go func() {
		if err := challenges.NotifyAllUsersAboutNewChallenge(h.stream, challenge.ID, challenge.Title); err != nil {
			// Log error but don't fail the request
			log.Printf("⚠️ Failed to notify users about new challenge %s: %v", challenge.ID, err)
		}
	}()

	c.JSON(http.StatusCreated, challenge)
}

// validateChallengeConstraints validates that the MIDI entry meets the challenge constraints
func (h *Handlers) validateChallengeConstraints(
	challenge *models.MIDIChallenge,
	postID *string,
	midiPatternID *string,
	midiData *models.MIDIData,
) error {
	constraints := challenge.Constraints

	// We need MIDI data to validate constraints
	var midiPattern *models.MIDIPattern
	var midiDataToValidate *models.MIDIData

	// Load MIDI pattern if ID is provided
	if midiPatternID != nil {
		var pattern models.MIDIPattern
		if err := database.DB.First(&pattern, "id = ?", *midiPatternID).Error; err != nil {
			return err
		}
		midiPattern = &pattern
		midiDataToValidate = pattern.ToMIDIData()
	} else if midiData != nil {
		midiDataToValidate = midiData
	} else if postID != nil {
		// Try to get MIDI from post
		var post models.AudioPost
		if err := database.DB.First(&post, "id = ?", *postID).Error; err != nil {
			return err
		}
		if post.MIDIPatternID != nil {
			var pattern models.MIDIPattern
			if err := database.DB.First(&pattern, "id = ?", *post.MIDIPatternID).Error; err != nil {
				return err
			}
			midiPattern = &pattern
			midiDataToValidate = pattern.ToMIDIData()
		}
	}

	// If we still don't have MIDI data, we can't validate most constraints
	// But we can still validate duration if we have audio metadata
	if midiDataToValidate == nil {
		// For now, skip validation if no MIDI data is available
		// In the future, we could extract metadata from audio file
		return nil
	}

	// Validate BPM constraints
	if constraints.BPMMin != nil && midiDataToValidate.Tempo < *constraints.BPMMin {
		return fmt.Errorf("BPM %d is below minimum %d", midiDataToValidate.Tempo, *constraints.BPMMin)
	}
	if constraints.BPMMax != nil && midiDataToValidate.Tempo > *constraints.BPMMax {
		return fmt.Errorf("BPM %d exceeds maximum %d", midiDataToValidate.Tempo, *constraints.BPMMax)
	}

	// Validate duration constraints
	if constraints.DurationMin != nil && midiDataToValidate.TotalTime < *constraints.DurationMin {
		return fmt.Errorf("duration %.2fs is below minimum %.2fs", midiDataToValidate.TotalTime, *constraints.DurationMin)
	}
	if constraints.DurationMax != nil && midiDataToValidate.TotalTime > *constraints.DurationMax {
		return fmt.Errorf("duration %.2fs exceeds maximum %.2fs", midiDataToValidate.TotalTime, *constraints.DurationMax)
	}

	// Count notes (only note_on events)
	noteCount := 0
	for _, event := range midiDataToValidate.Events {
		if event.Type == "note_on" {
			noteCount++
		}
	}

	// Validate note count constraints
	if constraints.NoteCountMin != nil && noteCount < *constraints.NoteCountMin {
		return fmt.Errorf("note count %d is below minimum %d", noteCount, *constraints.NoteCountMin)
	}
	if constraints.NoteCountMax != nil && noteCount > *constraints.NoteCountMax {
		return fmt.Errorf("note count %d exceeds maximum %d", noteCount, *constraints.NoteCountMax)
	}

	// Validate key constraint (if MIDI pattern has key stored)
	if constraints.Key != nil {
		if midiPattern == nil || midiPattern.Key == "" {
			// Try to detect key from MIDI events (basic implementation)
			detectedKey := detectKeyFromMIDI(midiDataToValidate.Events)
			if detectedKey == "" {
				return fmt.Errorf("key constraint requires MIDI pattern with key information")
			}
			if !matchesKey(detectedKey, *constraints.Key) {
				return fmt.Errorf("key %s does not match required key %s", detectedKey, *constraints.Key)
			}
		} else {
			if !matchesKey(midiPattern.Key, *constraints.Key) {
				return fmt.Errorf("key %s does not match required key %s", midiPattern.Key, *constraints.Key)
			}
		}
	}

	// Validate scale constraint
	if constraints.Scale != nil {
		// Extract unique notes from MIDI events
		notes := extractNotesFromMIDI(midiDataToValidate.Events)
		if !matchesScale(notes, *constraints.Scale, constraints.Key) {
			return fmt.Errorf("MIDI notes do not match required scale %s", *constraints.Scale)
		}
	}

	return nil
}

// detectKeyFromMIDI attempts to detect the key from MIDI events (basic implementation)
// Returns empty string if detection fails
func detectKeyFromMIDI(events []models.MIDIEvent) string {
	// Extract unique note classes (C, C#, D, etc.) from note_on events
	noteClasses := make(map[int]int) // map[note_class]count
	for _, event := range events {
		if event.Type == "note_on" {
			noteClass := event.Note % 12 // Get note class (0=C, 1=C#, etc.)
			noteClasses[noteClass]++
		}
	}

	// Simple heuristic: find the most common note class as potential root
	// This is a very basic implementation - in production, use proper key detection
	if len(noteClasses) == 0 {
		return ""
	}

	maxCount := 0
	rootNoteClass := 0
	for noteClass, count := range noteClasses {
		if count > maxCount {
			maxCount = count
			rootNoteClass = noteClass
		}
	}

	// Map note class to key name (simplified - assumes major)
	keyNames := []string{"C", "C# ", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}
	return keyNames[rootNoteClass]
}

// matchesKey checks if a detected/stored key matches the required key
// Handles variations like "C" vs "C major", "Am" vs "A minor", etc.
func matchesKey(detectedKey, requiredKey string) bool {
	// Normalize keys (case-insensitive, remove spaces)
	detected := strings.ToLower(strings.TrimSpace(detectedKey))
	required := strings.ToLower(strings.TrimSpace(requiredKey))

	// Direct match
	if detected == required {
		return true
	}

	// Check if detected key starts with required key (e.g., "C major" matches "C")
	if strings.HasPrefix(detected, required) || strings.HasPrefix(required, detected) {
		return true
	}

	// Handle minor keys (e.g., "Am" vs "A minor")
	detectedBase := strings.TrimSuffix(strings.TrimSuffix(detected, "m"), " minor")
	requiredBase := strings.TrimSuffix(strings.TrimSuffix(required, "m"), " minor")
	if detectedBase == requiredBase {
		return true
	}

	return false
}

// extractNotesFromMIDI extracts unique MIDI note numbers from events
func extractNotesFromMIDI(events []models.MIDIEvent) []int {
	noteSet := make(map[int]bool)
	for _, event := range events {
		if event.Type == "note_on" {
			noteSet[event.Note] = true
		}
	}

	notes := make([]int, 0, len(noteSet))
	for note := range noteSet {
		notes = append(notes, note)
	}
	return notes
}

// matchesScale checks if MIDI notes match the required scale
// This is a simplified implementation - proper scale detection is more complex
func matchesScale(notes []int, requiredScale string, key *string) bool {
	if len(notes) == 0 {
		return false
	}

	// Define scale patterns (semitones from root)
	scalePatterns := map[string][]int{
		"major":            {0, 2, 4, 5, 7, 9, 11},
		"minor":            {0, 2, 3, 5, 7, 8, 10},
		"pentatonic":       {0, 2, 4, 7, 9},
		"pentatonic_major": {0, 2, 4, 7, 9},
		"pentatonic_minor": {0, 3, 5, 7, 10},
		"blues":            {0, 3, 5, 6, 7, 10},
		"dorian":           {0, 2, 3, 5, 7, 9, 10},
		"mixolydian":       {0, 2, 4, 5, 7, 9, 10},
		"lydian":           {0, 2, 4, 6, 7, 9, 11},
		"phrygian":         {0, 1, 3, 5, 7, 8, 10},
		"locrian":          {0, 1, 3, 4, 6, 8, 10},
	}

	pattern, exists := scalePatterns[strings.ToLower(requiredScale)]
	if !exists {
		// Unknown scale - allow it for now
		return true
	}

	// Determine root note from key or use most common note
	rootNote := 0
	if key != nil {
		// Parse key to get root note (simplified)
		rootNote = parseKeyToRootNote(*key)
	} else {
		// Use most common note class as root
		noteClasses := make(map[int]int)
		for _, note := range notes {
			noteClass := note % 12
			noteClasses[noteClass]++
		}
		maxCount := 0
		for noteClass, count := range noteClasses {
			if count > maxCount {
				maxCount = count
				rootNote = noteClass
			}
		}
	}

	// Check if all notes are in the scale
	allowedNotes := make(map[int]bool)
	for _, semitone := range pattern {
		allowedNotes[(rootNote+semitone)%12] = true
	}

	for _, note := range notes {
		noteClass := note % 12
		if !allowedNotes[noteClass] {
			return false
		}
	}

	return true
}

// parseKeyToRootNote converts a key string to a root note class (0-11)
func parseKeyToRootNote(key string) int {
	key = strings.ToUpper(strings.TrimSpace(key))

	// Remove common suffixes
	key = strings.TrimSuffix(key, "M")
	key = strings.TrimSuffix(key, "MAJOR")
	key = strings.TrimSuffix(key, "MINOR")
	key = strings.TrimSuffix(key, "M")

	keyMap := map[string]int{
		"C": 0, "C# ": 1, "DB": 1, "D": 2, "D#": 3, "EB": 3,
		"E": 4, "F": 5, "F# ": 6, "GB": 6, "G": 7, "G#": 8,
		"AB": 8, "A": 9, "A# ": 10, "BB": 10, "B": 11,
	}

	if note, exists := keyMap[key]; exists {
		return note
	}

	return 0 // Default to C
}

// =============================================================================
// MIDI Challenge Notification Helpers
// =============================================================================

// notifyChallengeParticipants sends notifications to all users who submitted entries to a challenge
func (h *Handlers) notifyChallengeParticipants(challengeID string, notifyFunc func(userID string) error) error {
	var entries []models.MIDIChallengeEntry
	if err := database.DB.Where("challenge_id = ?", challengeID).
		Select("DISTINCT user_id").
		Find(&entries).Error; err != nil {
		return fmt.Errorf("failed to get challenge participants: %w", err)
	}

	var errors []string
	for _, entry := range entries {
		if err := notifyFunc(entry.UserID); err != nil {
			errors = append(errors, fmt.Sprintf("user %s: %v", entry.UserID, err))
		}
	}

	if len(errors) > 0 {
		return fmt.Errorf("some notifications failed: %s", strings.Join(errors, "; "))
	}
	return nil
}

// NotifyChallengeVotingOpenToParticipants notifies all participants when voting opens
// This should be called from a scheduled job that checks challenges transitioning to voting status
func (h *Handlers) NotifyChallengeVotingOpenToParticipants(challengeID string) error {
	var challenge models.MIDIChallenge
	if err := database.DB.First(&challenge, "id = ?", challengeID).Error; err != nil {
		return fmt.Errorf("challenge not found: %w", err)
	}

	return h.notifyChallengeParticipants(challengeID, func(userID string) error {
		var user models.User
		if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
			return err
		}
		if user.StreamUserID == "" {
			return nil // Skip users without stream IDs
		}
		return h.stream.NotifyChallengeVotingOpen(user.StreamUserID, challengeID, challenge.Title)
	})
}

// NotifyChallengeEndedToParticipants notifies all participants when challenge ends
// This should be called from a scheduled job that checks challenges transitioning to ended status
func (h *Handlers) NotifyChallengeEndedToParticipants(challengeID string) error {
	var challenge models.MIDIChallenge
	if err := database.DB.First(&challenge, "id = ?", challengeID).Error; err != nil {
		return fmt.Errorf("challenge not found: %w", err)
	}

	// Get winner (entry with most votes)
	var winnerEntry models.MIDIChallengeEntry
	if err := database.DB.Where("challenge_id = ?", challengeID).
		Order("vote_count DESC, submitted_at ASC").
		Preload("User").
		First(&winnerEntry).Error; err != nil {
		// No entries or no winner - still notify participants
		winnerEntry.UserID = ""
	}

	winnerUserID := ""
	winnerUsername := ""
	if winnerEntry.UserID != "" {
		winnerUserID = winnerEntry.UserID
		winnerUsername = winnerEntry.User.Username
	}

	return h.notifyChallengeParticipants(challengeID, func(userID string) error {
		var user models.User
		if err := database.DB.First(&user, "id = ?", userID).Error; err != nil {
			return err
		}
		if user.StreamUserID == "" {
			return nil // Skip users without stream IDs
		}

		// Get user's entry rank (0 = first place, -1 = no entry)
		var userEntry models.MIDIChallengeEntry
		var rank int = -1
		if err := database.DB.Where("challenge_id = ? AND user_id = ?", challengeID, userID).
			First(&userEntry).Error; err == nil {
			// Count entries with more votes
			var higherCount int64
			database.DB.Model(&models.MIDIChallengeEntry{}).
				Where("challenge_id = ? AND vote_count > ?", challengeID, userEntry.VoteCount).
				Count(&higherCount)
			rank = int(higherCount)
		}

		return h.stream.NotifyChallengeEnded(
			user.StreamUserID,
			challengeID,
			challenge.Title,
			winnerUserID,
			winnerUsername,
			rank,
		)
	})
}

// NOTE: The following notifications require scheduled jobs:
// 1. NotifyChallengeCreated - Notify all users when new challenge is created
// This requires a batch notification or broadcast mechanism
// 2. NotifyChallengeDeadlineApproaching - Notify participants X hours before deadline
// This requires a scheduled job that runs periodically to check approaching deadlines
// 3. NotifyChallengeVotingOpenToParticipants - Automatically notify when voting opens
// This requires a scheduled job that checks challenges transitioning to voting status
// 4. NotifyChallengeEndedToParticipants - Automatically notify when challenge ends
// This requires a scheduled job that checks challenges transitioning to ended status

// See backend/internal/stories/cleanup.go for an example of a scheduled service pattern.
// A similar service should be created for MIDI challenge notifications.
