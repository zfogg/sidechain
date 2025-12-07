package models

import (
	"encoding/json"
	"time"

	"gorm.io/gorm"
)

// MIDIChallengeStatus represents the status of a MIDI challenge
type MIDIChallengeStatus string

const (
	MIDIChallengeStatusUpcoming MIDIChallengeStatus = "upcoming"
	MIDIChallengeStatusActive   MIDIChallengeStatus = "active"
	MIDIChallengeStatusVoting   MIDIChallengeStatus = "voting"
	MIDIChallengeStatusEnded    MIDIChallengeStatus = "ended"
)

// MIDIChallengeConstraints represents the constraints for a MIDI challenge (R.2.2.1.1)
type MIDIChallengeConstraints struct {
	// BPM constraints
	BPMMin *int `json:"bpm_min,omitempty"` // Minimum BPM (inclusive)
	BPMMax *int `json:"bpm_max,omitempty"` // Maximum BPM (inclusive)

	// Key constraint
	Key *string `json:"key,omitempty"` // Required key (e.g., "C", "Am", "F#m")

	// Scale constraint
	Scale *string `json:"scale,omitempty"` // Required scale (e.g., "major", "minor", "pentatonic")

	// Note count constraints
	NoteCountMin *int `json:"note_count_min,omitempty"` // Minimum number of notes
	NoteCountMax *int `json:"note_count_max,omitempty"` // Maximum number of notes

	// Duration constraints (in seconds)
	DurationMin *float64 `json:"duration_min,omitempty"` // Minimum duration
	DurationMax *float64 `json:"duration_max,omitempty"` // Maximum duration

	// Additional constraints (for future extensibility)
	AdditionalRules map[string]interface{} `json:"additional_rules,omitempty"`
}

// MIDIChallenge represents a MIDI challenge (R.2.2.1.1)
type MIDIChallenge struct {
	ID          string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	Title       string `gorm:"not null" json:"title"`
	Description string `gorm:"type:text" json:"description"`

	// Constraints stored as JSONB
	ConstraintsJSON []byte                   `gorm:"type:jsonb" json:"-"`
	Constraints     MIDIChallengeConstraints `gorm:"-" json:"constraints"`

	// Dates
	StartDate     time.Time `gorm:"not null;index" json:"start_date"`
	EndDate       time.Time `gorm:"not null;index" json:"end_date"`        // Submission deadline
	VotingEndDate time.Time `gorm:"not null;index" json:"voting_end_date"` // Voting deadline

	// Status (calculated field, not stored)
	Status MIDIChallengeStatus `gorm:"-" json:"status"`

	// Relationships
	Entries []MIDIChallengeEntry `gorm:"foreignKey:ChallengeID" json:"entries,omitempty"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName overrides the default table name
func (MIDIChallenge) TableName() string {
	return "midi_challenges"
}

// BeforeCreate marshals Constraints to JSON before saving
func (c *MIDIChallenge) BeforeCreate(tx *gorm.DB) error {
	return c.marshalConstraints()
}

// BeforeUpdate marshals Constraints to JSON before saving
func (c *MIDIChallenge) BeforeUpdate(tx *gorm.DB) error {
	return c.marshalConstraints()
}

// AfterFind unmarshals Constraints from JSON after loading
func (c *MIDIChallenge) AfterFind(tx *gorm.DB) error {
	return c.unmarshalConstraints()
}

// marshalConstraints converts Constraints struct to JSON
func (c *MIDIChallenge) marshalConstraints() error {
	if c.ConstraintsJSON == nil {
		data, err := json.Marshal(c.Constraints)
		if err != nil {
			return err
		}
		c.ConstraintsJSON = data
	}
	return nil
}

// unmarshalConstraints converts JSON to Constraints struct
func (c *MIDIChallenge) unmarshalConstraints() error {
	if len(c.ConstraintsJSON) > 0 {
		return json.Unmarshal(c.ConstraintsJSON, &c.Constraints)
	}
	return nil
}

// CalculateStatus determines the current status based on dates
func (c *MIDIChallenge) CalculateStatus() MIDIChallengeStatus {
	now := time.Now()
	if now.Before(c.StartDate) {
		return MIDIChallengeStatusUpcoming
	}
	if now.After(c.EndDate) && now.Before(c.VotingEndDate) {
		return MIDIChallengeStatusVoting
	}
	if now.After(c.VotingEndDate) {
		return MIDIChallengeStatusEnded
	}
	return MIDIChallengeStatusActive
}

// MIDIChallengeEntry represents a user's submission to a MIDI challenge (R.2.2.1.2)
type MIDIChallengeEntry struct {
	ID          string        `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	ChallengeID string        `gorm:"not null;index" json:"challenge_id"`
	Challenge   MIDIChallenge `gorm:"foreignKey:ChallengeID" json:"challenge,omitempty"`
	UserID      string        `gorm:"not null;index" json:"user_id"`
	User        User          `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Audio and MIDI data
	AudioURL string     `gorm:"not null" json:"audio_url"`      // Link to AudioPost or separate storage
	PostID   *string    `gorm:"index" json:"post_id,omitempty"` // Optional link to AudioPost
	Post     *AudioPost `gorm:"foreignKey:PostID" json:"post,omitempty"`

	// MIDI data (can reference MIDIPattern or embed)
	MIDIPatternID *string      `gorm:"index" json:"midi_pattern_id,omitempty"`
	MIDIPattern   *MIDIPattern `gorm:"foreignKey:MIDIPatternID" json:"midi_pattern,omitempty"`

	// Voting
	VoteCount int `gorm:"default:0" json:"vote_count"`

	// Relationships
	Votes []MIDIChallengeVote `gorm:"foreignKey:EntryID" json:"votes,omitempty"`

	// GORM fields
	SubmittedAt time.Time      `json:"submitted_at"`
	CreatedAt   time.Time      `json:"created_at"`
	UpdatedAt   time.Time      `json:"updated_at"`
	DeletedAt   gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName overrides the default table name
func (MIDIChallengeEntry) TableName() string {
	return "midi_challenge_entries"
}

// MIDIChallengeVote represents a vote for a challenge entry (R.2.2.1.3)
type MIDIChallengeVote struct {
	ID          string             `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	ChallengeID string             `gorm:"not null;index" json:"challenge_id"`
	Challenge   MIDIChallenge      `gorm:"foreignKey:ChallengeID" json:"challenge,omitempty"`
	EntryID     string             `gorm:"not null;index" json:"entry_id"`
	Entry       MIDIChallengeEntry `gorm:"foreignKey:EntryID" json:"entry,omitempty"`
	VoterID     string             `gorm:"not null;index" json:"voter_id"`
	Voter       User               `gorm:"foreignKey:VoterID" json:"voter,omitempty"`

	// GORM fields
	VotedAt   time.Time      `json:"voted_at"`
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName overrides the default table name
func (MIDIChallengeVote) TableName() string {
	return "midi_challenge_votes"
}
