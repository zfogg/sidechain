package stream

import (
	"errors"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestMockStreamClientDefaults(t *testing.T) {
	mock := NewMockStreamClient()

	// Test default behavior - should not error
	err := mock.CreateUser("user123", "testuser")
	assert.NoError(t, err)
	assert.True(t, mock.AssertCalled("CreateUser"))

	// Test call tracking
	calls := mock.GetCallsForMethod("CreateUser")
	assert.Len(t, calls, 1)
	assert.Equal(t, "user123", calls[0].Args[0])
	assert.Equal(t, "testuser", calls[0].Args[1])
}

func TestMockStreamClientCreateToken(t *testing.T) {
	mock := NewMockStreamClient()

	expiration := time.Now().Add(time.Hour)
	token, err := mock.CreateToken("user123", expiration)

	assert.NoError(t, err)
	assert.Contains(t, token, "mock_token_user123")
	assert.True(t, mock.AssertCalled("CreateToken"))
}

func TestMockStreamClientCustomFunction(t *testing.T) {
	mock := NewMockStreamClient()

	// Configure custom behavior
	mock.CreateUserFunc = func(userID, username string) error {
		if userID == "blocked" {
			return errors.New("user blocked")
		}
		return nil
	}

	// Test blocked user
	err := mock.CreateUser("blocked", "blockeduser")
	assert.Error(t, err)
	assert.Equal(t, "user blocked", err.Error())

	// Test normal user
	err = mock.CreateUser("normal", "normaluser")
	assert.NoError(t, err)
}

func TestMockStreamClientDefaultError(t *testing.T) {
	mock := NewMockStreamClient()
	mock.DefaultError = errors.New("default error")

	err := mock.CreateUser("user123", "testuser")
	assert.Error(t, err)
	assert.Equal(t, "default error", err.Error())
}

func TestMockStreamClientReset(t *testing.T) {
	mock := NewMockStreamClient()

	mock.CreateUser("user1", "name1")
	mock.CreateUser("user2", "name2")
	assert.Len(t, mock.GetCalls(), 2)

	mock.Reset()
	assert.Len(t, mock.GetCalls(), 0)
	assert.True(t, mock.AssertNotCalled("CreateUser"))
}

func TestMockStreamClientCallCount(t *testing.T) {
	mock := NewMockStreamClient()

	mock.CreateUser("user1", "name1")
	mock.CreateUser("user2", "name2")
	mock.CreateUser("user3", "name3")

	assert.True(t, mock.AssertCallCount("CreateUser", 3))
	assert.False(t, mock.AssertCallCount("CreateUser", 2))
}

func TestMockStreamClientActivityCreation(t *testing.T) {
	mock := NewMockStreamClient()

	activity := &Activity{
		Actor:    "user:123",
		Verb:     "posted",
		Object:   "loop:abc",
		AudioURL: "https://example.com/loop.mp3",
		BPM:      128,
	}

	err := mock.CreateLoopActivity("user123", activity)
	assert.NoError(t, err)
	assert.NotEmpty(t, activity.ID) // Mock should set an ID
	assert.Contains(t, activity.ID, "mock_activity_user123")
}

func TestMockStreamClientFeedOperations(t *testing.T) {
	mock := NewMockStreamClient()

	// Configure custom timeline response
	mock.GetUserTimelineFunc = func(userID string, limit, offset int) ([]*Activity, error) {
		return []*Activity{
			{ID: "act1", Actor: "user:1", Verb: "posted"},
			{ID: "act2", Actor: "user:2", Verb: "posted"},
		}, nil
	}

	activities, err := mock.GetUserTimeline("user123", 10, 0)
	assert.NoError(t, err)
	assert.Len(t, activities, 2)
	assert.Equal(t, "act1", activities[0].ID)
}

func TestMockStreamClientFollowOperations(t *testing.T) {
	mock := NewMockStreamClient()

	// Follow
	err := mock.FollowUser("user1", "user2")
	assert.NoError(t, err)

	// Unfollow
	err = mock.UnfollowUser("user1", "user2")
	assert.NoError(t, err)

	// Check both were called
	assert.True(t, mock.AssertCalled("FollowUser"))
	assert.True(t, mock.AssertCalled("UnfollowUser"))
}

func TestMockStreamClientCheckIsFollowing(t *testing.T) {
	mock := NewMockStreamClient()

	// Default returns false
	isFollowing, err := mock.CheckIsFollowing("user1", "user2")
	assert.NoError(t, err)
	assert.False(t, isFollowing)

	// Configure to return true
	mock.CheckIsFollowingFunc = func(userID, targetUserID string) (bool, error) {
		return userID == "user1" && targetUserID == "user2", nil
	}

	isFollowing, err = mock.CheckIsFollowing("user1", "user2")
	assert.NoError(t, err)
	assert.True(t, isFollowing)
}

func TestMockStreamClientNotifications(t *testing.T) {
	mock := NewMockStreamClient()

	// Configure notification counts
	mock.GetNotificationCountsFunc = func(userID string) (unseen, unread int, err error) {
		return 5, 3, nil
	}

	unseen, unread, err := mock.GetNotificationCounts("user123")
	assert.NoError(t, err)
	assert.Equal(t, 5, unseen)
	assert.Equal(t, 3, unread)
}

func TestMockStreamClientReactions(t *testing.T) {
	mock := NewMockStreamClient()

	// Test add reaction
	err := mock.AddReaction("like", "user123", "activity456")
	assert.NoError(t, err)

	// Test add reaction with emoji
	err = mock.AddReactionWithEmoji("like", "user123", "activity456", "❤️")
	assert.NoError(t, err)

	// Test remove reaction
	err = mock.RemoveReaction("reaction789")
	assert.NoError(t, err)

	// Verify all were called
	assert.True(t, mock.AssertCalled("AddReaction"))
	assert.True(t, mock.AssertCalled("AddReactionWithEmoji"))
	assert.True(t, mock.AssertCalled("RemoveReaction"))
}

func TestMockStreamClientAggregatedFeeds(t *testing.T) {
	mock := NewMockStreamClient()

	// Configure aggregated timeline
	mock.GetAggregatedTimelineFunc = func(userID string, limit, offset int) (*AggregatedFeedResponse, error) {
		return &AggregatedFeedResponse{
			Groups: []*AggregatedGroup{
				{ID: "group1", Verb: "posted", ActivityCount: 5},
			},
		}, nil
	}

	resp, err := mock.GetAggregatedTimeline("user123", 10, 0)
	assert.NoError(t, err)
	assert.Len(t, resp.Groups, 1)
	assert.Equal(t, 5, resp.Groups[0].ActivityCount)
}

func TestMockStreamClientDeleteLoopActivity(t *testing.T) {
	mock := NewMockStreamClient()

	err := mock.DeleteLoopActivity("user123", "activity456")
	assert.NoError(t, err)
	assert.True(t, mock.AssertCalled("DeleteLoopActivity"))

	calls := mock.GetCallsForMethod("DeleteLoopActivity")
	assert.Equal(t, "user123", calls[0].Args[0])
	assert.Equal(t, "activity456", calls[0].Args[1])
}

func TestMockStreamClientConcurrency(t *testing.T) {
	mock := NewMockStreamClient()

	// Test that the mock is thread-safe
	done := make(chan bool, 10)

	for i := 0; i < 10; i++ {
		go func(id int) {
			mock.CreateUser(string(rune('a'+id)), "user")
			mock.GetCalls()
			done <- true
		}(i)
	}

	for i := 0; i < 10; i++ {
		<-done
	}

	assert.Equal(t, 10, len(mock.GetCalls()))
}
