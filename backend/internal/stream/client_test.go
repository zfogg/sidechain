package stream

import (
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
)

// TestNewClient tests Stream.io client initialization
func TestNewClient(t *testing.T) {
	// Test without credentials - should fail
	originalKey := os.Getenv("STREAM_API_KEY")
	originalSecret := os.Getenv("STREAM_API_SECRET")

	os.Setenv("STREAM_API_KEY", "")
	os.Setenv("STREAM_API_SECRET", "")
	defer func() {
		os.Setenv("STREAM_API_KEY", originalKey)
		os.Setenv("STREAM_API_SECRET", originalSecret)
	}()

	_, err := NewClient()
	assert.Error(t, err, "Should fail without credentials")
	assert.Contains(t, err.Error(), "STREAM_API_KEY")
}

// TestNewClientWithCredentials tests client initialization with credentials
func TestNewClientWithCredentials(t *testing.T) {
	if os.Getenv("STREAM_API_KEY") == "" || os.Getenv("STREAM_API_SECRET") == "" {
		t.Skip("STREAM_API_KEY and STREAM_API_SECRET not set, skipping")
	}

	client, err := NewClient()
	assert.NoError(t, err, "Should create client with valid credentials")
	assert.NotNil(t, client, "Client should not be nil")
	assert.NotNil(t, client.FeedsClient, "FeedsClient should not be nil")
	assert.NotNil(t, client.ChatClient, "ChatClient should not be nil")
}

// TestActivityStruct tests Activity struct creation and field access
func TestActivityStruct(t *testing.T) {
	activity := &Activity{
		ID:           "test_id_123",
		Actor:        "user:testuser",
		Verb:         "posted",
		Object:       "My awesome loop",
		ForeignID:    "loop:test123",
		Time:         "2024-01-15T10:30:00Z",
		AudioURL:     "https://cdn.example.com/test.mp3",
		BPM:          140,
		Key:          "F# minor",
		DAW:          "FL Studio",
		DurationBars: 16,
		Genre:        []string{"Techno", "Electronic"},
		Waveform:     "<svg>waveform data</svg>",
		Extra: map[string]interface{}{
			"custom_field": "custom_value",
		},
	}

	// Verify all fields
	assert.Equal(t, "test_id_123", activity.ID)
	assert.Equal(t, "user:testuser", activity.Actor)
	assert.Equal(t, "posted", activity.Verb)
	assert.Equal(t, "My awesome loop", activity.Object)
	assert.Equal(t, "loop:test123", activity.ForeignID)
	assert.Equal(t, "2024-01-15T10:30:00Z", activity.Time)
	assert.Equal(t, "https://cdn.example.com/test.mp3", activity.AudioURL)
	assert.Equal(t, 140, activity.BPM)
	assert.Equal(t, "F# minor", activity.Key)
	assert.Equal(t, "FL Studio", activity.DAW)
	assert.Equal(t, 16, activity.DurationBars)
	assert.Contains(t, activity.Genre, "Techno")
	assert.Contains(t, activity.Genre, "Electronic")
	assert.Equal(t, "<svg>waveform data</svg>", activity.Waveform)
	assert.Equal(t, "custom_value", activity.Extra["custom_field"])
}

// TestFeedGroupConstants tests feed group constant values
func TestFeedGroupConstants(t *testing.T) {
	assert.Equal(t, "user", FeedGroupUser, "User feed group should be 'user'")
	assert.Equal(t, "timeline", FeedGroupTimeline, "Timeline feed group should be 'timeline'")
	assert.Equal(t, "global", FeedGroupGlobal, "Global feed group should be 'global'")
}

// TestActivityWithMinimalFields tests Activity with only required fields
func TestActivityWithMinimalFields(t *testing.T) {
	activity := &Activity{
		AudioURL: "https://cdn.example.com/loop.mp3",
		BPM:      128,
	}

	assert.Empty(t, activity.ID)
	assert.Empty(t, activity.Actor)
	assert.Empty(t, activity.Key)
	assert.Empty(t, activity.DAW)
	assert.Nil(t, activity.Genre)
	assert.Equal(t, "https://cdn.example.com/loop.mp3", activity.AudioURL)
	assert.Equal(t, 128, activity.BPM)
}

// TestActivityGenreSlice tests genre slice operations
func TestActivityGenreSlice(t *testing.T) {
	activity := &Activity{
		Genre: []string{"House"},
	}

	// Add more genres
	activity.Genre = append(activity.Genre, "Electronic")
	activity.Genre = append(activity.Genre, "Deep House")

	assert.Len(t, activity.Genre, 3)
	assert.Contains(t, activity.Genre, "House")
	assert.Contains(t, activity.Genre, "Electronic")
	assert.Contains(t, activity.Genre, "Deep House")
}

// TestActivityExtraMap tests extra custom data map
func TestActivityExtraMap(t *testing.T) {
	activity := &Activity{
		Extra: map[string]interface{}{
			"plugins_used": []string{"Serum", "Massive"},
			"collab_with":  "user:producer123",
			"is_remix":     true,
			"sample_count": 4,
		},
	}

	assert.NotNil(t, activity.Extra)
	assert.Len(t, activity.Extra, 4)

	plugins, ok := activity.Extra["plugins_used"].([]string)
	assert.True(t, ok)
	assert.Contains(t, plugins, "Serum")

	isRemix, ok := activity.Extra["is_remix"].(bool)
	assert.True(t, ok)
	assert.True(t, isRemix)
}
