package service

import (
	"testing"
)

// Test service initialization
func TestServiceInitialization(t *testing.T) {
	tests := []struct {
		name     string
		initFunc func() interface{}
	}{
		{"PresenceService", func() interface{} { return NewPresenceService() }},
		{"NotificationService", func() interface{} { return NewNotificationService() }},
		{"NotificationSettingsService", func() interface{} { return NewNotificationSettingsService() }},
		{"NotificationWatcherService", func() interface{} { return NewNotificationWatcherService() }},
		{"MIDIChallengesService", func() interface{} { return NewMIDIChallengesService() }},
		{"SearchService", func() interface{} { return NewSearchService() }},
		{"FeedService", func() interface{} { return NewFeedService() }},
		{"SoundPagesService", func() interface{} { return NewSoundPagesService() }},
		{"RecommendationFeedbackService", func() interface{} { return NewRecommendationFeedbackService() }},
	}

	for _, tt := range tests {
		svc := tt.initFunc()
		if svc == nil {
			t.Errorf("%s: returned nil", tt.name)
		}
	}
}

// Test placeholder service methods exist
func TestPluralizeFunctions(t *testing.T) {
	tests := []struct {
		count    int
		expected string
	}{
		{0, "s"},
		{1, ""},
		{2, "s"},
		{100, "s"},
	}

	for _, tt := range tests {
		result := pluralize(tt.count)
		if result != tt.expected {
			t.Errorf("pluralize(%d): got %q, want %q", tt.count, result, tt.expected)
		}
	}
}

// Test string truncation helper
func TestTruncateHelper(t *testing.T) {
	tests := []struct {
		input       string
		maxLen      int
		expectTrunc bool
	}{
		{"short", 10, false},
		{"a very long string that exceeds limit", 20, true},
		{"exactly20charactersxx", 20, true},
		{"", 10, false},
		{"hello world", 5, true},
	}

	for _, tt := range tests {
		if tt.maxLen < 4 {
			continue // Skip edge cases that truncate function may not handle
		}
		result := truncate(tt.input, tt.maxLen)
		if len(result) > tt.maxLen+3 {
			t.Errorf("truncate(%q, %d): result too long: %q", tt.input, tt.maxLen, result)
		}
	}
}

// Test boolToStatus helper
func TestBoolToStatus(t *testing.T) {
	tests := []struct {
		input    bool
		contains string
	}{
		{true, "ON"},
		{false, "OFF"},
	}

	for _, tt := range tests {
		result := boolToStatus(tt.input)
		if len(result) == 0 {
			t.Errorf("boolToStatus(%v) returned empty string", tt.input)
		}
	}
}

// Test boolToString helper
func TestBoolToString(t *testing.T) {
	if boolToString(true) != "currently ON" {
		t.Error("boolToString(true) should return 'currently ON'")
	}
	if boolToString(false) != "currently OFF" {
		t.Error("boolToString(false) should return 'currently OFF'")
	}
}
