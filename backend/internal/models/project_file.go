package models

import (
	"time"

	"gorm.io/gorm"
)

// ProjectFile represents a DAW project file (Ableton .als, FL Studio .flp, etc.)
type ProjectFile struct {
	ID        string `gorm:"primaryKey;type:uuid;default:gen_random_uuid()" json:"id"`
	UserID    string `gorm:"not null;index" json:"user_id"`
	User      User   `gorm:"foreignKey:UserID" json:"user,omitempty"`

	// Optional association with a post
	AudioPostID *string    `gorm:"index" json:"audio_post_id,omitempty"`
	AudioPost   *AudioPost `gorm:"foreignKey:AudioPostID" json:"audio_post,omitempty"`

	// File metadata
	Filename    string `gorm:"not null" json:"filename"`              // Original filename (e.g., "My Track.als")
	FileURL     string `gorm:"not null" json:"file_url"`              // CDN URL for download
	DAWType     string `gorm:"not null;index" json:"daw_type"`        // "ableton", "fl_studio", "logic", etc.
	FileSize    int64  `gorm:"not null" json:"file_size"`             // Size in bytes
	Description string `json:"description"`                           // Optional description

	// Visibility
	IsPublic bool `gorm:"default:true" json:"is_public"`

	// Analytics
	DownloadCount int `gorm:"default:0" json:"download_count"`

	// GORM fields
	CreatedAt time.Time      `json:"created_at"`
	UpdatedAt time.Time      `json:"updated_at"`
	DeletedAt gorm.DeletedAt `gorm:"index" json:"-"`
}

// TableName overrides the default table name
func (ProjectFile) TableName() string {
	return "project_files"
}

// DAW type constants
const (
	DAWTypeAbleton   = "ableton"
	DAWTypeFLStudio  = "fl_studio"
	DAWTypeLogic     = "logic"
	DAWTypeProTools  = "pro_tools"
	DAWTypeCubase    = "cubase"
	DAWTypeStudioOne = "studio_one"
	DAWTypeReaper    = "reaper"
	DAWTypeBitwig    = "bitwig"
	DAWTypeOther     = "other"
)

// ValidDAWTypes lists all valid DAW types
var ValidDAWTypes = []string{
	DAWTypeAbleton,
	DAWTypeFLStudio,
	DAWTypeLogic,
	DAWTypeProTools,
	DAWTypeCubase,
	DAWTypeStudioOne,
	DAWTypeReaper,
	DAWTypeBitwig,
	DAWTypeOther,
}

// ProjectFileExtensions maps file extensions to DAW types
var ProjectFileExtensions = map[string]string{
	".als":    DAWTypeAbleton,   // Ableton Live Set
	".alp":    DAWTypeAbleton,   // Ableton Live Pack
	".flp":    DAWTypeFLStudio,  // FL Studio Project
	".logic":  DAWTypeLogic,     // Logic Pro (folder bundle)
	".logicx": DAWTypeLogic,     // Logic Pro X (folder bundle)
	".ptx":    DAWTypeProTools,  // Pro Tools Session
	".ptf":    DAWTypeProTools,  // Pro Tools Session (older)
	".cpr":    DAWTypeCubase,    // Cubase Project
	".song":   DAWTypeStudioOne, // Studio One Song
	".rpp":    DAWTypeReaper,    // REAPER Project
	".bwproject": DAWTypeBitwig, // Bitwig Project
}

// MaxProjectFileSize is the maximum allowed project file size (50MB)
const MaxProjectFileSize = 50 * 1024 * 1024

// Validate checks if the project file has valid data
func (p *ProjectFile) Validate() error {
	if p.UserID == "" {
		return ProjectFileError("project_file: user_id is required")
	}
	if p.Filename == "" {
		return ProjectFileError("project_file: filename is required")
	}
	if p.FileURL == "" {
		return ProjectFileError("project_file: file_url is required")
	}
	if p.DAWType == "" {
		return ProjectFileError("project_file: daw_type is required")
	}
	if !isValidDAWType(p.DAWType) {
		return ProjectFileError("project_file: invalid daw_type")
	}
	if p.FileSize <= 0 {
		return ProjectFileError("project_file: file_size must be positive")
	}
	if p.FileSize > MaxProjectFileSize {
		return ProjectFileError("project_file: file exceeds maximum size (50MB)")
	}
	return nil
}

func isValidDAWType(dawType string) bool {
	for _, valid := range ValidDAWTypes {
		if dawType == valid {
			return true
		}
	}
	return false
}

// ProjectFileError represents a project file validation error
type ProjectFileError string

func (e ProjectFileError) Error() string { return string(e) }

// GetDAWTypeFromExtension returns the DAW type based on file extension
func GetDAWTypeFromExtension(filename string) string {
	for ext, dawType := range ProjectFileExtensions {
		if len(filename) > len(ext) && filename[len(filename)-len(ext):] == ext {
			return dawType
		}
	}
	return DAWTypeOther
}

// GetDAWDisplayName returns a human-readable DAW name
func GetDAWDisplayName(dawType string) string {
	switch dawType {
	case DAWTypeAbleton:
		return "Ableton Live"
	case DAWTypeFLStudio:
		return "FL Studio"
	case DAWTypeLogic:
		return "Logic Pro"
	case DAWTypeProTools:
		return "Pro Tools"
	case DAWTypeCubase:
		return "Cubase"
	case DAWTypeStudioOne:
		return "Studio One"
	case DAWTypeReaper:
		return "REAPER"
	case DAWTypeBitwig:
		return "Bitwig Studio"
	default:
		return "Unknown DAW"
	}
}
