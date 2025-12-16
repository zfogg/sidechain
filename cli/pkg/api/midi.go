package api

import (
	"fmt"
	"time"

	"github.com/zfogg/sidechain/cli/pkg/client"
	"github.com/zfogg/sidechain/cli/pkg/logger"
)

// MIDIEvent represents a single MIDI event
type MIDIEvent struct {
	Time     float64 `json:"time"`
	Type     string  `json:"type"` // "note_on" or "note_off"
	Note     int     `json:"note"`
	Velocity int     `json:"velocity"`
	Channel  int     `json:"channel"`
}

// MIDIPattern represents a user's MIDI pattern
type MIDIPattern struct {
	ID              string      `json:"id"`
	UserID          string      `json:"user_id"`
	User            User        `json:"user"`
	Name            string      `json:"name"`
	Filename        string      `json:"filename"`
	Description     string      `json:"description"`
	Events          []MIDIEvent `json:"events"`
	TotalTime       float64     `json:"total_time"`
	Tempo           int         `json:"tempo"`
	TimeSignature   [2]int      `json:"time_signature"`
	Key             string      `json:"key"`
	IsPublic        bool        `json:"is_public"`
	DownloadCount   int         `json:"download_count"`
	CreatedAt       time.Time   `json:"created_at"`
	UpdatedAt       time.Time   `json:"updated_at"`
}

// MIDIPatternListResponse wraps a list of MIDI patterns
type MIDIPatternListResponse struct {
	Patterns   []MIDIPattern `json:"patterns"`
	TotalCount int           `json:"total_count"`
	Limit      int           `json:"limit"`
	Offset     int           `json:"offset"`
}

// MIDIChallengeConstraints represents challenge constraints
type MIDIChallengeConstraints struct {
	BPMMin       *int           `json:"bpm_min"`
	BPMMax       *int           `json:"bpm_max"`
	Key          *string        `json:"key"`
	Scale        *string        `json:"scale"`
	NoteCountMin *int           `json:"note_count_min"`
	NoteCountMax *int           `json:"note_count_max"`
	DurationMin  *float64       `json:"duration_min"`
	DurationMax  *float64       `json:"duration_max"`
}

// MIDIChallenge represents a MIDI creation challenge
type MIDIChallenge struct {
	ID            string                      `json:"id"`
	Title         string                      `json:"title"`
	Description   string                      `json:"description"`
	Constraints   MIDIChallengeConstraints    `json:"constraints"`
	StartDate     time.Time                   `json:"start_date"`
	EndDate       time.Time                   `json:"end_date"`
	VotingEndDate time.Time                   `json:"voting_end_date"`
	Status        string                      `json:"status"` // upcoming|active|voting|ended
	EntryCount    int                         `json:"entry_count"`
	CreatedAt     time.Time                   `json:"created_at"`
	UpdatedAt     time.Time                   `json:"updated_at"`
}

// MIDIChallengeEntry represents an entry in a challenge
type MIDIChallengeEntry struct {
	ID            string    `json:"id"`
	ChallengeID   string    `json:"challenge_id"`
	UserID        string    `json:"user_id"`
	User          User      `json:"user"`
	AudioURL      string    `json:"audio_url"`
	PostID        *string   `json:"post_id"`
	MIDIPatternID *string   `json:"midi_pattern_id"`
	VoteCount     int       `json:"vote_count"`
	SubmittedAt   time.Time `json:"submitted_at"`
	CreatedAt     time.Time `json:"created_at"`
}

// MIDIChallengeListResponse wraps a list of challenges
type MIDIChallengeListResponse struct {
	Challenges []MIDIChallenge `json:"challenges"`
	TotalCount int             `json:"total_count"`
}

// MIDIChallengeDetailResponse wraps a challenge with entries
type MIDIChallengeDetailResponse struct {
	Challenge MIDIChallenge         `json:"challenge"`
	Entries   []MIDIChallengeEntry  `json:"entries"`
}

// CreateMIDIPattern creates a new MIDI pattern
func CreateMIDIPattern(name string, description string, events []MIDIEvent, totalTime float64, tempo int, key string, isPublic bool) (*MIDIPattern, error) {
	logger.Debug("Creating MIDI pattern", "name", name)

	req := struct {
		Name          string      `json:"name"`
		Description   string      `json:"description"`
		Events        []MIDIEvent `json:"events"`
		TotalTime     float64     `json:"total_time"`
		Tempo         int         `json:"tempo"`
		TimeSignature [2]int      `json:"time_signature"`
		Key           string      `json:"key"`
		IsPublic      bool        `json:"is_public"`
	}{
		Name:          name,
		Description:   description,
		Events:        events,
		TotalTime:     totalTime,
		Tempo:         tempo,
		TimeSignature: [2]int{4, 4},
		Key:           key,
		IsPublic:      isPublic,
	}

	var response struct {
		Pattern MIDIPattern `json:"pattern"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post("/api/v1/midi")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to create MIDI pattern: %s", resp.Status())
	}

	return &response.Pattern, nil
}

// ListMIDIPatterns retrieves user's MIDI patterns
func ListMIDIPatterns(limit, offset int) (*MIDIPatternListResponse, error) {
	logger.Debug("Fetching MIDI patterns", "limit", limit)

	var response MIDIPatternListResponse
	resp, err := client.GetClient().
		R().
		SetQueryParam("limit", fmt.Sprintf("%d", limit)).
		SetQueryParam("offset", fmt.Sprintf("%d", offset)).
		SetResult(&response).
		Get("/api/v1/midi")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch MIDI patterns: %s", resp.Status())
	}

	return &response, nil
}

// GetMIDIPattern retrieves a specific MIDI pattern
func GetMIDIPattern(patternID string) (*MIDIPattern, error) {
	logger.Debug("Fetching MIDI pattern", "pattern_id", patternID)

	var response struct {
		Pattern MIDIPattern `json:"pattern"`
	}

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/midi/%s", patternID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch MIDI pattern: %s", resp.Status())
	}

	return &response.Pattern, nil
}

// UpdateMIDIPattern updates MIDI pattern metadata
func UpdateMIDIPattern(patternID string, name string, description string, isPublic bool) (*MIDIPattern, error) {
	logger.Debug("Updating MIDI pattern", "pattern_id", patternID)

	req := struct {
		Name        string `json:"name"`
		Description string `json:"description"`
		IsPublic    bool   `json:"is_public"`
	}{
		Name:        name,
		Description: description,
		IsPublic:    isPublic,
	}

	var response struct {
		Pattern MIDIPattern `json:"pattern"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Patch(fmt.Sprintf("/api/v1/midi/%s", patternID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to update MIDI pattern: %s", resp.Status())
	}

	return &response.Pattern, nil
}

// DeleteMIDIPattern deletes a MIDI pattern
func DeleteMIDIPattern(patternID string) error {
	logger.Debug("Deleting MIDI pattern", "pattern_id", patternID)

	resp, err := client.GetClient().
		R().
		Delete(fmt.Sprintf("/api/v1/midi/%s", patternID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to delete MIDI pattern: %s", resp.Status())
	}

	return nil
}

// GetMIDIChallenges retrieves MIDI challenges with optional status filter
func GetMIDIChallenges(status string) (*MIDIChallengeListResponse, error) {
	logger.Debug("Fetching MIDI challenges", "status", status)

	var response MIDIChallengeListResponse
	req := client.GetClient().R().SetResult(&response)

	if status != "" {
		req = req.SetQueryParam("status", status)
	}

	resp, err := req.Get("/api/v1/midi-challenges")

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch challenges: %s", resp.Status())
	}

	return &response, nil
}

// GetMIDIChallenge retrieves a specific challenge with entries
func GetMIDIChallenge(challengeID string) (*MIDIChallengeDetailResponse, error) {
	logger.Debug("Fetching challenge", "challenge_id", challengeID)

	var response MIDIChallengeDetailResponse

	resp, err := client.GetClient().
		R().
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/midi-challenges/%s", challengeID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch challenge: %s", resp.Status())
	}

	return &response, nil
}

// GetMIDIChallengeEntries retrieves all entries for a challenge
func GetMIDIChallengeEntries(challengeID string, limit, offset int) ([]MIDIChallengeEntry, error) {
	logger.Debug("Fetching challenge entries", "challenge_id", challengeID)

	var response struct {
		Entries []MIDIChallengeEntry `json:"entries"`
		Total   int                  `json:"total_count"`
	}

	resp, err := client.GetClient().
		R().
		SetQueryParam("limit", fmt.Sprintf("%d", limit)).
		SetQueryParam("offset", fmt.Sprintf("%d", offset)).
		SetResult(&response).
		Get(fmt.Sprintf("/api/v1/midi-challenges/%s/entries", challengeID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to fetch challenge entries: %s", resp.Status())
	}

	return response.Entries, nil
}

// SubmitChallengeEntry submits an entry to a challenge
func SubmitChallengeEntry(challengeID string, audioURL string, postID *string, midiPatternID *string) (*MIDIChallengeEntry, error) {
	logger.Debug("Submitting challenge entry", "challenge_id", challengeID)

	req := struct {
		AudioURL      string  `json:"audio_url"`
		PostID        *string `json:"post_id"`
		MIDIPatternID *string `json:"midi_pattern_id"`
	}{
		AudioURL:      audioURL,
		PostID:        postID,
		MIDIPatternID: midiPatternID,
	}

	var response struct {
		Entry MIDIChallengeEntry `json:"entry"`
	}

	resp, err := client.GetClient().
		R().
		SetBody(req).
		SetResult(&response).
		Post(fmt.Sprintf("/api/v1/midi-challenges/%s/entries", challengeID))

	if err != nil {
		return nil, err
	}

	if !resp.IsSuccess() {
		return nil, fmt.Errorf("failed to submit challenge entry: %s", resp.Status())
	}

	return &response.Entry, nil
}

// VoteChallengeEntry votes for a challenge entry
func VoteChallengeEntry(challengeID string, entryID string) error {
	logger.Debug("Voting for challenge entry", "entry_id", entryID)

	resp, err := client.GetClient().
		R().
		Post(fmt.Sprintf("/api/v1/midi-challenges/%s/entries/%s/vote", challengeID, entryID))

	if err != nil {
		return err
	}

	if !resp.IsSuccess() {
		return fmt.Errorf("failed to vote for entry: %s", resp.Status())
	}

	return nil
}
