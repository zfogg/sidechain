package models

import (
	"time"

	"gorm.io/gorm"
)

// MIDIEvent represents a single MIDI note event
type MIDIEvent struct {
	Time     float64 `json:"time"`     // Relative time in seconds from start
	Type     string  `json:"type"`     // "note_on" or "note_off"
	Note     int     `json:"note"`     // MIDI note number (0-127)
	Velocity int     `json:"velocity"` // Note velocity (0-127)
	Channel  int     `json:"channel"`  // MIDI channel (0-15)
}

// MIDIData represents MIDI event data for serialization/API responses
// This is the embedded struct used in API requests and for JSONB storage
type MIDIData struct {
	Events        []MIDIEvent `json:"events"`
	TotalTime     float64     `json:"total_time"`     // Total duration in seconds
	Tempo         int         `json:"tempo"`          // BPM
	TimeSignature []int       `json:"time_signature"` // [numerator, denominator], e.g., [4, 4]
}

// MIDIPattern represents a standalone MIDI pattern in the database
// This is the first-class MIDI resource that can be shared, downloaded, and referenced
type MIDIPattern struct {
	ID     string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID string `gorm:"not null;index" json:"user_id"`
	User   User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Metadata
	Name        string `json:"name"`        // Optional display name (e.g., "Chord Progression A")
	Description string `json:"description"` // Optional description

	// MIDI data
	Events        []MIDIEvent `gorm:"type:jsonb;serializer:json" json:"events"`
	TotalTime     float64     `json:"total_time"`                              // Duration in seconds
	Tempo         int         `json:"tempo"`                                   // BPM
	TimeSignature []int       `gorm:"type:jsonb;serializer:json" json:"time_signature"` // [numerator, denominator]
	Key           string      `json:"key"`                                     // Detected or user-specified musical key

	// Visibility and analytics
	IsPublic      bool `gorm:"default:true" json:"is_public"`
	DownloadCount int  `gorm:"default:0" json:"download_count"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName overrides the default table name to avoid GORM's pluralization issues with "MIDI"
func (MIDIPattern) TableName() string {
	return "midi_patterns"
}

// ToMIDIData converts a MIDIPattern to MIDIData for API responses
func (p *MIDIPattern) ToMIDIData() *MIDIData {
	return &MIDIData{
		Events:        p.Events,
		TotalTime:     p.TotalTime,
		Tempo:         p.Tempo,
		TimeSignature: p.TimeSignature,
	}
}

// FromMIDIData creates a new MIDIPattern from MIDIData
func FromMIDIData(data *MIDIData, userID string) *MIDIPattern {
	return &MIDIPattern{
		UserID:        userID,
		Events:        data.Events,
		TotalTime:     data.TotalTime,
		Tempo:         data.Tempo,
		TimeSignature: data.TimeSignature,
		IsPublic:      true,
	}
}

// Validate checks if the MIDI pattern has valid data
func (p *MIDIPattern) Validate() error {
	if p.UserID == "" {
		return ErrMIDIInvalidUserID
	}
	if p.Tempo <= 0 {
		return ErrMIDIInvalidTempo
	}
	if len(p.TimeSignature) != 2 || p.TimeSignature[0] <= 0 || p.TimeSignature[1] <= 0 {
		return ErrMIDIInvalidTimeSignature
	}
	// Events can be empty (silent pattern)
	for _, event := range p.Events {
		if err := event.Validate(); err != nil {
			return err
		}
	}
	return nil
}

// Validate checks if a MIDI event has valid data
func (e *MIDIEvent) Validate() error {
	if e.Type != "note_on" && e.Type != "note_off" {
		return ErrMIDIInvalidEventType
	}
	if e.Note < 0 || e.Note > 127 {
		return ErrMIDIInvalidNote
	}
	if e.Velocity < 0 || e.Velocity > 127 {
		return ErrMIDIInvalidVelocity
	}
	if e.Channel < 0 || e.Channel > 15 {
		return ErrMIDIInvalidChannel
	}
	if e.Time < 0 {
		return ErrMIDIInvalidTime
	}
	return nil
}

// MIDI validation errors
type MIDIError string

func (e MIDIError) Error() string { return string(e) }

const (
	ErrMIDIInvalidUserID        MIDIError = "midi: user_id is required"
	ErrMIDIInvalidTempo         MIDIError = "midi: tempo must be greater than 0"
	ErrMIDIInvalidTimeSignature MIDIError = "midi: time_signature must be [numerator, denominator] with positive values"
	ErrMIDIInvalidEventType     MIDIError = "midi: event type must be 'note_on' or 'note_off'"
	ErrMIDIInvalidNote          MIDIError = "midi: note must be between 0 and 127"
	ErrMIDIInvalidVelocity      MIDIError = "midi: velocity must be between 0 and 127"
	ErrMIDIInvalidChannel       MIDIError = "midi: channel must be between 0 and 15"
	ErrMIDIInvalidTime          MIDIError = "midi: time must be non-negative"
)
