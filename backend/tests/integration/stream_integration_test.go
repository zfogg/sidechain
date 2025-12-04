//go:build integration
// +build integration

package integration

import (
	"os"
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zfogg/sidechain/backend/internal/stream"
)

// skipIfNoCredentials skips the test if Stream.io credentials are not available
func skipIfNoCredentials(t *testing.T) {
	if os.Getenv("STREAM_API_KEY") == "" || os.Getenv("STREAM_API_SECRET") == "" {
		t.Skip("STREAM_API_KEY and STREAM_API_SECRET not set, skipping integration test")
	}
}

// TestStreamClientInitialization tests that Stream.io client initializes correctly
func TestStreamClientInitialization(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err, "Should create Stream.io client with valid credentials")
	require.NotNil(t, client, "Client should not be nil")
	require.NotNil(t, client.FeedsClient, "FeedsClient should not be nil")
	require.NotNil(t, client.ChatClient, "ChatClient should not be nil")
}

// TestStreamUserCreation tests creating a user in Stream.io Chat
func TestStreamUserCreation(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err)

	// Create a test user with unique ID
	testUserID := "test_user_" + uuid.New().String()[:8]
	testUsername := "TestProducer_" + uuid.New().String()[:4]

	err = client.CreateUser(testUserID, testUsername)
	require.NoError(t, err, "Should create Stream.io user")

	// Creating the same user again should not error (upsert behavior)
	err = client.CreateUser(testUserID, testUsername)
	assert.NoError(t, err, "Upsert should not error on existing user")
}

// TestStreamTokenCreation tests JWT token generation
func TestStreamTokenCreation(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err)

	testUserID := "test_user_" + uuid.New().String()[:8]

	// First create the user
	err = client.CreateUser(testUserID, "TokenTestUser")
	require.NoError(t, err)

	// Generate token
	token, err := client.CreateToken(testUserID)
	require.NoError(t, err, "Should create token for user")
	require.NotEmpty(t, token, "Token should not be empty")

	// Token should be a valid JWT format (header.payload.signature)
	assert.Contains(t, token, ".", "Token should be in JWT format")
}

// TestStreamLoopActivityCreation tests creating a loop activity
func TestStreamLoopActivityCreation(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err)

	testUserID := "test_producer_" + uuid.New().String()[:8]

	activity := &stream.Activity{
		Actor:        "user:" + testUserID,
		Verb:         "posted",
		Object:       "loop:test_" + uuid.New().String()[:8],
		AudioURL:     "https://cdn.sidechain.app/test/demo.mp3",
		BPM:          128,
		Key:          "A minor",
		DAW:          "Ableton Live",
		DurationBars: 8,
		Genre:        []string{"House", "Electronic"},
	}

	err = client.CreateLoopActivity(testUserID, activity)
	require.NoError(t, err, "Should create loop activity")
	assert.NotEmpty(t, activity.ID, "Activity should have ID after creation")
	assert.NotEmpty(t, activity.Time, "Activity should have time after creation")
}

// TestStreamFollowUnfollow tests follow and unfollow operations
func TestStreamFollowUnfollow(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err)

	// Create two test users
	user1 := "follower_" + uuid.New().String()[:8]
	user2 := "followee_" + uuid.New().String()[:8]

	// Test follow
	err = client.FollowUser(user1, user2)
	require.NoError(t, err, "Should follow user")

	// Give Stream.io time to process
	time.Sleep(500 * time.Millisecond)

	// Test unfollow
	err = client.UnfollowUser(user1, user2)
	assert.NoError(t, err, "Should unfollow user")
}

// TestStreamReactions tests adding reactions to activities
func TestStreamReactions(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err)

	// First create a real activity to react to
	testUserID := "reactor_" + uuid.New().String()[:8]

	activity := &stream.Activity{
		Actor:        "user:" + testUserID,
		Verb:         "posted",
		Object:       "loop:reaction_test_" + uuid.New().String()[:8],
		AudioURL:     "https://cdn.sidechain.app/test/reaction_test.mp3",
		BPM:          128,
		Key:          "C major",
		DurationBars: 8,
	}

	err = client.CreateLoopActivity(testUserID, activity)
	require.NoError(t, err, "Should create activity for reaction test")
	require.NotEmpty(t, activity.ID, "Activity should have ID")

	// Test like reaction
	err = client.AddReaction("like", testUserID, activity.ID)
	require.NoError(t, err, "Should add like reaction")

	// Test emoji reaction
	err = client.AddReactionWithEmoji("fire", testUserID, activity.ID, "🔥")
	assert.NoError(t, err, "Should add emoji reaction")

	// Note: RemoveReaction now requires the reaction ID, not activity ID
	// For now we skip the remove test since we'd need to track the reaction ID
}

// TestStreamGetUserTimeline tests fetching user timeline
func TestStreamGetUserTimeline(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err)

	testUserID := "timeline_user_" + uuid.New().String()[:8]

	// Get timeline (may be empty for new user)
	activities, err := client.GetUserTimeline(testUserID, 10, 0)
	require.NoError(t, err, "Should fetch timeline without error")

	// Activities is a slice, which may be empty but should not be nil
	require.NotNil(t, activities, "Activities should not be nil")
}

// TestStreamGetGlobalFeed tests fetching global feed
func TestStreamGetGlobalFeed(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err)

	// Get global feed
	activities, err := client.GetGlobalFeed(20, 0)
	require.NoError(t, err, "Should fetch global feed without error")

	// Activities is a slice, which may be empty but should not be nil
	require.NotNil(t, activities, "Activities should not be nil")
}

// TestStreamEndToEndWorkflow tests a complete workflow
func TestStreamEndToEndWorkflow(t *testing.T) {
	skipIfNoCredentials(t)

	client, err := stream.NewClient()
	require.NoError(t, err)

	// Setup: Create two users
	producerID := "producer_" + uuid.New().String()[:8]
	followerID := "follower_" + uuid.New().String()[:8]

	err = client.CreateUser(producerID, "TestProducer")
	require.NoError(t, err)

	err = client.CreateUser(followerID, "TestFollower")
	require.NoError(t, err)

	// Step 1: Follower follows Producer
	err = client.FollowUser(followerID, producerID)
	require.NoError(t, err)

	// Step 2: Producer creates a loop
	activity := &stream.Activity{
		Actor:        "user:" + producerID,
		Verb:         "posted",
		Object:       "loop:e2e_test_" + uuid.New().String()[:8],
		AudioURL:     "https://cdn.sidechain.app/e2e/test_loop.mp3",
		BPM:          140,
		Key:          "F# minor",
		DAW:          "FL Studio",
		DurationBars: 16,
		Genre:        []string{"Techno"},
	}

	err = client.CreateLoopActivity(producerID, activity)
	require.NoError(t, err)

	// Step 3: Follower reacts to the loop
	if activity.ID != "" {
		err = client.AddReactionWithEmoji("fire", followerID, activity.ID, "🔥")
		assert.NoError(t, err)
	}

	// Step 4: Fetch follower's timeline (should eventually contain producer's activity)
	// Note: Stream.io has eventual consistency, so this might not immediately show
	activities, err := client.GetUserTimeline(followerID, 10, 0)
	require.NoError(t, err)
	require.NotNil(t, activities)

	// Cleanup: Unfollow
	err = client.UnfollowUser(followerID, producerID)
	assert.NoError(t, err)
}

// BenchmarkStreamActivityCreation benchmarks activity creation
func BenchmarkStreamActivityCreation(b *testing.B) {
	if os.Getenv("STREAM_API_KEY") == "" || os.Getenv("STREAM_API_SECRET") == "" {
		b.Skip("STREAM_API_KEY and STREAM_API_SECRET not set")
	}

	client, err := stream.NewClient()
	if err != nil {
		b.Fatalf("Failed to create client: %v", err)
	}

	testUserID := "bench_user_" + uuid.New().String()[:8]

	b.ResetTimer()

	for i := 0; i < b.N; i++ {
		activity := &stream.Activity{
			Actor:        "user:" + testUserID,
			Verb:         "posted",
			Object:       "loop:bench_" + uuid.New().String()[:8],
			AudioURL:     "https://cdn.sidechain.app/bench/test.mp3",
			BPM:          128,
			Key:          "C major",
			DurationBars: 8,
			Genre:        []string{"Electronic"},
		}

		err := client.CreateLoopActivity(testUserID, activity)
		if err != nil {
			b.Errorf("Activity creation failed: %v", err)
		}
	}
}
