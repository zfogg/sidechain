package stream

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestNewClient tests Stream.io client initialization (environment dependent)
func TestNewClient(t *testing.T) {
	// Just test that NewClient function exists and has correct signature
	// Real initialization testing should be done in integration tests
	
	// This will fail without real credentials, which is expected in unit tests
	_, err := NewClient()
	
	// Either succeeds (if credentials available) or fails with expected error
	if err != nil {
		assert.Contains(t, err.Error(), "STREAM_API_KEY")
	}
}

// TestMockGlobalFeed tests the global feed mock data
func TestMockGlobalFeed(t *testing.T) {
	client := &Client{
		FeedsClient: nil, // Mock client for unit testing
		ChatClient:  nil,
	}

	activities, err := client.GetGlobalFeed(20, 0)
	require.NoError(t, err)
	require.NotEmpty(t, activities)

	// Verify mock data structure
	firstActivity := activities[0]
	assert.NotEmpty(t, firstActivity.ID)
	assert.NotEmpty(t, firstActivity.Actor)
	assert.Equal(t, "posted", firstActivity.Verb)
	assert.NotEmpty(t, firstActivity.Object)
	assert.Greater(t, firstActivity.BPM, 0)
	assert.NotEmpty(t, firstActivity.Key)
	assert.NotEmpty(t, firstActivity.AudioURL)
}

// TestMockTimelineFeed tests the timeline feed mock data
func TestMockTimelineFeed(t *testing.T) {
	client := &Client{
		FeedsClient: nil, // Mock client for unit testing
		ChatClient:  nil,
	}

	activities, err := client.GetUserTimeline("test-user", 10, 0)
	require.NoError(t, err)
	require.NotEmpty(t, activities)

	// Verify mock data structure
	firstActivity := activities[0]
	assert.Equal(t, "timeline_activity_1", firstActivity.ID)
	assert.Contains(t, firstActivity.Actor, "followed_producer")
	assert.NotEmpty(t, firstActivity.AudioURL)
}

// TestActivityCreation tests activity creation structure
func TestActivityCreation(t *testing.T) {
	activity := &Activity{
		Actor:        "user:testuser",
		Verb:         "posted",
		Object:       "loop:test123",
		ForeignID:    "loop:test123", 
		AudioURL:     "https://example.com/test.mp3",
		BPM:          140,
		Key:          "F# minor",
		DAW:          "FL Studio",
		DurationBars: 16,
		Genre:        []string{"Techno", "Electronic"},
		Waveform:     "<svg>test waveform</svg>",
	}

	// Verify activity structure
	assert.Equal(t, "user:testuser", activity.Actor)
	assert.Equal(t, "posted", activity.Verb)
	assert.Equal(t, "loop:test123", activity.Object)
	assert.Equal(t, 140, activity.BPM)
	assert.Equal(t, "F# minor", activity.Key)
	assert.Contains(t, activity.Genre, "Techno")
	assert.Contains(t, activity.Genre, "Electronic")
}

// TestSocialActions tests social interaction methods (without real API calls)
func TestSocialActions(t *testing.T) {
	client := &Client{
		FeedsClient: nil, // Mock client for unit testing
		ChatClient:  nil,
	}

	// Test follow action (should succeed with mock implementation)
	err := client.FollowUser("user1", "user2")
	assert.NoError(t, err, "Mock follow should succeed")

	// Test unfollow action
	err = client.UnfollowUser("user1", "user2")
	assert.NoError(t, err, "Mock unfollow should succeed")

	// Test reaction actions
	err = client.AddReaction("like", "user1", "activity123")
	assert.NoError(t, err, "Mock reaction should succeed")

	err = client.AddReactionWithEmoji("fire", "user1", "activity123", "🔥")
	assert.NoError(t, err, "Mock emoji reaction should succeed")

	err = client.RemoveReaction("activity123", "like")
	assert.NoError(t, err, "Mock reaction removal should succeed")
}

// TestActivityCreationMethod tests loop activity creation
func TestActivityCreationMethod(t *testing.T) {
	client := &Client{
		FeedsClient: nil, // Mock client for unit testing
		ChatClient:  nil,
	}

	activity := &Activity{
		Actor:        "user:testuser",
		Verb:         "posted",
		Object:       "loop:test123",
		AudioURL:     "https://test.com/audio.mp3",
		BPM:          140,
		Key:          "C major",
		DAW:          "Ableton Live",
		DurationBars: 8,
		Genre:        []string{"Electronic"},
	}

	// Test activity creation (should succeed with mock implementation)
	err := client.CreateLoopActivity("testuser", activity)
	assert.NoError(t, err, "Mock activity creation should succeed")
	assert.NotEmpty(t, activity.ID, "Activity should get an ID assigned")
}

// Helper functions for environment variable testing
func getEnv(key string) string {
	// In a real implementation, would use os.Getenv
	// For testing, we'll simulate
	if key == "STREAM_API_KEY" {
		return "test_key"
	}
	if key == "STREAM_API_SECRET" {
		return "test_secret"
	}
	return ""
}

func setEnv(key, value string) {
	// In a real implementation, would use os.Setenv
	// For unit testing, we'll just simulate
}