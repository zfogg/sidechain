package stream

import (
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
)

// TestNewClient tests getstream.io client initialization
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
	assert.NotNil(t, client.FeedsClient(), "FeedsClient should not be nil")
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

// =============================================================================
// NOTIFICATION FEED TESTS
// =============================================================================

// TestNotificationFeedGroupConstants tests notification-related constants
func TestNotificationFeedGroupConstants(t *testing.T) {
	assert.Equal(t, "notification", FeedGroupNotification, "Notification feed group should be 'notification'")
	assert.Equal(t, "timeline_aggregated", FeedGroupTimelineAggregated, "Aggregated timeline feed group")
	assert.Equal(t, "trending", FeedGroupTrending, "Trending feed group")
	assert.Equal(t, "user_activity", FeedGroupUserActivity, "User activity feed group")
}

// TestNotificationVerbConstants tests notification verb constants
func TestNotificationVerbConstants(t *testing.T) {
	assert.Equal(t, "like", NotifVerbLike)
	assert.Equal(t, "follow", NotifVerbFollow)
	assert.Equal(t, "comment", NotifVerbComment)
	assert.Equal(t, "mention", NotifVerbMention)
	assert.Equal(t, "repost", NotifVerbRepost)
}

// TestNotificationGroupStruct tests NotificationGroup struct
func TestNotificationGroupStruct(t *testing.T) {
	activities := []*Activity{
		{
			ID:    "act1",
			Actor: "user:alice",
			Verb:  NotifVerbLike,
		},
		{
			ID:    "act2",
			Actor: "user:bob",
			Verb:  NotifVerbLike,
		},
	}

	group := &NotificationGroup{
		ID:            "group_123",
		Group:         "like_2024-01-15",
		Verb:          NotifVerbLike,
		ActivityCount: 2,
		ActorCount:    2,
		Activities:    activities,
		IsRead:        false,
		IsSeen:        false,
		CreatedAt:     "2024-01-15T10:00:00Z",
		UpdatedAt:     "2024-01-15T12:00:00Z",
	}

	assert.Equal(t, "group_123", group.ID)
	assert.Equal(t, "like_2024-01-15", group.Group)
	assert.Equal(t, NotifVerbLike, group.Verb)
	assert.Equal(t, 2, group.ActivityCount)
	assert.Equal(t, 2, group.ActorCount)
	assert.Len(t, group.Activities, 2)
	assert.False(t, group.IsRead)
	assert.False(t, group.IsSeen)
}

// TestNotificationResponseStruct tests NotificationResponse struct
func TestNotificationResponseStruct(t *testing.T) {
	resp := &NotificationResponse{
		Groups: []*NotificationGroup{
			{ID: "group1", Verb: NotifVerbLike},
			{ID: "group2", Verb: NotifVerbFollow},
		},
		Unseen: 5,
		Unread: 3,
	}

	assert.Len(t, resp.Groups, 2)
	assert.Equal(t, 5, resp.Unseen)
	assert.Equal(t, 3, resp.Unread)
}

// =============================================================================
// AGGREGATED FEED TESTS
// =============================================================================

// TestAggregatedGroupStruct tests AggregatedGroup struct
func TestAggregatedGroupStruct(t *testing.T) {
	activities := []*Activity{
		{ID: "act1", Actor: "user:alice", Verb: "posted"},
		{ID: "act2", Actor: "user:alice", Verb: "posted"},
		{ID: "act3", Actor: "user:alice", Verb: "posted"},
	}

	group := &AggregatedGroup{
		ID:            "agg_group_123",
		Group:         "user:alice_posted_2024-01-15",
		Verb:          "posted",
		ActivityCount: 3,
		ActorCount:    1,
		Activities:    activities,
		CreatedAt:     "2024-01-15T08:00:00Z",
		UpdatedAt:     "2024-01-15T16:00:00Z",
	}

	assert.Equal(t, "agg_group_123", group.ID)
	assert.Equal(t, "user:alice_posted_2024-01-15", group.Group)
	assert.Equal(t, "posted", group.Verb)
	assert.Equal(t, 3, group.ActivityCount)
	assert.Equal(t, 1, group.ActorCount)
	assert.Len(t, group.Activities, 3)
}

// TestAggregatedFeedResponseStruct tests AggregatedFeedResponse struct
func TestAggregatedFeedResponseStruct(t *testing.T) {
	resp := &AggregatedFeedResponse{
		Groups: []*AggregatedGroup{
			{ID: "group1", Verb: "posted", ActivityCount: 5},
			{ID: "group2", Verb: "posted", ActivityCount: 3},
		},
	}

	assert.Len(t, resp.Groups, 2)
	assert.Equal(t, 5, resp.Groups[0].ActivityCount)
	assert.Equal(t, 3, resp.Groups[1].ActivityCount)
}

// TestTruncateString tests the truncateString helper function
func TestTruncateString(t *testing.T) {
	// Test string shorter than max length
	short := truncateString("hello", 10)
	assert.Equal(t, "hello", short)

	// Test string equal to max length
	exact := truncateString("hello", 5)
	assert.Equal(t, "hello", exact)

	// Test string longer than max length
	long := truncateString("hello world this is a long string", 15)
	assert.Equal(t, "hello world ...", long)
	assert.Len(t, long, 15)
}

// TestExtractUserIDFromFeed tests the extractUserIDFromFeed helper
func TestExtractUserIDFromFeed(t *testing.T) {
	// Test timeline feed ID format
	userID := extractUserIDFromFeed("timeline:user123")
	assert.Equal(t, "user123", userID)

	// Test user feed ID format
	userID2 := extractUserIDFromFeed("user:producer456")
	assert.Equal(t, "producer456", userID2)

	// Test ID without colon
	noColon := extractUserIDFromFeed("justanid")
	assert.Equal(t, "justanid", noColon)
}

// =============================================================================
// INTEGRATION TESTS (require getstream.io credentials)
// =============================================================================

// TestNotificationsIntegration tests notification methods with real getstream.io
func TestNotificationsIntegration(t *testing.T) {
	if os.Getenv("STREAM_API_KEY") == "" || os.Getenv("STREAM_API_SECRET") == "" {
		t.Skip("STREAM_API_KEY and STREAM_API_SECRET not set, skipping integration test")
	}

	client, err := NewClient()
	assert.NoError(t, err)

	// Note: These tests require the "notification" feed group to be configured
	// in the getstream.io dashboard with aggregation_format set

	testUserID := "test_notif_user_" + os.Getenv("TEST_RUN_ID")
	if testUserID == "test_notif_user_" {
		testUserID = "test_notif_user_default"
	}

	// Test getting notification counts (should work even with empty feed)
	unseen, unread, err := client.GetNotificationCounts(testUserID)
	if err != nil {
		// Feed group might not exist - skip rest of test
		t.Skipf("Notification feed not configured: %v", err)
	}
	assert.GreaterOrEqual(t, unseen, 0)
	assert.GreaterOrEqual(t, unread, 0)

	// Test getting notifications
	notifs, err := client.GetNotifications(testUserID, 10, 0)
	if err != nil {
		t.Skipf("Failed to get notifications: %v", err)
	}
	assert.NotNil(t, notifs)
	assert.NotNil(t, notifs.Groups)
}

// TestAggregatedFeedsIntegration tests aggregated feed methods with real getstream.io
func TestAggregatedFeedsIntegration(t *testing.T) {
	if os.Getenv("STREAM_API_KEY") == "" || os.Getenv("STREAM_API_SECRET") == "" {
		t.Skip("STREAM_API_KEY and STREAM_API_SECRET not set, skipping integration test")
	}

	client, err := NewClient()
	assert.NoError(t, err)

	// Note: These tests require aggregated feed groups to be configured
	// in the getstream.io dashboard

	testUserID := "test_agg_user_" + os.Getenv("TEST_RUN_ID")
	if testUserID == "test_agg_user_" {
		testUserID = "test_agg_user_default"
	}

	// Test getting trending feed
	trending, err := client.GetTrendingFeed(10, 0)
	if err != nil {
		// Feed group might not exist - just log it
		t.Logf("Trending feed not configured: %v", err)
	} else {
		assert.NotNil(t, trending)
		assert.NotNil(t, trending.Groups)
	}

	// Test getting user activity summary
	activity, err := client.GetUserActivitySummary(testUserID, 10)
	if err != nil {
		t.Logf("User activity feed not configured: %v", err)
	} else {
		assert.NotNil(t, activity)
		assert.NotNil(t, activity.Groups)
	}
}
